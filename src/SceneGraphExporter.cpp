#include "SceneGraphExporter.h"

#include "SceneGraph.h"
#include "SceneNode.h"
#include "TriangleMesh.h"
#include "AssImpMesh.h"

#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/scene.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace
{
    aiNode* makeIdentityNode(const QString& name)
    {
        aiNode* node = new aiNode();
        node->mName.Set(name.toUtf8().constData());
        node->mTransformation = aiMatrix4x4(); // identity
        return node;
    }

    aiMaterial* makeDefaultMaterial()
    {
        aiMaterial* mat = new aiMaterial();

        aiString matName("DefaultMaterial");
        mat->AddProperty(&matName, AI_MATKEY_NAME);

        // Optional simple neutral diffuse
        aiColor3D diffuse(0.8f, 0.8f, 0.8f);
        mat->AddProperty(&diffuse, 1, AI_MATKEY_COLOR_DIFFUSE);

        aiColor3D ambient(0.2f, 0.2f, 0.2f);
        mat->AddProperty(&ambient, 1, AI_MATKEY_COLOR_AMBIENT);

        aiColor3D specular(0.2f, 0.2f, 0.2f);
        mat->AddProperty(&specular, 1, AI_MATKEY_COLOR_SPECULAR);

        return mat;
    }
}

aiScene* SceneGraphExporter::buildExportScene(
    const SceneGraph* sceneGraph,
    const MeshResolver& resolveMesh)
{
    if (!sceneGraph)
        return nullptr;

    SceneNode* graphRoot = sceneGraph->root();
    if (!graphRoot)
        return nullptr;

    aiScene* scene = new aiScene();

    // One default material for the first pass.
    scene->mNumMaterials = 1;
    scene->mMaterials = new aiMaterial * [1];
    scene->mMaterials[0] = makeDefaultMaterial();

    std::vector<aiMesh*> builtMeshes;
    builtMeshes.reserve(64);

    // Export root (invisible SceneGraph root becomes a real aiNode container).
    aiNode* exportRoot = makeIdentityNode(QStringLiteral("SceneRoot"));
    scene->mRootNode = exportRoot;

    // Rebuild top-level children from SceneGraph::_root->children
    exportRoot->mNumChildren = static_cast<unsigned int>(graphRoot->children.size());
    if (exportRoot->mNumChildren > 0)
    {
        exportRoot->mChildren = new aiNode * [exportRoot->mNumChildren];
        for (unsigned int i = 0; i < exportRoot->mNumChildren; ++i)
        {
            SceneNode* srcChild = graphRoot->children.at(static_cast<int>(i));
            aiNode* dstChild = buildNodeRecursive(srcChild, resolveMesh, builtMeshes);
            exportRoot->mChildren[i] = dstChild;
            if (dstChild)
                dstChild->mParent = exportRoot;
        }
    }

    // Transfer built meshes into aiScene::mMeshes
    scene->mNumMeshes = static_cast<unsigned int>(builtMeshes.size());
    if (scene->mNumMeshes > 0)
    {
        scene->mMeshes = new aiMesh * [scene->mNumMeshes];
        for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
        {
            scene->mMeshes[i] = builtMeshes[i];
        }
    }

    return scene;
}

aiNode* SceneGraphExporter::buildNodeRecursive(
    const SceneNode* srcNode,
    const MeshResolver& resolveMesh,
    std::vector<aiMesh*>& outMeshes)
{
    if (!srcNode)
        return nullptr;

    aiNode* dstNode = makeIdentityNode(srcNode->name);

    // Build meshes owned directly by this node.
    std::vector<unsigned int> nodeMeshIndices;
    nodeMeshIndices.reserve(static_cast<size_t>(srcNode->meshUuids.size()));

    for (const QUuid& meshUuid : srcNode->meshUuids)
    {
        TriangleMesh* triMesh = resolveMesh ? resolveMesh(meshUuid) : nullptr;
        if (!triMesh)
            continue;

        aiMesh* built = buildMeshFromTriangleMesh(triMesh);
        if (!built)
            continue;

        nodeMeshIndices.push_back(static_cast<unsigned int>(outMeshes.size()));
        outMeshes.push_back(built);
    }

    dstNode->mNumMeshes = static_cast<unsigned int>(nodeMeshIndices.size());
    if (dstNode->mNumMeshes > 0)
    {
        dstNode->mMeshes = new unsigned int[dstNode->mNumMeshes];
        for (unsigned int i = 0; i < dstNode->mNumMeshes; ++i)
        {
            dstNode->mMeshes[i] = nodeMeshIndices[i];
        }
    }

    // Recurse children.
    dstNode->mNumChildren = static_cast<unsigned int>(srcNode->children.size());
    if (dstNode->mNumChildren > 0)
    {
        dstNode->mChildren = new aiNode * [dstNode->mNumChildren];
        for (unsigned int i = 0; i < dstNode->mNumChildren; ++i)
        {
            SceneNode* srcChild = srcNode->children.at(static_cast<int>(i));
            aiNode* dstChild = buildNodeRecursive(srcChild, resolveMesh, outMeshes);
            dstNode->mChildren[i] = dstChild;
            if (dstChild)
                dstChild->mParent = dstNode;
        }
    }

    return dstNode;
}

aiMesh* SceneGraphExporter::buildMeshFromTriangleMesh(const TriangleMesh* mesh)
{
    if (!mesh)
        return nullptr;

    // We only export AssImp-backed runtime meshes in this first pass,
    // because those expose CPU-side vertices()/indices().
    const AssImpMesh* assimpMesh = dynamic_cast<const AssImpMesh*>(mesh);
    if (!assimpMesh)
        return nullptr;

    const std::vector<Vertex> verts = assimpMesh->vertices();
    const std::vector<unsigned int> indices = assimpMesh->indices();

    if (verts.empty() || indices.empty())
        return nullptr;

    // First-pass assumption: export as triangle list.
    if ((indices.size() % 3) != 0)
        return nullptr;

    aiMesh* out = new aiMesh();

    out->mName.Set(mesh->getName().toUtf8().constData());
    out->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;
    out->mMaterialIndex = 0;

    // --- Vertices ---
    out->mNumVertices = static_cast<unsigned int>(verts.size());
    out->mVertices = new aiVector3D[out->mNumVertices];
    out->mNormals = new aiVector3D[out->mNumVertices];

    for (unsigned int i = 0; i < out->mNumVertices; ++i)
    {
        const Vertex& v = verts[i];

        out->mVertices[i] = aiVector3D(
            v.Position.x,
            v.Position.y,
            v.Position.z);

        out->mNormals[i] = aiVector3D(
            v.Normal.x,
            v.Normal.y,
            v.Normal.z);
    }

    // --- UV0 only (first pass) ---
    out->mTextureCoords[0] = new aiVector3D[out->mNumVertices];
    out->mNumUVComponents[0] = 2;

    for (unsigned int i = 0; i < out->mNumVertices; ++i)
    {
        const glm::vec2& uv = verts[i].TexCoords[0];
        out->mTextureCoords[0][i] = aiVector3D(uv.x, uv.y, 0.0f);
    }

    // --- Faces ---
    out->mNumFaces = static_cast<unsigned int>(indices.size() / 3);
    out->mFaces = new aiFace[out->mNumFaces];

    for (unsigned int f = 0; f < out->mNumFaces; ++f)
    {
        aiFace& face = out->mFaces[f];
        face.mNumIndices = 3;
        face.mIndices = new unsigned int[3];

        face.mIndices[0] = indices[f * 3 + 0];
        face.mIndices[1] = indices[f * 3 + 1];
        face.mIndices[2] = indices[f * 3 + 2];
    }

    return out;
}
