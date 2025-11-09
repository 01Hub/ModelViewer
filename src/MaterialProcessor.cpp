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

    /*for (unsigned int i = 0; i < material->mNumProperties; ++i)
    {
        const aiMaterialProperty* prop = material->mProperties[i];
        std::cout << "Property: key=\"" << prop->mKey.C_Str()
            << "\" index=" << prop->mIndex
            << " type=" << (int)prop->mType
            << " dataLen=" << prop->mDataLength
            << " semantic=" << prop->mSemantic
            << std::endl;
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
    // === TRANSMISSION TO OPACITY CONVERSION ===
    // Convert transmission materials to opacity-based transparency
    // This leverages the existing smooth opacity/blend pipeline
    if (AI_SUCCESS == material->Get(AI_MATKEY_TRANSMISSION_FACTOR, value))
    {
        mat.setTransmission(std::clamp(value, 0.0f, 1.0f));
    }

    // Volume (KHR_materials_volume)
    if (AI_SUCCESS == material->Get(AI_MATKEY_VOLUME_THICKNESS_FACTOR, value))
    {
        mat.setThicknessFactor(std::max(value, 0.0f));
    }

    if (AI_SUCCESS == material->Get(AI_MATKEY_VOLUME_ATTENUATION_DISTANCE, value))
    {
        mat.setAttenuationDistance(std::max(value, 0.0f));
    }

    if (AI_SUCCESS == material->Get(AI_MATKEY_VOLUME_ATTENUATION_COLOR, color))
    {
        mat.setAttenuationColor(QVector3D(color.r, color.g, color.b));
    }

    // IOR (KHR_materials_ior) - May override the basic IOR
    if (AI_SUCCESS == material->Get("$mat.gltf.ior", 0, 0, value))
    {
        mat.setIOR(std::clamp(value, 1.0f, 3.0f));
    }

    // Specular (KHR_materials_specular)
    if (AI_SUCCESS == material->Get(AI_MATKEY_SPECULAR_FACTOR, value))
    {
        mat.setSpecularFactor(std::clamp(value, 0.0f, 1.0f));
    }

    if (AI_SUCCESS == material->Get("$mat.gltf.specularColorFactor", 0, 0, color))
    {
        mat.setSpecularColorFactor(QVector3D(color.r, color.g, color.b));
    }

    // Anisotropy (KHR_materials_anisotropy)
    if (AI_SUCCESS == material->Get("$mat.gltf.anisotropyStrength", 0, 0, value))
    {
        mat.setAnisotropyStrength(std::clamp(value, 0.0f, 1.0f));
    }

    if (AI_SUCCESS == material->Get(AI_MATKEY_ANISOTROPY_ROTATION, value))
    {
        mat.setAnisotropyRotation(value);
    }

    // Iridescence (KHR_materials_iridescence)
    if (AI_SUCCESS == material->Get("$mat.gltf.iridescenceFactor", 0, 0, value))
    {
        mat.setIridescenceFactor(std::clamp(value, 0.0f, 1.0f));
    }

    if (AI_SUCCESS == material->Get("$mat.gltf.iridescenceIor", 0, 0, value))
    {
        mat.setIridescenceIor(std::clamp(value, 1.0f, 3.0f));
    }

    if (AI_SUCCESS == material->Get("$mat.gltf.iridescenceThicknessMinimum", 0, 0, value))
    {
        mat.setIridescenceThicknessMin(std::max(value, 0.0f));
    }

    if (AI_SUCCESS == material->Get("$mat.gltf.iridescenceThicknessMaximum", 0, 0, value))
    {
        mat.setIridescenceThicknessMax(std::max(value, 0.0f));
    }

    // Emissive Strength (KHR_materials_emissive_strength)
    if (AI_SUCCESS == material->Get(AI_MATKEY_EMISSIVE_INTENSITY, value))
    {
        mat.setEmissiveStrength(std::max(value, 0.0f));
    }

    // Dispersion (KHR_materials_dispersion)
    if (AI_SUCCESS == material->Get("$mat.gltf.dispersion", 0, 0, value))
    {
        mat.setDispersion(std::max(value, 0.0f));
    }

    // Unlit (KHR_materials_unlit)
    if (AI_SUCCESS == material->Get("$mat.gltf.unlit", 0, 0, intValue))
    {
        mat.setUnlit(intValue != 0);
    }

    // === Rendering Hints ===
    // Two-sided material
    if (AI_SUCCESS == material->Get(AI_MATKEY_TWOSIDED, intValue))
    {
        mat.setTwoSided(intValue != 0);
    }

    // Also check glTF double-sided property
    if (AI_SUCCESS == material->Get("$mat.gltf.doubleSided", 0, 0, intValue))
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

        // Base color factor (alternative glTF property)
        if (AI_SUCCESS == material->Get("$mat.gltf.pbrMetallicRoughness.baseColorFactor", 0, 0, color))
        {
            mat.setAlbedoColor(QVector3D(color.r, color.g, color.b));
        }

        // Metallic and roughness factors (alternative glTF property)
        if (AI_SUCCESS == material->Get("$mat.gltf.pbrMetallicRoughness.metallicFactor", 0, 0, value))
        {
            mat.setMetalness(std::clamp(value, 0.0f, 1.0f));
        }

        if (AI_SUCCESS == material->Get("$mat.gltf.pbrMetallicRoughness.roughnessFactor", 0, 0, value))
        {
            mat.setRoughness(std::clamp(value, 0.01f, 1.0f));
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

unsigned int MaterialProcessor::textureFromFile(const char* path, bool& hasAlpha)
{
    //Generate texture ID and load texture data
    unsigned int textureID;
    glGenTextures(1, &textureID);

    QImage texImage;
    bool imageHasAlpha = false;

    if (!texImage.load(QString(path)))
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


#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QDebug>
/*
 Comprehensive per-material reader for many KHR_materials_* extensions.
 - Only for external .gltf files (no embedded bufferView/.glb handling here)
 - Uses caching of parsed JSON per gltfPath to avoid reparsing on repeated calls
 - Mapping: JSON materials[] index -> aiScene material index; fallback to name-based match
 - Resolves textures -> images[].uri (external path) for thickness/specular/iridescence maps etc.
*/

void MaterialProcessor::applyGltfMaterialExtensionsToMaterial(
    const QString& gltfPath,
    const aiScene* scene,
    unsigned int materialIndex,
    GLMaterial& outMaterial)
{
    if (!scene)
    {
        qWarning() << "applyGltfMaterialExtensionsToMaterial: scene is null";
        return;
    }

    // Early skip for non-.gltf paths
    if (!gltfPath.endsWith(".gltf", Qt::CaseInsensitive))
    {
        return;
    }

    if (materialIndex >= static_cast<unsigned int>(scene->mNumMaterials))
    {
        qWarning() << "applyGltfMaterialExtensionsToMaterial: materialIndex out of range:" << materialIndex;
        return;
    }

    // simple cached JSON per file
    static QHash<QString, QJsonDocument> s_gltfJsonCache;
    QJsonDocument doc;
    if (s_gltfJsonCache.contains(gltfPath))
    {
        doc = s_gltfJsonCache.value(gltfPath);
    }
    else
    {
        QFile f(gltfPath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qWarning() << "applyGltfMaterialExtensionsToMaterial: cannot open glTF:" << gltfPath;
            return;
        }
        QByteArray bytes = f.readAll();
        f.close();

        QJsonParseError perr;
        doc = QJsonDocument::fromJson(bytes, &perr);
        if (perr.error != QJsonParseError::NoError)
        {
            qWarning() << "applyGltfMaterialExtensionsToMaterial: JSON parse error:" << perr.errorString();
            return;
        }
        s_gltfJsonCache.insert(gltfPath, doc);
    }

    if (!doc.isObject())
    {
        qWarning() << "applyGltfMaterialExtensionsToMaterial: invalid JSON root for" << gltfPath;
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray jsonMaterials = root.value("materials").toArray();
    QJsonArray jsonTextures = root.value("textures").toArray();
    QJsonArray jsonImages = root.value("images").toArray();

    // Utility: resolve a relative image URI against the gltf file path (returns data: URIs as-is)
    auto resolveUri = [&](const QString& uri) -> QString {
        if (uri.startsWith("data:")) return uri;
        QFileInfo fi(gltfPath);
        return QDir(fi.absolutePath()).absoluteFilePath(uri);
        };

    // Utility: texture index -> image URI (external .gltf only)
    auto resolveTextureUri = [&](int texIndex) -> QString {
        if (texIndex < 0 || texIndex >= jsonTextures.size()) return QString();
        QJsonObject texObj = jsonTextures.at(texIndex).toObject();
        if (!texObj.contains("source")) return QString();
        int imgIndex = texObj.value("source").toInt(-1);
        if (imgIndex < 0 || imgIndex >= jsonImages.size()) return QString();
        QJsonObject imgObj = jsonImages.at(imgIndex).toObject();
        if (imgObj.contains("uri") && imgObj.value("uri").isString())
        {
            return resolveUri(imgObj.value("uri").toString());
        }
        return QString();
        };

    // Helper: apply KHR extensions present in the material JSON object to the GLMaterial.
    // Returns true if any extension was applied (for logging)
    auto applyExtensionsFromJsonMaterial = [&](const QJsonObject& matObj, GLMaterial& mat) -> bool {
        bool appliedAny = false;
        if (!matObj.contains("extensions")) return false;
        QJsonObject extRoot = matObj.value("extensions").toObject();

        // --- KHR_materials_volume ---
        if (extRoot.contains("KHR_materials_volume") && extRoot.value("KHR_materials_volume").isObject())
        {
            QJsonObject vol = extRoot.value("KHR_materials_volume").toObject();
            if (vol.contains("thicknessFactor"))
            {
                float v = static_cast<float>(vol.value("thicknessFactor").toDouble(0.0));
                mat.setThicknessFactor(qMax(0.0f, v));
                appliedAny = true;
            }
            if (vol.contains("attenuationDistance"))
            {
                float v = static_cast<float>(vol.value("attenuationDistance").toDouble(0.0));
                mat.setAttenuationDistance(qMax(0.0f, v));
                appliedAny = true;
            }
            if (vol.contains("attenuationColor") && vol.value("attenuationColor").isArray())
            {
                QJsonArray a = vol.value("attenuationColor").toArray();
                if (a.size() >= 3)
                {
                    QVector3D c(
                        static_cast<float>(a.at(0).toDouble(1.0)),
                        static_cast<float>(a.at(1).toDouble(1.0)),
                        static_cast<float>(a.at(2).toDouble(1.0))
                    );
                    mat.setAttenuationColor(c);
                    appliedAny = true;
                }
            }
            // thicknessTexture
            if (vol.contains("thicknessTexture") && vol.value("thicknessTexture").isObject())
            {
                QJsonObject tt = vol.value("thicknessTexture").toObject();
                int texIndex = tt.value("index").toInt(-1);
                if (texIndex >= 0)
                {
                    mat.setThicknessTextureId(static_cast<unsigned int>(texIndex));
                    QString uri = resolveTextureUri(texIndex);
                    if (!uri.isEmpty()) mat.setThicknessMap(uri);
                    appliedAny = true;
                }
            }
        }

        // --- KHR_materials_transmission ---
        if (extRoot.contains("KHR_materials_transmission") && extRoot.value("KHR_materials_transmission").isObject())
        {
            QJsonObject trans = extRoot.value("KHR_materials_transmission").toObject();
            if (trans.contains("transmissionFactor"))
            {
                float v = static_cast<float>(trans.value("transmissionFactor").toDouble(0.0));
                mat.setTransmission(qBound(0.0f, v, 1.0f));
                appliedAny = true;
            }
            // transmissionTexture
            if (trans.contains("transmissionTexture") && trans.value("transmissionTexture").isObject())
            {
                QJsonObject tt = trans.value("transmissionTexture").toObject();
                int texIndex = tt.value("index").toInt(-1);
                if (texIndex >= 0)
                {
                    // reuse thickness map fields? keep both separate in GLMaterial if possible.
                    mat.setTransmissionTextureId(static_cast<unsigned int>(texIndex));
                    QString uri = resolveTextureUri(texIndex);
                    if (!uri.isEmpty()) mat.setTransmissionMap(uri);
                    appliedAny = true;
                }
            }
        }

        // --- KHR_materials_ior ---
        if (extRoot.contains("KHR_materials_ior") && extRoot.value("KHR_materials_ior").isObject())
        {
            QJsonObject iorJ = extRoot.value("KHR_materials_ior").toObject();
            if (iorJ.contains("ior"))
            {
                float v = static_cast<float>(iorJ.value("ior").toDouble(1.5));
                mat.setIOR(qBound(1.0f, v, 5.0f));
                appliedAny = true;
            }
        }

        // --- KHR_materials_specular ---
        if (extRoot.contains("KHR_materials_specular") && extRoot.value("KHR_materials_specular").isObject())
        {
            QJsonObject sp = extRoot.value("KHR_materials_specular").toObject();
            if (sp.contains("specularFactor"))
            {
                float v = static_cast<float>(sp.value("specularFactor").toDouble(1.0));
                mat.setSpecularFactor(qBound(0.0f, v, 1.0f));
                appliedAny = true;
            }
            if (sp.contains("specularColorFactor") && sp.value("specularColorFactor").isArray())
            {
                QJsonArray a = sp.value("specularColorFactor").toArray();
                if (a.size() >= 3)
                {
                    mat.setSpecularColorFactor(QVector3D(
                        static_cast<float>(a.at(0).toDouble(1.0)),
                        static_cast<float>(a.at(1).toDouble(1.0)),
                        static_cast<float>(a.at(2).toDouble(1.0))
                    ));
                    appliedAny = true;
                }
            }
            if (sp.contains("specularTexture") && sp.value("specularTexture").isObject())
            {
                int texIndex = sp.value("specularTexture").toObject().value("index").toInt(-1);
                if (texIndex >= 0)
                {
                    mat.setSpecularColorTextureId(static_cast<unsigned int>(texIndex));
                    QString uri = resolveTextureUri(texIndex);
                    if (!uri.isEmpty()) mat.setSpecularColorMap(uri);
                    appliedAny = true;
                }
            }
        }

        // --- KHR_materials_clearcoat ---
        if (extRoot.contains("KHR_materials_clearcoat") && extRoot.value("KHR_materials_clearcoat").isObject())
        {
            QJsonObject cc = extRoot.value("KHR_materials_clearcoat").toObject();
            if (cc.contains("clearcoatFactor"))
            {
                float v = static_cast<float>(cc.value("clearcoatFactor").toDouble(0.0));
                mat.setClearcoat(qBound(0.0f, v, 1.0f));
                appliedAny = true;
            }
            if (cc.contains("clearcoatRoughnessFactor"))
            {
                float v = static_cast<float>(cc.value("clearcoatRoughnessFactor").toDouble(0.0));
                mat.setClearcoatRoughness(qBound(0.0f, v, 1.0f));
                appliedAny = true;
            }
            if (cc.contains("clearcoatTexture") && cc.value("clearcoatTexture").isObject())
            {
                int texIndex = cc.value("clearcoatTexture").toObject().value("index").toInt(-1);
                if (texIndex >= 0)
                {
                    mat.setClearcoatColorTextureId(static_cast<unsigned int>(texIndex));
                    QString uri = resolveTextureUri(texIndex);
                    if (!uri.isEmpty()) mat.setClearcoatColorMap(uri);
                    appliedAny = true;
                }
            }
            if (cc.contains("clearcoatNormalTexture") && cc.value("clearcoatNormalTexture").isObject())
            {
                int texIndex = cc.value("clearcoatNormalTexture").toObject().value("index").toInt(-1);
                if (texIndex >= 0)
                {
                    mat.setClearcoatNormalTextureId(static_cast<unsigned int>(texIndex));
                    QString uri = resolveTextureUri(texIndex);
                    if (!uri.isEmpty()) mat.setClearcoatNormalMap(uri);
                    appliedAny = true;
                }
            }
        }

        // --- KHR_materials_sheen ---
        if (extRoot.contains("KHR_materials_sheen") && extRoot.value("KHR_materials_sheen").isObject())
        {
            QJsonObject sh = extRoot.value("KHR_materials_sheen").toObject();
            if (sh.contains("sheenColorFactor") && sh.value("sheenColorFactor").isArray())
            {
                QJsonArray a = sh.value("sheenColorFactor").toArray();
                if (a.size() >= 3)
                {
                    mat.setSheenColor(QVector3D(
                        static_cast<float>(a.at(0).toDouble(0.0)),
                        static_cast<float>(a.at(1).toDouble(0.0)),
                        static_cast<float>(a.at(2).toDouble(0.0))
                    ));
                    appliedAny = true;
                }
            }
            if (sh.contains("sheenRoughnessFactor"))
            {
                float v = static_cast<float>(sh.value("sheenRoughnessFactor").toDouble(0.0));
                mat.setSheenRoughness(qBound(0.0f, v, 1.0f));
                appliedAny = true;
            }
            if (sh.contains("sheenColorTexture") && sh.value("sheenColorTexture").isObject())
            {
                int texIndex = sh.value("sheenColorTexture").toObject().value("index").toInt(-1);
                if (texIndex >= 0)
                {
                    mat.setSheenColorTextureId(static_cast<unsigned int>(texIndex));
                    QString uri = resolveTextureUri(texIndex);
                    if (!uri.isEmpty()) mat.setSheenColorMap(uri);
                    appliedAny = true;
                }
            }
        }

        // --- KHR_materials_iridescence ---
        if (extRoot.contains("KHR_materials_iridescence") && extRoot.value("KHR_materials_iridescence").isObject())
        {
            QJsonObject ir = extRoot.value("KHR_materials_iridescence").toObject();
            if (ir.contains("iridescenceFactor"))
            {
                float v = static_cast<float>(ir.value("iridescenceFactor").toDouble(0.0));
                mat.setIridescenceFactor(qBound(0.0f, v, 1.0f));
                appliedAny = true;
            }
            if (ir.contains("iridescenceIor"))
            {
                float v = static_cast<float>(ir.value("iridescenceIor").toDouble(1.0));
                mat.setIridescenceIor(qMax(0.0f, v));
                appliedAny = true;
            }
            if (ir.contains("iridescenceThicknessMinimum"))
            {
                float v = static_cast<float>(ir.value("iridescenceThicknessMinimum").toDouble(0.0));
                mat.setIridescenceThicknessMin(qMax(0.0f, v));
                appliedAny = true;
            }
            if (ir.contains("iridescenceThicknessMaximum"))
            {
                float v = static_cast<float>(ir.value("iridescenceThicknessMaximum").toDouble(0.0));
                mat.setIridescenceThicknessMax(qMax(0.0f, v));
                appliedAny = true;
            }
            if (ir.contains("iridescenceThicknessTexture") && ir.value("iridescenceThicknessTexture").isObject())
            {
                int texIndex = ir.value("iridescenceThicknessTexture").toObject().value("index").toInt(-1);
                if (texIndex >= 0)
                {
                    mat.setIridescenceThicknessTextureId(static_cast<unsigned int>(texIndex));
                    QString uri = resolveTextureUri(texIndex);
                    if (!uri.isEmpty()) mat.setIridescenceThicknessMap(uri);
                    appliedAny = true;
                }
            }
        }

        // --- KHR_materials_anisotropy ---
        if (extRoot.contains("KHR_materials_anisotropy") && extRoot.value("KHR_materials_anisotropy").isObject())
        {
            QJsonObject an = extRoot.value("KHR_materials_anisotropy").toObject();
            if (an.contains("anisotropyStrength"))
            {
                float v = static_cast<float>(an.value("anisotropyStrength").toDouble(0.0));
                mat.setAnisotropyStrength(qBound(0.0f, v, 1.0f));
                appliedAny = true;
            }
            if (an.contains("anisotropyRotation"))
            {
                float v = static_cast<float>(an.value("anisotropyRotation").toDouble(0.0));
                mat.setAnisotropyRotation(v);
                appliedAny = true;
            }
            if (an.contains("anisotropyTexture") && an.value("anisotropyTexture").isObject())
            {
                int texIndex = an.value("anisotropyTexture").toObject().value("index").toInt(-1);
                if (texIndex >= 0)
                {
                    mat.setAnisotropyTextureId(static_cast<unsigned int>(texIndex));
                    QString uri = resolveTextureUri(texIndex);
                    if (!uri.isEmpty()) mat.setAnisotropyMap(uri);
                    appliedAny = true;
                }
            }
        }

        // --- KHR_materials_emissive_strength ---
        if (extRoot.contains("KHR_materials_emissive_strength"))
        {
            QJsonValue v = extRoot.value("KHR_materials_emissive_strength");
            if (v.isObject())
            {
                float es = static_cast<float>(v.toObject().value("emissiveStrength").toDouble(1.0));
                mat.setEmissiveStrength(qMax(0.0f, es));
                appliedAny = true;
            }
            else if (v.isDouble())
            {
                mat.setEmissiveStrength(qMax(0.0f, static_cast<float>(v.toDouble())));
                appliedAny = true;
            }
        }

        // --- KHR_materials_unlit ---
        if (extRoot.contains("KHR_materials_unlit"))
        {
            mat.setUnlit(true);
            appliedAny = true;
        }

        // --- KHR_materials_dispersion (experimental/non-standard) ---
        if (extRoot.contains("KHR_materials_dispersion") && extRoot.value("KHR_materials_dispersion").isObject())
        {
            QJsonObject d = extRoot.value("KHR_materials_dispersion").toObject();
            if (d.contains("dispersionFactor"))
            {
                float v = static_cast<float>(d.value("dispersionFactor").toDouble(0.0));
                mat.setDispersion(qMax(0.0f, v));
                appliedAny = true;
            }
        }

        return appliedAny;
        };

    int jsonCount = jsonMaterials.size();
    if (jsonCount == 0)
    {
        return;
    }
       
    // This is robust to Assimp reordering materials. We get the material name
    // from Assimp and match it against the glTF JSON by name.
    aiString aiName;
    if (scene->mMaterials[materialIndex]->Get(AI_MATKEY_NAME, aiName) == AI_SUCCESS)
    {
        QString name = QString::fromUtf8(aiName.C_Str());
        if (!name.isEmpty())
        {
            for (int j = 0; j < jsonCount; ++j)
            {
                QJsonObject matObj = jsonMaterials.at(j).toObject();
                QString jname = matObj.value("name").toString();
                if (!jname.isEmpty() && jname == name)
                {                    
                    if (applyExtensionsFromJsonMaterial(matObj, outMaterial))
                    {
                        qDebug() << "Applied KHR materials extensions to material (name)" << name;                        
                    }                    
                    return;
                }
            }            
        }
    }

    // This handles edge cases where material names might not match.
    // Only try this if we couldn't find by name.

    int idx = static_cast<int>(materialIndex);
    if (idx >= 0 && idx < jsonCount)
    {
        qDebug() << "  Name-based lookup failed, trying index-based fallback:" << idx;
        QJsonObject matObj = jsonMaterials.at(idx).toObject();
        if (applyExtensionsFromJsonMaterial(matObj, outMaterial))
        {
            qDebug() << "Applied KHR materials extensions to material index" << idx;
            return;
        }
    }

    // nothing found — silently return
    // qDebug() << "No KHR materials extensions found for materialIndex" << materialIndex;
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

void MaterialProcessor::extractUVTransform(
    aiMaterial* mat,
    aiTextureType type,
    unsigned int slotIndex,
    Texture& texture)
{
    // 1. Get UV coordinate set index (which TEXCOORD_N to use)
    int uvwsrc = 0;
    if (mat->Get(AI_MATKEY_UVWSRC(type, slotIndex), uvwsrc) == AI_SUCCESS)
    {
        texture.texCoordIndex = uvwsrc;
    }
    else
    {
        texture.texCoordIndex = 0; // Default to TEXCOORD_0
    }

    // 2. Get UV transform (scale, offset, rotation)
    aiUVTransform uvTransform;
    unsigned int max = sizeof(aiUVTransform);

    if (aiGetMaterialFloatArray(mat, AI_MATKEY_UVTRANSFORM(type, slotIndex),
        (float*)&uvTransform, &max) == aiReturn_SUCCESS)
    {
        // Successfully retrieved transform
        texture.scale = glm::vec2(uvTransform.mScaling.x, uvTransform.mScaling.y);
        texture.offset = glm::vec2(uvTransform.mTranslation.x, uvTransform.mTranslation.y);
        texture.rotation = uvTransform.mRotation;        
    }
    else
    {
        // No transform specified - use identity defaults
        texture.scale = glm::vec2(1.0f, 1.0f);
        texture.offset = glm::vec2(0.0f, 0.0f);
        texture.rotation = 0.0f;
    }

    // Debug output (optional)
    /*std::cout << "UV Transform for " << texture.type << ":\n"
        << "  TexCoord: " << texture.texCoordIndex << "\n"
        << "  Scale: (" << texture.scale.x << ", " << texture.scale.y << ")\n"
        << "  Offset: (" << texture.offset.x << ", " << texture.offset.y << ")\n"
        << "  Rotation: " << texture.rotation << " rad\n";*/
}

// Sets the texture maps for a material based on the defined texture mappings.
void MaterialProcessor::setTextureMaps(aiMaterial* material, std::vector<Texture>& textures, GLMaterial& mat)
{

	//debugMaterialTextures(material, material->GetName().C_Str());

    auto addTextureIfMissing = [&](std::vector<Texture>& textures,
        const QString& qpath,
        const std::string& type,
        int texCoordIndex = 0,
        const glm::vec2& scale = glm::vec2(1.0f),
        const glm::vec2& offset = glm::vec2(0.0f),
        float rotation = 0.0f) -> bool {

            if (qpath.isEmpty()) return false;
            std::string pathUtf8 = qpath.toStdString();

            // Avoid duplicates in 'textures' (path + type)
            for (const auto& t : textures)
            {
                std::string existingPath = t.path.C_Str();
                if (existingPath == pathUtf8 && t.type == type)
                {
                    return false;
                }
            }

            // Build candidate Texture with UV metadata (like loadMaterialTextures does)
            Texture candidate;
            candidate.type = type;
            candidate.path = aiString(pathUtf8);
            candidate.texCoordIndex = texCoordIndex;
            candidate.scale = scale;
            candidate.offset = offset;
            candidate.rotation = rotation;

            // UV transform comparator (same as in loadMaterialTextures)
            auto uvTransformMatches = [](const Texture& a, const Texture& b) {
                return a.texCoordIndex == b.texCoordIndex &&
                    std::abs(a.scale.x - b.scale.x) < 0.0001f &&
                    std::abs(a.scale.y - b.scale.y) < 0.0001f &&
                    std::abs(a.offset.x - b.offset.x) < 0.0001f &&
                    std::abs(a.offset.y - b.offset.y) < 0.0001f &&
                    std::abs(a.rotation - b.rotation) < 0.0001f;
                };

            // Check 1: exact match (path + type + UV metadata) => reuse whole Texture
            for (const auto& lt : _loadedTextures)
            {
                if (std::string(lt.path.C_Str()) == pathUtf8 &&
                    lt.type == type &&
                    uvTransformMatches(lt, candidate))
                {
                    textures.push_back(lt); // reuse existing
                    return true;
                }
            }

            // Check 2: same path but different type or different UV metadata
            // reuse GPU texture id but create new entry with candidate metadata
            for (const auto& lt : _loadedTextures)
            {
                if (std::string(lt.path.C_Str()) == pathUtf8)
                {
                    Texture alias;
                    alias.id = lt.id;                 // reuse GPU texture id
                    alias.type = type;                // new type
                    alias.path = lt.path;             // same path
                    alias.hasAlpha = lt.hasAlpha;     // same alpha info

                    // use candidate's UV metadata
                    alias.texCoordIndex = candidate.texCoordIndex;
                    alias.scale = candidate.scale;
                    alias.offset = candidate.offset;
                    alias.rotation = candidate.rotation;

                    textures.push_back(alias);
                    _loadedTextures.push_back(alias); // cache this variant
                    return true;
                }
            }

            // Check 3: not loaded yet -> try to load from disk (uses existing textureFromFile)
            // Resolve file path: qpath may be absolute (from applyGltf) or relative.
            std::string textureFilePath = pathUtf8;
            if (!QFile::exists(QString::fromStdString(textureFilePath)))
            {
                // try relative to _folderPath (your existing convention in loadMaterialTextures)
                if (!_folderPath.empty())
                {
                    std::string tryPath = _folderPath + '/' + textureFilePath;
                    std::replace(tryPath.begin(), tryPath.end(), '\\', '/');
                    if (QFile::exists(QString::fromStdString(tryPath)))
                    {
                        textureFilePath = tryPath;
                    }
                }
            }

            bool hasAlpha = false;
            candidate.id = textureFromFile(textureFilePath.c_str(), hasAlpha);
            candidate.hasAlpha = hasAlpha;

            // push and cache
            textures.push_back(candidate);
            _loadedTextures.push_back(candidate);

            if(type == "thicknessMap")
                mat.setHasThicknessAlpha(hasAlpha);

            /*qDebug() << "Loaded extension texture:" << QString::fromStdString(textureFilePath)
                << "type:" << QString::fromStdString(type)
                << "id:" << candidate.id;*/

            return true;
        };



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
	auto toQVector2D = [](const glm::vec2& v) { return QVector2D(v.x, v.y); };
	for (const auto& tex : textures)
	{
		if (tex.type == "albedoMap")
		{
			mat.setAlbedoTextureId(tex.id);
			mat.setAlbedoMap(QString(tex.path.C_Str()));
			mat.setAlbedoTexCoord(tex.texCoordIndex);
			mat.setAlbedoTexScale(toQVector2D(tex.scale));
			mat.setAlbedoTexOffset(toQVector2D(tex.offset));
			mat.setAlbedoTexRotation(tex.rotation);
		}
		else if (tex.type == "normalMap")
		{
			mat.setNormalTextureId(tex.id);
			mat.setNormalMap(QString(tex.path.C_Str()));
			mat.setNormalTexCoord(tex.texCoordIndex);
			mat.setNormalTexScale(toQVector2D(tex.scale));
			mat.setNormalTexOffset(toQVector2D(tex.offset));
			mat.setNormalTexRotation(tex.rotation);
		}
		else if (tex.type == "metallicMap")
		{
			mat.setMetallicTextureId(tex.id);
			mat.setMetallicMap(QString(tex.path.C_Str()));
			mat.setMetallicTexCoord(tex.texCoordIndex);
			mat.setMetallicTexScale(toQVector2D(tex.scale));
			mat.setMetallicTexOffset(toQVector2D(tex.offset));
			mat.setMetallicTexRotation(tex.rotation);
		}
		else if (tex.type == "roughnessMap")
		{
			mat.setRoughnessTextureId(tex.id);
			mat.setRoughnessMap(QString(tex.path.C_Str()));
			mat.setRoughnessTexCoord(tex.texCoordIndex);
			mat.setRoughnessTexScale(toQVector2D(tex.scale));
			mat.setRoughnessTexOffset(toQVector2D(tex.offset));
			mat.setRoughnessTexRotation(tex.rotation);
		}
		else if (tex.type == "emissiveMap")
		{
			mat.setEmissiveTextureId(tex.id);
			mat.setEmissiveMap(QString(tex.path.C_Str()));
			mat.setEmissiveTexCoord(tex.texCoordIndex);
			mat.setEmissiveTexScale(toQVector2D(tex.scale));
			mat.setEmissiveTexOffset(toQVector2D(tex.offset));
			mat.setEmissiveTexRotation(tex.rotation);
		}
		else if (tex.type == "heightMap")
		{
			mat.setHeightTextureId(tex.id);
			mat.setHeightMap(QString(tex.path.C_Str()));
			mat.setHeightTexCoord(tex.texCoordIndex);
			mat.setHeightTexScale(toQVector2D(tex.scale));
			mat.setHeightTexOffset(toQVector2D(tex.offset));
			mat.setHeightTexRotation(tex.rotation);
		}
		else if (tex.type == "opacityMap")
		{
			mat.setOpacityTextureId(tex.id);
			mat.setOpacityMap(QString(tex.path.C_Str()));
			mat.setOpacityTexCoord(tex.texCoordIndex);
			mat.setOpacityTexScale(toQVector2D(tex.scale));
			mat.setOpacityTexOffset(toQVector2D(tex.offset));
			mat.setOpacityTexRotation(tex.rotation);
		}
		else if (tex.type == "aoMap")
		{
			mat.setOcclusionTextureId(tex.id);
			mat.setAOMap(QString(tex.path.C_Str()));
			mat.setOcclusionTexCoord(tex.texCoordIndex);
			mat.setOcclusionTexScale(toQVector2D(tex.scale));
			mat.setOcclusionTexOffset(toQVector2D(tex.offset));
			mat.setOcclusionTexRotation(tex.rotation);
		}
		else if (tex.type == "texture_diffuse")
		{
			mat.setAlbedoTextureId(tex.id);
			mat.setAlbedoMap(QString(tex.path.C_Str()));
			mat.setAlbedoTexCoord(tex.texCoordIndex);
			mat.setAlbedoTexScale(toQVector2D(tex.scale));
			mat.setAlbedoTexOffset(toQVector2D(tex.offset));
			mat.setAlbedoTexRotation(tex.rotation);
		}
		else if (tex.type == "texture_normal")
		{
			mat.setNormalTextureId(tex.id);
			mat.setNormalMap(QString(tex.path.C_Str()));
			mat.setNormalTexCoord(tex.texCoordIndex);
			mat.setNormalTexScale(toQVector2D(tex.scale));
			mat.setNormalTexOffset(toQVector2D(tex.offset));
			mat.setNormalTexRotation(tex.rotation);
		}
		else if (tex.type == "texture_specular")
		{
			mat.setMetallicTextureId(tex.id);
			mat.setMetallicMap(QString(tex.path.C_Str()));
			mat.setMetallicTexCoord(tex.texCoordIndex);
			mat.setMetallicTexScale(toQVector2D(tex.scale));
			mat.setMetallicTexOffset(toQVector2D(tex.offset));
			mat.setMetallicTexRotation(tex.rotation);
		}
		else if (tex.type == "texture_emissive")
		{
			mat.setEmissiveTextureId(tex.id);
			mat.setEmissiveMap(QString(tex.path.C_Str()));
			mat.setEmissiveTexCoord(tex.texCoordIndex);
			mat.setEmissiveTexScale(toQVector2D(tex.scale));
			mat.setEmissiveTexOffset(toQVector2D(tex.offset));
			mat.setEmissiveTexRotation(tex.rotation);
		}
		else if (tex.type == "texture_height")
		{
			mat.setHeightTextureId(tex.id);
			mat.setHeightMap(QString(tex.path.C_Str()));
			mat.setHeightTexCoord(tex.texCoordIndex);
			mat.setHeightTexScale(toQVector2D(tex.scale));
			mat.setHeightTexOffset(toQVector2D(tex.offset));
			mat.setHeightTexRotation(tex.rotation);
		}
		else if (tex.type == "texture_opacity")
		{
			mat.setOpacityTextureId(tex.id);
			mat.setOpacityMap(QString(tex.path.C_Str()));
			mat.setOpacityTexCoord(tex.texCoordIndex);
			mat.setOpacityTexScale(toQVector2D(tex.scale));
			mat.setOpacityTexOffset(toQVector2D(tex.offset));
			mat.setOpacityTexRotation(tex.rotation);
		}
		// sheen
		else if (tex.type == "sheenColorMap")
		{
			mat.setSheenColorTextureId(tex.id);
			mat.setSheenColorMap(QString(tex.path.C_Str()));
			mat.setSheenColorTexCoord(tex.texCoordIndex);
			mat.setSheenColorTexScale(toQVector2D(tex.scale));
			mat.setSheenColorTexOffset(toQVector2D(tex.offset));
			mat.setSheenColorTexRotation(tex.rotation);
		}
		else if (tex.type == "sheenRoughnessMap")
		{
			mat.setSheenRoughnessTextureId(tex.id);
			mat.setSheenRoughnessMap(QString(tex.path.C_Str()));
			mat.setSheenRoughnessTexCoord(tex.texCoordIndex);
			mat.setSheenRoughnessTexScale(toQVector2D(tex.scale));
			mat.setSheenRoughnessTexOffset(toQVector2D(tex.offset));
			mat.setSheenRoughnessTexRotation(tex.rotation);
		}
		// clearcoat
		else if (tex.type == "clearcoatMap")
		{
			mat.setClearcoatColorTextureId(tex.id);
			mat.setClearcoatColorMap(QString(tex.path.C_Str()));
			mat.setClearcoatColorTexCoord(tex.texCoordIndex);
			mat.setClearcoatColorTexScale(toQVector2D(tex.scale));
			mat.setClearcoatColorTexOffset(toQVector2D(tex.offset));
			mat.setClearcoatColorTexRotation(tex.rotation);
		}
		else if (tex.type == "clearcoatRoughnessMap")
		{
			mat.setClearcoatRoughnessTextureId(tex.id);
			mat.setClearcoatRoughnessMap(QString(tex.path.C_Str()));
			mat.setClearcoatRoughnessTexCoord(tex.texCoordIndex);
			mat.setClearcoatRoughnessTexScale(toQVector2D(tex.scale));
			mat.setClearcoatRoughnessTexOffset(toQVector2D(tex.offset));
			mat.setClearcoatRoughnessTexRotation(tex.rotation);
		}
		else if (tex.type == "clearcoatNormalMap")
		{
			mat.setClearcoatNormalTextureId(tex.id);
			mat.setClearcoatNormalMap(QString(tex.path.C_Str()));
			mat.setClearcoatNormalTexCoord(tex.texCoordIndex);
			mat.setClearcoatNormalTexScale(toQVector2D(tex.scale));
			mat.setClearcoatNormalTexOffset(toQVector2D(tex.offset));
			mat.setClearcoatNormalTexRotation(tex.rotation);
		}
		// transmission
		else if (tex.type == "transmissionMap")
		{
			mat.setTransmissionTextureId(tex.id);
			mat.setTransmissionMap(QString(tex.path.C_Str()));
			mat.setTransmissionTexCoord(tex.texCoordIndex);
			mat.setTransmissionTexScale(toQVector2D(tex.scale));
			mat.setTransmissionTexOffset(toQVector2D(tex.offset));
			mat.setTransmissionTexRotation(tex.rotation);
		}
		// KHR_materials_volume
		else if (tex.type == "thicknessMap")
		{
			mat.setThicknessTextureId(tex.id);
			mat.setThicknessMap(QString(tex.path.C_Str()));
			mat.setThicknessTexCoord(tex.texCoordIndex);
			mat.setThicknessTexScale(toQVector2D(tex.scale));
			mat.setThicknessTexOffset(toQVector2D(tex.offset));
			mat.setThicknessTexRotation(tex.rotation);
			mat.setHasThicknessAlpha(tex.hasAlpha);
		}
		// ioR map
		else if (tex.type == "iorMap")
		{
			mat.setIORTextureId(tex.id);
			mat.setIORMap(QString(tex.path.C_Str()));
			mat.setIorTexCoord(tex.texCoordIndex);
			mat.setIorTexScale(toQVector2D(tex.scale));
			mat.setIorTexOffset(toQVector2D(tex.offset));
			mat.setIorTexRotation(tex.rotation);
		}
		// KHR_materials_specular
		else if (tex.type == "specularFactorMap")
		{
			mat.setSpecularFactorTextureId(tex.id);
			mat.setSpecularFactorMap(QString(tex.path.C_Str()));
			mat.setSpecularFactorTexCoord(tex.texCoordIndex);
			mat.setSpecularFactorTexScale(toQVector2D(tex.scale));
			mat.setSpecularFactorTexOffset(toQVector2D(tex.offset));
			mat.setSpecularFactorTexRotation(tex.rotation);
		}
		else if (tex.type == "specularColorMap")
		{
			mat.setSpecularColorTextureId(tex.id);
			mat.setSpecularColorMap(QString(tex.path.C_Str()));
			mat.setSpecularColorTexCoord(tex.texCoordIndex);
			mat.setSpecularColorTexScale(toQVector2D(tex.scale));
			mat.setSpecularColorTexOffset(toQVector2D(tex.offset));
			mat.setSpecularColorTexRotation(tex.rotation);
		}

		// KHR_materials_anisotropy
		else if (tex.type == "anisotropyMap")
		{
			mat.setAnisotropyTextureId(tex.id);
			mat.setAnisotropyMap(QString(tex.path.C_Str()));
			mat.setAnisotropyTexCoord(tex.texCoordIndex);
			mat.setAnisotropyTexScale(toQVector2D(tex.scale));
			mat.setAnisotropyTexOffset(toQVector2D(tex.offset));
			mat.setAnisotropyTexRotation(tex.rotation);
		}

		// KHR_materials_iridescence
		else if (tex.type == "iridescenceMap")
		{
			mat.setIridescenceTextureId(tex.id);
			mat.setIridescenceMap(QString(tex.path.C_Str()));
			mat.setIridescenceTexCoord(tex.texCoordIndex);
			mat.setIridescenceTexScale(toQVector2D(tex.scale));
			mat.setIridescenceTexOffset(toQVector2D(tex.offset));
			mat.setIridescenceTexRotation(tex.rotation);
		}
		else if (tex.type == "iridescenceThicknessMap")
		{
			mat.setIridescenceThicknessTextureId(tex.id);
			mat.setIridescenceThicknessMap(QString(tex.path.C_Str()));
			mat.setIridescenceThicknessTexCoord(tex.texCoordIndex);
			mat.setIridescenceThicknessTexScale(toQVector2D(tex.scale));
			mat.setIridescenceThicknessTexOffset(toQVector2D(tex.offset));
			mat.setIridescenceThicknessTexRotation(tex.rotation);
		}
	}

    // === Add extension maps discovered by applyGltfMaterialExtensionsToMaterial (stored in GLMaterial) ===
    // We assume applyGltfMaterialExtensionsToMaterial() was called earlier (before setTextureMaps)
    // and populated mat.setXxxMap(...) and mat.setXxxTextureId(...) where appropriate.
    // transmission
    addTextureIfMissing(textures, mat.transmissionMapPath(), "transmissionMap", /*texCoord*/mat.transmissionTexCoord(),
        /*scale*/glm::vec2(mat.transmissionTexScale().x(), mat.transmissionTexScale().y()),
        /*offset*/glm::vec2(mat.transmissionTexOffset().x(), mat.transmissionTexOffset().y()),
        /*rotation*/mat.transmissionTexRotation());

    // volume / thickness
    addTextureIfMissing(textures, mat.thicknessMap(), "thicknessMap", /*texCoord*/mat.thicknessTexCoord(),
        /*scale*/glm::vec2(mat.thicknessTexScale().x(), mat.thicknessTexScale().y()),
        /*offset*/glm::vec2(mat.thicknessTexOffset().x(), mat.thicknessTexOffset().y()),
        /*rotation*/mat.thicknessTexRotation());

    // clearcoat maps
    addTextureIfMissing(textures, mat.clearcoatColorMapPath(), "clearcoatMap", mat.clearcoatColorTexCoord(),
        glm::vec2(mat.clearcoatColorTexScale().x(), mat.clearcoatColorTexScale().y()),
        glm::vec2(mat.clearcoatColorTexOffset().x(), mat.clearcoatColorTexOffset().y()),
        mat.clearcoatColorTexRotation());

    addTextureIfMissing(textures, mat.clearcoatNormalMapPath(), "clearcoatNormalMap", mat.clearcoatNormalTexCoord(),
        glm::vec2(mat.clearcoatNormalTexScale().x(), mat.clearcoatNormalTexScale().y()),
        glm::vec2(mat.clearcoatNormalTexOffset().x(), mat.clearcoatNormalTexOffset().y()),
        mat.clearcoatNormalTexRotation());

    // sheen maps
    addTextureIfMissing(textures, mat.sheenColorMapPath(), "sheenColorMap", mat.sheenColorTexCoord(),
        glm::vec2(mat.sheenColorTexScale().x(), mat.sheenColorTexScale().y()),
        glm::vec2(mat.sheenColorTexOffset().x(), mat.sheenColorTexOffset().y()),
        mat.sheenColorTexRotation());

    addTextureIfMissing(textures, mat.sheenRoughnessMapPath(), "sheenRoughnessMap", mat.sheenRoughnessTexCoord(),
        glm::vec2(mat.sheenRoughnessTexScale().x(), mat.sheenRoughnessTexScale().y()),
        glm::vec2(mat.sheenRoughnessTexOffset().x(), mat.sheenRoughnessTexOffset().y()),
        mat.sheenRoughnessTexRotation());

    // iridescence maps
    addTextureIfMissing(textures, mat.iridescenceMap(), "iridescenceMap", mat.iridescenceTexCoord(),
        glm::vec2(mat.iridescenceTexScale().x(), mat.iridescenceTexScale().y()),
        glm::vec2(mat.iridescenceTexOffset().x(), mat.iridescenceTexOffset().y()),
        mat.iridescenceTexRotation());

    addTextureIfMissing(textures, mat.iridescenceThicknessMap(), "iridescenceThicknessMap", mat.iridescenceThicknessTexCoord(),
        glm::vec2(mat.iridescenceThicknessTexScale().x(), mat.iridescenceThicknessTexScale().y()),
        glm::vec2(mat.iridescenceThicknessTexOffset().x(), mat.iridescenceThicknessTexOffset().y()),
        mat.iridescenceThicknessTexRotation());

    // anisotropy map
    addTextureIfMissing(textures, mat.anisotropyMap(), "anisotropyMap", mat.anisotropyTexCoord(),
        glm::vec2(mat.anisotropyTexScale().x(), mat.anisotropyTexScale().y()),
        glm::vec2(mat.anisotropyTexOffset().x(), mat.anisotropyTexOffset().y()),
        mat.anisotropyTexRotation());

    // ior map
    addTextureIfMissing(textures, mat.iorMapPath(), "iorMap", mat.iorTexCoord(),
        glm::vec2(mat.iorTexScale().x(), mat.iorTexScale().y()),
        glm::vec2(mat.iorTexOffset().x(), mat.iorTexOffset().y()),
        mat.iorTexRotation());

    // specular maps (KHR_materials_specular)
    addTextureIfMissing(textures, mat.specularFactorMap(), "specularFactorMap", mat.specularFactorTexCoord());
    //addTextureIfMissing(textures, mat.specularColorMap(), "specularColorMap", mat.specularColorTexCoord());

    // transmission map (already added above as 'transmissionMap' but we try again to be robust)
    addTextureIfMissing(textures, mat.transmissionMapPath(), "transmissionMap", mat.transmissionTexCoord());
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

    std::string textureFilePath = this->_folderPath + '/' + string(str.C_Str());
    std::replace(textureFilePath.begin(), textureFilePath.end(), '\\', '/');

    // Extract UV transform FIRST (before checking cache)
    Texture newTexture;
    newTexture.type = typeName;
    newTexture.path = aiString(textureFilePath);
    extractUVTransform(mat, type, slotIndex, newTexture);

    // Lambda to compare UV transform metadata
    auto uvTransformMatches = [](const Texture& a, const Texture& b) {
        return a.texCoordIndex == b.texCoordIndex &&
            std::abs(a.scale.x - b.scale.x) < 0.0001f &&
            std::abs(a.scale.y - b.scale.y) < 0.0001f &&
            std::abs(a.offset.x - b.offset.x) < 0.0001f &&
            std::abs(a.offset.y - b.offset.y) < 0.0001f &&
            std::abs(a.rotation - b.rotation) < 0.0001f;
        };

    // Check 1: Exact match (path + type + UV metadata)
    for (const auto& lt : _loadedTextures)
    {
        if (string(lt.path.C_Str()) == textureFilePath &&
            lt.type == typeName &&
            uvTransformMatches(lt, newTexture))
        {
            // Perfect match - reuse everything
            textures.push_back(lt);
            return textures;
        }
    }

    // Check 2: Same path, but different type OR different UV metadata
    // In this case, reuse GPU texture ID but create new entry with different metadata
    for (const auto& lt : _loadedTextures)
    {
        if (string(lt.path.C_Str()) == textureFilePath)
        {
            // Found same texture file - reuse GPU ID but apply new metadata
            Texture alias;
            alias.id = lt.id;                    // Reuse GPU texture
            alias.type = typeName;               // New type name
            alias.path = lt.path;                // Same path
            alias.hasAlpha = lt.hasAlpha;        // Same alpha info

            // Use the NEW UV transform metadata (from newTexture)
            alias.texCoordIndex = newTexture.texCoordIndex;
            alias.scale = newTexture.scale;
            alias.offset = newTexture.offset;
            alias.rotation = newTexture.rotation;

            textures.push_back(alias);
            _loadedTextures.push_back(alias);    // Cache this variant
            return textures;
        }
    }

    // Check 3: Not loaded at all - load from file
    bool hasAlpha = false;
    newTexture.id = textureFromFile(textureFilePath.c_str(), hasAlpha);
    newTexture.hasAlpha = hasAlpha;

    textures.push_back(newTexture);
    _loadedTextures.push_back(newTexture);

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

                // UV transform metadata
                alias.scale = tex.scale;
                alias.offset = tex.offset;
                alias.rotation = tex.rotation;
                alias.texCoordIndex = tex.texCoordIndex;
                alias.hasAlpha = tex.hasAlpha;

                textures.push_back(alias);
                _loadedTextures.push_back(alias); // register to global cache so future loads reuse
                break;
            }
        }
    }
}


