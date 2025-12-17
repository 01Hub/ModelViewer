#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <assimp/scene.h>
#include "GLMaterial.h"
#include "AssImpMesh.h"
#include "GLLights.h"
#include <QOpenGLFunctions_4_5_Core>


class MaterialProcessor : public QOpenGLFunctions_4_5_Core
{
public:
	MaterialProcessor();
	MaterialProcessor(std::string& folderPath);

	void setFolderPath(const std::string& folderPath) { _folderPath = folderPath; }

	void setColorAndMaterial(aiMaterial* material, GLMaterial& mat);
	void setDefaultMaterial(GLMaterial& mat);
	void setTextureMaps(aiMaterial* material, std::vector<GLMaterial::Texture>& textures, GLMaterial& mat);
	void setTextureTransforms(const std::vector<GLMaterial::Texture>& textures, GLMaterial& mat);
	void addExtensionMaps(GLMaterial& mat, std::vector<GLMaterial::Texture>& textures);
	void clearLoadedTextures() { _loadedTextures.clear(); }

	// Checks all material textures of a given type and loads the textures if they're not loaded yet.
	// The required info is returned as a Texture struct.    
	std::vector<GLMaterial::Texture> loadMaterialTextures(
		aiMaterial* mat,
		aiTextureType type,
		const std::string& typeName,
		unsigned int slotIndex);

	bool checkImageForAlpha(const QImage& image);

	void synthesizeADSAliases(std::vector<GLMaterial::Texture>& textures);

	void applyGltfMaterialExtensionsToMaterial(
		const QString& gltfPath,
		const aiScene* scene,
		const QString& nodeName,
		const aiMesh* currentMesh, 
		int materialIndex,
		GLMaterial& outMaterial,
		std::vector<GLMaterial::Texture>& outTextures);

	std::tuple<int, glm::vec2, glm::vec2, float> extractKHRTextureTransform(const QJsonObject& texObj);
	void applyKHRTextureTransformsToMaterial(
		const GLMaterial::Texture& texture,
		const std::string& mapType,
		GLMaterial& outMaterial);

	std::vector<GPULight> parseKHRLightsPunctual(const QString& gltfPath);

private:
	void setShadingModel(GLMaterial& mat, aiShadingMode shadingModel);
	void setBlendMode(GLMaterial& mat, aiBlendMode blendMode);

	void validateMaterialConsistency(GLMaterial& mat);

	unsigned int createTextureOnGPU(GLMaterial::Texture& texture);

private:
	// Each entry: primary type + uniform name, and an optional fallback type+uniform name
	struct TextureSlotMapping
	{
		aiTextureType primaryType;
		std::string primaryName;
		unsigned int slotIndex;
		aiTextureType fallbackType;
		std::string fallbackName;
		unsigned int fallbackSlotIndex;

		// print for debugging
		friend std::ostream& operator<<(std::ostream& os, const TextureSlotMapping& mapping)
		{
			os << "Primary: " << mapping.primaryType << ", Name: " << mapping.primaryName
				<< ", Slot: " << mapping.slotIndex
				<< ", Fallback: " << mapping.fallbackType << ", Fallback Name: " << mapping.fallbackName
				<< ", Fallback Slot: " << mapping.fallbackSlotIndex;
			return os;
		}
	};

	const std::vector<TextureSlotMapping> textureMappings = {
		// PBR core 
		{ aiTextureType_BASE_COLOR,        "albedoMap",         0, aiTextureType_DIFFUSE,    "texture_diffuse", 0 },
		{ aiTextureType_METALNESS,         "metallicMap",       0, aiTextureType_NONE,       "",                0 },
		{ aiTextureType_DIFFUSE_ROUGHNESS, "roughnessMap",      0, aiTextureType_NONE,       "",                0 },
		{ aiTextureType_NORMALS,           "normalMap",         0, aiTextureType_HEIGHT,     "texture_normal",  0 },
		{ aiTextureType_LIGHTMAP,          "aoMap",             0, aiTextureType_AMBIENT_OCCLUSION, "aoMap",    0 },
		{ aiTextureType_EMISSIVE,          "emissiveMap",       0, aiTextureType_EMISSION_COLOR, "emissiveMap", 0 },
		{ aiTextureType_AMBIENT_OCCLUSION, "aoMap",             0, aiTextureType_LIGHTMAP,   "aoMap",           0 },
		{ aiTextureType_DISPLACEMENT,      "heightMap",         0, aiTextureType_DISPLACEMENT, "texture_height", 0 },
		{ aiTextureType_OPACITY,           "opacityMap",        0, aiTextureType_OPACITY,    "texture_opacity", 0 },
		{ aiTextureType_EMISSION_COLOR,    "emissiveMap",       0, aiTextureType_EMISSIVE,   "texture_emissive", 0 },

		// KHR_materials_clearcoat
		{ aiTextureType_CLEARCOAT,         "clearcoatMap",            0, aiTextureType_NONE, "", 0 },
		{ aiTextureType_CLEARCOAT,         "clearcoatRoughnessMap",   1, aiTextureType_NONE, "", 0 },
		{ aiTextureType_CLEARCOAT,         "clearcoatNormalMap",      2, aiTextureType_HEIGHT, "texture_normal", 0 },

		// KHR_materials_sheen
		{ aiTextureType_SHEEN,             "sheenColorMap",     0, aiTextureType_NONE, "", 0 },
		{ aiTextureType_SHEEN,             "sheenRoughnessMap", 1, aiTextureType_NONE, "", 0 },

		// KHR_materials_transmission
		{ aiTextureType_TRANSMISSION,      "transmissionMap",   0, aiTextureType_NONE, "", 0 },

		// KHR_materials_volume
		{ aiTextureType_TRANSMISSION,      "thicknessMap",      1, aiTextureType_NONE, "", 0 },

		// KHR_materials_ior (if mapped to a texture)
		{ aiTextureType_REFLECTION,        "iorMap",            0, aiTextureType_NONE, "", 0 },

		// KHR_materials_specular
		{ aiTextureType_SPECULAR,          "specularFactorMap", 0, aiTextureType_NONE, "", 0 },
		{ aiTextureType_SPECULAR,          "specularColorMap",  1, aiTextureType_NONE, "", 0 },

		// KHR_materials_anisotropy
		{ aiTextureType_ANISOTROPY,        "anisotropyMap",     0, aiTextureType_NONE, "", 0 },

		// KHR_materials_iridescence
		{ aiTextureType_REFLECTION,        "iridescenceMap",    2, aiTextureType_NONE, "", 0 },
		{ aiTextureType_REFLECTION,        "iridescenceThicknessMap", 3, aiTextureType_NONE, "", 0 },

		// Legacy ADS types as fallbacks
		{ aiTextureType_DIFFUSE,           "texture_diffuse",   0, aiTextureType_NONE, "", 0 },
		{ aiTextureType_SPECULAR,          "texture_specular",  0, aiTextureType_NONE, "", 0 },
		{ aiTextureType_HEIGHT,            "texture_normal",    0, aiTextureType_NORMALS, "normalMap", 0 },
		{ aiTextureType_DISPLACEMENT,      "texture_height",    0, aiTextureType_NONE, "", 0 }
	};

	void debugMaterialTextures(aiMaterial* material, const std::string& materialName);

	void extractUVTransform(
		aiMaterial* mat,
		aiTextureType type,
		unsigned int slotIndex,
		GLMaterial::Texture& texture);



private:
	std::vector<GLMaterial::Texture> _loadedTextures;	// Stores all the textures loaded so far, optimization to make sure textures aren't loaded more than once.
	std::string _folderPath; // Directory where textures are located

};