#pragma once

#include <QObject>
#include <vector>
#include <assimp/scene.h>
#include <assimp/Exporter.hpp>

class GLMaterial;
class AssImpMesh;
struct Vertex;

class AssImpMeshExporter : public QObject
{
    Q_OBJECT

public:
    explicit AssImpMeshExporter(QObject* parent = nullptr);
    aiReturn exportMeshes(const std::vector<AssImpMesh*>& meshes, const std::string& exportPath);

private:
    aiMesh* createMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    aiMaterial* createMaterial(const GLMaterial& material);
    aiScene* createScene(const std::vector<aiMesh*>& meshes, const std::vector<aiMaterial*>& materials);
    const aiExportFormatDesc* findExportFormat(const std::string& filePath, Assimp::Exporter& exporter);
};
