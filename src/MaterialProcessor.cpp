#include "MaterialProcessor.h"
#include "Utils.h"

#include <string>

using namespace std;

MaterialProcessor::MaterialProcessor() : _folderPath("")
{
    initializeOpenGLFunctions();
}

MaterialProcessor::MaterialProcessor(std::string& folderPath) : _folderPath(folderPath)
{
    initializeOpenGLFunctions();
}

void MaterialProcessor::setColorAndMaterial(aiMaterial* material, GLMaterial& mat)
{
    if (!material)
    {
        setDefaultMaterial(mat);
        return;
    }

    // Initialize default values
    aiColor3D color(0.f, 0.f, 0.f);
    float value = 0.0f;
    int intValue = 0;

    // === PBR Material Properties ===

    // Albedo/Base Color (prioritize PBR over legacy diffuse)
    if (AI_SUCCESS == material->Get(AI_MATKEY_BASE_COLOR, color) ||
        AI_SUCCESS == material->Get(AI_MATKEY_COLOR_DIFFUSE, color))
    {
        mat.setAlbedoColor(QVector3D(color.r, color.g, color.b));
        mat.setDiffuse(QVector3D(color.r, color.g, color.b)); // Legacy compatibility
    }
    else
    {
        mat.setAlbedoColor(QVector3D(0.8f, 0.8f, 0.8f));
        mat.setDiffuse(QVector3D(0.8f, 0.8f, 0.8f));
    }

    // Metallic Factor
    if (AI_SUCCESS == material->Get(AI_MATKEY_METALLIC_FACTOR, value))
    {
        mat.setMetalness(std::clamp(value, 0.0f, 1.0f));
    }
    else
    {
        // Fallback: derive from specular color for legacy materials
        if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_SPECULAR, color))
        {
            mat.setSpecular(QVector3D(color.r, color.g, color.b));

            // Convert specular to metallic approximation
            bool isGrayscale = (std::abs(color.r - color.g) < 0.01f &&
                std::abs(color.g - color.b) < 0.01f);
            float intensity = 0.299f * color.r + 0.587f * color.g + 0.114f * color.b;

            if (isGrayscale && intensity > 0.9f)
            {
                mat.setMetalness(0.8f); // Likely metallic
            }
            else if (intensity > 0.5f)
            {
                mat.setMetalness(0.3f); // Semi-metallic
            }
            else
            {
                mat.setMetalness(0.0f); // Dielectric
            }
        }
        else
        {
            mat.setSpecular(QVector3D(0.04f, 0.04f, 0.04f)); // Default dielectric F0
            mat.setMetalness(0.0f);
        }
    }

    // Roughness Factor
    if (AI_SUCCESS == material->Get(AI_MATKEY_ROUGHNESS_FACTOR, value))
    {
        mat.setRoughness(std::clamp(value, 0.01f, 1.0f)); // Avoid zero roughness
    }
    else if (AI_SUCCESS == material->Get(AI_MATKEY_SHININESS, value))
    {
        // Convert shininess to roughness (Phong to PBR approximation)
        float roughness = std::sqrt(2.0f / (value + 2.0f));
        mat.setRoughness(std::clamp(roughness, 0.01f, 1.0f));
    }
    else
    {
        mat.setRoughness(0.8f); // Default medium roughness
    }

    // === Additional Material Properties ===

    // Ambient (for legacy lighting models)
    if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_AMBIENT, color))
    {
        mat.setAmbient(QVector3D(color.r, color.g, color.b));
    }
    else
    {
        // Derive ambient from albedo with reduced intensity
        QVector3D albedo = mat.getAlbedoColor();
        mat.setAmbient(albedo * 0.1f);
    }

    // Emissive
    if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_EMISSIVE, color))
    {
        mat.setEmissive(QVector3D(color.r, color.g, color.b));

        // Set emissive strength if available
        if (AI_SUCCESS == material->Get(AI_MATKEY_EMISSIVE_INTENSITY, value))
        {
            mat.setEmissiveStrength(value);
        }
    }
    else
    {
        mat.setEmissive(QVector3D(0.0f, 0.0f, 0.0f));
        mat.setEmissiveStrength(1.0f);
    }

    // Opacity/Transparency
    float opacity = 1.0f;
    if (AI_SUCCESS == material->Get(AI_MATKEY_OPACITY, opacity))
    {
        if (opacity == 0) // 0 opacity is of no use
            opacity = 1;

        opacity = std::clamp(opacity, 0.0f, 1.0f);
        mat.setOpacity(opacity);        
    }
    else if (AI_SUCCESS == material->Get(AI_MATKEY_TRANSPARENCYFACTOR, value))
    {
        // Some formats use transparency factor instead of opacity
        opacity = 1.0f - std::clamp(value, 0.0f, 1.0f);

        if (opacity == 0) // 0 opacity is of no use
            opacity = 1;

        mat.setOpacity(opacity);        
    }
    else
    {
        mat.setOpacity(1.0f);
    }

    // Index of Refraction (IOR)
    if (AI_SUCCESS == material->Get(AI_MATKEY_REFRACTI, value))
    {
        mat.setIOR(std::clamp(value, 1.0f, 3.0f)); // Physically plausible range
    }
    else
    {
        mat.setIOR(1.5f); // Default glass IOR
    }

    // === Advanced Properties ===

    // Clearcoat (if supported by your material system)
    if (AI_SUCCESS == material->Get(AI_MATKEY_CLEARCOAT_FACTOR, value))
    {
        mat.setClearcoat(std::clamp(value, 0.0f, 1.0f));
		std::cout << "Clearcoat factor: " << value << std::endl;
        if (AI_SUCCESS == material->Get(AI_MATKEY_CLEARCOAT_ROUGHNESS_FACTOR, value))
        {
            mat.setClearcoatRoughness(std::clamp(value, 0.0f, 1.0f));
            std::cout << "Clearcoat Roughness factor: " << value << std::endl;
        }		
    }

    // Sheen (for fabric-like materials)
    if (AI_SUCCESS == material->Get(AI_MATKEY_SHEEN_COLOR_FACTOR, color))
    {
        mat.setSheenColor(QVector3D(color.r, color.g, color.b));

        if (AI_SUCCESS == material->Get(AI_MATKEY_SHEEN_ROUGHNESS_FACTOR, value))
        {
            mat.setSheenRoughness(std::clamp(value, 0.0f, 1.0f));
        }
    }

    // Transmission (for glass-like materials)
    if (AI_SUCCESS == material->Get(AI_MATKEY_TRANSMISSION_FACTOR, value))
    {
        mat.setTransmission(std::clamp(value, 0.0f, 1.0f));
    }

    // === Rendering Hints ===

    // Two-sided material
    if (AI_SUCCESS == material->Get(AI_MATKEY_TWOSIDED, intValue))
    {
        mat.setTwoSided(intValue != 0);
    }

    // Wireframe mode
    if (AI_SUCCESS == material->Get(AI_MATKEY_ENABLE_WIREFRAME, intValue))
    {
        mat.setWireframe(intValue != 0);
    }

    // Shading model
    if (AI_SUCCESS == material->Get(AI_MATKEY_SHADING_MODEL, intValue))
    {
        setShadingModel(mat, static_cast<aiShadingMode>(intValue));
    }

    // Blend mode for transparency
    if (opacity < 1.0f)
    {
        // Determine blend mode based on material properties
        if (AI_SUCCESS == material->Get(AI_MATKEY_BLEND_FUNC, intValue))
        {
            setBlendMode(mat, static_cast<aiBlendMode>(intValue));
        }
        else
        {
            // Default to alpha blending for transparent materials
            mat.setBlendMode(GLMaterial::BlendMode::Alpha);
        }
    }

    // === Validation and Consistency Checks ===
    validateMaterialConsistency(mat);
}

void MaterialProcessor::setShadingModel(GLMaterial& mat, aiShadingMode shadingModel)
{
    switch (shadingModel)
    {
    case aiShadingMode_Flat:
        mat.setShadingModel(GLMaterial::ShadingModel::Unlit);
        break;
    case aiShadingMode_Phong:
    case aiShadingMode_Blinn:
        mat.setShadingModel(GLMaterial::ShadingModel::BlinnPhong);
        break;
    case aiShadingMode_PBR_BRDF:
        mat.setShadingModel(GLMaterial::ShadingModel::PBR);
        break;
    case aiShadingMode_Unlit:
        mat.setShadingModel(GLMaterial::ShadingModel::Unlit);
        break;
    default:
        mat.setShadingModel(GLMaterial::ShadingModel::PBR); // Default to PBR
        break;
    }
}

void MaterialProcessor::setBlendMode(GLMaterial& mat, aiBlendMode blendMode)
{
    switch (blendMode)
    {
    case aiBlendMode_Additive:
        mat.setBlendMode(GLMaterial::BlendMode::Additive);
        break;
    case aiBlendMode_Default:
    default:
        mat.setBlendMode(GLMaterial::BlendMode::Alpha);
        break;
    }
}

void MaterialProcessor::validateMaterialConsistency(GLMaterial& mat)
{
    // Ensure metallic materials have appropriate F0 values
    if (mat.getMetalness() > 0.5f)
    {
        QVector3D albedo = mat.getAlbedoColor();
        // For metals, F0 should be derived from albedo, not be the default dielectric value
        mat.setSpecular(albedo);
    }

    // Ensure rough metals don't have unrealistic specular values
    if (mat.getMetalness() > 0.8f && mat.getRoughness() < 0.1f)
    {
        // Very smooth metals are rare in practice
        mat.setRoughness(std::max(0.1f, mat.getRoughness()));
    }

    // Validate opacity consistency
    if (mat.getOpacity() < 1.0f && mat.getTransmission() > 0.0f)
    {
        // For transmitted materials, opacity and transmission should be consistent
        float totalTransparency = 1.0f - mat.getOpacity() + mat.getTransmission();
        if (totalTransparency > 1.0f)
        {
            mat.setTransmission(1.0f - (1.0f - mat.getOpacity()));
        }
    }
}

void MaterialProcessor::setDefaultMaterial(GLMaterial& mat)
{
	mat = GLMaterial::DEFAULT_MAT();
}

unsigned int MaterialProcessor::textureFromFile(const char* path, std::string directory)
{
    //Generate texture ID and load texture data
    string filename = string(path);
    filename = directory + '/' + filename;
    unsigned int textureID;
    glGenTextures(1, &textureID);

    QImage texImage;

    if (!texImage.load(QString(filename.c_str())))
    { // Load first image from file
        qWarning("MaterialProcessor::textureFromFile - Could not read image file, using single-color instead.");
        QImage dummy(128, 128, QImage::Format_ARGB32);
        dummy.fill(Qt::white);
        texImage = dummy;
    }
    else
    {
        texImage = convertToGLFormat(texImage);
    }

    // Assign texture to ID
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texImage.width(), texImage.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, texImage.bits());
    glGenerateMipmap(GL_TEXTURE_2D);

    // Parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureID;
}

// Sets the texture maps for a material based on the defined texture mappings.
void MaterialProcessor::setTextureMaps(aiMaterial* material, std::vector<Texture>& textures)
{
    // existing mapping loop that calls loadMaterialTextures(...) for all entries
    for (const auto& mapping : textureMappings)
    {
        std::cout << mapping << std::endl;
        // try primary
        auto maps = loadMaterialTextures(material, mapping.primaryType, mapping.primaryName, mapping.slotIndex);
		std::cout << "Loaded " << maps.size() << " textures for " << mapping.primaryName << std::endl;
        textures.insert(textures.end(), maps.begin(), maps.end());

        // optionally, also try explicit fallback as a separate load (safe because
        // loadMaterialTextures reuses existing IDs and avoids reloading)
        if (mapping.fallbackType != aiTextureType_NONE)
        {
            auto fmaps = loadMaterialTextures(material, mapping.fallbackType, mapping.fallbackName, mapping.fallbackSlotIndex);
            textures.insert(textures.end(), fmaps.begin(), fmaps.end());
        }
    }

    // Now create ADS aliases from PBR maps for backward compatibility
    synthesizeADSAliases(textures);
}


// Checks all material textures of a given type and loads the textures if they're not loaded yet.
// The required info is returned as a Texture struct.
std::vector<Texture> MaterialProcessor::loadMaterialTextures(
    aiMaterial* mat,
    aiTextureType type,
    const std::string& typeName,
    unsigned int slotIndex)
{
    std::vector<Texture> textures;

    if (mat->GetTextureCount(type) <= slotIndex)
        return textures;

    aiString str;
    if (mat->GetTexture(type, slotIndex, &str) != AI_SUCCESS)
        return textures;

    // If same path+type already loaded -> reuse
    for (const auto& lt : _loadedTextures)
    {
        if (lt.path == str && lt.type == typeName)
        {
            textures.push_back(lt);
            std::cout << lt << std::endl;
            return textures;
        }
    }

    // If same path loaded but with different uniform name -> reuse its GPU id
    for (const auto& lt : _loadedTextures)
    {
        if (lt.path == str && lt.type != typeName)
        {
            Texture alias;
            alias.id = lt.id;           // reuse GPU texture
            alias.type = typeName;        // requested uniform name
            alias.path = lt.path;
            textures.push_back(alias);
            _loadedTextures.push_back(alias); // register alias to avoid re-creating later
            std::cout << lt << std::endl;
            return textures;
        }
    }

    // Not loaded at all: load from file
    Texture texture;
    texture.id = textureFromFile(str.C_Str(), this->_folderPath);
    texture.type = typeName;
    texture.path = str;
    textures.push_back(texture);
    _loadedTextures.push_back(texture);

    std::cout << texture << std::endl;

    return textures;
}

void MaterialProcessor::synthesizeADSAliases(std::vector<Texture>& textures)
{
    // map PBR uniform -> ADS uniform we want to create if missing
    static const std::vector<std::pair<std::string, std::string>> pbrToAds = {
        { "albedoMap",   "texture_diffuse" },
        { "normalMap",   "texture_normal"  },
        { "emissiveMap", "texture_emissive"},
        { "metallicMap", "texture_specular"},
        { "roughnessMap","texture_specular"}, // rough idea: share with specular slot
        { "heightMap",   "texture_height"  },
        { "opacityMap",  "texture_opacity" },
    };

    // quick helpers to test existence
    auto hasType = [&](const std::string& t) {
        for (const auto& tex : textures) if (tex.type == t) return true;
        return false;
        };

    // for each mapping, if PBR is present but ADS is missing -> create alias entry
    for (auto& map : pbrToAds)
    {
        const std::string& pbrName = map.first;
        const std::string& adsName = map.second;

        if (hasType(adsName)) continue; // ADS already present -> skip

        // find first PBR texture with pbrName
        for (const auto& tex : textures)
        {
            if (tex.type == pbrName)
            {
                // produce alias (reuse id and path)
                Texture alias;
                alias.id = tex.id;
                alias.path = tex.path;
                alias.type = adsName;

                textures.push_back(alias);
                _loadedTextures.push_back(alias); // register to global cache so future loads reuse
                break;
            }
        }
    }
}


