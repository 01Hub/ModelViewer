#include "SceneGraph.h"

#include <QFileInfo>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

SceneGraph::SceneGraph(QObject* parent)
    : QObject(parent)
{
    _root           = new SceneNode();
    _root->nodeUuid = QUuid::createUuid();
    _root->name     = QStringLiteral("__SceneRoot__");
}

SceneGraph::~SceneGraph()
{
    freeSubtree(_root);
}

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

void SceneGraph::appendFromScene(const aiScene*      scene,
                                 const QString&      sourceFile,
                                 const QList<QUuid>& meshUuidsInOrder)
{
    if (!scene || !scene->mRootNode)
        return;

    // --- Synthetic file-level node ------------------------------------------
    // This sits directly under _root and provides a clean per-import boundary
    // in the tree.  Its display name is the bare filename (e.g. "Model1.obj").
    SceneNode* fileNode        = new SceneNode();
    fileNode->nodeUuid         = QUuid::createUuid();
    fileNode->name             = QFileInfo(sourceFile).fileName();
    fileNode->isSynthetic      = true;
    fileNode->sourceFile       = sourceFile;
    // localTransform stays default-constructed (identity).
    fileNode->parent           = _root;
    _root->children.append(fileNode);

    // --- Mirror the aiNode tree under the file node -------------------------
    // Option B: always preserve the full aiNode hierarchy verbatim, including
    // the root node even when its name matches the filename.  This guarantees
    // that all node transforms are preserved for a lossless export round-trip.
    int cursor = 0;
    SceneNode* subtreeRoot = buildSubtree(scene->mRootNode,
                                          fileNode,
                                          meshUuidsInOrder,
                                          cursor);
    fileNode->children.append(subtreeRoot);

    emit structureChanged();
}

SceneNode* SceneGraph::buildSubtree(const aiNode*       ainode,
                                    SceneNode*          parent,
                                    const QList<QUuid>& uuids,
                                    int&                cursor)
{
    SceneNode* node       = new SceneNode();
    node->nodeUuid        = QUuid::createUuid();
    node->name            = QString::fromUtf8(ainode->mName.C_Str());
    node->localTransform  = ainode->mTransformation;
    node->parent          = parent;

    // Assign mesh UUIDs in the same order that processNode() encountered them:
    // inner loop over mMeshes before recursing into children.
    for (unsigned int i = 0; i < ainode->mNumMeshes; ++i)
    {
        if (cursor < uuids.size())
        {
            const QUuid& uuid = uuids.at(cursor++);
            node->meshUuids.append(uuid);
            _meshUuidToNode[uuid] = node;
        }
    }

    for (unsigned int i = 0; i < ainode->mNumChildren; ++i)
    {
        SceneNode* child = buildSubtree(ainode->mChildren[i],
                                        node,
                                        uuids,
                                        cursor);
        node->children.append(child);
    }

    return node;
}

// ---------------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------------

void SceneGraph::clear()
{
    _meshUuidToNode.clear();
    freeSubtree(_root);

    _root           = new SceneNode();
    _root->nodeUuid = QUuid::createUuid();
    _root->name     = QStringLiteral("__SceneRoot__");

    emit structureChanged();
}

// ---------------------------------------------------------------------------
// Query
// ---------------------------------------------------------------------------

SceneNode* SceneGraph::findNodeForMesh(const QUuid& meshUuid) const
{
    return _meshUuidToNode.value(meshUuid, nullptr);
}

QList<QUuid> SceneGraph::collectMeshUuids(const SceneNode* node) const
{
    QList<QUuid> result;
    collectUuidsRecursive(node, result);
    return result;
}

void SceneGraph::collectUuidsRecursive(const SceneNode* node,
                                       QList<QUuid>&    out) const
{
    out.append(node->meshUuids);
    for (const SceneNode* child : node->children)
        collectUuidsRecursive(child, out);
}

// ---------------------------------------------------------------------------
// Mutation
// ---------------------------------------------------------------------------

SceneNode* SceneGraph::removeMeshUuid(const QUuid& meshUuid, int& outPosition)
{
    SceneNode* node = _meshUuidToNode.value(meshUuid, nullptr);
    if (!node)
    {
        outPosition = -1;
        return nullptr;
    }

    outPosition = node->meshUuids.indexOf(meshUuid);
    node->meshUuids.removeAll(meshUuid);
    _meshUuidToNode.remove(meshUuid);

    emit structureChanged();
    return node;
}

void SceneGraph::restoreMeshUuid(SceneNode*   node,
                                 const QUuid& meshUuid,
                                 int          position)
{
    if (!node)
        return;

    // Clamp to valid range in case concurrent removes shifted the list.
    position = qBound(0, position, static_cast<int>(node->meshUuids.size()));
    node->meshUuids.insert(position, meshUuid);
    _meshUuidToNode[meshUuid] = node;

    emit structureChanged();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void SceneGraph::freeSubtree(SceneNode* node)
{
    if (!node)
        return;
    for (SceneNode* child : node->children)
        freeSubtree(child);
    delete node;
}
