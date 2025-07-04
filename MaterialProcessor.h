#pragma once

#include <assimp/scene.h>
#include "GLMaterial.h"
#include "AssImpMesh.h"
#include <QOpenGLFunctions_4_5_Core>

class MaterialProcessor : public QOpenGLFunctions_4_5_Core
{
public:
    MaterialProcessor();

    void setColorAndMaterial(aiMaterial* material, GLMaterial& mat);
    void setDefaultMaterial(GLMaterial& mat);

private:
    void setShadingModel(GLMaterial& mat, aiShadingMode shadingModel);
    void setBlendMode(GLMaterial& mat, aiBlendMode blendMode);
    void validateMaterialConsistency(GLMaterial& mat);
    void setPBRTextureMaps(aiMaterial* material, std::vector<Texture>& textures);
    void setADSTextureMaps(aiMaterial* material, std::vector<Texture>& textures);
    
    unsigned int textureFromFile(const char* path, std::string directory);
};