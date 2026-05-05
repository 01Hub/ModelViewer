#pragma once

#include <functional>
#include <vector>
#include <QUuid>

#include <assimp/scene.h>

class SceneGraph;
class SceneNode;
class TriangleMesh;

class SceneGraphExporter
{
public:
    using MeshResolver = std::function<TriangleMesh* (const QUuid&)>;

    // Rebuilds an export-only aiScene from:
    //  - SceneGraph hierarchy
    //  - current live meshes resolved by UUID
    //
    // This first-pass implementation:
    //  - uses identity node transforms
    //  - exports positions, normals, UV0, and triangle faces
    //  - creates one default material and assigns materialIndex = 0 to all meshes
    //
    // Later we can extend this to:
    //  - rebuild full materials from current GLMaterial state
    //  - include lights
    //  - support multiple UV channels / vertex colors / tangents as needed
    static aiScene* buildExportScene(
        const SceneGraph* sceneGraph,
        const MeshResolver& resolveMesh);

private:
    static aiNode* buildNodeRecursive(
        const SceneNode* srcNode,
        const MeshResolver& resolveMesh,
        std::vector<aiMesh*>& outMeshes);

    static aiMesh* buildMeshFromTriangleMesh(const TriangleMesh* mesh);
};
