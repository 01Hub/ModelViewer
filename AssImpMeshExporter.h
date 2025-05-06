#ifndef ASSIMPMESHEXPORTER_H
#define ASSIMPMESHEXPORTER_H

#include <QObject>

#include <assimp/vector3.h>
#include <assimp/mesh.h>
#include <assimp/scene.h>

class GLMaterial;
class AssImpMesh;
class AssImpMeshExporter : public QObject
{
    Q_OBJECT
public:
    explicit AssImpMeshExporter(QObject *parent = nullptr);


    aiReturn exportMeshes(const std::vector<AssImpMesh*>& meshes, const std::string& exportPath);
signals:


private:
    aiMesh* createMesh(const std::vector<aiVector3D>& vertices,
                       const std::vector<unsigned int>& indices,
                       const std::vector<aiVector3D>& normals,
                       const std::vector<aiVector3D>& texCoords,
                        const std::vector<aiColor4D>& colors);

    aiMaterial* createMaterial(const GLMaterial& material);

    aiScene* createScene(const std::vector<aiMesh*>& meshes,
        const std::vector<aiMaterial*>& materials);

};

#endif // ASSIMPMESHEXPORTER_H
