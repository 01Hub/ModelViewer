#pragma once

#include "BoundingBox.h"
#include "SceneMeshRecord.h"
#include "SceneMesh.h"
#include "TransformCommand.h"

#include <algorithm>
#include <functional>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMap>
#include <QUuid>
#include <QVector>
#include <exception>
#include <iostream>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <assimp/scene.h>

struct SceneNode;

// ---------------------------------------------------------------------------
// TextureSamplerSettings
// ---------------------------------------------------------------------------
struct TextureSamplerSettings
{
	GLenum wrapS      = GL_REPEAT;
	GLenum wrapT      = GL_REPEAT;
	GLenum minFilter  = GL_LINEAR_MIPMAP_LINEAR;
	GLenum magFilter  = GL_LINEAR;

	bool operator==(const TextureSamplerSettings& other) const
	{
		return wrapS      == other.wrapS   &&
		       wrapT      == other.wrapT   &&
		       minFilter  == other.minFilter &&
		       magFilter  == other.magFilter;
	}
};

// ---------------------------------------------------------------------------
// CachedTextureEntry
// ---------------------------------------------------------------------------
struct CachedTextureEntry
{
	QImage  image;
	int     imageWidth      = 0;
	int     imageHeight     = 0;
	int     imageComponents = 0;
	GLenum  imageFormat     = GL_RGBA;

	GLuint                lastGPUTexture   = 0;
	TextureSamplerSettings lastSamplerSettings;
	int                   refCount         = 0;
};

// ---------------------------------------------------------------------------
// RuntimeVisibilityNode
// ---------------------------------------------------------------------------
struct RuntimeVisibilityNode
{
	const SceneNode* sceneNode = nullptr;
	QVector<int>     children;
	QVector<int>     meshIndices;
	BoundingBox      subtreeBounds;
	bool             subtreeHasVisibleMeshes = false;
};

// ---------------------------------------------------------------------------
// SceneRuntime
//
// Owns all scene-data state: mesh store, texture cache, runtime visibility
// BVH, the Assimp scene tree, and load-lifecycle flags.
// All fields are private; GLWidget accesses them through the typed API below.
// ---------------------------------------------------------------------------
class SceneRuntime
{
public:
	// RecycleBinEntry is part of the public interface (used in method signatures).
	struct RecycleBinEntry
	{
		SceneMesh* mesh;
		int           originalIndex;
		QDateTime     deletedAt;
	};

	// ---- Mesh store --------------------------------------------------------
	std::vector<SceneMeshRecord>&       meshStore()            { return _meshStore; }
	const std::vector<SceneMeshRecord>& meshStore()      const { return _meshStore; }
	SceneMeshRecord&       meshRecordAt(size_t index)         { return _meshStore.at(index); }
	const SceneMeshRecord& meshRecordAt(size_t index)   const { return _meshStore.at(index); }
	SceneMesh*             meshAt(size_t index)               { return _meshStore.at(index).mesh; }
	const SceneMesh*       meshAt(size_t index)         const { return _meshStore.at(index).mesh; }

	std::vector<int>&       displayedObjectsIds()              { return _displayedObjectsIds; }
	const std::vector<int>& displayedObjectsIds()        const { return _displayedObjectsIds; }

	std::vector<int>&       hiddenObjectsIds()                 { return _hiddenObjectsIds; }
	const std::vector<int>& hiddenObjectsIds()           const { return _hiddenObjectsIds; }

	const std::vector<int>& currentVisibleObjectIds() const
	{
		return _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
	}

	// ---- Recycle bin -------------------------------------------------------
	QMap<QUuid, RecycleBinEntry>&       recycleBin()           { return _recycleBin; }
	const QMap<QUuid, RecycleBinEntry>& recycleBin()     const { return _recycleBin; }

	// ---- Texture cache -----------------------------------------------------
	std::unordered_map<QString, CachedTextureEntry>&       texCache()       { return _texCache; }
	const std::unordered_map<QString, CachedTextureEntry>& texCache() const { return _texCache; }

	std::unordered_map<unsigned int, int>&       texRefCount()       { return _texRefCount; }
	const std::unordered_map<unsigned int, int>& texRefCount() const { return _texRefCount; }

	// ---- Runtime visibility BVH --------------------------------------------
	QVector<RuntimeVisibilityNode>&       runtimeVisibilityNodes()       { return _runtimeVisibilityNodes; }
	const QVector<RuntimeVisibilityNode>& runtimeVisibilityNodes() const { return _runtimeVisibilityNodes; }

	int     runtimeVisibilityRootIndex()              const { return _runtimeVisibilityRootIndex; }
	bool    runtimeVisibilityPrepared()               const { return _runtimeVisibilityPrepared; }

	std::vector<unsigned char>&       runtimeBaseVisibleMask()       { return _runtimeBaseVisibleMask; }
	const std::vector<unsigned char>& runtimeBaseVisibleMask() const { return _runtimeBaseVisibleMask; }
	void invalidateRuntimeVisibilityHierarchy();
	void rebuildRuntimeVisibilityHierarchy(const SceneNode* root);
	bool ensureRuntimeVisibilityHierarchy(const SceneNode* root);
	void refreshRuntimeVisibilityCacheForCurrentView(
		const SceneNode* root,
		quint64 currentBoundsRevision,
		const std::function<bool(const SceneMesh*)>& isMeshVisibleFn);

	// ---- Assimp scene ------------------------------------------------------
	const aiScene*  assimpScene()                   const { return _assimpScene; }
	void            setAssimpScene(const aiScene* s)      { _assimpScene = s; }

	aiScene*        globalScene()                   const { return _globalScene; }
	aiScene*&       globalScene()                         { return _globalScene; }
	void            setGlobalScene(aiScene* s)            { _globalScene = s; }

	glm::mat4&      globalSceneTransform()                { return _globalSceneTransform; }
	const glm::mat4& globalSceneTransform()         const { return _globalSceneTransform; }

	// ---- Load lifecycle ----------------------------------------------------
	QList<QUuid>&       pendingSceneUuids()         { return _pendingSceneUuids; }
	const QList<QUuid>& pendingSceneUuids()   const { return _pendingSceneUuids; }

	std::vector<int>&       centerScreenObjectIDs()         { return _centerScreenObjectIDs; }
	const std::vector<int>& centerScreenObjectIDs()   const { return _centerScreenObjectIDs; }

	bool  visibleSwapped()                    const { return _visibleSwapped; }
	bool& visibleSwapped()                          { return _visibleSwapped; }
	void  setVisibleSwapped(bool v)                 { _visibleSwapped = v; }
	bool  swapVisible(bool checked)
	{
		if (_visibleSwapped == checked)
			return false;
		_visibleSwapped = checked;
		++_runtimeVisibilityMaskRevision;
		return true;
	}

	bool progressiveLoadingEnabled()          const { return _progressiveLoadingEnabled; }
	void setProgressiveLoadingEnabled(bool v)       { _progressiveLoadingEnabled = v; }

	bool cancelRequested()                    const { return _cancelRequested; }
	void setCancelRequested(bool v)                 { _cancelRequested = v; }

	bool loadCancelled()                      const { return _loadCancelled; }
	void setLoadCancelled(bool v)                   { _loadCancelled = v; }

	// ---- Mesh/material batch helpers --------------------------------------
	int addMeshToDisplay(SceneMesh* mesh)
	{
		if (!mesh)
			return -1;

		_meshStore.push_back({ mesh, mesh->uuid() });
		const int index = static_cast<int>(_meshStore.size() - 1);
		_displayedObjectsIds.push_back(index);
		return index;
	}

	SceneMesh* detachMeshAt(int index)
	{
		if (index < 0 || index >= static_cast<int>(_meshStore.size()))
			return nullptr;

		SceneMesh* mesh = meshAt(static_cast<size_t>(index));
		_meshStore.erase(_meshStore.begin() + index);

		auto removeIndexAndShift = [index](std::vector<int>& ids) {
			auto it = std::find(ids.begin(), ids.end(), index);
			if (it != ids.end())
				ids.erase(it);
			for (int& id : ids)
			{
				if (id > index)
					--id;
			}
		};

		removeIndexAndShift(_displayedObjectsIds);
		removeIndexAndShift(_hiddenObjectsIds);

		if (_meshStore.empty())
		{
			_displayedObjectsIds.clear();
			_hiddenObjectsIds.clear();
			_visibleSwapped = false;
		}

		return mesh;
	}

	int restoreDetachedMesh(SceneMesh* mesh, int originalIndex)
	{
		if (!mesh)
			return -1;

		int insertIndex = originalIndex;
		if (insertIndex < 0 || insertIndex > static_cast<int>(_meshStore.size()))
			insertIndex = static_cast<int>(_meshStore.size());

		_meshStore.insert(_meshStore.begin() + insertIndex, { mesh, mesh->uuid() });

		for (int& id : _displayedObjectsIds)
		{
			if (id >= insertIndex)
				++id;
		}
		for (int& id : _hiddenObjectsIds)
		{
			if (id >= insertIndex)
				++id;
		}

		_displayedObjectsIds.push_back(insertIndex);
		std::sort(_displayedObjectsIds.begin(), _displayedObjectsIds.end());
		return insertIndex;
	}

	bool clearMeshStore()
	{
		for (const SceneMeshRecord& meshRecord : _meshStore)
			delete meshRecord.mesh;
		_meshStore.clear();
		_displayedObjectsIds.clear();
		_hiddenObjectsIds.clear();
		const bool wasSwapped = _visibleSwapped;
		_visibleSwapped = false;
		return wasSwapped;
	}

	bool setDisplayList(const std::vector<int>& ids)
	{
		_displayedObjectsIds = ids;
		++_runtimeVisibilityMaskRevision;

		std::vector<int> allObjectIDs;
		allObjectIDs.reserve(_meshStore.size());
		for (size_t i = 0; i < _meshStore.size(); ++i)
			allObjectIDs.push_back(static_cast<int>(i));

		_hiddenObjectsIds.clear();
		std::set_difference(
			allObjectIDs.begin(), allObjectIDs.end(),
			_displayedObjectsIds.begin(), _displayedObjectsIds.end(),
			std::back_inserter(_hiddenObjectsIds));

		const bool wasSwapped = _visibleSwapped;
		if (_hiddenObjectsIds.empty())
			_visibleSwapped = false;

		return wasSwapped && !_visibleSwapped;
	}

	bool moveToRecycleBin(const QUuid& uuid, int originalIndex)
	{
		SceneMesh* mesh = nullptr;
		int index = -1;

		for (size_t i = 0; i < _meshStore.size(); ++i)
		{
			SceneMesh* candidate = _meshStore[i].mesh;
			if (candidate && candidate->uuid() == uuid)
			{
				mesh = candidate;
				index = static_cast<int>(i);
				break;
			}
		}

		if (!mesh)
			return false;

		SceneMesh* detached = detachMeshAt(index);
		if (!detached)
			return false;

		RecycleBinEntry entry;
		entry.mesh = detached;
		entry.originalIndex = originalIndex;
		entry.deletedAt = QDateTime::currentDateTime();
		_recycleBin[uuid] = entry;
		return true;
	}

	bool restoreFromRecycleBin(const QUuid& uuid)
	{
		if (!_recycleBin.contains(uuid))
			return false;

		const RecycleBinEntry entry = _recycleBin.take(uuid);
		return restoreDetachedMesh(entry.mesh, entry.originalIndex) >= 0;
	}

	bool permanentlyDeleteFromRecycleBin(const QUuid& uuid)
	{
		if (!_recycleBin.contains(uuid))
			return false;

		const RecycleBinEntry entry = _recycleBin.take(uuid);
		delete entry.mesh;
		return true;
	}

	bool isInRecycleBin(const QUuid& uuid) const
	{
		return _recycleBin.contains(uuid);
	}

	QVector<QUuid> recycleBinUuids() const
	{
		return _recycleBin.keys().toVector();
	}

	std::vector<SceneMesh*> meshPointers() const
	{
		std::vector<SceneMesh*> result;
		result.reserve(_meshStore.size());
		for (const SceneMeshRecord& meshRecord : _meshStore)
			result.push_back(meshRecord.mesh);
		return result;
	}

	SceneMesh* getMeshByUuid(const QUuid& uuid) const
	{
		for (const SceneMeshRecord& meshRecord : _meshStore)
		{
			SceneMesh* mesh = meshRecord.mesh;
			if (mesh && mesh->uuid() == uuid)
				return mesh;
		}

		if (_recycleBin.contains(uuid))
			return _recycleBin.value(uuid).mesh;

		return nullptr;
	}

	SceneMesh* getMeshByIndex(int index) const
	{
		if (index >= 0 && index < static_cast<int>(_meshStore.size()))
			return _meshStore[static_cast<size_t>(index)].mesh;
		return nullptr;
	}

	int getIndexByUuid(const QUuid& uuid) const
	{
		for (size_t i = 0; i < _meshStore.size(); ++i)
		{
			SceneMesh* mesh = _meshStore[i].mesh;
			if (mesh && mesh->uuid() == uuid)
				return static_cast<int>(i);
		}
		return -1;
	}

	QUuid getUuidByIndex(int index) const
	{
		if (index >= 0 && index < static_cast<int>(_meshStore.size()))
		{
			SceneMesh* mesh = _meshStore[static_cast<size_t>(index)].mesh;
			return mesh ? mesh->uuid() : QUuid();
		}
		return QUuid();
	}

	bool applyMaterialToMeshes(const std::vector<int>& ids, const Material& mat)
	{
		bool needsTransmission = false;
		for (int id : ids)
		{
			try
			{
				if (id < 0 || id >= static_cast<int>(_meshStore.size()))
					continue;
				SceneMesh* mesh = meshAt(static_cast<size_t>(id));
				if (!mesh)
					continue;
				mesh->setMaterial(mat);
				if (mat.hasTransmission() || mat.diffuseTransmissionFactor() > 0.0f)
					needsTransmission = true;
			}
			catch (const std::exception& ex)
			{
				std::cout << "Exception in SceneRuntime::applyMaterialToMeshes\n" << ex.what() << std::endl;
			}
		}
		return needsTransmission;
	}

	void applyTextureMapsToMesh(int id, const Material& resolved)
	{
		try
		{
			if (id < 0 || id >= static_cast<int>(_meshStore.size()))
				return;
			SceneMesh* mesh = meshAt(static_cast<size_t>(id));
			if (!mesh)
				return;
			mesh->setTextureMaps(resolved);
			mesh->invertOpacityADSMap(resolved.isOpacityMapInverted());
			mesh->invertOpacityPBRMap(resolved.isOpacityMapInverted());
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in SceneRuntime::applyTextureMapsToMesh\n" << ex.what() << std::endl;
		}
	}

	void invertAdsOpacityMaps(const std::vector<int>& ids, bool inverted)
	{
		for (int id : ids)
		{
			try
			{
				if (id < 0 || id >= static_cast<int>(_meshStore.size()))
					continue;
				SceneMesh* mesh = meshAt(static_cast<size_t>(id));
				if (!mesh)
					continue;
				mesh->invertOpacityADSMap(inverted);
			}
			catch (const std::exception& ex)
			{
				std::cout << "Exception in SceneRuntime::invertAdsOpacityMaps\n" << ex.what() << std::endl;
			}
		}
	}

	std::vector<unsigned int> drainTextureCacheGpuIds()
	{
		std::vector<unsigned int> gpuIds;
		for (auto& entry : _texCache)
		{
			if (entry.second.lastGPUTexture != 0)
				gpuIds.push_back(entry.second.lastGPUTexture);
		}
		_texCache.clear();
		_texRefCount.clear();
		return gpuIds;
	}

	void setMeshTransforms(const std::vector<int>& ids,
		const QVector3D& translation,
		const QVector3D& rotation,
		const QVector3D& scale)
	{
		for (int id : ids)
		{
			try
			{
				if (id < 0 || id >= static_cast<int>(_meshStore.size()))
					continue;
				SceneMesh* mesh = meshAt(static_cast<size_t>(id));
				if (!mesh)
					continue;
				mesh->setTranslation(translation);
				mesh->setRotation(rotation);
				mesh->setScaling(scale);
			}
			catch (const std::exception& ex)
			{
				std::cout << "Exception in SceneRuntime::setMeshTransforms\n" << ex.what() << std::endl;
			}
		}
	}

	void resetMeshTransforms(const std::vector<int>& ids)
	{
		for (int id : ids)
		{
			try
			{
				if (id < 0 || id >= static_cast<int>(_meshStore.size()))
					continue;
				SceneMesh* mesh = meshAt(static_cast<size_t>(id));
				if (!mesh)
					continue;
				mesh->resetTransformations();
			}
			catch (const std::exception& ex)
			{
				std::cout << "Exception in SceneRuntime::resetMeshTransforms\n" << ex.what() << std::endl;
			}
		}
	}

	void applyMeshTransforms(const QMap<int, TransformState>& transforms)
	{
		for (auto it = transforms.begin(); it != transforms.end(); ++it)
		{
			const int index = it.key();
			const TransformState& state = it.value();
			if (index < 0 || index >= static_cast<int>(_meshStore.size()))
				continue;
			SceneMesh* mesh = meshAt(static_cast<size_t>(index));
			if (!mesh)
				continue;
			mesh->setTranslation(state.translation);
			if (state.hasExactRotation)
				mesh->setRotationQuaternion(state.rotationQuat, state.rotation);
			else
				mesh->setRotation(state.rotation);
			mesh->setScaling(state.scale);
		}
	}

	bool isModelLevelTransform(int transformCount) const
	{
		return transformCount == static_cast<int>(_meshStore.size());
	}

	bool userModelTransformForFile(const QString& sourceFile, QMatrix4x4& outTransform) const
	{
		bool found = false;
		QMatrix4x4 trsf;
		for (const SceneMeshRecord& meshRecord : _meshStore)
		{
			SceneMesh* mesh = meshRecord.mesh;
			if (!mesh || mesh->getSourceFile() != sourceFile)
				continue;
			if (!found)
			{
				trsf = mesh->getTransformation();
				found = true;
			}
			else if (mesh->getTransformation() != trsf)
			{
				return false;
			}
		}

		if (!found || trsf.isIdentity())
			return false;

		outTransform = trsf;
		return true;
	}

	QString generateUniqueMeshName(const QString& baseName) const
	{
		bool nameExists = false;
		for (const SceneMeshRecord& meshRecord : _meshStore)
		{
			const SceneMesh* mesh = meshRecord.mesh;
			if (!mesh)
				continue;
			if (mesh->getName() == baseName)
			{
				nameExists = true;
				break;
			}
		}

		if (!nameExists)
			return baseName;

		int counter = 2;
		QString uniqueName;
		while (true)
		{
			uniqueName = QString("%1 (%2)").arg(baseName).arg(counter);

			bool exists = false;
			for (const SceneMeshRecord& meshRecord : _meshStore)
			{
				const SceneMesh* mesh = meshRecord.mesh;
				if (!mesh)
					continue;
				if (mesh->getName() == uniqueName)
				{
					exists = true;
					break;
				}
			}

			if (!exists)
				break;

			++counter;
		}

		return uniqueName;
	}

private:
	// ---- Mesh store ----
	std::vector<SceneMeshRecord> _meshStore;
	std::vector<int>             _displayedObjectsIds;
	std::vector<int>             _hiddenObjectsIds;

	// ---- Recycle bin ----
	QMap<QUuid, RecycleBinEntry> _recycleBin;

	// ---- Texture cache ----
	std::unordered_map<QString, CachedTextureEntry> _texCache;
	std::unordered_map<unsigned int, int>           _texRefCount;

	// ---- Runtime visibility BVH ----
	QVector<RuntimeVisibilityNode> _runtimeVisibilityNodes;
	int     _runtimeVisibilityRootIndex              = -1;
	bool    _runtimeVisibilityHierarchyDirty         = true;
	int     _runtimeVisibilityMeshStoreCount         = -1;
	bool    _runtimeVisibilityPrepared               = false;
	quint64 _runtimeVisibilityBoundsRevision         = 0;
	quint64 _runtimeVisibilityMaskRevision           = 1;
	quint64 _runtimeVisibilityMaskProcessedRevision  = 0;
	std::vector<unsigned char> _runtimeBaseVisibleMask;

	// ---- Assimp scene ----
	const aiScene* _assimpScene         = nullptr;
	aiScene*       _globalScene         = nullptr;
	glm::mat4      _globalSceneTransform{ 1.0f };

	// ---- Load lifecycle ----
	QList<QUuid>     _pendingSceneUuids;
	std::vector<int> _centerScreenObjectIDs;
	bool _visibleSwapped             = false;
	bool _progressiveLoadingEnabled  = false;
	bool _cancelRequested            = false;
	bool _loadCancelled              = false;

	int buildRuntimeVisibilityNodeRecursive(const SceneNode* node,
	                                        const QHash<QUuid, int>& meshIndexByUuid);
	bool refreshRuntimeVisibilityNodeBounds(int nodeIndex,
	                                        const std::vector<unsigned char>& baseVisibleMask,
	                                        bool refreshBounds,
	                                        const std::function<bool(const SceneMesh*)>& isMeshVisibleFn);
};
