#pragma once

#include <functional>
#include <vector>
#include <QStringList>
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
    // flattenTransforms: when true, every aiNode gets an identity transform and
    // vertices are left in world space.  Required for formats whose Assimp writer
    // ignores node transforms (OBJ, PLY, STL).  Default false for hierarchy-aware
    // formats (glTF, FBX, COLLADA).
    // allowedSourceFiles: if non-empty, only file nodes whose sourceFile path is in
    // this list are exported.  An empty list means "export all loaded scenes".
    static aiScene* buildExportScene(
        const SceneGraph* sceneGraph,
        const MeshResolver& resolveMesh,
        bool flattenTransforms = false,
        const QStringList& allowedSourceFiles = QStringList());

private:
    static aiNode* buildNodeRecursive(
        const SceneNode* srcNode,
        const MeshResolver& resolveMesh,
        std::vector<aiMesh*>& outMeshes,
        std::vector<aiMaterial*>& outMaterials,
        QMap<QString, unsigned int>& materialKeyToIndex,
        const aiMatrix4x4& parentWorldTransform,
        bool flattenTransforms,
        const aiMatrix4x4& importCorrection = aiMatrix4x4()
    );

    static aiMesh* buildMeshFromTriangleMesh(const TriangleMesh* mesh, unsigned int materialIndex);

    static aiMaterial* buildMaterialFromTriangleMesh(const TriangleMesh* mesh);
};
