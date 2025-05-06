#include "AssImpMeshExporter.h"
#include <AssImpMesh.h>
#include <assimp/Exporter.hpp>
#include <GLMaterial.h>

AssImpMeshExporter::AssImpMeshExporter(QObject* parent)
    : QObject{ parent }
{

}

aiReturn AssImpMeshExporter::exportMeshes(const std::vector<AssImpMesh*>& meshes, const std::string& exportPath)
{
    std::vector<aiMesh*> aiMeshes;
    std::vector<aiMaterial*> aiMaterials;
    for (AssImpMesh* mesh : meshes)
    {
        std::vector<aiVector3D> aivertices;
        std::vector<unsigned int> aiindices;
        std::vector<aiVector3D> ainormals;
        std::vector<aiVector3D> aitexCoords;
        std::vector<aiColor4D> aicolors;

        for (Vertex v : mesh->vertices())
        {
            aivertices.push_back(aiVector3D(v.Position.x, v.Position.y, v.Position.z));
            ainormals.push_back(aiVector3D(v.Normal.x, v.Normal.y, v.Normal.z));
            aitexCoords.push_back(aiVector3D(v.TexCoords.s, v.TexCoords.t, 0));
            aicolors.push_back(aiColor4D(v.Color.r, v.Color.g, v.Color.b, v.Color.a)); // Include vertex colors
        }
        for (int index : mesh->indices())
        {
            aiindices.push_back(index);
        }

        aiMesh* aimesh = createMesh(aivertices, aiindices, ainormals, aitexCoords, aicolors);
        aiMeshes.push_back(aimesh);

        aiMaterial* material = createMaterial(mesh->getMaterial());
        aiMaterials.push_back(material);
    }

    aiScene* scene = createScene(aiMeshes, aiMaterials);
    // Export the scene to the selected file type
    Assimp::Exporter exporter;
    std::string fileExtension = exportPath.substr(exportPath.find_last_of('.') + 1);
    aiReturn result = exporter.Export(scene, fileExtension.c_str(), exportPath.c_str());

    if (result == aiReturn_SUCCESS) {
        std::cout << "Scene with multiple meshes exported successfully!" << std::endl;
    }
    else {
        std::cerr << "Error exporting scene: " << exporter.GetErrorString() << std::endl;
    }

    return result;
}

aiMesh* AssImpMeshExporter::createMesh(const std::vector<aiVector3D>& vertices,
    const std::vector<unsigned int>& indices,
    const std::vector<aiVector3D>& normals,
    const std::vector<aiVector3D>& texCoords,
    const std::vector<aiColor4D>& colors) {
    // Create a new aiMesh
    aiMesh* mesh = new aiMesh();

    // Set number of vertices and allocate memory for vertices
    mesh->mNumVertices = static_cast<unsigned int>(vertices.size());
    mesh->mVertices = new aiVector3D[mesh->mNumVertices];
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        mesh->mVertices[i] = vertices[i];
    }

    // Set indices (faces)
    mesh->mNumFaces = static_cast<unsigned int>(indices.size()) / 3;
    mesh->mFaces = new aiFace[mesh->mNumFaces];
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        mesh->mFaces[i].mNumIndices = 3;
        mesh->mFaces[i].mIndices = new unsigned int[3];
        for (unsigned int j = 0; j < 3; ++j) {
            mesh->mFaces[i].mIndices[j] = indices[i * 3 + j];
        }
    }

    // If you have normals, allocate and set them
    if (!normals.empty()) {
        mesh->mNormals = new aiVector3D[mesh->mNumVertices];
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            mesh->mNormals[i] = normals[i];
        }
    }

    // If you have texture coordinates, allocate and set them
    if (!texCoords.empty()) {
        mesh->mTextureCoords[0] = new aiVector3D[mesh->mNumVertices];
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            mesh->mTextureCoords[0][i] = texCoords[i];
        }
    }

    // If you have vertex colors, allocate and set them
    if (!colors.empty()) {
        mesh->mColors[0] = new aiColor4D[mesh->mNumVertices];
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            mesh->mColors[0][i] = colors[i];
        }
    }

    return mesh;
}

aiMaterial* AssImpMeshExporter::createMaterial(const GLMaterial& material) {
    aiMaterial* aiMat = new aiMaterial();

    aiColor3D color(material.ambient().x(), material.ambient().y(), material.ambient().z());
    aiMat->AddProperty(&color, 1, AI_MATKEY_COLOR_AMBIENT);

    color = aiColor3D(material.diffuse().x(), material.diffuse().y(), material.diffuse().z());
    aiMat->AddProperty(&color, 1, AI_MATKEY_COLOR_DIFFUSE);

    color = aiColor3D(material.specular().x(), material.specular().y(), material.specular().z());
    aiMat->AddProperty(&color, 1, AI_MATKEY_COLOR_SPECULAR);

    color = aiColor3D(material.emissive().x(), material.emissive().y(), material.emissive().z());
    aiMat->AddProperty(&color, 1, AI_MATKEY_COLOR_EMISSIVE);

    float shininess = material.shininess();
    aiMat->AddProperty(&shininess, 1, AI_MATKEY_SHININESS);

    float opacity = material.opacity();
    aiMat->AddProperty(&opacity, 1, AI_MATKEY_OPACITY);

    return aiMat;
}

aiScene* AssImpMeshExporter::createScene(const std::vector<aiMesh*>& meshes,
    const std::vector<aiMaterial*>& materials) {
    aiScene* scene = new aiScene();

    scene->mNumMeshes = static_cast<unsigned int>(meshes.size());
    scene->mMeshes = new aiMesh * [scene->mNumMeshes];
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        scene->mMeshes[i] = meshes[i];
    }

    scene->mNumMaterials = static_cast<unsigned int>(materials.size());
    scene->mMaterials = new aiMaterial * [scene->mNumMaterials];
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
        scene->mMaterials[i] = materials[i];
    }

    scene->mRootNode = new aiNode();
    scene->mRootNode->mNumMeshes = scene->mNumMeshes;
    scene->mRootNode->mMeshes = new unsigned int[scene->mRootNode->mNumMeshes];
    for (unsigned int i = 0; i < scene->mRootNode->mNumMeshes; ++i) {
        scene->mRootNode->mMeshes[i] = i;
    }

    return scene;
}

