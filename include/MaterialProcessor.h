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
    void setTextureMaps(aiMaterial* material, std::vector<Texture>& textures);

	void clearLoadedTextures() { _loadedTextures.clear(); }

    // Checks all material textures of a given type and loads the textures if they're not loaded yet.
    // The required info is returned as a Texture struct.    
    std::vector<Texture> loadMaterialTextures(
        aiMaterial* mat,
        aiTextureType type,
        const std::string& typeName,
        unsigned int slotIndex);

    void synthesizeADSAliases(std::vector<Texture>& textures);

private:
    void setShadingModel(GLMaterial& mat, aiShadingMode shadingModel);
    void setBlendMode(GLMaterial& mat, aiBlendMode blendMode);    
    

    void validateMaterialConsistency(GLMaterial& mat);
    
    unsigned int textureFromFile(const char* path, std::string directory);

    // Each entry: primary type + uniform name, and an optional fallback type+uniform name
    struct TextureSlotMapping
    {
        aiTextureType primaryType;
        std::string primaryName;
        unsigned int slotIndex;
        aiTextureType fallbackType;
        std::string fallbackName;
        unsigned int fallbackSlotIndex;
    };

    const std::vector<TextureSlotMapping> textureMappings = {
        // PBR core
        { aiTextureType_BASE_COLOR,        "albedoMap",         0, aiTextureType_DIFFUSE,  "texture_diffuse", 0 },
        { aiTextureType_METALNESS,         "metallicMap",       0, aiTextureType_SPECULAR, "texture_specular", 0 },
        { aiTextureType_DIFFUSE_ROUGHNESS, "roughnessMap",      0, aiTextureType_SPECULAR, "texture_specular", 0 },
        { aiTextureType_NORMAL_CAMERA,     "normalMap",         0, aiTextureType_HEIGHT,   "texture_normal",   0 },
        { aiTextureType_AMBIENT_OCCLUSION, "aoMap",             0, aiTextureType_NONE,     "",                 0 },
        { aiTextureType_DISPLACEMENT,      "heightMap",         0, aiTextureType_DISPLACEMENT, "texture_height", 0 },
        { aiTextureType_OPACITY,           "opacityMap",        0, aiTextureType_OPACITY,  "texture_opacity",  0 },
        { aiTextureType_EMISSIVE,          "emissiveMap",       0, aiTextureType_EMISSIVE, "texture_emissive", 0 },
        { aiTextureType_EMISSION_COLOR,    "emissiveMap",       0, aiTextureType_EMISSIVE, "texture_emissive", 0 },

        // Clearcoat extension
        { aiTextureType_CLEARCOAT,         "clearcoatMap",         0, aiTextureType_NONE, "" , 0 },
        { aiTextureType_CLEARCOAT,         "clearcoatRoughnessMap",1, aiTextureType_NONE, "" , 0 },
        { aiTextureType_CLEARCOAT,         "clearcoatNormalMap",   2, aiTextureType_HEIGHT, "texture_normal", 0 },

        // Sheen extension
        { aiTextureType_SHEEN,             "sheenColorMap",     0, aiTextureType_NONE, "", 0 },
        { aiTextureType_SHEEN,             "sheenRoughnessMap", 1, aiTextureType_NONE, "", 0 },

        // Transmission
        { aiTextureType_TRANSMISSION,      "transmissionMap",   0, aiTextureType_NONE, "", 0 },

        // IOR (if mapped to a texture in future)
        { aiTextureType_REFLECTION,        "iorMap",            0, aiTextureType_NONE, "", 0 },

        // ADS legacy not covered above
        { aiTextureType_DIFFUSE,           "texture_diffuse",   0, aiTextureType_NONE, "", 0 },
        { aiTextureType_SPECULAR,          "texture_specular",  0, aiTextureType_NONE, "", 0 },
        { aiTextureType_HEIGHT,            "texture_normal",    0, aiTextureType_NONE, "", 0 },
        { aiTextureType_DISPLACEMENT,      "texture_height",    0, aiTextureType_NONE, "", 0 },
        { aiTextureType_OPACITY,           "texture_opacity",   0, aiTextureType_NONE, "", 0 }
    };


    std::vector<Texture> _loadedTextures;	// Stores all the textures loaded so far, optimization to make sure textures aren't loaded more than once.
    
	std::string _folderPath; // Directory where textures are located
};