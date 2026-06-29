#include "SceneRuntime.h"

#include "SceneNode.h"

void SceneRuntime::invalidateRuntimeVisibilityHierarchy()
{
	_runtimeVisibilityHierarchyDirty = true;
	_runtimeVisibilityPrepared = false;
	_runtimeVisibilityRootIndex = -1;
	_runtimeVisibilityMeshStoreCount = static_cast<int>(_meshStore.size());
	_runtimeVisibilityBoundsRevision = 0;
	_runtimeVisibilityNodes.clear();
	_runtimeBaseVisibleMask.clear();
}

int SceneRuntime::buildRuntimeVisibilityNodeRecursive(const SceneNode* node,
                                                      const QHash<QUuid, int>& meshIndexByUuid)
{
	if (!node)
		return -1;

	const int nodeIndex = _runtimeVisibilityNodes.size();
	_runtimeVisibilityNodes.push_back(RuntimeVisibilityNode{});
	_runtimeVisibilityNodes[nodeIndex].sceneNode = node;
	_runtimeVisibilityNodes[nodeIndex].meshIndices.reserve(node->meshUuids.size());

	for (const QUuid& uuid : node->meshUuids)
	{
		const auto it = meshIndexByUuid.find(uuid);
		if (it != meshIndexByUuid.end())
			_runtimeVisibilityNodes[nodeIndex].meshIndices.push_back(it.value());
	}

	_runtimeVisibilityNodes[nodeIndex].children.reserve(node->children.size());
	for (const SceneNode* child : node->children)
	{
		const int childIndex = buildRuntimeVisibilityNodeRecursive(child, meshIndexByUuid);
		if (childIndex >= 0)
			_runtimeVisibilityNodes[nodeIndex].children.push_back(childIndex);
	}

	return nodeIndex;
}

void SceneRuntime::rebuildRuntimeVisibilityHierarchy(const SceneNode* root)
{
	_runtimeVisibilityNodes.clear();
	_runtimeVisibilityRootIndex = -1;
	_runtimeVisibilityPrepared = false;

	if (!root)
	{
		_runtimeVisibilityHierarchyDirty = false;
		_runtimeVisibilityMeshStoreCount = static_cast<int>(_meshStore.size());
		_runtimeVisibilityBoundsRevision = 0;
		return;
	}

	QHash<QUuid, int> meshIndexByUuid;
	meshIndexByUuid.reserve(static_cast<int>(_meshStore.size()));
	for (int meshIndex = 0; meshIndex < static_cast<int>(_meshStore.size()); ++meshIndex)
	{
		if (SceneMesh* mesh = meshAt(static_cast<size_t>(meshIndex)))
			meshIndexByUuid.insert(mesh->uuid(), meshIndex);
	}

	_runtimeVisibilityRootIndex = buildRuntimeVisibilityNodeRecursive(root, meshIndexByUuid);
	_runtimeVisibilityHierarchyDirty = false;
	_runtimeVisibilityMeshStoreCount = static_cast<int>(_meshStore.size());
	_runtimeVisibilityBoundsRevision = 0;
}

bool SceneRuntime::ensureRuntimeVisibilityHierarchy(const SceneNode* root)
{
	if (_runtimeVisibilityHierarchyDirty ||
	    _runtimeVisibilityMeshStoreCount != static_cast<int>(_meshStore.size()))
	{
		rebuildRuntimeVisibilityHierarchy(root);
	}

	return _runtimeVisibilityRootIndex >= 0 &&
	       _runtimeVisibilityRootIndex < _runtimeVisibilityNodes.size();
}

bool SceneRuntime::refreshRuntimeVisibilityNodeBounds(
	int nodeIndex,
	const std::vector<unsigned char>& baseVisibleMask,
	bool refreshBounds,
	const std::function<bool(const SceneMesh*)>& isMeshVisibleFn)
{
	if (nodeIndex < 0 || nodeIndex >= _runtimeVisibilityNodes.size())
		return false;

	RuntimeVisibilityNode& runtimeNode = _runtimeVisibilityNodes[nodeIndex];
	bool boundsInitialized = false;
	bool hasVisibleMesh = false;
	BoundingBox bounds;

	for (int meshIndex : std::as_const(runtimeNode.meshIndices))
	{
		if (meshIndex < 0 || meshIndex >= static_cast<int>(_meshStore.size()))
			continue;
		if (meshIndex >= static_cast<int>(baseVisibleMask.size()) || !baseVisibleMask[meshIndex])
			continue;

		const SceneMesh* mesh = meshAt(static_cast<size_t>(meshIndex));
		if (!mesh || !isMeshVisibleFn(mesh))
			continue;

		if (!boundsInitialized)
		{
			bounds = mesh->getBoundingBox();
			boundsInitialized = true;
		}
		else
		{
			bounds.addBox(mesh->getBoundingBox());
		}
		hasVisibleMesh = true;
	}

	for (int childIndex : std::as_const(runtimeNode.children))
	{
		if (!refreshRuntimeVisibilityNodeBounds(childIndex, baseVisibleMask, refreshBounds, isMeshVisibleFn))
			continue;

		const RuntimeVisibilityNode& childNode = _runtimeVisibilityNodes[childIndex];
		if (refreshBounds && !boundsInitialized)
		{
			bounds = childNode.subtreeBounds;
			boundsInitialized = true;
		}
		else if (refreshBounds)
		{
			bounds.addBox(childNode.subtreeBounds);
		}
		hasVisibleMesh = true;
	}

	runtimeNode.subtreeHasVisibleMeshes = hasVisibleMesh;
	if (refreshBounds && boundsInitialized)
		runtimeNode.subtreeBounds = bounds;

	return hasVisibleMesh;
}

void SceneRuntime::refreshRuntimeVisibilityCacheForCurrentView(
	const SceneNode* root,
	quint64 currentBoundsRevision,
	const std::function<bool(const SceneMesh*)>& isMeshVisibleFn)
{
	_runtimeVisibilityPrepared = false;
	if (!ensureRuntimeVisibilityHierarchy(root))
		return;

	const std::vector<int>& visibleIds = currentVisibleObjectIds();
	_runtimeBaseVisibleMask.assign(_meshStore.size(), 0u);
	for (int meshIndex : visibleIds)
	{
		if (meshIndex >= 0 && meshIndex < static_cast<int>(_runtimeBaseVisibleMask.size()))
			_runtimeBaseVisibleMask[meshIndex] = 1u;
	}

	const bool refreshBounds = (_runtimeVisibilityBoundsRevision != currentBoundsRevision);
	const bool maskChanged = (_runtimeVisibilityMaskProcessedRevision != _runtimeVisibilityMaskRevision);

	if (!refreshBounds && !maskChanged)
	{
		_runtimeVisibilityPrepared = true;
		return;
	}

	refreshRuntimeVisibilityNodeBounds(
		_runtimeVisibilityRootIndex,
		_runtimeBaseVisibleMask,
		refreshBounds,
		isMeshVisibleFn);
	if (refreshBounds)
		_runtimeVisibilityBoundsRevision = currentBoundsRevision;
	if (maskChanged)
		_runtimeVisibilityMaskProcessedRevision = _runtimeVisibilityMaskRevision;
	_runtimeVisibilityPrepared = true;
}
