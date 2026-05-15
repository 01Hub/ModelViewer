#include "SceneGraph.h"

#include <QFileInfo>
#include <QJsonObject>
#include <QJsonValue>
#include <functional>

namespace
{
constexpr quint32 SCENEGRAPH_SESSION_VERSION = 1;

void writeMatrix(QDataStream& out, const aiMatrix4x4& m)
{
    out << m.a1 << m.a2 << m.a3 << m.a4
        << m.b1 << m.b2 << m.b3 << m.b4
        << m.c1 << m.c2 << m.c3 << m.c4
        << m.d1 << m.d2 << m.d3 << m.d4;
}

bool readMatrix(QDataStream& in, aiMatrix4x4& m)
{
    in >> m.a1 >> m.a2 >> m.a3 >> m.a4
       >> m.b1 >> m.b2 >> m.b3 >> m.b4
       >> m.c1 >> m.c2 >> m.c3 >> m.c4
       >> m.d1 >> m.d2 >> m.d3 >> m.d4;
    return in.status() == QDataStream::Ok;
}
}

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

void SceneGraph::appendFromScene(const aiScene*                   scene,
                                 const QString&                   sourceFile,
                                 const QList<QUuid>&              meshUuidsInOrder,
                                 const std::vector<GPULight>&    lights)
{
    if (!scene || !scene->mRootNode)
        return;

    // Store lights from this import (replaces any previous lights)
    _lights = lights;

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

void SceneGraph::rebuildFlat(const QString& sessionName,
                             const QList<QUuid>& meshUuids)
{
    _meshUuidToNode.clear();
    _lights.clear();
    freeSubtree(_root);

    _root           = new SceneNode();
    _root->nodeUuid = QUuid::createUuid();
    _root->name     = QStringLiteral("__SceneRoot__");

    SceneNode* fileNode    = new SceneNode();
    fileNode->nodeUuid     = QUuid::createUuid();
    fileNode->name         = sessionName;
    fileNode->isSynthetic  = true;
    fileNode->sourceFile   = sessionName;
    fileNode->parent       = _root;
    _root->children.append(fileNode);

    for (const QUuid& uuid : meshUuids)
    {
        fileNode->meshUuids.append(uuid);
        _meshUuidToNode[uuid] = fileNode;
    }

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
    _lights.clear();
    freeSubtree(_root);

    _root           = new SceneNode();
    _root->nodeUuid = QUuid::createUuid();
    _root->name     = QStringLiteral("__SceneRoot__");

    emit structureChanged();
}

void SceneGraph::rebuildFromMvf(const QJsonArray& documentNodes,
                                const QJsonArray& sceneRootNodes)
{
    _meshUuidToNode.clear();
    _lights.clear();
    freeSubtree(_root);

    _root           = new SceneNode();
    _root->nodeUuid = QUuid::createUuid();
    _root->name     = QStringLiteral("__SceneRoot__");

    std::function<SceneNode*(int, SceneNode*)> buildNode =
        [&](int idx, SceneNode* parent) -> SceneNode*
    {
        if (idx < 0 || idx >= documentNodes.size())
            return nullptr;

        const QJsonObject obj = documentNodes[idx].toObject();

        SceneNode* node    = new SceneNode();
        node->parent       = parent;

        const QString idStr = obj[QStringLiteral("id")].toString();
        node->nodeUuid = idStr.isEmpty() ? QUuid::createUuid()
                                         : QUuid::fromString(idStr);
        node->name = obj[QStringLiteral("name")].toString();

        const QJsonArray mat = obj[QStringLiteral("matrix")].toArray();
        if (mat.size() == 16)
        {
            aiMatrix4x4& m = node->localTransform;
            m.a1 = (float)mat[0].toDouble();  m.a2 = (float)mat[1].toDouble();
            m.a3 = (float)mat[2].toDouble();  m.a4 = (float)mat[3].toDouble();
            m.b1 = (float)mat[4].toDouble();  m.b2 = (float)mat[5].toDouble();
            m.b3 = (float)mat[6].toDouble();  m.b4 = (float)mat[7].toDouble();
            m.c1 = (float)mat[8].toDouble();  m.c2 = (float)mat[9].toDouble();
            m.c3 = (float)mat[10].toDouble(); m.c4 = (float)mat[11].toDouble();
            m.d1 = (float)mat[12].toDouble(); m.d2 = (float)mat[13].toDouble();
            m.d3 = (float)mat[14].toDouble(); m.d4 = (float)mat[15].toDouble();
        }

        const QJsonArray bindings = obj[QStringLiteral("meshBindings")].toArray();
        for (const QJsonValue& b : bindings)
        {
            const QString uuidStr = b.toObject()[QStringLiteral("uuid")].toString();
            if (!uuidStr.isEmpty())
            {
                const QUuid uuid = QUuid::fromString(uuidStr);
                node->meshUuids.append(uuid);
                _meshUuidToNode[uuid] = node;
            }
        }

        const QJsonArray children = obj[QStringLiteral("children")].toArray();
        for (const QJsonValue& c : children)
        {
            SceneNode* child = buildNode(c.toInt(-1), node);
            if (child)
                node->children.append(child);
        }

        return node;
    };

    for (const QJsonValue& rootIdx : sceneRootNodes)
    {
        SceneNode* child = buildNode(rootIdx.toInt(-1), _root);
        if (child)
            _root->children.append(child);
    }

    emit structureChanged();
}

void SceneGraph::serialize(QDataStream& out) const
{
    out << SCENEGRAPH_SESSION_VERSION;

    std::function<void(const SceneNode*)> writeNode = [&](const SceneNode* node) {
        out << node->nodeUuid
            << node->name
            << node->isSynthetic
            << node->sourceFile;
        writeMatrix(out, node->localTransform);

        out << static_cast<quint32>(node->meshUuids.size());
        for (const QUuid& uuid : node->meshUuids)
            out << uuid;

        out << static_cast<quint32>(node->children.size());
        for (const SceneNode* child : node->children)
            writeNode(child);
    };

    out << static_cast<quint32>(_root->children.size());
    for (const SceneNode* child : _root->children)
        writeNode(child);
}

bool SceneGraph::deserialize(QDataStream& in)
{
    quint32 version = 0;
    in >> version;
    if (in.status() != QDataStream::Ok || version != SCENEGRAPH_SESSION_VERSION)
        return false;

    _meshUuidToNode.clear();
    freeSubtree(_root);

    _root           = new SceneNode();
    _root->nodeUuid = QUuid::createUuid();
    _root->name     = QStringLiteral("__SceneRoot__");

    std::function<SceneNode*(SceneNode*)> readNode = [&](SceneNode* parent) -> SceneNode* {
        auto* node = new SceneNode();
        node->parent = parent;

        quint32 meshCount = 0;
        quint32 childCount = 0;
        in >> node->nodeUuid
           >> node->name
           >> node->isSynthetic
           >> node->sourceFile;
        if (!readMatrix(in, node->localTransform))
        {
            delete node;
            return nullptr;
        }

        in >> meshCount;
        for (quint32 i = 0; i < meshCount; ++i)
        {
            QUuid uuid;
            in >> uuid;
            node->meshUuids.append(uuid);
            _meshUuidToNode[uuid] = node;
        }

        in >> childCount;
        for (quint32 i = 0; i < childCount; ++i)
        {
            SceneNode* child = readNode(node);
            if (!child)
            {
                delete node;
                return nullptr;
            }
            node->children.append(child);
        }

        return node;
    };

    quint32 topLevelCount = 0;
    in >> topLevelCount;
    for (quint32 i = 0; i < topLevelCount; ++i)
    {
        SceneNode* child = readNode(_root);
        if (!child)
        {
            clear();
            return false;
        }
        _root->children.append(child);
    }

    emit structureChanged();
    return in.status() == QDataStream::Ok;
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

void SceneGraph::insertChildNode(SceneNode* parent, SceneNode* newChild, int position)
{
    if (!parent || !newChild)
        return;

    newChild->parent = parent;
    if (position < 0 || position >= parent->children.size())
        parent->children.append(newChild);
    else
        parent->children.insert(position, newChild);

    registerSubtreeUuids(newChild);

    emit structureChanged();
}

void SceneGraph::removeChildNode(SceneNode* parent, SceneNode* child, int& outPosition)
{
    if (!parent || !child)
    {
        outPosition = -1;
        return;
    }

    outPosition = parent->children.indexOf(child);
    if (outPosition < 0)
        return;

    parent->children.removeAt(outPosition);
    child->parent = nullptr;

    deregisterSubtreeUuids(child);

    emit structureChanged();
}

SceneNode* SceneGraph::findNodeByUuid(const QUuid& nodeUuid) const
{
    std::function<SceneNode*(SceneNode*)> search = [&](SceneNode* n) -> SceneNode*
    {
        if (n->nodeUuid == nodeUuid)
            return n;
        for (SceneNode* child : n->children)
        {
            if (SceneNode* found = search(child))
                return found;
        }
        return nullptr;
    };
    return search(_root);
}

void SceneGraph::deleteDetachedSubtree(SceneNode* root)
{
    if (!root)
        return;
    for (SceneNode* child : root->children)
        deleteDetachedSubtree(child);
    delete root;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void SceneGraph::registerSubtreeUuids(SceneNode* node)
{
    for (const QUuid& uuid : node->meshUuids)
        _meshUuidToNode[uuid] = node;
    for (SceneNode* child : node->children)
        registerSubtreeUuids(child);
}

void SceneGraph::deregisterSubtreeUuids(SceneNode* node)
{
    for (const QUuid& uuid : node->meshUuids)
        _meshUuidToNode.remove(uuid);
    for (SceneNode* child : node->children)
        deregisterSubtreeUuids(child);
}

void SceneGraph::freeSubtree(SceneNode* node)
{
    if (!node)
        return;
    for (SceneNode* child : node->children)
        freeSubtree(child);
    delete node;
}

// ---------------------------------------------------------------------------
// KHR_materials_variants
// ---------------------------------------------------------------------------

void SceneGraph::setVariantData(const QString& sourceFile, const GltfVariantData& data)
{
    _variantDataByFile[sourceFile] = data;
    _activeVariantByFile.insert(sourceFile, -1);
    emit variantDataChanged();
}

void SceneGraph::clearVariantData(const QString& sourceFile)
{
    if (_variantDataByFile.remove(sourceFile) > 0)
    {
        _activeVariantByFile.remove(sourceFile);
        emit variantDataChanged();
    }
}

GltfVariantData SceneGraph::variantDataForFile(const QString& sourceFile) const
{
    return _variantDataByFile.value(sourceFile, GltfVariantData{});
}

QStringList SceneGraph::filesWithVariants() const
{
    return _variantDataByFile.keys();
}

void SceneGraph::setActiveVariant(const QString& sourceFile, int variantIndex)
{
    _activeVariantByFile[sourceFile] = variantIndex;
}

int SceneGraph::activeVariantForFile(const QString& sourceFile) const
{
    return _activeVariantByFile.value(sourceFile, -1);
}
