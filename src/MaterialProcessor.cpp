#include "MaterialProcessor.h"
#include "Utils.h"
#include <string>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QDebug>

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

	if (finalOpacity == 0)
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

unsigned int MaterialProcessor::createTextureOnGPU(GLMaterial::Texture& texture)
{
	//Generate texture ID and load texture data
	unsigned int textureID;
	glGenTextures(1, &textureID);

	QImage texImage;
	bool imageHasAlpha = false;
	QString mixedPath = QString(texture.path.c_str()); // Handle possible percent-encoding in file paths
	QString decodedPath = QUrl::fromPercentEncoding(mixedPath.toUtf8());
	if (!texImage.load(decodedPath))
	{ // Load first image from file
		qWarning("MaterialProcessor::createTextureOnGPU - Could not read image file, using single-color instead.");
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

	texture.id = textureID;
	texture.hasAlpha = imageHasAlpha;

	// Assign texture to ID
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texImage.width(), texImage.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, texImage.bits());
	glGenerateMipmap(GL_TEXTURE_2D);

	// Parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texture.wrapS);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texture.wrapT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture.minFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture.magFilter);
	glBindTexture(GL_TEXTURE_2D, 0);

	return textureID;
}

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
	const QString& nodeName,
	const aiMesh* currentMesh,
	int materialIndex,
	GLMaterial& outMaterial,
	std::vector<GLMaterial::Texture>& outTextures)
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
	QJsonArray jsonSamplers = root.value("samplers").toArray();
	QJsonArray jsonNodes = root.value("nodes").toArray();
	QJsonArray jsonMeshes = root.value("meshes").toArray();

	// Utility: resolve a relative image URI against the gltf file path (returns data: URIs as-is)
	auto resolveUri = [&](const QString& uri) -> QString {
		if (uri.startsWith("data:")) return uri;
		QFileInfo fi(gltfPath);
		return QDir(fi.absolutePath()).absoluteFilePath(uri);
		};

	// Utility: texture index -> image URI (external .gltf only)
	// Handles both standard textures and EXT_texture_webp wrapped textures
	auto resolveTextureUri = [&](int texIndex) -> QString {
		if (texIndex < 0 || texIndex >= jsonTextures.size()) return QString();
		QJsonObject texObj = jsonTextures.at(texIndex).toObject();

		int imgIndex = -1;

		// Try EXT_texture_webp extension first
		if (texObj.contains("extensions"))
		{
			QJsonObject extObj = texObj.value("extensions").toObject();
			if (extObj.contains("EXT_texture_webp"))
			{
				QJsonObject webpObj = extObj.value("EXT_texture_webp").toObject();
				imgIndex = webpObj.value("source").toInt(-1);
			}
		}

		// Fallback to standard "source" property
		if (imgIndex < 0 && texObj.contains("source"))
		{
			imgIndex = texObj.value("source").toInt(-1);
		}

		// Resolve image
		if (imgIndex < 0 || imgIndex >= jsonImages.size()) return QString();
		QJsonObject imgObj = jsonImages.at(imgIndex).toObject();
		if (imgObj.contains("uri") && imgObj.value("uri").isString())
		{
			return resolveUri(imgObj.value("uri").toString());
		}
		return QString();
		};

	// Helper: get sampler params (wrapS, wrapT, magFilter, minFilter) for a texture index.
	// Returns tuple<GLenum wrapS, GLenum wrapT, GLenum magFilter, GLenum minFilter>
	auto getSamplerParams = [&](int texIndex) -> std::tuple<GLenum, GLenum, GLenum, GLenum> {
		// glTF defaults:
		const GLenum DEFAULT_WRAP_S = static_cast<GLenum>(10497); // GL_REPEAT
		const GLenum DEFAULT_WRAP_T = static_cast<GLenum>(10497); // GL_REPEAT
		const GLenum DEFAULT_MAG = static_cast<GLenum>(9729);     // GL_LINEAR
		const GLenum DEFAULT_MIN = static_cast<GLenum>(9987);     // GL_LINEAR_MIPMAP_LINEAR

		GLenum wrapS = DEFAULT_WRAP_S;
		GLenum wrapT = DEFAULT_WRAP_T;
		GLenum magFilter = DEFAULT_MAG;
		GLenum minFilter = DEFAULT_MIN;

		if (texIndex < 0 || texIndex >= jsonTextures.size()) return { wrapS, wrapT, magFilter, minFilter };
		QJsonObject texObj = jsonTextures.at(texIndex).toObject();
		if (!texObj.contains("sampler")) return { wrapS, wrapT, magFilter, minFilter };

		int samplerIndex = texObj.value("sampler").toInt(-1);
		if (samplerIndex < 0 || samplerIndex >= jsonSamplers.size()) return { wrapS, wrapT, magFilter, minFilter };

		QJsonObject samplerObj = jsonSamplers.at(samplerIndex).toObject();

		if (samplerObj.contains("wrapS") && samplerObj.value("wrapS").isDouble())
		{
			wrapS = static_cast<GLenum>(samplerObj.value("wrapS").toInt());
		}
		if (samplerObj.contains("wrapT") && samplerObj.value("wrapT").isDouble())
		{
			wrapT = static_cast<GLenum>(samplerObj.value("wrapT").toInt());
		}
		if (samplerObj.contains("magFilter") && samplerObj.value("magFilter").isDouble())
		{
			magFilter = static_cast<GLenum>(samplerObj.value("magFilter").toInt());
		}
		if (samplerObj.contains("minFilter") && samplerObj.value("minFilter").isDouble())
		{
			minFilter = static_cast<GLenum>(samplerObj.value("minFilter").toInt());
		}
		return { wrapS, wrapT, magFilter, minFilter };
		};


	static const std::map<std::string, std::string> textureTypeMapping = {
		// Base PBR textures
		{"baseColor", "albedoMap"},
		{"albedoMap", "albedoMap"},
		{"normal", "normalMap"},
		{"normalMap", "normalMap"},
		{"metallicRoughness", "metallicRoughnessMap"},
		{"metallicRoughnessMap", "metallicRoughnessMap"},
		{"metallic", "metallicMap"},
		{"metallicMap", "metallicMap"},
		{"roughness", "roughnessMap"},
		{"roughnessMap", "roughnessMap"},
		{"occlusion", "occlusionMap"},
		{"occlusionMap", "occlusionMap"},
		{"ambientOcclusion", "occlusionMap"},
		{"ao", "occlusionMap"},
		{"emissive", "emissiveMap"},
		{"emissiveMap", "emissiveMap"},
		{"opacity", "opacityMap"},
		{"opacityMap", "opacityMap"},
		{"height", "heightMap"},
		{"heightMap", "heightMap"},

		// Extensions
		{"transmission", "transmissionMap"},
		{"transmissionMap", "transmissionMap"},
		{"thickness", "thicknessMap"},
		{"thicknessMap", "thicknessMap"},
		{"ior", "iorMap"},
		{"iorMap", "iorMap"},

		// Sheen
		{"sheenColor", "sheenColorMap"},
		{"sheenColorMap", "sheenColorMap"},
		{"sheenRoughness", "sheenRoughnessMap"},
		{"sheenRoughnessMap", "sheenRoughnessMap"},

		// Clearcoat
		{"clearcoatColor", "clearcoatColorMap"},
		{"clearcoatColorMap", "clearcoatColorMap"},
		{"clearcoatRoughness", "clearcoatRoughnessMap"},
		{"clearcoatRoughnessMap", "clearcoatRoughnessMap"},
		{"clearcoatNormal", "clearcoatNormalMap"},
		{"clearcoatNormalMap", "clearcoatNormalMap"},

		// Anisotropy & Iridescence
		{"anisotropy", "anisotropyMap"},
		{"anisotropyMap", "anisotropyMap"},
		{"iridescence", "iridescenceMap"},
		{"iridescenceMap", "iridescenceMap"},
		{"iridescenceThickness", "iridescenceThicknessMap"},
		{"iridescenceThicknessMap", "iridescenceThicknessMap"},

		// Specular
		{"specularFactor", "specularFactorMap"},
		{"specularFactorMap", "specularFactorMap"},
		{"specularColor", "specularColorMap"},
		{"specularColorMap", "specularColorMap"},

		// Diffuse Transmission
		{"diffuseTransmission", "diffuseTransmissionMap"},
		{"diffuseTransmissionMap", "diffuseTransmissionMap"},
		{"diffuseTransmissionColor", "diffuseTransmissionColorMap"},
		{"diffuseTransmissionColorMap", "diffuseTransmissionColorMap"},
	};

	auto loadAndAddTexture = [&](const QString& texturePath,
		const std::string& mapType,
		int texCoord,
		const glm::vec2& scale,
		const glm::vec2& offset,
		float rotation,
		GLenum wrapS,
		GLenum wrapT,
		GLenum magFilter,
		GLenum minFilter,
		std::vector<GLMaterial::Texture>& textures) -> bool {
			if (texturePath.isEmpty()) return false;

			// Look up the texture type
			auto it = textureTypeMapping.find(mapType);
			if (it == textureTypeMapping.end())
			{
				qWarning() << "Unknown texture type:" << QString::fromStdString(mapType);
				return false;
			}

			std::string textureType = it->second;  // Get the mapped type name
			std::string textureFilePathStd = texturePath.toStdString();

			// Lambda to compare UV transform + sampler metadata (same as in loadMaterialTextures)
			auto uvAndSamplerMatches = [&](const GLMaterial::Texture& a, const GLMaterial::Texture& b) {
				return a.texCoordIndex == b.texCoordIndex &&
					std::abs(a.scale.x - b.scale.x) < 0.0001f &&
					std::abs(a.scale.y - b.scale.y) < 0.0001f &&
					std::abs(a.offset.x - b.offset.x) < 0.0001f &&
					std::abs(a.offset.y - b.offset.y) < 0.0001f &&
					std::abs(a.rotation - b.rotation) < 0.0001f &&
					a.wrapS == b.wrapS &&
					a.wrapT == b.wrapT &&
					a.magFilter == b.magFilter &&
					a.minFilter == b.minFilter;
				};

			// Create a temporary texture struct with the new metadata for comparison
			GLMaterial::Texture newTexture;
			newTexture.type = textureType;
			newTexture.path = textureFilePathStd;
			newTexture.texCoordIndex = texCoord;
			newTexture.scale = scale;
			newTexture.offset = offset;
			newTexture.rotation = rotation;

			// NEW: set sampler metadata on the texture struct
			newTexture.wrapS = wrapS;
			newTexture.wrapT = wrapT;
			newTexture.magFilter = magFilter;
			newTexture.minFilter = minFilter;

			// CHECK 1: Exact match (path + type + UV metadata + sampler) - perfect reuse
			for (const auto& lt : _loadedTextures)
			{
				if (std::string(lt.path) == textureFilePathStd &&
					lt.type == textureType &&
					uvAndSamplerMatches(lt, newTexture))
				{
					// Perfect match - reuse everything
					textures.push_back(lt);
					return true;
				}
			}

			// CHECK 2: Same path, same sampler (reuse GPU texture id but different type OR different UV metadata)
			for (const auto& lt : _loadedTextures)
			{
				if (std::string(lt.path) == textureFilePathStd)
				{
					// Only reuse GPU ID if sampler matches exactly; otherwise do NOT reuse.
					bool samplerMatches =
						(lt.wrapS == newTexture.wrapS) &&
						(lt.wrapT == newTexture.wrapT) &&
						(lt.magFilter == newTexture.magFilter) &&
						(lt.minFilter == newTexture.minFilter);

					if (samplerMatches)
					{
						// Reuse GPU texture ID but create new metadata entry
						GLMaterial::Texture alias;
						alias.id = lt.id;                    // Reuse GPU texture
						alias.type = textureType;            // New type name
						alias.path = lt.path;                // Same path
						alias.hasAlpha = lt.hasAlpha;        // Same alpha info

						// Use the NEW UV transform metadata
						alias.texCoordIndex = newTexture.texCoordIndex;
						alias.scale = newTexture.scale;
						alias.offset = newTexture.offset;
						alias.rotation = newTexture.rotation;

						// Preserve sampler metadata (they match)
						alias.wrapS = newTexture.wrapS;
						alias.wrapT = newTexture.wrapT;
						alias.magFilter = newTexture.magFilter;
						alias.minFilter = newTexture.minFilter;

						textures.push_back(alias);
						_loadedTextures.push_back(alias);    // Cache this variant
						return true;
					}
					// if sampler doesn't match, keep searching (we will create a new GPU texture below)
				}
			}

			// CHECK 3: Not loaded at all (or sampler mismatch) - load from file (will create a new GPU texture)
			// Make sure to store the sampler metadata on the newTexture before uploading
			unsigned int texID = 0;
			// Ensure newTexture.id is set by createTextureOnGPU (which should use wrap/mag/min when creating GL sampler)
			newTexture.id = 0; // init

			// createTextureOnGPU must examine newTexture.wrapS/wrapT/magFilter/minFilter when creating the GL sampler
			texID = createTextureOnGPU(newTexture); // assume this function sets newTexture.id (or returns id)
			if (texID == 0) return false;

			// If createTextureOnGPU didn't set newTexture.id, set from returned texID
			newTexture.id = texID;

			// Add to both vectors
			textures.push_back(newTexture);
			_loadedTextures.push_back(newTexture);    // Cache for future reuse

			return true;
		};

	// Helper: Load base PBR textures from glTF JSON
	// Replace the existing loadPbrTexturesFromJson lambda with this:
	// Refactored loadPbrTexturesFromJson using a small helper lambda + lookup list
	auto loadPbrTexturesFromJson = [&](const QJsonObject& matObj, GLMaterial& mat) -> void {
		// If no PBR object, nothing to do for baseColor/metallicRoughness entries
		QJsonObject pbr;
		if (matObj.contains("pbrMetallicRoughness") && matObj.value("pbrMetallicRoughness").isObject())
		{
			pbr = matObj.value("pbrMetallicRoughness").toObject();
		}

		// Helper that processes one texture slot given a parent JSON object and a key within it.
		// parentObj: JSON object that may contain the texture descriptor (e.g. pbr or matObj)
		// jsonKey: the key under parentObj that contains the texture object (e.g. "baseColorTexture")
		// mapType: string used for mapping to your internal texture type (e.g. "baseColor", "normal")
		auto processTextureSlot = [&](const QJsonObject& parentObj, const QString& jsonKey, const std::string& mapType) {
			if (!parentObj.contains(jsonKey) || !parentObj.value(jsonKey).isObject()) return;

			QJsonObject texObj = parentObj.value(jsonKey).toObject();
			int texIndex = texObj.value("index").toInt(-1);
			if (texIndex < 0) return;

			// resolve URI (handles EXT_texture_webp via resolveTextureUri)
			QString uri = resolveTextureUri(texIndex);
			if (uri.isEmpty()) return;

			// extract transform and sampler then load
			auto [texCoord, scale, offset, rotation] = extractKHRTextureTransform(texObj);
			auto [wrapS, wrapT, magF, minF] = getSamplerParams(texIndex);

			if (loadAndAddTexture(uri, mapType, texCoord, scale, offset, rotation, wrapS, wrapT, magF, minF, outTextures))
			{	
				qDebug() << "  Loaded" << jsonKey << "->" << QString::fromStdString(mapType) << ":" << uri;
			}
			};

		// pbr-specific slots (parent = pbr)
		if (!pbr.isEmpty())
		{
			processTextureSlot(pbr, "baseColorTexture", "baseColor");
			// Load metallicRoughnessTexture TWICE with different mapTypes
			processTextureSlot(pbr, "metallicRoughnessTexture", "metallic");     // First load for metallic
			processTextureSlot(pbr, "metallicRoughnessTexture", "roughness");    // Second load for roughness
		}

		// material-level slots (parent = matObj)
		processTextureSlot(matObj, "normalTexture", "normal");
		processTextureSlot(matObj, "occlusionTexture", "occlusion");
		processTextureSlot(matObj, "emissiveTexture", "emissive");

		// If you later want to add more pbr-level or material-level slots, just call processTextureSlot with the parent
		// e.g. processTextureSlot(matObj, "heightTexture", "height");
		};


	// Helper: apply KHR extensions present in the material JSON object to the GLMaterial.
	// Returns true if any extension was applied (for logging)
	auto applyExtensionsFromJsonMaterial = [&](const QJsonObject& matObj, GLMaterial& mat) -> bool {
		bool appliedAny = false;
		if (!matObj.contains("extensions")) return false;
		QJsonObject extRoot = matObj.value("extensions").toObject();

		// Small helper to process a texture entry found inside an extension object.
		// extObj: the extension object (e.g. extRoot["KHR_materials_sheen"].toObject())
		// jsonKey: the key inside extObj that holds the texture descriptor (e.g. "sheenColorTexture")
		// mapType: the internal mapType used by loadAndAddTexture (e.g. "sheenColor")
		auto processExtensionTextureSlot = [&](const QJsonObject& extObj, const QString& jsonKey, const std::string& mapType) {
			if (!extObj.contains(jsonKey) || !extObj.value(jsonKey).isObject()) return false;
			QJsonObject texObj = extObj.value(jsonKey).toObject();
			int texIndex = texObj.value("index").toInt(-1);
			if (texIndex < 0) return false;

			QString uri = resolveTextureUri(texIndex);
			if (uri.isEmpty()) return false;

			auto [texCoord, scale, offset, rotation] = extractKHRTextureTransform(texObj);
			auto [wrapS, wrapT, magF, minF] = getSamplerParams(texIndex);
			bool ok = loadAndAddTexture(uri, mapType, texCoord, scale, offset, rotation, wrapS, wrapT, magF, minF, outTextures);
			if (ok)
			{
				qDebug() << "  Loaded extension texture" << jsonKey << "->" << QString::fromStdString(mapType) << ":" << uri;
			}
			return ok;
			};

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
			if (processExtensionTextureSlot(vol, "thicknessTexture", "thickness"))
			{
				appliedAny = true;
			}
		}

		// --- KHR_materials_volume_scatter ---
		if (extRoot.contains("KHR_materials_volume_scatter") && extRoot.value("KHR_materials_volume_scatter").isObject())
		{
			QJsonObject scatter = extRoot.value("KHR_materials_volume_scatter").toObject();

			// multiscatterColor (default: [1.0, 1.0, 1.0])
			if (scatter.contains("multiscatterColor") && scatter.value("multiscatterColor").isArray())
			{
				QJsonArray a = scatter.value("multiscatterColor").toArray();
				if (a.size() >= 3)
				{
					QVector3D multiScatterColor(
						static_cast<float>(a.at(0).toDouble(1.0)),
						static_cast<float>(a.at(1).toDouble(1.0)),
						static_cast<float>(a.at(2).toDouble(1.0))
					);
					mat.setMultiScatterColor(multiScatterColor);
					mat.setHasVolumeScattering(true);
					qDebug() << "KHR_materials_volume_scatter: multiscatterColor=" << multiScatterColor;
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
			if (processExtensionTextureSlot(trans, "transmissionTexture", "transmission"))
			{
				appliedAny = true;
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

		// --- KHR_materials dispersion ---
		if (extRoot.contains("KHR_materials_dispersion") && extRoot.value("KHR_materials_dispersion").isObject())
		{
			QJsonObject disp = extRoot.value("KHR_materials_dispersion").toObject();
			if (disp.contains("dispersion"))
			{
				float v = static_cast<float>(disp.value("dispersion").toDouble(0.0));
				mat.setDispersion(qMax(0.0f, v));
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
			if (processExtensionTextureSlot(sp, "specularTexture", "specularFactor"))
			{
				appliedAny = true;
			}
			if (processExtensionTextureSlot(sp, "specularColorTexture", "specularColor"))
			{
				appliedAny = true;
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
			// clearcoatTexture
			if (processExtensionTextureSlot(cc, "clearcoatTexture", "clearcoatColorMap"))
			{
				appliedAny = true;
			}
			// clearcoatRoughnessTexture is separate from clearcoatTexture
			if (processExtensionTextureSlot(cc, "clearcoatRoughnessTexture", "clearcoatRoughnessMap"))
			{
				appliedAny = true;
			}
			// clearcoatNormal is typically an object with "index" etc. same as other textures
			if (processExtensionTextureSlot(cc, "clearcoatNormalTexture", "clearcoatNormalMap"))
			{
				appliedAny = true;
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
			if (processExtensionTextureSlot(sh, "sheenColorTexture", "sheenColorMap"))
			{
				appliedAny = true;
			}
			if (processExtensionTextureSlot(sh, "sheenRoughnessTexture", "sheenRoughnessMap"))
			{
				appliedAny = true;
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
			if (processExtensionTextureSlot(ir, "iridescenceThicknessTexture", "iridescenceThicknessMap"))
			{
				appliedAny = true;
			}
			if (processExtensionTextureSlot(ir, "iridescenceTexture", "iridescenceMap"))
			{
				appliedAny = true;
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
			if (processExtensionTextureSlot(an, "anisotropyTexture", "anisotropyMap"))
			{
				appliedAny = true;
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

		// --- KHR_materials_dispersion
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

		// --- KHR_materials_diffuse_transmission ---
		if (extRoot.contains("KHR_materials_diffuse_transmission") && extRoot.value("KHR_materials_diffuse_transmission").isObject())
		{
			QJsonObject dt = extRoot.value("KHR_materials_diffuse_transmission").toObject();

			// diffuseTransmissionFactor (default: 0.0, range: [0, 1])
			if (dt.contains("diffuseTransmissionFactor"))
			{
				float v = static_cast<float>(dt.value("diffuseTransmissionFactor").toDouble(0.0));
				mat.setDiffuseTransmissionFactor(qBound(0.0f, v, 1.0f));
				appliedAny = true;
			}

			// diffuseTransmissionColorFactor (default: [1.0, 1.0, 1.0])
			if (dt.contains("diffuseTransmissionColorFactor") && dt.value("diffuseTransmissionColorFactor").isArray())
			{
				QJsonArray a = dt.value("diffuseTransmissionColorFactor").toArray();
				if (a.size() >= 3)
				{
					mat.setDiffuseTransmissionColorFactor(QVector3D(
						static_cast<float>(a.at(0).toDouble(1.0)),
						static_cast<float>(a.at(1).toDouble(1.0)),
						static_cast<float>(a.at(2).toDouble(1.0))
					));
					appliedAny = true;
				}
			}

			// diffuseTransmissionTexture (alpha channel contains the factor)
			if (processExtensionTextureSlot(dt, "diffuseTransmissionTexture", "diffuseTransmissionMap"))
			{
				appliedAny = true;
			}

			// diffuseTransmissionColorTexture (RGB channels in sRGB)
			if (processExtensionTextureSlot(dt, "diffuseTransmissionColorTexture", "diffuseTransmissionColorMap"))
			{
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

	// ===== FIND ACTUAL MESH INDEX IN SCENE =====	
	int actualMeshIndex = materialIndex;
	if (scene && currentMesh)
	{
		for (unsigned int idx = 0; idx < scene->mNumMeshes; ++idx)
		{
			if (scene->mMeshes[idx] == currentMesh)
			{
				actualMeshIndex = static_cast<int>(idx);
				break;
			}
		}
	}

	if (actualMeshIndex < 0)
	{
		qWarning() << "Could not find mesh in scene";
		return;
	}

	int gltfMaterialIndex = -1;

	QString lookupKey = nodeName;
	if (lookupKey.isEmpty())
	{
		for (unsigned int idx = 0; idx < scene->mNumMeshes; ++idx)
		{
			if (scene->mMeshes[idx] == currentMesh)
			{
				lookupKey = QString::number(idx);
				break;
			}
		}
	}

	// ===== STRATEGY A: Find by material NAME (from Assimp) - ONLY if name is unique =====
	aiString aiName;
	if (scene->mMaterials[materialIndex]->Get(AI_MATKEY_NAME, aiName) == AI_SUCCESS)
	{
		QString name = QString::fromUtf8(aiName.C_Str());
		if (!name.isEmpty())
		{
			// Count how many materials have this name
			int nameMatchCount = 0;
			int nameMatchIndex = -1;

			for (int j = 0; j < jsonCount; ++j)
			{
				QString jname = jsonMaterials.at(j).toObject().value("name").toString();
				if (!jname.isEmpty() && jname == name)
				{
					nameMatchCount++;
					nameMatchIndex = j;
				}
			}

			// Only use name-based lookup if the name is UNIQUE
			if (nameMatchCount == 1)
			{
				gltfMaterialIndex = nameMatchIndex;
				qDebug() << "NAME MATCH (unique): Found material by name:" << name;
			}
			else if (nameMatchCount > 1)
			{
				qDebug() << "NAME AMBIGUOUS: Material name '" << name << "' appears" << nameMatchCount << "times - skipping to index-based lookup";
			}
		}
	}

	// ===== Only run remaining strategies if name-based lookup FAILED =====
	if (gltfMaterialIndex < 0)
	{
		// ===== STRATEGY B: Find by NODE name =====
		for (int nodeIdx = 0; nodeIdx < jsonNodes.size(); ++nodeIdx)
		{
			QJsonObject node = jsonNodes.at(nodeIdx).toObject();
			if (node.value("name").toString() == lookupKey && node.contains("mesh"))
			{
				int nodeMeshIndex = node.value("mesh").toInt(-1);
				if (nodeMeshIndex >= 0 && nodeMeshIndex < jsonMeshes.size())
				{
					QJsonArray prims = jsonMeshes.at(nodeMeshIndex).toObject().value("primitives").toArray();
					if (!prims.isEmpty())
					{
						int matIdx = prims.at(0).toObject().value("material").toInt(-1);
						if (matIdx >= 0 && matIdx < jsonCount)
						{
							gltfMaterialIndex = matIdx;
							break;
						}
					}
				}
			}
		}

		// ===== STRATEGY C: Find by MESH name, with index tie-breaker =====
		if (gltfMaterialIndex < 0)
		{
			QList<int> matchingGltfMeshIndices;
			for (int meshIdx = 0; meshIdx < jsonMeshes.size(); ++meshIdx)
			{
				if (jsonMeshes.at(meshIdx).toObject().value("name").toString() == lookupKey)
				{
					matchingGltfMeshIndices.append(meshIdx);
				}
			}

			if (!matchingGltfMeshIndices.isEmpty())
			{
				if (matchingGltfMeshIndices.size() == 1)
				{
					int meshIdx = matchingGltfMeshIndices.at(0);
					QJsonArray prims = jsonMeshes.at(meshIdx).toObject().value("primitives").toArray();
					if (!prims.isEmpty())
					{
						int matIdx = prims.at(0).toObject().value("material").toInt(-1);
						if (matIdx >= 0 && matIdx < jsonCount)
						{
							gltfMaterialIndex = matIdx;
						}
					}
				}
				else if (matchingGltfMeshIndices.size() > 1)
				{
					if (actualMeshIndex < jsonMeshes.size() && matchingGltfMeshIndices.contains(actualMeshIndex))
					{
						QJsonArray prims = jsonMeshes.at(actualMeshIndex).toObject().value("primitives").toArray();
						if (!prims.isEmpty())
						{
							int matIdx = prims.at(0).toObject().value("material").toInt(-1);
							if (matIdx >= 0 && matIdx < jsonCount)
							{
								gltfMaterialIndex = matIdx;
							}
						}
					}
					else if (matchingGltfMeshIndices.size() > actualMeshIndex)
					{
						int meshIdx = matchingGltfMeshIndices.at(actualMeshIndex % matchingGltfMeshIndices.size());
						QJsonArray prims = jsonMeshes.at(meshIdx).toObject().value("primitives").toArray();
						if (!prims.isEmpty())
						{
							int matIdx = prims.at(0).toObject().value("material").toInt(-1);
							if (matIdx >= 0 && matIdx < jsonCount)
							{
								gltfMaterialIndex = matIdx;
							}
						}
					}
				}
			}
		}

		// ===== STRATEGY D: Use direct index match =====
		if (gltfMaterialIndex < 0 && actualMeshIndex < jsonMeshes.size())
		{
			QJsonArray prims = jsonMeshes.at(actualMeshIndex).toObject().value("primitives").toArray();
			if (!prims.isEmpty())
			{
				int matIdx = prims.at(0).toObject().value("material").toInt(-1);
				if (matIdx >= 0 && matIdx < jsonCount)
				{
					gltfMaterialIndex = matIdx;
				}
			}
		}
	}

	// ===== FINAL LOADING (single code path for all strategies) =====
	if (gltfMaterialIndex < 0)
	{
		qWarning() << "Could not find glTF material for mesh:" << lookupKey;
		return;
	}

	QJsonObject matObj = jsonMaterials.at(gltfMaterialIndex).toObject();

	// Extract base PBR scalar factors from pbrMetallicRoughness
	if (matObj.contains("pbrMetallicRoughness") && matObj.value("pbrMetallicRoughness").isObject())
	{
		QJsonObject pbr = matObj.value("pbrMetallicRoughness").toObject();

		if (pbr.contains("metallicFactor"))
		{
			float v = static_cast<float>(pbr.value("metallicFactor").toDouble(0.0));
			outMaterial.setMetalness(qBound(0.0f, v, 1.0f));
			qDebug() << "  Loaded pbrMetallicRoughness.metallicFactor:" << v;
		}

		if (pbr.contains("roughnessFactor"))
		{
			float v = static_cast<float>(pbr.value("roughnessFactor").toDouble(0.5));
			outMaterial.setRoughness(qBound(0.01f, v, 1.0f));
			qDebug() << "  Loaded pbrMetallicRoughness.roughnessFactor:" << v;
		}
	}

	if (matObj.contains("extensions"))
	{
		QJsonObject ext = matObj.value("extensions").toObject();
		if (ext.contains("KHR_materials_transmission"))
		{
			QJsonObject trans = ext.value("KHR_materials_transmission").toObject();
		}
	}

	loadPbrTexturesFromJson(matObj, outMaterial);
	applyExtensionsFromJsonMaterial(matObj, outMaterial);

	// Now create ADS aliases from PBR maps for backward compatibility
	synthesizeADSAliases(outTextures);

	// === Apply texture transforms to material ===
	setTextureTransforms(outTextures, outMaterial);

	// Finally, assign extension textures to material maps
	addExtensionMaps(outMaterial, outTextures);

	// nothing found - silently return
	// qDebug() << "No KHR materials extensions found for materialIndex" << materialIndex;
}

std::tuple<int, glm::vec2, glm::vec2, float> MaterialProcessor::extractKHRTextureTransform(const QJsonObject& texObj)
{
	int texCoord = texObj.value("texCoord").toInt(0);  // Default from texture object
	glm::vec2 scale(1.0f, 1.0f);
	glm::vec2 offset(0.0f, 0.0f);
	float rotation = 0.0f;

	// Check for KHR_texture_transform extension
	if (texObj.contains("extensions"))
	{
		QJsonObject ext = texObj.value("extensions").toObject();
		if (ext.contains("KHR_texture_transform"))
		{
			QJsonObject transform = ext.value("KHR_texture_transform").toObject();

			// Check if texCoord is specified INSIDE the extension (overrides texture level)
			if (transform.contains("texCoord"))
			{
				texCoord = transform.value("texCoord").toInt(texCoord);
			}

			if (transform.contains("scale") && transform.value("scale").isArray())
			{
				QJsonArray s = transform.value("scale").toArray();
				if (s.size() >= 2)
				{
					scale.x = static_cast<float>(s.at(0).toDouble(1.0));
					scale.y = static_cast<float>(s.at(1).toDouble(1.0));
				}
			}

			if (transform.contains("offset") && transform.value("offset").isArray())
			{
				QJsonArray o = transform.value("offset").toArray();
				if (o.size() >= 2)
				{
					offset.x = static_cast<float>(o.at(0).toDouble(0.0));
					offset.y = static_cast<float>(o.at(1).toDouble(0.0));
				}
			}

			if (transform.contains("rotation"))
			{
				rotation = static_cast<float>(transform.value("rotation").toDouble(0.0));
			}
		}
	}

	qDebug() << "KHR Parse Result:"
		<< "texCoord:" << texCoord
		<< "scale:" << scale.x << scale.y
		<< "offset:" << offset.x << offset.y
		<< "rotation:" << rotation;

	return std::make_tuple(texCoord, scale, offset, rotation);
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
	GLMaterial::Texture& texture)
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
	std::cout << "UV Transform for " << texture.type << ":\n"
		<< "  TexCoord: " << texture.texCoordIndex << "\n"
		<< "  Scale: (" << texture.scale.x << ", " << texture.scale.y << ")\n"
		<< "  Offset: (" << texture.offset.x << ", " << texture.offset.y << ")\n"
		<< "  Rotation: " << texture.rotation << " rad\n";
}


std::vector<GPULight> MaterialProcessor::parseKHRLightsPunctual(const QString& gltfPath)
{
	std::vector<GPULight> lights;

	// Early skip for non-.gltf paths
	if (!gltfPath.endsWith(".gltf", Qt::CaseInsensitive))
	{
		return lights;
	}

	// Reuse the existing JSON cache
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
			qWarning() << "parseKHRLightsPunctual: cannot open glTF:" << gltfPath;
			return lights;
		}
		QByteArray bytes = f.readAll();
		f.close();

		QJsonParseError perr;
		doc = QJsonDocument::fromJson(bytes, &perr);
		if (perr.error != QJsonParseError::NoError)
		{
			qWarning() << "parseKHRLightsPunctual: JSON parse error:" << perr.errorString();
			return lights;
		}
		s_gltfJsonCache.insert(gltfPath, doc);
	}

	if (!doc.isObject())
	{
		qWarning() << "parseKHRLightsPunctual: invalid JSON root for" << gltfPath;
		return lights;
	}

	QJsonObject root = doc.object();

	// Check for KHR_lights_punctual extension at root level
	if (!root.contains("extensions"))
	{
		return lights;  // No extensions at all
	}

	QJsonObject extensions = root.value("extensions").toObject();
	if (!extensions.contains("KHR_lights_punctual"))
	{
		return lights;  // No lights extension
	}

	QJsonObject lightsExt = extensions.value("KHR_lights_punctual").toObject();
	if (!lightsExt.contains("lights") || !lightsExt.value("lights").isArray())
	{
		return lights;  // No lights array
	}

	QJsonArray lightsArray = lightsExt.value("lights").toArray();
	QJsonArray nodesArray = root.value("nodes").toArray();

	qDebug() << "========================================";
	qDebug() << "parseKHRLightsPunctual: Found" << lightsArray.size() << "lights in glTF";
	qDebug() << "========================================";

	// Parse each light WITH its node transform
	for (int lightIdx = 0; lightIdx < lightsArray.size(); ++lightIdx)
	{
		QJsonObject lightDef = lightsArray.at(lightIdx).toObject();
		GPULight light = {};

		// === Parse light type (REQUIRED) ===
		if (!lightDef.contains("type"))
		{
			qWarning() << "  Light" << lightIdx << ": missing required 'type' field";
			continue;
		}

		QString typeStr = lightDef.value("type").toString();
		if (typeStr == "directional")
		{
			light.type = static_cast<int>(LightType::Directional);
		}
		else if (typeStr == "point")
		{
			light.type = static_cast<int>(LightType::Point);
		}
		else if (typeStr == "spot")
		{
			light.type = static_cast<int>(LightType::Spot);
		}
		else
		{
			qWarning() << "  Light" << lightIdx << ": unknown type" << typeStr;
			continue;
		}

		// === Parse color (default: white) ===
		light.color = glm::vec3(1.0f);
		if (lightDef.contains("color") && lightDef.value("color").isArray())
		{
			QJsonArray colorArray = lightDef.value("color").toArray();
			if (colorArray.size() >= 3)
			{
				light.color.x = static_cast<float>(colorArray.at(0).toDouble(1.0));
				light.color.y = static_cast<float>(colorArray.at(1).toDouble(1.0));
				light.color.z = static_cast<float>(colorArray.at(2).toDouble(1.0));
			}
		}

		// === Parse intensity (default: 1.0) ===
		light.intensity = 1.0f;
		if (lightDef.contains("intensity"))
		{
			light.intensity = static_cast<float>(lightDef.value("intensity").toDouble(1.0));
		}

		// === Parse range (default: 0 = infinite) ===
		light.range = 0.0f;
		if (lightDef.contains("range"))
		{
			light.range = static_cast<float>(lightDef.value("range").toDouble(0.0));
		}

		// === Parse spot angles (only if type == spot) ===
		if (light.type == static_cast<int>(LightType::Spot))
		{
			if (lightDef.contains("spot") && lightDef.value("spot").isObject())
			{
				QJsonObject spotDef = lightDef.value("spot").toObject();

				float innerAngle = 0.0f;
				if (spotDef.contains("innerConeAngle"))
				{
					innerAngle = static_cast<float>(spotDef.value("innerConeAngle").toDouble(0.0));
				}
				light.innerConeCos = std::cos(innerAngle);

				float outerAngle = glm::pi<float>() / 4.0f;  // π/4 default
				if (spotDef.contains("outerConeAngle"))
				{
					outerAngle = static_cast<float>(spotDef.value("outerConeAngle").toDouble(glm::pi<float>() / 4.0f));
				}
				light.outerConeCos = std::cos(outerAngle);
			}
			else
			{
				// Spot without explicit angles - use defaults
				light.innerConeCos = std::cos(0.0f);
				light.outerConeCos = std::cos(glm::pi<float>() / 4.0f);
			}
		}
		else
		{
			// Non-spot lights don't use cone angles
			light.innerConeCos = std::cos(0.0f);
			light.outerConeCos = std::cos(glm::pi<float>() / 4.0f);
		}

		// === Find which node has this light and read its MATRIX transform ===
		light.position = glm::vec3(0.0f);
		light.direction = glm::vec3(0.0f, 0.0f, -1.0f);  // Default direction

		for (int nodeIdx = 0; nodeIdx < nodesArray.size(); ++nodeIdx)
		{
			QJsonObject nodeDef = nodesArray.at(nodeIdx).toObject();

			// Check if this node references this light
			bool hasLight = false;
			if (nodeDef.contains("extensions"))
			{
				QJsonObject nodeExt = nodeDef.value("extensions").toObject();
				if (nodeExt.contains("KHR_lights_punctual"))
				{
					QJsonObject lightExt = nodeExt.value("KHR_lights_punctual").toObject();
					if (lightExt.contains("light") && lightExt.value("light").toInt(-1) == lightIdx)
					{
						hasLight = true;
					}
				}
			}

			if (hasLight)
			{
				// === Read node matrix (4x4, column-major) ===
				glm::mat4 nodeTransform(1.0f);

				if (nodeDef.contains("matrix") && nodeDef.value("matrix").isArray())
				{
					QJsonArray matrixArray = nodeDef.value("matrix").toArray();

					if (matrixArray.size() == 16)
					{
						// Column-major order: fill column by column
						for (int row = 0; row < 4; ++row)
						{
							for (int col = 0; col < 4; ++col)
							{
								int idx = col * 4 + row;  // Column-major index
								nodeTransform[col][row] = static_cast<float>(matrixArray.at(idx).toDouble(0.0));
							}
						}
					}
				}
				else if (nodeDef.contains("translation") || nodeDef.contains("rotation") || nodeDef.contains("scale"))
				{
					// Fallback: build from TRS if matrix not present
					glm::vec3 translation(0.0f);
					if (nodeDef.contains("translation") && nodeDef.value("translation").isArray())
					{
						QJsonArray transArray = nodeDef.value("translation").toArray();
						if (transArray.size() >= 3)
						{
							translation.x = static_cast<float>(transArray.at(0).toDouble(0.0));
							translation.y = static_cast<float>(transArray.at(1).toDouble(0.0));
							translation.z = static_cast<float>(transArray.at(2).toDouble(0.0));
						}
					}

					glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);  // identity
					if (nodeDef.contains("rotation") && nodeDef.value("rotation").isArray())
					{
						QJsonArray rotArray = nodeDef.value("rotation").toArray();
						if (rotArray.size() >= 4)
						{
							rotation.x = static_cast<float>(rotArray.at(0).toDouble(0.0));
							rotation.y = static_cast<float>(rotArray.at(1).toDouble(0.0));
							rotation.z = static_cast<float>(rotArray.at(2).toDouble(0.0));
							rotation.w = static_cast<float>(rotArray.at(3).toDouble(1.0));
						}
					}

					glm::vec3 scale(1.0f);
					if (nodeDef.contains("scale") && nodeDef.value("scale").isArray())
					{
						QJsonArray scaleArray = nodeDef.value("scale").toArray();
						if (scaleArray.size() >= 3)
						{
							scale.x = static_cast<float>(scaleArray.at(0).toDouble(1.0));
							scale.y = static_cast<float>(scaleArray.at(1).toDouble(1.0));
							scale.z = static_cast<float>(scaleArray.at(2).toDouble(1.0));
						}
					}

					nodeTransform = glm::mat4(1.0f);
					nodeTransform = glm::translate(nodeTransform, translation);
					nodeTransform *= glm::mat4_cast(rotation);
					nodeTransform = glm::scale(nodeTransform, scale);
				}

				// === Extract position from matrix ===
				light.position = glm::vec3(nodeTransform[3][0], nodeTransform[3][1], nodeTransform[3][2]);

				// === Extract direction by rotating (0, 0, -1) through the matrix ===
				glm::vec3 localDir(0.0f, 0.0f, -1.0f);
				glm::vec4 worldDir4 = nodeTransform * glm::vec4(localDir, 0.0f);
				light.direction = glm::normalize(glm::vec3(worldDir4));

				break;
			}
		}

		// === Enhanced debug output with all light parameters ===
		QString lightTypeStr;
		if (light.type == static_cast<int>(LightType::Directional))
		{
			lightTypeStr = "Directional";
		}
		else if (light.type == static_cast<int>(LightType::Point))
		{
			lightTypeStr = "Point";
		}
		else if (light.type == static_cast<int>(LightType::Spot))
		{
			lightTypeStr = "Spot";
		}
		else
		{
			lightTypeStr = "Unknown";
		}

		qDebug() << "";
		qDebug() << "Light" << lightIdx << ":";
		qDebug() << "  Type:        " << lightTypeStr;
		qDebug() << "  Color:       (" << light.color.x << "," << light.color.y << "," << light.color.z << ")";
		qDebug() << "  Intensity:   " << light.intensity;
		qDebug() << "  Range:       " << (light.range == 0.0f ? QString("infinite") : QString::number(light.range));
		qDebug() << "  Position:    (" << light.position.x << "," << light.position.y << "," << light.position.z << ")";
		qDebug() << "  Direction:   (" << light.direction.x << "," << light.direction.y << "," << light.direction.z << ")";

		if (light.type == static_cast<int>(LightType::Spot))
		{
			float innerAngleDeg = glm::degrees(std::acos(glm::clamp(light.innerConeCos, -1.0f, 1.0f)));
			float outerAngleDeg = glm::degrees(std::acos(glm::clamp(light.outerConeCos, -1.0f, 1.0f)));
			qDebug() << "  Inner Cone:  " << innerAngleDeg << "degrees";
			qDebug() << "  Outer Cone:  " << outerAngleDeg << "degrees";
		}

		lights.push_back(light);
	}

	qDebug() << "========================================";
	qDebug() << "Total lights parsed:" << lights.size();
	qDebug() << "========================================";

	return lights;
}

// Sets the texture maps for a material based on the defined texture mappings.
void MaterialProcessor::setTextureMaps(aiMaterial* material, std::vector<GLMaterial::Texture>& textures, GLMaterial& mat)
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

	// === Apply texture transforms to material ===
	setTextureTransforms(textures, mat);

	// Finally, assign extension textures to material maps
	addExtensionMaps(mat, textures);
}

// This method takes a vector of loaded textures and applies their properties to the material
void MaterialProcessor::setTextureTransforms(const std::vector<GLMaterial::Texture>& textures, GLMaterial& mat)
{
	// Helper to convert glm::vec2 to QVector2D
	auto toQVector2D = [](const glm::vec2& v) { return QVector2D(v.x, v.y); };

	// Update material maps based on loaded textures
	// For each texture type, assign to material if found
	for (const auto& tex : textures)
	{
		if (tex.type == "albedoMap")
		{
			mat.setAlbedoTextureId(tex.id);
			mat.setAlbedoMap(QString(tex.path.c_str()));
			mat.setAlbedoTexCoord(tex.texCoordIndex);
			mat.setAlbedoTexScale(toQVector2D(tex.scale));
			mat.setAlbedoTexOffset(toQVector2D(tex.offset));
			mat.setAlbedoTexRotation(tex.rotation);			
		}
		else if (tex.type == "normalMap")
		{
			mat.setNormalTextureId(tex.id);
			mat.setNormalMap(QString(tex.path.c_str()));
			mat.setNormalTexCoord(tex.texCoordIndex);
			mat.setNormalTexScale(toQVector2D(tex.scale));
			mat.setNormalTexOffset(toQVector2D(tex.offset));
			mat.setNormalTexRotation(tex.rotation);
		}
		else if (tex.type == "metallicRoughnessMap")
		{
			GLMaterial::ChannelPacking metalPacking;
			// Metallic-Roughness is a COMBINED texture (one image)
			// Metallic is in B channel, Roughness is in G channel
			mat.setMetallicTextureId(tex.id);
			mat.setMetallicMap(QString(tex.path.c_str()));
			mat.setMetallicTexCoord(tex.texCoordIndex);
			mat.setMetallicTexScale(toQVector2D(tex.scale));
			mat.setMetallicTexOffset(toQVector2D(tex.offset));
			mat.setMetallicTexRotation(tex.rotation);
			metalPacking.channel = 2;
			mat.setPackingFor("metallic", metalPacking);

			// ALSO set Roughness to the same texture (it's the same image!)
			GLMaterial::ChannelPacking roughPacking;
			mat.setRoughnessTextureId(tex.id);
			mat.setRoughnessMap(QString(tex.path.c_str()));
			mat.setRoughnessTexCoord(tex.texCoordIndex);
			mat.setRoughnessTexScale(toQVector2D(tex.scale));
			mat.setRoughnessTexOffset(toQVector2D(tex.offset));
			mat.setRoughnessTexRotation(tex.rotation);
			roughPacking.channel = 1;
			mat.setPackingFor("roughness", roughPacking);
		}
		else if (tex.type == "metallicMap")
		{
			mat.setMetallicTextureId(tex.id);
			mat.setMetallicMap(QString(tex.path.c_str()));
			mat.setMetallicTexCoord(tex.texCoordIndex);
			mat.setMetallicTexScale(toQVector2D(tex.scale));
			mat.setMetallicTexOffset(toQVector2D(tex.offset));
			mat.setMetallicTexRotation(tex.rotation);
		}
		else if (tex.type == "roughnessMap")
		{
			mat.setRoughnessTextureId(tex.id);
			mat.setRoughnessMap(QString(tex.path.c_str()));
			mat.setRoughnessTexCoord(tex.texCoordIndex);
			mat.setRoughnessTexScale(toQVector2D(tex.scale));
			mat.setRoughnessTexOffset(toQVector2D(tex.offset));
			mat.setRoughnessTexRotation(tex.rotation);
		}		
		else if (tex.type == "emissiveMap")
		{
			mat.setEmissiveTextureId(tex.id);
			mat.setEmissiveMap(QString(tex.path.c_str()));
			mat.setEmissiveTexCoord(tex.texCoordIndex);
			mat.setEmissiveTexScale(toQVector2D(tex.scale));
			mat.setEmissiveTexOffset(toQVector2D(tex.offset));
			mat.setEmissiveTexRotation(tex.rotation);
		}
		else if (tex.type == "heightMap")
		{
			mat.setHeightTextureId(tex.id);
			mat.setHeightMap(QString(tex.path.c_str()));
			mat.setHeightTexCoord(tex.texCoordIndex);
			mat.setHeightTexScale(toQVector2D(tex.scale));
			mat.setHeightTexOffset(toQVector2D(tex.offset));
			mat.setHeightTexRotation(tex.rotation);
		}
		else if (tex.type == "opacityMap")
		{
			mat.setOpacityTextureId(tex.id);
			mat.setOpacityMap(QString(tex.path.c_str()));
			mat.setOpacityTexCoord(tex.texCoordIndex);
			mat.setOpacityTexScale(toQVector2D(tex.scale));
			mat.setOpacityTexOffset(toQVector2D(tex.offset));
			mat.setOpacityTexRotation(tex.rotation);
		}
		else if (tex.type == "aoMap" || tex.type == "occlusionMap")
		{
			mat.setOcclusionTextureId(tex.id);
			mat.setAOMap(QString(tex.path.c_str()));
			mat.setOcclusionTexCoord(tex.texCoordIndex);
			mat.setOcclusionTexScale(toQVector2D(tex.scale));
			mat.setOcclusionTexOffset(toQVector2D(tex.offset));
			mat.setOcclusionTexRotation(tex.rotation);
			GLMaterial::ChannelPacking occlusionPacking;
			occlusionPacking.channel = 0;  // Red channel for occlusion
			mat.setPackingFor("ao", occlusionPacking);
		}
		else if (tex.type == "texture_diffuse")
		{
			mat.setAlbedoTextureId(tex.id);
			mat.setAlbedoMap(QString(tex.path.c_str()));
			mat.setAlbedoTexCoord(tex.texCoordIndex);
			mat.setAlbedoTexScale(toQVector2D(tex.scale));
			mat.setAlbedoTexOffset(toQVector2D(tex.offset));
			mat.setAlbedoTexRotation(tex.rotation);
		}
		else if (tex.type == "texture_normal")
		{
			mat.setNormalTextureId(tex.id);
			mat.setNormalMap(QString(tex.path.c_str()));
			mat.setNormalTexCoord(tex.texCoordIndex);
			mat.setNormalTexScale(toQVector2D(tex.scale));
			mat.setNormalTexOffset(toQVector2D(tex.offset));
			mat.setNormalTexRotation(tex.rotation);
		}
		else if (tex.type == "texture_specular")
		{
			mat.setMetallicTextureId(tex.id);
			mat.setMetallicMap(QString(tex.path.c_str()));
			mat.setMetallicTexCoord(tex.texCoordIndex);
			mat.setMetallicTexScale(toQVector2D(tex.scale));
			mat.setMetallicTexOffset(toQVector2D(tex.offset));
			mat.setMetallicTexRotation(tex.rotation);
		}
		else if (tex.type == "texture_emissive")
		{
			mat.setEmissiveTextureId(tex.id);
			mat.setEmissiveMap(QString(tex.path.c_str()));
			mat.setEmissiveTexCoord(tex.texCoordIndex);
			mat.setEmissiveTexScale(toQVector2D(tex.scale));
			mat.setEmissiveTexOffset(toQVector2D(tex.offset));
			mat.setEmissiveTexRotation(tex.rotation);
		}
		else if (tex.type == "texture_height")
		{
			mat.setHeightTextureId(tex.id);
			mat.setHeightMap(QString(tex.path.c_str()));
			mat.setHeightTexCoord(tex.texCoordIndex);
			mat.setHeightTexScale(toQVector2D(tex.scale));
			mat.setHeightTexOffset(toQVector2D(tex.offset));
			mat.setHeightTexRotation(tex.rotation);
		}
		else if (tex.type == "texture_opacity")
		{
			mat.setOpacityTextureId(tex.id);
			mat.setOpacityMap(QString(tex.path.c_str()));
			mat.setOpacityTexCoord(tex.texCoordIndex);
			mat.setOpacityTexScale(toQVector2D(tex.scale));
			mat.setOpacityTexOffset(toQVector2D(tex.offset));
			mat.setOpacityTexRotation(tex.rotation);
		}
		// sheen
		else if (tex.type == "sheenColorMap")
		{
			mat.setSheenColorTextureId(tex.id);
			mat.setSheenColorMap(QString(tex.path.c_str()));
			mat.setSheenColorTexCoord(tex.texCoordIndex);
			mat.setSheenColorTexScale(toQVector2D(tex.scale));
			mat.setSheenColorTexOffset(toQVector2D(tex.offset));
			mat.setSheenColorTexRotation(tex.rotation);
		}
		else if (tex.type == "sheenRoughnessMap")
		{
			mat.setSheenRoughnessTextureId(tex.id);
			mat.setSheenRoughnessMap(QString(tex.path.c_str()));
			mat.setSheenRoughnessTexCoord(tex.texCoordIndex);
			mat.setSheenRoughnessTexScale(toQVector2D(tex.scale));
			mat.setSheenRoughnessTexOffset(toQVector2D(tex.offset));
			mat.setSheenRoughnessTexRotation(tex.rotation);
		}
		// clearcoat
		else if (tex.type == "clearcoatColorMap")
		{
			mat.setClearcoatColorTextureId(tex.id);
			mat.setClearcoatColorMap(QString(tex.path.c_str()));
			mat.setClearcoatColorTexCoord(tex.texCoordIndex);
			mat.setClearcoatColorTexScale(toQVector2D(tex.scale));
			mat.setClearcoatColorTexOffset(toQVector2D(tex.offset));
			mat.setClearcoatColorTexRotation(tex.rotation);
		}
		else if (tex.type == "clearcoatRoughnessMap")
		{
			mat.setClearcoatRoughnessTextureId(tex.id);
			mat.setClearcoatRoughnessMap(QString(tex.path.c_str()));
			mat.setClearcoatRoughnessTexCoord(tex.texCoordIndex);
			mat.setClearcoatRoughnessTexScale(toQVector2D(tex.scale));
			mat.setClearcoatRoughnessTexOffset(toQVector2D(tex.offset));
			mat.setClearcoatRoughnessTexRotation(tex.rotation);
		}
		else if (tex.type == "clearcoatNormalMap")
		{
			mat.setClearcoatNormalTextureId(tex.id);
			mat.setClearcoatNormalMap(QString(tex.path.c_str()));
			mat.setClearcoatNormalTexCoord(tex.texCoordIndex);
			mat.setClearcoatNormalTexScale(toQVector2D(tex.scale));
			mat.setClearcoatNormalTexOffset(toQVector2D(tex.offset));
			mat.setClearcoatNormalTexRotation(tex.rotation);
		}
		// transmission
		else if (tex.type == "transmissionMap")
		{
			mat.setTransmissionTextureId(tex.id);
			mat.setTransmissionMap(QString(tex.path.c_str()));
			mat.setTransmissionTexCoord(tex.texCoordIndex);
			mat.setTransmissionTexScale(toQVector2D(tex.scale));
			mat.setTransmissionTexOffset(toQVector2D(tex.offset));
			mat.setTransmissionTexRotation(tex.rotation);
		}
		// KHR_materials_volume
		else if (tex.type == "thicknessMap")
		{
			mat.setThicknessTextureId(tex.id);
			mat.setThicknessMap(QString(tex.path.c_str()));
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
			mat.setIORMap(QString(tex.path.c_str()));
			mat.setIORTexCoord(tex.texCoordIndex);
			mat.setIORTexScale(toQVector2D(tex.scale));
			mat.setIORTexOffset(toQVector2D(tex.offset));
			mat.setIORTexRotation(tex.rotation);
		}
		// KHR_materials_specular
		else if (tex.type == "specularFactorMap")
		{
			mat.setSpecularFactorTextureId(tex.id);
			mat.setSpecularFactorMap(QString(tex.path.c_str()));
			mat.setSpecularFactorTexCoord(tex.texCoordIndex);
			mat.setSpecularFactorTexScale(toQVector2D(tex.scale));
			mat.setSpecularFactorTexOffset(toQVector2D(tex.offset));
			mat.setSpecularFactorTexRotation(tex.rotation);
		}
		else if (tex.type == "specularColorMap")
		{
			mat.setSpecularColorTextureId(tex.id);
			mat.setSpecularColorMap(QString(tex.path.c_str()));
			mat.setSpecularColorTexCoord(tex.texCoordIndex);
			mat.setSpecularColorTexScale(toQVector2D(tex.scale));
			mat.setSpecularColorTexOffset(toQVector2D(tex.offset));
			mat.setSpecularColorTexRotation(tex.rotation);
		}

		// KHR_materials_anisotropy
		else if (tex.type == "anisotropyMap")
		{
			mat.setAnisotropyTextureId(tex.id);
			mat.setAnisotropyMap(QString(tex.path.c_str()));
			mat.setAnisotropyTexCoord(tex.texCoordIndex);
			mat.setAnisotropyTexScale(toQVector2D(tex.scale));
			mat.setAnisotropyTexOffset(toQVector2D(tex.offset));
			mat.setAnisotropyTexRotation(tex.rotation);
		}

		// KHR_materials_iridescence
		else if (tex.type == "iridescenceMap")
		{
			mat.setIridescenceTextureId(tex.id);
			mat.setIridescenceMap(QString(tex.path.c_str()));
			mat.setIridescenceTexCoord(tex.texCoordIndex);
			mat.setIridescenceTexScale(toQVector2D(tex.scale));
			mat.setIridescenceTexOffset(toQVector2D(tex.offset));
			mat.setIridescenceTexRotation(tex.rotation);
		}
		else if (tex.type == "iridescenceThicknessMap")
		{
			mat.setIridescenceThicknessTextureId(tex.id);
			mat.setIridescenceThicknessMap(QString(tex.path.c_str()));
			mat.setIridescenceThicknessTexCoord(tex.texCoordIndex);
			mat.setIridescenceThicknessTexScale(toQVector2D(tex.scale));
			mat.setIridescenceThicknessTexOffset(toQVector2D(tex.offset));
			mat.setIridescenceThicknessTexRotation(tex.rotation);
		}
		else if (tex.type == "diffuseTransmissionMap")
		{
			mat.setDiffuseTransmissionTextureId(tex.id);
			mat.setDiffuseTransmissionMap(QString(tex.path.c_str()));
			mat.setDiffuseTransmissionTexCoord(tex.texCoordIndex);
			mat.setDiffuseTransmissionTexScale(toQVector2D(tex.scale));
			mat.setDiffuseTransmissionTexOffset(toQVector2D(tex.offset));
			mat.setDiffuseTransmissionTexRotation(tex.rotation);
		}
		else if (tex.type == "diffuseTransmissionColorMap")
		{
			mat.setDiffuseTransmissionColorTextureId(tex.id);
			mat.setDiffuseTransmissionColorMap(QString(tex.path.c_str()));
			mat.setDiffuseTransmissionColorTexCoord(tex.texCoordIndex);
			mat.setDiffuseTransmissionColorTexScale(toQVector2D(tex.scale));
			mat.setDiffuseTransmissionColorTexOffset(toQVector2D(tex.offset));
			mat.setDiffuseTransmissionColorTexRotation(tex.rotation);
		}
	}
}

void MaterialProcessor::addExtensionMaps(GLMaterial& mat, std::vector<GLMaterial::Texture>& textures)
{
	auto addTextureIfMissing = [&](std::vector<GLMaterial::Texture>& textures,
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
				std::string existingPath = t.path;
				if (existingPath == pathUtf8 && t.type == type)
				{
					return false;
				}
			}

			// Build candidate Texture with UV metadata (like loadMaterialTextures does)
			GLMaterial::Texture candidate;
			candidate.type = type;
			candidate.path = pathUtf8;
			candidate.texCoordIndex = texCoordIndex;
			candidate.scale = scale;
			candidate.offset = offset;
			candidate.rotation = rotation;

			// UV transform comparator (same as in loadMaterialTextures)
			auto uvTransformMatches = [](const GLMaterial::Texture& a, const GLMaterial::Texture& b) {
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
				if (std::string(lt.path) == pathUtf8 &&
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
				if (std::string(lt.path) == pathUtf8)
				{
					GLMaterial::Texture alias;
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

			// Check 3: not loaded yet -> try to load from disk (uses existing createTextureOnGPU)
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

			createTextureOnGPU(candidate);

			// push and cache
			textures.push_back(candidate);
			_loadedTextures.push_back(candidate);

			if (type == "thicknessMap")
				mat.setHasThicknessAlpha(candidate.hasAlpha);

			return true;
		};

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
	addTextureIfMissing(textures, mat.clearcoatColorMapPath(), "clearcoatColorMap", mat.clearcoatColorTexCoord(),
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

	// diffuse transmission maps
	addTextureIfMissing(textures, mat.diffuseTransmissionMap(), "diffuseTransmissionMap",
		/*texCoord*/mat.diffuseTransmissionTexCoord(),
		/*scale*/glm::vec2(mat.diffuseTransmissionTexScale().x(), mat.diffuseTransmissionTexScale().y()),
		/*offset*/glm::vec2(mat.diffuseTransmissionTexOffset().x(), mat.diffuseTransmissionTexOffset().y()),
		/*rotation*/mat.diffuseTransmissionTexRotation());

	addTextureIfMissing(textures, mat.diffuseTransmissionColorMap(), "diffuseTransmissionColorMap",
		/*texCoord*/mat.diffuseTransmissionColorTexCoord(),
		/*scale*/glm::vec2(mat.diffuseTransmissionColorTexScale().x(), mat.diffuseTransmissionColorTexScale().y()),
		/*offset*/glm::vec2(mat.diffuseTransmissionColorTexOffset().x(), mat.diffuseTransmissionColorTexOffset().y()),
		/*rotation*/mat.diffuseTransmissionColorTexRotation());
}


// Checks all material textures of a given type and loads the textures if they're not loaded yet.
// The required info is returned as a Texture struct.
std::vector<GLMaterial::Texture> MaterialProcessor::loadMaterialTextures(
	aiMaterial* mat,
	aiTextureType type,
	const std::string& typeName,
	unsigned int slotIndex)
{
	std::vector<GLMaterial::Texture> textures;

	if (mat->GetTextureCount(type) <= slotIndex)
		return textures;

	aiString str;
	if (mat->GetTexture(type, slotIndex, &str) != AI_SUCCESS)
		return textures;

	std::string textureFilePath = this->_folderPath + '/' + string(str.C_Str());
	std::replace(textureFilePath.begin(), textureFilePath.end(), '\\', '/');

	// Extract UV transform FIRST (before checking cache)
	GLMaterial::Texture newTexture;
	newTexture.type = typeName;
	newTexture.path = textureFilePath;
	newTexture.rotation = 0.0f;
	newTexture.scale = glm::vec2(1.0f);
	newTexture.offset = glm::vec2(0.0f);
	extractUVTransform(mat, type, slotIndex, newTexture);	
		
	// Lambda to compare UV transform metadata
	auto uvTransformMatches = [](const GLMaterial::Texture& a, const GLMaterial::Texture& b) {
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
		if (string(lt.path.c_str()) == textureFilePath &&
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
		if (string(lt.path.c_str()) == textureFilePath)
		{
			// Found same texture file - reuse GPU ID but apply new metadata
			GLMaterial::Texture alias;
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
	createTextureOnGPU(newTexture);

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

void MaterialProcessor::synthesizeADSAliases(std::vector<GLMaterial::Texture>& textures)
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
				GLMaterial::Texture alias;
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


