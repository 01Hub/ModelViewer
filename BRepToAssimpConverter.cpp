#include "BRepToAssimpConverter.h"
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Solid.hxx>
#include <Poly_Triangulation.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <vector>
#include <cmath>

aiScene* BRepToAssimpConverter::Convert(const TopoDS_Shape& shape)
{
    auto* scene = new aiScene();
    scene->mRootNode = new aiNode();

    std::vector<aiMesh*> meshes;
    std::vector<aiNode*> meshNodes;
    int meshIndex = 0;

    TopExp_Explorer solidExplorer(shape, TopAbs_SOLID);
    if (!solidExplorer.More()) {
        // No solids found, try faces
        TopTools_IndexedMapOfShape faceGroup;
        TopExp_Explorer faceExplorer(shape, TopAbs_FACE);
        for (; faceExplorer.More(); faceExplorer.Next()) {
            faceGroup.Add(faceExplorer.Current());
        }
        aiMesh* mesh = convertFaceGroupToMesh(faceGroup, meshIndex);
        if (mesh) {
            meshes.push_back(mesh);
            aiNode* meshNode = new aiNode();
            meshNode->mMeshes = new unsigned int[1]{static_cast<unsigned int>(meshIndex)};
            meshNode->mNumMeshes = 1;
            meshNode->mParent = scene->mRootNode;
            meshNode->mName = aiString("Mesh_" + std::to_string(meshIndex));
            mesh->mName = aiString("Mesh_" + std::to_string(meshIndex));
            meshNodes.push_back(meshNode);
            ++meshIndex;
        }
    } else {
        for (; solidExplorer.More(); solidExplorer.Next()) {
            TopoDS_Solid solid = TopoDS::Solid(solidExplorer.Current());
            TopTools_IndexedMapOfShape faceGroup;
            TopExp_Explorer faceExplorer(solid, TopAbs_FACE);
            for (; faceExplorer.More(); faceExplorer.Next()) {
                faceGroup.Add(faceExplorer.Current());
            }
            aiMesh* mesh = convertFaceGroupToMesh(faceGroup, meshIndex);
            if (mesh) {
                meshes.push_back(mesh);
                aiNode* meshNode = new aiNode();
                meshNode->mMeshes = new unsigned int[1]{static_cast<unsigned int>(meshIndex)};
                meshNode->mNumMeshes = 1;
                meshNode->mParent = scene->mRootNode;
                meshNode->mName = aiString("SolidMesh_" + std::to_string(meshIndex));
                mesh->mName = aiString("SolidMesh_" + std::to_string(meshIndex));
                meshNodes.push_back(meshNode);
                ++meshIndex;
            }
        }
    }

    scene->mNumMeshes = meshes.size();
    scene->mMeshes = new aiMesh*[scene->mNumMeshes];
    for (size_t i = 0; i < meshes.size(); ++i) {
        scene->mMeshes[i] = meshes[i];
    }

    scene->mRootNode->mNumChildren = meshNodes.size();
    scene->mRootNode->mChildren = new aiNode*[meshNodes.size()];
    for (size_t i = 0; i < meshNodes.size(); ++i) {
        scene->mRootNode->mChildren[i] = meshNodes[i];
    }

    scene->mMaterials = new aiMaterial*[1];
    scene->mMaterials[0] = new aiMaterial();
    scene->mNumMaterials = 1;

    return scene;
}

aiMesh* BRepToAssimpConverter::convertFaceGroupToMesh(const TopTools_IndexedMapOfShape& faceGroup, int meshIndex) {
    std::vector<aiVector3D> vertices, normals, texCoords;
    std::vector<aiFace> faces;
    int vertexOffset = 0;

    for (int f = 1; f <= faceGroup.Extent(); ++f) {
        TopoDS_Face face = TopoDS::Face(faceGroup(f));
        BRepMesh_IncrementalMesh(face, 5.0);
        TopLoc_Location loc;
        Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, loc);
        if (triangulation.IsNull()) continue;

        gp_Trsf trsf = loc.Transformation();

        int nNodes = triangulation->NbNodes();
        int nTriangles = triangulation->NbTriangles();

        // Compute bounding box for UV mapping
        double minX = 1e10, maxX = -1e10, minY = 1e10, maxY = -1e10;
        std::vector<gp_Pnt> points(nNodes);
        for (int i = 1; i <= nNodes; ++i) {
            gp_Pnt p = triangulation->Node(i).Transformed(trsf);
            points[i - 1] = p;
            minX = std::min(minX, p.X());
            maxX = std::max(maxX, p.X());
            minY = std::min(minY, p.Y());
            maxY = std::max(maxY, p.Y());
        }
        double deltaX = (maxX - minX == 0) ? 1 : (maxX - minX);
        double deltaY = (maxY - minY == 0) ? 1 : (maxY - minY);

        std::vector<aiVector3D> localVertices;
        std::vector<aiVector3D> localNormals(nNodes);
        std::vector<aiVector3D> localUVs;

        for (int i = 0; i < nNodes; ++i) {
            const gp_Pnt& p = points[i];
            localVertices.emplace_back(p.X(), p.Y(), p.Z());
            localUVs.emplace_back((p.X() - minX) / deltaX, (p.Y() - minY) / deltaY, 0.0f);
        }

        for (int i = 1; i <= nTriangles; ++i) {
            Standard_Integer n1, n2, n3;
            triangulation->Triangle(i).Get(n1, n2, n3);
            --n1; --n2; --n3;

            const aiVector3D& v0 = localVertices[n1];
            const aiVector3D& v1 = localVertices[n2];
            const aiVector3D& v2 = localVertices[n3];

            aiVector3D normal = (v1 - v0) ^ (v2 - v0);
            normal.Normalize();

            localNormals[n1] = normal;
            localNormals[n2] = normal;
            localNormals[n3] = normal;

            aiFace face;
            face.mNumIndices = 3;
            face.mIndices = new unsigned int[3] {
                static_cast<unsigned int>(vertexOffset + n1),
                static_cast<unsigned int>(vertexOffset + n2),
                static_cast<unsigned int>(vertexOffset + n3)
            };
            faces.push_back(face);
        }

        vertices.insert(vertices.end(), localVertices.begin(), localVertices.end());
        texCoords.insert(texCoords.end(), localUVs.begin(), localUVs.end());
        normals.insert(normals.end(), localNormals.begin(), localNormals.end());
        vertexOffset = vertices.size();
    }

    if (vertices.empty()) return nullptr;

    aiMesh* mesh = new aiMesh();
    mesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;
    mesh->mNumVertices = vertices.size();
    mesh->mVertices = new aiVector3D[vertices.size()];
    mesh->mNormals = new aiVector3D[normals.size()];
    mesh->mTextureCoords[0] = new aiVector3D[texCoords.size()];
    mesh->mNumUVComponents[0] = 2;

    for (size_t i = 0; i < vertices.size(); ++i) {
        mesh->mVertices[i] = vertices[i];
        mesh->mNormals[i] = normals[i];
        mesh->mTextureCoords[0][i] = texCoords[i];
    }

    mesh->mNumFaces = faces.size();
    mesh->mFaces = new aiFace[faces.size()];
    for (size_t i = 0; i < faces.size(); ++i) {
        mesh->mFaces[i] = faces[i];
    }

    mesh->mMaterialIndex = 0;
    return mesh;
}
