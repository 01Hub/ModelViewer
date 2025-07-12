#pragma once

#include <assimp/scene.h>
#include "GLMaterial.h"
#include "AssImpMesh.h"
#include <QOpenGLFunctions_4_5_Core>

class MaterialProcessor : public QOpenGLFunctions_4_5_Core
{
public:
    MaterialProcessor();
    MaterialProcessor(std::string& folderPath);

	void setFolderPath(const std::string& folderPath) { _folderPath = folderPath; }

    void setColorAndMaterial(aiMaterial* material, GLMaterial& mat);
    void setDefaultMaterial(GLMaterial& mat);
    void setPBRTextureMaps(aiMaterial* material, std::vector<Texture>& textures);
    void setADSTextureMaps(aiMaterial* material, std::vector<Texture>& textures);

	void clearLoadedTextures() { _loadedTextures.clear(); }

    // Checks all material textures of a given type and loads the textures if they're not loaded yet.
    // The required info is returned as a Texture struct.
    std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName);


private:
    void setShadingModel(GLMaterial& mat, aiShadingMode shadingModel);
    void setBlendMode(GLMaterial& mat, aiBlendMode blendMode);    
    

    void validateMaterialConsistency(GLMaterial& mat);
    
    unsigned int textureFromFile(const char* path, std::string directory);

    std::vector<Texture> _loadedTextures;	// Stores all the textures loaded so far, optimization to make sure textures aren't loaded more than once.
    
	std::string _folderPath; // Directory where textures are located
};