#pragma once

#include <string>
#include <map>
#include <glm/glm.hpp>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

/**
 * Stores texture transformation and coordinate metadata extracted from glTF
 */
struct TextureMetadata
{
    int texCoordIndex = 0;           // Which TEXCOORD_N to use (0-3)
    glm::vec2 scale = glm::vec2(1.0f);      // Tiling scale
    glm::vec2 offset = glm::vec2(0.0f);     // UV offset
    float rotation = 0.0f;                   // Rotation in radians
    std::string filePath = "";               // Full resolved path to texture file
    std::string uri = "";                    // Original URI from glTF (relative path)

    bool hasTransform = false;       // Whether KHR_texture_transform was applied
};

struct ScalarMetadata
{
    float thicknessFactor = 0.0f;
    float attenuationDistance = 0.0f;
    glm::vec3 attenuationColor = glm::vec3(1.0f); // default white
    float ior = 1.5f;
    float transmissionFactor = 0.0f;
    float clearcoatFactor = 0.0f;
    float clearcoatRoughness = 0.0f;
    float sheenRoughness = 0.0f;
    float specularFactor = 0.0f;
    glm::vec3 specularColorFactor = glm::vec3(1.0f);
    float iridescenceFactor = 0.0f;
    float iridescenceIor = 1.3f;
    float iridescenceThicknessMin = 100.0f;
    float iridescenceThicknessMax = 400.0f;
    float anisotropyStrength = 0.0f;
    float anisotropyRotation = 0.0f;
    bool unlit = false;
};

/**
 * Extracts glTF metadata from .gltf JSON files
 * Focuses on:
 * - Texture coordinate indices per texture
 * - KHR_texture_transform extension data
 * - Material to texture mappings
 */
class GLTFMetadataExtractor
{
public:
    GLTFMetadataExtractor();
    ~GLTFMetadataExtractor() = default;

    /**
     * Parse glTF file and extract texture metadata
     * @param filePath Path to .gltf file
     * @return true if parsing succeeded
     */
    bool parseGLTFFile(const std::string& filePath);

    /**
     * Get metadata for a texture by index
     * @param textureIndex Index from glTF texture array
     * @return TextureMetadata with coordinate and transform info
     */
    TextureMetadata getTextureMetadata(int textureIndex) const;

    /**
     * Get metadata for all textures in a material
     * @param materialIndex Index from glTF material array
     * @return Map of texture role (baseColor, emissive, etc.) to metadata
     */
    std::map<std::string, TextureMetadata> getMaterialTextureMetadata(int materialIndex) const;

    /**
     * Check if a texture index exists in parsed data
     */
    bool hasTextureMetadata(int textureIndex) const;

    /**
     * Get the full file path for a texture (useful for fallback loading)
     * @param textureIndex Index from glTF texture array
     * @return Full resolved path to texture file, empty string if not found
     */
    std::string getTextureFilePath(int textureIndex) const;

    /**
     * Get file path for a specific texture role in a material (e.g., "baseColor", "normal")
     * @param materialIndex Index from glTF material array
     * @param role Texture role name ("baseColor", "normal", "emissive", etc.)
     * @return Full resolved path to texture file, empty string if not found
     */
    std::string getMaterialTextureFilePath(int materialIndex, const std::string& role) const;

    /**
    *
    */
    ScalarMetadata getScalarMetadata(int materialIndex) const;

    /**
     * Clear all cached metadata
     */
    void clear();

private:
    // Maps: texture index -> metadata
    std::map<int, TextureMetadata> _textureMetadata;

    // Maps: material index -> (role -> texture info)
    // role: "baseColor", "normal", "emissive", etc.
    std::map<int, std::map<std::string, std::pair<int, TextureMetadata>>> _materialTextures;

    std::map<int, ScalarMetadata> _materialScalars;

    std::string _glTFDirectoryPath;  // Directory containing the glTF file (for resolving relative texture paths)

    /**
     * Helper: Parse a single texture info object from JSON
     * Handles both direct properties and KHR_texture_transform extension
     */
    TextureMetadata parseTextureInfo(const QJsonObject& textureInfoObj);

    /**
     * Helper: Parse KHR_texture_transform extension
     */
    void parseTextureTransform(const QJsonObject& transformObj, TextureMetadata& metadata);

    /**
     * Helper: Extract texCoord index from texture info object
     */
    int extractTexCoordIndex(const QJsonObject& textureInfoObj);

    /**
     * Helper: Extract texture URI from a texture index (looks up textures[] and images[] arrays)
     */
    std::string extractTextureURI(const QJsonObject& root, int textureIndex);

    /**
     * Helper: Resolve a texture URI to a full file path relative to glTF directory
     */
    std::string resolveTexturePath(const std::string& uri) const;

    /**
     * Helper: Parse all materials and build material->texture mappings
     */
    void parseMaterials(const QJsonObject& root);
};
