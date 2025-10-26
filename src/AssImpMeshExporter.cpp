#include "AssImpMeshExporter.h"
#include "AssImpMesh.h"
#include "GLMaterial.h"

#include <QDebug>
#include <algorithm>
#include <memory>

AssImpMeshExporter::AssImpMeshExporter(QObject* parent) : QObject(parent) {}

aiReturn AssImpMeshExporter::exportScene(const aiScene* scene, const std::string& exportPath)
{
    Assimp::Exporter exporter;
    const aiExportFormatDesc* format = findExportFormat(exportPath, exporter);
    if (!format)
    {
        qCritical() << "Unsupported export format for:" << QString::fromStdString(exportPath);
        return aiReturn_FAILURE;
    }

    aiReturn result = exporter.Export(scene, format->id, exportPath.c_str());
    if (result != aiReturn_SUCCESS)
    {
        qCritical() << "Export failed:" << exporter.GetErrorString();
    }
    else
    {
        qDebug() << "Export successful:" << QString::fromStdString(exportPath);
    }

    return result;
}

aiReturn AssImpMeshExporter::exportMeshes(const std::vector<AssImpMesh*>& meshes, const std::string& exportPath)
{
    std::vector<aiMesh*> aiMeshes;
    std::vector<aiMaterial*> aiMaterials;

    for (AssImpMesh* mesh : meshes)
    {
        const auto& vertices = mesh->vertices();
        const auto& indices = mesh->indices();

        if (vertices.empty() || indices.empty()) {
            qWarning() << "Skipping empty mesh";
            continue;
        }

        aiMesh* aimesh = createMesh(vertices, indices, mesh->getName().toStdString());
        if (!aimesh)
            continue;

        aiMaterial* mat = createMaterial(mesh->getMaterial());
        aimesh->mMaterialIndex = static_cast<unsigned int>(aiMaterials.size());

        aiMeshes.push_back(aimesh);
        aiMaterials.push_back(mat);
    }

    std::unique_ptr<aiScene> scene(createScene(aiMeshes, aiMaterials));

    Assimp::Exporter exporter;
    const aiExportFormatDesc* format = findExportFormat(exportPath, exporter);
    if (!format) {
        qCritical() << "Unsupported export format for:" << QString::fromStdString(exportPath);
        return aiReturn_FAILURE;
    }

    aiReturn result = exporter.Export(scene.get(), format->id, exportPath.c_str());
    if (result != aiReturn_SUCCESS) {
        qCritical() << "Export failed:" << exporter.GetErrorString();
    } else {
        qDebug() << "Export successful:" << QString::fromStdString(exportPath);
    }

    return result;
}

aiMesh* AssImpMeshExporter::createMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, const std::string& name)
{
    auto mesh = new aiMesh();

	mesh->mName = aiString(name.c_str());
    mesh->mNumVertices = static_cast<unsigned int>(vertices.size());
    mesh->mVertices = new aiVector3D[mesh->mNumVertices];
    mesh->mNormals = new aiVector3D[mesh->mNumVertices];
    mesh->mTextureCoords[0] = new aiVector3D[mesh->mNumVertices];
    mesh->mColors[0] = new aiColor4D[mesh->mNumVertices];

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        const auto& v = vertices[i];
        mesh->mVertices[i] = aiVector3D(v.Position.x, v.Position.y, v.Position.z);
        mesh->mNormals[i] = aiVector3D(v.Normal.x, v.Normal.y, v.Normal.z);
        mesh->mTextureCoords[0][i] = aiVector3D(v.TexCoords[0].s, v.TexCoords[0].t, 0.0f);
        mesh->mColors[0][i] = aiColor4D(v.Color.r, v.Color.g, v.Color.b, v.Color.a);
    }

    mesh->mNumFaces = static_cast<unsigned int>(indices.size() / 3);
    mesh->mFaces = new aiFace[mesh->mNumFaces];

    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        aiFace& face = mesh->mFaces[i];
        face.mNumIndices = 3;
        face.mIndices = new unsigned int[3]{
            indices[i * 3],
            indices[i * 3 + 1],
            indices[i * 3 + 2]
        };
    }

    return mesh;
}

aiMaterial* AssImpMeshExporter::createMaterial(const GLMaterial& material)
{
    auto aiMat = new aiMaterial();

    aiColor3D color;

    color = aiColor3D(material.ambient().x(), material.ambient().y(), material.ambient().z());
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

aiScene* AssImpMeshExporter::createScene(const std::vector<aiMesh*>& meshes, const std::vector<aiMaterial*>& materials)
{
    aiScene* scene = new aiScene();

    // Set up meshes array
    scene->mNumMeshes = static_cast<unsigned int>(meshes.size());
    scene->mMeshes = new aiMesh * [scene->mNumMeshes];
    std::copy(meshes.begin(), meshes.end(), scene->mMeshes);

    // Set up materials array
    scene->mNumMaterials = static_cast<unsigned int>(materials.size());
    scene->mMaterials = new aiMaterial * [scene->mNumMaterials];
    std::copy(materials.begin(), materials.end(), scene->mMaterials);

    // Create root node
    scene->mRootNode = new aiNode();
    scene->mRootNode->mName = aiString("RootNode");

    // Create child nodes - one for each mesh
    scene->mRootNode->mNumChildren = static_cast<unsigned int>(meshes.size());
    scene->mRootNode->mChildren = new aiNode * [scene->mRootNode->mNumChildren];

    for (unsigned int i = 0; i < meshes.size(); ++i)
    {
        aiNode* childNode = new aiNode();

        // Give each node a unique name (important for OBJ export)
        std::string nodeName = "Mesh_" + std::to_string(i);
        if (meshes[i]->mName.length > 0)
        {
            nodeName = std::string(meshes[i]->mName.C_Str());			
        }		
        childNode->mName = aiString(nodeName);

        // Set parent relationship
        childNode->mParent = scene->mRootNode;

        // Assign one mesh to this node
        childNode->mNumMeshes = 1;
        childNode->mMeshes = new unsigned int[1];
        childNode->mMeshes[0] = i;

        // Set identity transformation matrix
        childNode->mTransformation = aiMatrix4x4();

        scene->mRootNode->mChildren[i] = childNode;
    }

    // Root node should not have meshes directly assigned
    scene->mRootNode->mNumMeshes = 0;
    scene->mRootNode->mMeshes = nullptr;

    return scene;
}

const aiExportFormatDesc* AssImpMeshExporter::findExportFormat(const std::string& filePath, Assimp::Exporter& exporter)
{
    std::string ext = filePath.substr(filePath.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    for (unsigned int i = 0; i < exporter.GetExportFormatCount(); ++i) {
        const aiExportFormatDesc* fmt = exporter.GetExportFormatDescription(i);
        if (fmt && ext == fmt->fileExtension) {
            return fmt;
        }
    }
    return nullptr;
}
