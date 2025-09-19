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

	// debugging: print material name
    /*for (unsigned int i = 0; i < material->mNumProperties; ++i)
    {
        const aiMaterialProperty* prop = material->mProperties[i];
        std::cout << "Property: " << prop->mKey.C_Str() << std::endl;
    }*/


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
	// --- Opacity / Transparency ---
    // MTL: d = opacity ; Tr = transparency
	float dVal = 1.0f;  bool hasD = (material->Get(AI_MATKEY_OPACITY, dVal) == AI_SUCCESS);
	float trVal = 0.0f;  bool hasTr = (material->Get(AI_MATKEY_TRANSPARENCYFACTOR, trVal) == AI_SUCCESS);

	dVal = std::clamp(dVal, 0.0f, 1.0f);
	trVal = std::clamp(trVal, 0.0f, 1.0f);

	float fromD = hasD ? dVal : 1.0f;        // already "opacity"
	float fromTr = hasTr ? (1.0f - trVal) : 1.0f;        // convert "transparency" to opacity

	// Use the most transparent interpretation when both are present
	float finalOpacity = std::min(fromD, fromTr);

    if(finalOpacity == 0)
		finalOpacity = 1.0f; // Avoid fully transparent materials by default

	// No "opacity == 0 -> 1" fallback; 0.0 is valid (fully transparent)
	mat.setOpacity(finalOpacity);

	// If not fully opaque, default the blend mode to alpha blending
	if (finalOpacity < 0.999f)
	{
		mat.setBlendMode(GLMaterial::BlendMode::Alpha);
	}

    // Blend mode for transparency
    if (finalOpacity < 1.0f)
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
        if (AI_SUCCESS == material->Get(AI_MATKEY_CLEARCOAT_ROUGHNESS_FACTOR, value))
        {
            mat.setClearcoatRoughness(std::clamp(value, 0.0f, 1.0f));            
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

	// Alpha cutoff for transparency for glTF materials
    aiString alphaModeStr;
    if (material->Get("$mat.gltf.alphaMode", 0, 0, alphaModeStr) == AI_SUCCESS)
    {
        std::string mode = alphaModeStr.C_Str();
        if (mode == "BLEND")
        {
            mat.setBlendMode(GLMaterial::BlendMode::Alpha);
        }
        else if (mode == "MASK")
        {
            mat.setBlendMode(GLMaterial::BlendMode::Masked);

            float cutoff = 0.5f; // Default value
            if (material->Get("$mat.gltf.alphaCutoff", 0, 0, cutoff) == AI_SUCCESS)
            {
                mat.setAlphaThreshold(cutoff);
            }
            else
            {
                mat.setAlphaThreshold(0.5f); // Fallback
            }
        }
        else
        {
            mat.setBlendMode(GLMaterial::BlendMode::Opaque);
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

unsigned int MaterialProcessor::textureFromFile(const char* path, std::string directory, bool& hasAlpha)
{
    //Generate texture ID and load texture data
    string filename = string(path);
    filename = directory + '/' + filename;
    unsigned int textureID;
    glGenTextures(1, &textureID);

    QImage texImage;
    bool imageHasAlpha = false;

    if (!texImage.load(QString(filename.c_str())))
    { // Load first image from file
        qWarning("MaterialProcessor::textureFromFile - Could not read image file, using single-color instead.");
        QImage dummy(128, 128, QImage::Format_ARGB32);
        dummy.fill(Qt::white);
        texImage = dummy;
        imageHasAlpha = false;        
    }
    else
    {
        texImage = convertToGLFormat(texImage);
        // Check for meaningful alpha channel
        if (texImage.hasAlphaChannel())
        {
            imageHasAlpha = checkImageForAlpha(texImage);
        }
    }

	hasAlpha = imageHasAlpha;

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

void MaterialProcessor::debugMaterialTextures(aiMaterial* material, const std::string& materialName)
{
    std::cout << "=== Material: " << materialName << " ===" << std::endl;

    auto getTextureTypeName = [](int type) -> const char* {
        switch (type)
        {
        case aiTextureType_DIFFUSE: return "DIFFUSE";
        case aiTextureType_SPECULAR: return "SPECULAR";
        case aiTextureType_AMBIENT: return "AMBIENT";
        case aiTextureType_EMISSIVE: return "EMISSIVE";
        case aiTextureType_HEIGHT: return "HEIGHT";
        case aiTextureType_NORMALS: return "NORMALS";
        case aiTextureType_SHININESS: return "SHININESS";
        case aiTextureType_OPACITY: return "OPACITY";
        case aiTextureType_DISPLACEMENT: return "DISPLACEMENT";
        case aiTextureType_LIGHTMAP: return "LIGHTMAP";
        case aiTextureType_REFLECTION: return "REFLECTION";
        case aiTextureType_BASE_COLOR: return "BASE_COLOR";
        case aiTextureType_NORMAL_CAMERA: return "NORMAL_CAMERA";
        case aiTextureType_EMISSION_COLOR: return "EMISSION_COLOR";
        case aiTextureType_METALNESS: return "METALNESS";
        case aiTextureType_DIFFUSE_ROUGHNESS: return "DIFFUSE_ROUGHNESS";
        case aiTextureType_AMBIENT_OCCLUSION: return "AMBIENT_OCCLUSION";
        case aiTextureType_SHEEN: return "SHEEN";
        case aiTextureType_CLEARCOAT: return "CLEARCOAT";
        case aiTextureType_TRANSMISSION: return "TRANSMISSION";
        case aiTextureType_UNKNOWN: return "UNKNOWN";
        default: return "OTHER";
        }
        };

    // Check MUCH wider range - some extensions might use higher type numbers
    for (int type = 0; type <= 50; type++)
    {  // Expanded range
        aiTextureType texType = static_cast<aiTextureType>(type);
        unsigned int count = material->GetTextureCount(texType);
        if (count > 0)
        {
            std::cout << "Type " << type << " (" << getTextureTypeName(type) << "): " << count << " textures" << std::endl;
            for (unsigned int i = 0; i < count; i++)
            {
                aiString path;
                material->GetTexture(texType, i, &path);
                std::cout << "  [" << i << "]: " << path.C_Str() << std::endl;
            }
        }
    }
    std::cout << "========================" << std::endl;
}

// Sets the texture maps for a material based on the defined texture mappings.
void MaterialProcessor::setTextureMaps(aiMaterial* material, std::vector<Texture>& textures, GLMaterial& mat)
{

	//debugMaterialTextures(material, material->GetName().C_Str());

    // existing mapping loop that calls loadMaterialTextures(...) for all entries
    for (const auto& mapping : textureMappings)
    {
        //std::cout << mapping << std::endl;
        // try primary
        auto maps = loadMaterialTextures(material, mapping.primaryType, mapping.primaryName, mapping.slotIndex);
		//std::cout << "Loaded " << maps.size() << " textures for " << mapping.primaryName << std::endl;
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

	// update material maps based on loaded textures
	// for each texture type, assign to material if found
    for (const auto& tex : textures)
    {
        if (tex.type == "albedoMap")
        {
            mat.setAlbedoTextureId(tex.id);
            mat.setAlbedoMap(QString(tex.path.C_Str()));
        }
        else if (tex.type == "normalMap")
        {
            mat.setNormalTextureId(tex.id);
            mat.setNormalMap(QString(tex.path.C_Str()));
        }
        else if (tex.type == "metallicMap")
        {
            mat.setMetallicTextureId(tex.id);
            mat.setMetallicMap(QString(tex.path.C_Str()));
        }
        else if (tex.type == "roughnessMap")
        {
            mat.setRoughnessTextureId(tex.id);
            mat.setRoughnessMap(QString(tex.path.C_Str()));
        }
        else if (tex.type == "emissiveMap")
        {
            mat.setEmissiveTextureId(tex.id);
            mat.setEmissiveMap(QString(tex.path.C_Str()));
        }
        else if (tex.type == "heightMap")
        {
            mat.setHeightTextureId(tex.id);
            mat.setHeightMap(QString(tex.path.C_Str()));
        }
        else if (tex.type == "opacityMap")
        {
            mat.setOpacityTextureId(tex.id);
            mat.setOpacityMap(QString(tex.path.C_Str()));
        }
        else if (tex.type == "texture_diffuse")
        {
            mat.setAlbedoTextureId(tex.id);
            mat.setAlbedoMap(QString(tex.path.C_Str()));
        }
        else if (tex.type == "texture_normal")
        {
            mat.setNormalTextureId(tex.id);
            mat.setNormalMap(QString(tex.path.C_Str()));
        }
        else if (tex.type == "texture_specular")
        {
            mat.setMetallicTextureId(tex.id);
            mat.setMetallicMap(QString(tex.path.C_Str()));
        }
        else if (tex.type == "texture_emissive")
        {
            mat.setEmissiveTextureId(tex.id);
            mat.setEmissiveMap(QString(tex.path.C_Str()));
        }
        else if (tex.type == "texture_height")
        {
            mat.setHeightTextureId(tex.id);
            mat.setHeightMap(QString(tex.path.C_Str()));
        }
        else if (tex.type == "texture_opacity")
        {
            mat.setOpacityTextureId(tex.id);
            mat.setOpacityMap(QString(tex.path.C_Str()));
		}
        // sheen
        else if (tex.type == "sheenColorMap")
        {
            mat.setSheenColorTextureId(tex.id);
            mat.setSheenColorMap(QString(tex.path.C_Str()));
		}
        else if (tex.type == "sheenRoughnessMap")
        {
            mat.setSheenRoughnessTextureId(tex.id);
            mat.setSheenRoughnessMap(QString(tex.path.C_Str()));
        }
        // clearcoat
        else if (tex.type == "clearcoatMap")
        {
            mat.setClearcoatColorTextureId(tex.id);
            mat.setClearcoatColorMap(QString(tex.path.C_Str()));
        }
        else if (tex.type == "clearcoatRoughnessMap")
        {
            mat.setClearcoatRoughnessTextureId(tex.id);
            mat.setClearcoatRoughnessMap(QString(tex.path.C_Str()));
        }
        else if (tex.type == "clearcoatNormalMap")
        {
            mat.setClearcoatNormalTextureId(tex.id);
            mat.setClearcoatNormalMap(QString(tex.path.C_Str()));
		}
        // transmission
        else if (tex.type == "transmissionMap")
        {
            mat.setTransmissionTextureId(tex.id);
            mat.setTransmissionMap(QString(tex.path.C_Str()));
		}
        // ioR map
        else if (tex.type == "iorMap")
        {
            mat.setIORTextureId(tex.id);
            mat.setIORMap(QString(tex.path.C_Str()));
		}
	}
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
            //std::cout << lt << std::endl;
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
			alias.hasAlpha = lt.hasAlpha; // reuse alpha info
            textures.push_back(alias);
            _loadedTextures.push_back(alias); // register alias to avoid re-creating later
            //std::cout << lt << std::endl;
            return textures;
        }
    }

    // Not loaded at all: load from file
    Texture texture;
	bool hasAlpha = false;
    std::string filename = string(str.C_Str());
    std::string directory = this->_folderPath;
    filename = directory + '/' + filename;
    texture.id = textureFromFile(str.C_Str(), directory, hasAlpha);
    texture.type = typeName;
    texture.path = aiString(filename);
	texture.hasAlpha = hasAlpha; // Store alpha info for later use
    textures.push_back(texture);
    _loadedTextures.push_back(texture);

    //std::cout << texture << std::endl;

    return textures;
}

bool MaterialProcessor::checkImageForAlpha(const QImage& image)
{
    // Quick check: if no alpha channel, definitely no transparency
    if (!image.hasAlphaChannel())
    {
        return false;
    }

    // Sample some pixels to check for non-opaque alpha values
    // For performance, we don't need to check every single pixel
    int step = std::max(1, image.width() / 32); // Sample every 32nd pixel

    for (int y = 0; y < image.height(); y += step)
    {
        for (int x = 0; x < image.width(); x += step)
        {
            QRgb pixel = image.pixel(x, y);
            int alpha = qAlpha(pixel);
            if (alpha < 254)
            { // 254 to account for compression artifacts
                return true;
            }
        }
    }
    return false;
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


