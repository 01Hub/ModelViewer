#ifndef GLMATERIAL_H
#define GLMATERIAL_H

#include <QVector3D>
#include <QString>
#include <QImage>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <QOpenGLFunctions_4_5_Core>


class GLMaterial
{
public:

	friend std::ostream& operator<<(std::ostream& os, const GLMaterial& m);

	// ============================================================================
	// Texture Type Enumeration (20 texture types)
	// ============================================================================
	enum class TextureType
	{
		Albedo,                         // 0  - Base color / diffuse
		Metallic,                       // 1
		Roughness,                      // 2
		Normal,                         // 3
		AmbientOcclusion,               // 4  - AO
		Opacity,                        // 5
		Emissive,                       // 6
		Height,                         // 7
		Transmission,                   // 8
		IOR,                            // 9
		SheenColor,                     // 10
		SheenRoughness,                 // 11
		ClearcoatColor,                 // 12
		ClearcoatRoughness,             // 13
		ClearcoatNormal,                // 14
		Iridescence,                    // 15
		IridescenceThickness,           // 16
		SpecularFactor,                 // 17
		SpecularColor,                  // 18
		Anisotropy,                     // 19
		DiffuseTransmission,            // 20
		DiffuseTransmissionColor,       // 21
		Thickness,                      // 22
		Diffuse,                        // 23 - PBR Specular Glossiness
		SpecularGlossiness,             // 24 - PBR Specular Glossiness

		// Sentinel
		Count                           // 25 total types
	};	

	struct Texture
	{
		unsigned int id = 0;
		std::string type = "";
		std::string path = "";
		bool hasAlpha = false;

		// KHR_texture_transform support
		int texCoordIndex = 0;					// Which TEXCOORD to use (0-3, default 0)
		glm::vec2 scale = glm::vec2(1.0f);      // Tiling/scale
		glm::vec2 offset = glm::vec2(0.0f);     // UV offset
		float rotation = 0.0f;                  // Rotation in radians

		GLenum wrapS = GL_REPEAT;
		GLenum wrapT = GL_REPEAT;
		GLenum magFilter = GL_LINEAR;
		GLenum minFilter = GL_LINEAR_MIPMAP_LINEAR;

		QImage imageData;

		friend std::ostream& operator<<(std::ostream& os, const Texture& texture)
		{
			os << "Id: " << texture.id << " type: " << texture.type
				<< " texCoordIndex: " << texture.texCoordIndex;
			os << " path: " << texture.path;
			os << " hasAlpha: " << texture.hasAlpha;
			os << " scale: (" << texture.scale.x << ", " << texture.scale.y << ")";
			os << " offset: (" << texture.offset.x << ", " << texture.offset.y << ")";
			os << " rotation: " << texture.rotation;
			os << " wrapS: " << texture.wrapS << " wrapT: " << texture.wrapT;
			os << " magFilter: " << texture.magFilter << " minFilter: " << texture.minFilter;
			return os;
		}
	};

	struct TextureTransform
	{
		int texCoord = 0;
		QVector2D texScale = QVector2D(1.0f, 1.0f);
		QVector2D texOffset = QVector2D(0.0f, 0.0f);
		float texRotation = 0.0f;
	};

	enum class PredefinedMaterials
	{
		BRASS, BRONZE, COPPER, GOLD, SILVER, CHROME,
		RUBY, EMERALD, TURQUOISE, PEARL, JADE, OBSIDIAN,
		RED_PLASTIC, GREEN_PLASTIC, CYAN_PLASTIC, YELLOW_PLASTIC, WHITE_PLASTIC, BLACK_PLASTIC,
		RED_RUBBER, GREEN_RUBBER, CYAN_RUBBER, YELLOW_RUBBER, WHITE_RUBBER, BLACK_RUBBER
	};

	enum class ShadingModel
	{
		Unlit,
		BlinnPhong,
		PBR,
		Toon
	};

	enum class BlendMode
	{
		Opaque,
		Masked,
		Alpha,
		Additive,
		Multiply
	};

	struct ChannelPacking
	{
		// Select a single source channel: 0=R, 1=G, 2=B, 3=A, -1 = none
		int channel = 0;		bool invert = false;
		float scale = 1.0f;
		float bias = 0.0f;
	};

	enum class TintMode { Off = 0, AutoGray = 1, ForceGray = 2, LerpMask = 3 };

	struct AlbedoTintParams
	{
		TintMode mode = TintMode::AutoGray;
		float strength = 1.0f;
		float grayEps = 0.02f;
		bool useVertexColor = false;
		int  maskChannel = 3; // A by default
	};
	AlbedoTintParams albedoTint;

public:
	GLMaterial();
	GLMaterial(QVector3D ambient, QVector3D diffuse, QVector3D specular, QVector3D emissive, float shininess, bool metallic = true, float opacity = 1.0f);
	GLMaterial(QVector3D albedo, float metalness, float roughness, float opacity = 1.0f);
	~GLMaterial();

	// ============================================================================
	// Unified Texture API (TextureType-based)
	// ============================================================================

	/// Set complete texture data for a texture type
	/// @param type The texture type (from TextureType enum)
	/// @param texture The Texture instance containing all metadata, samplers, and transforms
	void setTexture(TextureType type, const Texture& texture);

	/// Get const reference to texture data
	/// @param type The texture type
	/// @return Const reference to the Texture instance
	const Texture& texture(TextureType type) const;

	/// Get non-const reference to texture data (for modification)
	/// @param type The texture type
	/// @return Non-const reference to the Texture instance
	Texture& texture(TextureType type);

	/// Helper: Convert TextureType enum to friendly string name
	/// @param type The texture type
	/// @return Friendly name (e.g., "Metallic", "Roughness", etc.)
	static QString textureTypeToString(TextureType type);

	/// Helper: Convert friendly string name to TextureType enum
	/// @param name The friendly name
	/// @return TextureType enum value, or TextureType::Count if not found
	static TextureType stringToTextureType(const QString& name);

	// Legacy Phong/Blinn properties
	QVector3D ambient() const { return _ambient; }
	void setAmbient(const QVector3D& ambient);

	QVector3D diffuse() const { return _diffuse; }
	void setDiffuse(const QVector3D& diffuse);

	QVector3D specular() const { return _specular; }
	void setSpecular(const QVector3D& specular);

	QVector3D emissive() const { return _emissive; }
	void setEmissive(const QVector3D& emissive);

	float shininess() const { return _shininess; }
	void setShininess(float shininess);

	bool metallic() const { return _metallic; }
	void setMetallic(bool metallic);

	// PBR properties
	QVector3D albedoColor() const { return _albedoColor; }
	void setAlbedoColor(const QVector3D& albedoColor);

	float metalness() const { return _metalness; }
	void setMetalness(float metalness);

	float roughness() const { return _roughness; }
	void setRoughness(float roughness);

	float opacity() const { return _opacity; }
	void setOpacity(float opacity);

	// Enhanced emissive properties
	float emissiveStrength() const { return _emissiveStrength; }
	void setEmissiveStrength(float strength) { _emissiveStrength = strength; }

	// Advanced PBR properties
	float ior() const { return _ior; }
	void setIOR(float ior) { _ior = ior; }

	float clearcoat() const { return _clearcoat; }
	void setClearcoat(float clearcoat) { _clearcoat = clearcoat; }

	float clearcoatRoughness() const { return _clearcoatRoughness; }
	void setClearcoatRoughness(float roughness) { _clearcoatRoughness = roughness; }

	QVector3D sheenColor() const { return _sheenColor; }
	void setSheenColor(const QVector3D& color) { _sheenColor = color; }

	float sheenRoughness() const { return _sheenRoughness; }
	void setSheenRoughness(float roughness) { _sheenRoughness = roughness; }

	float transmission() const { return _transmission; }
	void setTransmission(float transmission) { _transmission = transmission; }

	// Rendering properties
	ShadingModel shadingModel() const { return _shadingModel; }
	void setShadingModel(ShadingModel model) { _shadingModel = model; }

	BlendMode blendMode() const { return _blendMode; }
	void setBlendMode(BlendMode mode) { _blendMode = mode; }

	bool twoSided() const { return _twoSided; }
	void setTwoSided(bool twoSided) { _twoSided = twoSided; }

	bool wireframe() const { return _wireframe; }
	void setWireframe(bool wireframe) { _wireframe = wireframe; }

	float alphaThreshold() const { return _alphaThreshold; }
	void setAlphaThreshold(float threshold) { _alphaThreshold = threshold; }

	float normalScale() const { return _normalScale; }
	void setNormalScale(float scale) { _normalScale = scale; }

	float occlusionStrength() const { return _occlusionStrength; }
	void setOcclusionStrength(float strength) { _occlusionStrength = strength; }

	// Texture slot identifiers (for use with texture manager)
	int albedoTextureId() const { return _albedoTextureId; }
	void setAlbedoTextureId(int id) { _albedoTextureId = id; _textures[static_cast<size_t>(TextureType::Albedo)].id = id; }

	int metallicTextureId() const { return _metallicTextureId; }
	void setMetallicTextureId(int id) { _metallicTextureId = id; _textures[static_cast<size_t>(TextureType::Metallic)].id = id; }

	int roughnessTextureId() const { return _roughnessTextureId; }
	void setRoughnessTextureId(int id) { _roughnessTextureId = id; _textures[static_cast<size_t>(TextureType::Roughness)].id = id; }

	int normalTextureId() const { return _normalTextureId; }
	void setNormalTextureId(int id) { _normalTextureId = id;  _textures[static_cast<size_t>(TextureType::Normal)].id = id; }

	int emissiveTextureId() const { return _emissiveTextureId; }
	void setEmissiveTextureId(int id) { _emissiveTextureId = id; _textures[static_cast<size_t>(TextureType::Emissive)].id = id; }

	int occlusionTextureId() const { return _occlusionTextureId; }
	void setOcclusionTextureId(int id) { _occlusionTextureId = id; _textures[static_cast<size_t>(TextureType::AmbientOcclusion)].id = id; }

	int opacityTextureId() const { return _opacityTextureId; }
	void setOpacityTextureId(int id) { _opacityTextureId = id; _textures[static_cast<size_t>(TextureType::Opacity)].id = id; }

	int heightTextureId() const { return _heightTextureId; }
	void setHeightTextureId(int id) { _heightTextureId = id; _textures[static_cast<size_t>(TextureType::Height)].id = id; }

	int clearcoatColorTextureId() const { return _clearcoatColorTextureId; }
	void setClearcoatColorTextureId(int id) { _clearcoatColorTextureId = id; _textures[static_cast<size_t>(TextureType::ClearcoatColor)].id = id; }

	int clearcoatRoughnessTextureId() const { return _clearcoatRoughnessTextureId; }
	void setClearcoatRoughnessTextureId(int id) { _clearcoatRoughnessTextureId = id; _textures[static_cast<size_t>(TextureType::ClearcoatRoughness)].id = id; }

	int clearcoatNormalTextureId() const { return _clearcoatNormalTextureId; }
	void setClearcoatNormalTextureId(int id) { _clearcoatNormalTextureId = id; _textures[static_cast<size_t>(TextureType::ClearcoatNormal)].id = id; }

	int sheenColorTextureId() const { return _sheenColorTextureId; }
	void setSheenColorTextureId(int id) { _sheenColorTextureId = id; _textures[static_cast<size_t>(TextureType::SheenColor)].id = id; }
	int sheenRoughnessTextureId() const { return _sheenRoughnessTextureId; }
	void setSheenRoughnessTextureId(int id) { _sheenRoughnessTextureId = id; _textures[static_cast<size_t>(TextureType::SheenRoughness)].id = id; }

	int iorTextureId() const { return _iorTextureId; }
	void setIORTextureId(int id) { _iorTextureId = id; _textures[static_cast<size_t>(TextureType::IOR)].id = id; }

	int transmissionTextureId() const { return _transmissionTextureId; }
	void setTransmissionTextureId(int id) { _transmissionTextureId = id; _textures[static_cast<size_t>(TextureType::Transmission)].id = id; }

	// Texture coordinate sets for multi-UV support
	int albedoTexCoord() const { return _albedoTexTransform.texCoord; }
	void setAlbedoTexCoord(int coord) { _albedoTexTransform.texCoord = coord; }

	QVector2D albedoTexScale() const { return _albedoTexTransform.texScale; }
	void setAlbedoTexScale(const QVector2D& scale) { _albedoTexTransform.texScale = scale; }

	QVector2D albedoTexOffset() const { return _albedoTexTransform.texOffset; }
	void setAlbedoTexOffset(const QVector2D& offset) { _albedoTexTransform.texOffset = offset; }

	float albedoTexRotation() const { return _albedoTexTransform.texRotation; }
	void setAlbedoTexRotation(float rotation) { _albedoTexTransform.texRotation = rotation; }


	int metallicTexCoord() const { return _metallicTexTransform.texCoord; }
	void setMetallicTexCoord(int coord) { _metallicTexTransform.texCoord = coord; }

	QVector2D metallicTexScale() const { return _metallicTexTransform.texScale; }
	void setMetallicTexScale(const QVector2D& scale) { _metallicTexTransform.texScale = scale; }

	QVector2D metallicTexOffset() const { return _metallicTexTransform.texOffset; }
	void setMetallicTexOffset(const QVector2D& offset) { _metallicTexTransform.texOffset = offset; }

	float metallicTexRotation() const { return _metallicTexTransform.texRotation; }
	void setMetallicTexRotation(float rotation) { _metallicTexTransform.texRotation = rotation; }


	int normalTexCoord() const { return _normalTexTransform.texCoord; }
	void setNormalTexCoord(int coord) { _normalTexTransform.texCoord = coord; }

	QVector2D normalTexScale() const { return _normalTexTransform.texScale; }
	void setNormalTexScale(const QVector2D& scale) { _normalTexTransform.texScale = scale; }

	QVector2D normalTexOffset() const { return _normalTexTransform.texOffset; }
	void setNormalTexOffset(const QVector2D& offset) { _normalTexTransform.texOffset = offset; }

	float normalTexRotation() const { return _normalTexTransform.texRotation; }
	void setNormalTexRotation(float rotation) { _normalTexTransform.texRotation = rotation; }


	int metallicRoughnessTexCoord() const { return _metallicRoughnessTexTransform.texCoord; }
	void setMetallicRoughnessTexCoord(int coord) { _metallicRoughnessTexTransform.texCoord = coord; }

	QVector2D metallicRoughnessTexScale() const { return _metallicRoughnessTexTransform.texScale; }
	void setMetallicRoughnessTexScale(const QVector2D& scale) { _metallicRoughnessTexTransform.texScale = scale; }

	QVector2D metallicRoughnessTexOffset() const { return _metallicRoughnessTexTransform.texOffset; }
	void setMetallicRoughnessTexOffset(const QVector2D& offset) { _metallicRoughnessTexTransform.texOffset = offset; }

	float metallicRoughnessTexRotation() const { return _metallicRoughnessTexTransform.texRotation; }
	void setMetallicRoughnessTexRotation(float rotation) { _metallicRoughnessTexTransform.texRotation = rotation; }


	int roughnessTexCoord() const { return _roughnessTexTransform.texCoord; }
	void setRoughnessTexCoord(int coord) { _roughnessTexTransform.texCoord = coord; }

	QVector2D roughnessTexScale() const { return _roughnessTexTransform.texScale; }
	void setRoughnessTexScale(const QVector2D& scale) { _roughnessTexTransform.texScale = scale; }

	QVector2D roughnessTexOffset() const { return _roughnessTexTransform.texOffset; }
	void setRoughnessTexOffset(const QVector2D& offset) { _roughnessTexTransform.texOffset = offset; }

	float roughnessTexRotation() const { return _roughnessTexTransform.texRotation; }
	void setRoughnessTexRotation(float rotation) { _roughnessTexTransform.texRotation = rotation; }


	int occlusionTexCoord() const { return _occlusionTexTransform.texCoord; }
	void setOcclusionTexCoord(int coord) { _occlusionTexTransform.texCoord = coord; }

	QVector2D occlusionTexScale() const { return _occlusionTexTransform.texScale; }
	void setOcclusionTexScale(const QVector2D& scale) { _occlusionTexTransform.texScale = scale; }

	QVector2D occlusionTexOffset() const { return _occlusionTexTransform.texOffset; }
	void setOcclusionTexOffset(const QVector2D& offset) { _occlusionTexTransform.texOffset = offset; }

	float occlusionTexRotation() const { return _occlusionTexTransform.texRotation; }
	void setOcclusionTexRotation(float rotation) { _occlusionTexTransform.texRotation = rotation; }


	int emissiveTexCoord() const { return _emissiveTexTransform.texCoord; }
	void setEmissiveTexCoord(int coord) { _emissiveTexTransform.texCoord = coord; }

	QVector2D emissiveTexScale() const { return _emissiveTexTransform.texScale; }
	void setEmissiveTexScale(const QVector2D& scale) { _emissiveTexTransform.texScale = scale; }

	QVector2D emissiveTexOffset() const { return _emissiveTexTransform.texOffset; }
	void setEmissiveTexOffset(const QVector2D& offset) { _emissiveTexTransform.texOffset = offset; }

	float emissiveTexRotation() const { return _emissiveTexTransform.texRotation; }
	void setEmissiveTexRotation(float rotation) { _emissiveTexTransform.texRotation = rotation; }


	int opacityTexCoord() const { return _opacityTexTransform.texCoord; }
	void setOpacityTexCoord(int coord) { _opacityTexTransform.texCoord = coord; }

	QVector2D opacityTexScale() const { return _opacityTexTransform.texScale; }
	void setOpacityTexScale(const QVector2D& scale) { _opacityTexTransform.texScale = scale; }

	QVector2D opacityTexOffset() const { return _opacityTexTransform.texOffset; }
	void setOpacityTexOffset(const QVector2D& offset) { _opacityTexTransform.texOffset = offset; }

	float opacityTexRotation() const { return _opacityTexTransform.texRotation; }
	void setOpacityTexRotation(float rotation) { _opacityTexTransform.texRotation = rotation; }


	int heightTexCoord() const { return _heightTexTransform.texCoord; }
	void setHeightTexCoord(int coord) { _heightTexTransform.texCoord = coord; }

	QVector2D heightTexScale() const { return _heightTexTransform.texScale; }
	void setHeightTexScale(const QVector2D& scale) { _heightTexTransform.texScale = scale; }

	QVector2D heightTexOffset() const { return _heightTexTransform.texOffset; }
	void setHeightTexOffset(const QVector2D& offset) { _heightTexTransform.texOffset = offset; }

	float heightTexRotation() const { return _heightTexTransform.texRotation; }
	void setHeightTexRotation(float rotation) { _heightTexTransform.texRotation = rotation; }


	int clearcoatColorTexCoord() const { return _clearcoatColorTexTransform.texCoord; }
	void setClearcoatColorTexCoord(int coord) { _clearcoatColorTexTransform.texCoord = coord; }

	QVector2D clearcoatColorTexScale() const { return _clearcoatColorTexTransform.texScale; }
	void setClearcoatColorTexScale(const QVector2D& scale) { _clearcoatColorTexTransform.texScale = scale; }

	QVector2D clearcoatColorTexOffset() const { return _clearcoatColorTexTransform.texOffset; }
	void setClearcoatColorTexOffset(const QVector2D& offset) { _clearcoatColorTexTransform.texOffset = offset; }

	float clearcoatColorTexRotation() const { return _clearcoatColorTexTransform.texRotation; }
	void setClearcoatColorTexRotation(float rotation) { _clearcoatColorTexTransform.texRotation = rotation; }


	int clearcoatRoughnessTexCoord() const { return _clearcoatRoughnessTexTransform.texCoord; }
	void setClearcoatRoughnessTexCoord(int coord) { _clearcoatRoughnessTexTransform.texCoord = coord; }

	QVector2D clearcoatRoughnessTexScale() const { return _clearcoatRoughnessTexTransform.texScale; }
	void setClearcoatRoughnessTexScale(const QVector2D& scale) { _clearcoatRoughnessTexTransform.texScale = scale; }

	QVector2D clearcoatRoughnessTexOffset() const { return _clearcoatRoughnessTexTransform.texOffset; }
	void setClearcoatRoughnessTexOffset(const QVector2D& offset) { _clearcoatRoughnessTexTransform.texOffset = offset; }

	float clearcoatRoughnessTexRotation() const { return _clearcoatRoughnessTexTransform.texRotation; }
	void setClearcoatRoughnessTexRotation(float rotation) { _clearcoatRoughnessTexTransform.texRotation = rotation; }


	int clearcoatNormalTexCoord() const { return _clearcoatNormalTexTransform.texCoord; }
	void setClearcoatNormalTexCoord(int coord) { _clearcoatNormalTexTransform.texCoord = coord; }

	QVector2D clearcoatNormalTexScale() const { return _clearcoatNormalTexTransform.texScale; }
	void setClearcoatNormalTexScale(const QVector2D& scale) { _clearcoatNormalTexTransform.texScale = scale; }

	QVector2D clearcoatNormalTexOffset() const { return _clearcoatNormalTexTransform.texOffset; }
	void setClearcoatNormalTexOffset(const QVector2D& offset) { _clearcoatNormalTexTransform.texOffset = offset; }

	float clearcoatNormalTexRotation() const { return _clearcoatNormalTexTransform.texRotation; }
	void setClearcoatNormalTexRotation(float rotation) { _clearcoatNormalTexTransform.texRotation = rotation; }


	int sheenColorTexCoord() const { return _sheenColorTexTransform.texCoord; }
	void setSheenColorTexCoord(int coord) { _sheenColorTexTransform.texCoord = coord; }

	QVector2D sheenColorTexScale() const { return _sheenColorTexTransform.texScale; }
	void setSheenColorTexScale(const QVector2D& scale) { _sheenColorTexTransform.texScale = scale; }

	QVector2D sheenColorTexOffset() const { return _sheenColorTexTransform.texOffset; }
	void setSheenColorTexOffset(const QVector2D& offset) { _sheenColorTexTransform.texOffset = offset; }

	float sheenColorTexRotation() const { return _sheenColorTexTransform.texRotation; }
	void setSheenColorTexRotation(float rotation) { _sheenColorTexTransform.texRotation = rotation; }


	int sheenRoughnessTexCoord() const { return _sheenRoughnessTexTransform.texCoord; }
	void setSheenRoughnessTexCoord(int coord) { _sheenRoughnessTexTransform.texCoord = coord; }

	QVector2D sheenRoughnessTexScale() const { return _sheenRoughnessTexTransform.texScale; }
	void setSheenRoughnessTexScale(const QVector2D& scale) { _sheenRoughnessTexTransform.texScale = scale; }

	QVector2D sheenRoughnessTexOffset() const { return _sheenRoughnessTexTransform.texOffset; }
	void setSheenRoughnessTexOffset(const QVector2D& offset) { _sheenRoughnessTexTransform.texOffset = offset; }

	float sheenRoughnessTexRotation() const { return _sheenRoughnessTexTransform.texRotation; }
	void setSheenRoughnessTexRotation(float rotation) { _sheenRoughnessTexTransform.texRotation = rotation; }


	// IOR texture transform
	int iorTexCoord() const { return _iorTexTransform.texCoord; }
	void setIORTexCoord(int coord) { _iorTexTransform.texCoord = coord; }
	QVector2D iorTexScale() const { return _iorTexTransform.texScale; }
	void setIORTexScale(const QVector2D& scale) { _iorTexTransform.texScale = scale; }
	QVector2D iorTexOffset() const { return _iorTexTransform.texOffset; }
	void setIORTexOffset(const QVector2D& offset) { _iorTexTransform.texOffset = offset; }
	float iorTexRotation() const { return _iorTexTransform.texRotation; }
	void setIORTexRotation(float rotation) { _iorTexTransform.texRotation = rotation; }

	// Transmission texture transform
	int transmissionTexCoord() const { return _transmissionTexTransform.texCoord; }
	void setTransmissionTexCoord(int coord) { _transmissionTexTransform.texCoord = coord; }
	QVector2D transmissionTexScale() const { return _transmissionTexTransform.texScale; }
	void setTransmissionTexScale(const QVector2D& scale) { _transmissionTexTransform.texScale = scale; }
	QVector2D transmissionTexOffset() const { return _transmissionTexTransform.texOffset; }
	void setTransmissionTexOffset(const QVector2D& offset) { _transmissionTexTransform.texOffset = offset; }
	float transmissionTexRotation() const { return _transmissionTexTransform.texRotation; }
	void setTransmissionTexRotation(float rotation) { _transmissionTexTransform.texRotation = rotation; }


	// --- KHR_materials_specular ---
	void setSpecularFactor(float factor) { _specularFactor = factor; }
	float specularFactor() const { return _specularFactor; }
	void setSpecularColorFactor(const QVector3D& color) { _specularColorFactor = color; }
	QVector3D specularColorFactor() const { return _specularColorFactor; }

	void setSpecularFactorMap(const QString& path) { _specularFactorMap = path; }
	void clearSpecularFactorMap() { _specularFactorMap.clear(); }
	QString specularFactorMap() const { return _specularFactorMap; }
	bool hasSpecularFactorMap() const { return !_specularFactorMap.isEmpty(); }
	void setSpecularFactorTextureId(unsigned int id) { _specularFactorTextureId = id; _textures[static_cast<size_t>(TextureType::SpecularFactor)].id = id; }
	unsigned int specularFactorTextureId() const { return _specularFactorTextureId; }
	void setSpecularFactorTexCoord(int coord) { _specularFactorTexTransform.texCoord = coord; }
	int specularFactorTexCoord() const { return _specularFactorTexTransform.texCoord; }
	QVector2D specularFactorTexScale() const { return _specularFactorTexTransform.texScale; }
	void setSpecularFactorTexScale(const QVector2D& scale) { _specularFactorTexTransform.texScale = scale; }
	QVector2D specularFactorTexOffset() const { return _specularFactorTexTransform.texOffset; }
	void setSpecularFactorTexOffset(const QVector2D& offset) { _specularFactorTexTransform.texOffset = offset; }
	float specularFactorTexRotation() const { return _specularFactorTexTransform.texRotation; }
	void setSpecularFactorTexRotation(float rotation) { _specularFactorTexTransform.texRotation = rotation; }

	void setSpecularColorMap(const QString& path) { _specularColorMap = path; }
	void clearSpecularColorMap() { _specularColorMap.clear(); }
	QString specularColorMap() const { return _specularColorMap; }
	bool hasSpecularColorMap() const { return !_specularColorMap.isEmpty(); }
	void setSpecularColorTextureId(unsigned int id) { _specularColorTextureId = id; _textures[static_cast<size_t>(TextureType::SpecularColor)].id = id; }
	unsigned int specularColorTextureId() const { return _specularColorTextureId; }
	void setSpecularColorTexCoord(int coord) { _specularColorTexTransform.texCoord = coord; }
	int specularColorTexCoord() const { return _specularColorTexTransform.texCoord; }
	QVector2D specularColorTexScale() const { return _specularColorTexTransform.texScale; }
	void setSpecularColorTexScale(const QVector2D& scale) { _specularColorTexTransform.texScale = scale; }
	QVector2D specularColorTexOffset() const { return _specularColorTexTransform.texOffset; }
	void setSpecularColorTexOffset(const QVector2D& offset) { _specularColorTexTransform.texOffset = offset; }
	float specularColorTexRotation() const { return _specularColorTexTransform.texRotation; }
	void setSpecularColorTexRotation(float rotation) { _specularColorTexTransform.texRotation = rotation; }

	// --- KHR_materials_pbrSpecularGlossiness ---

	// Factor uniforms
	void setDiffuseColor(const QVector3D& color) { _diffuseColor = color; }
	QVector3D diffuseColor() const { return _diffuseColor; }

	void setSpecularColor(const QVector3D& color) { _specularColor = color; }
	QVector3D specularColor() const { return _specularColor; }

	void setGlossinessFactor(float glossiness) { _glossinessFactor = qBound(0.0f, glossiness, 1.0f); }
	float glossinessFactor() const { return _glossinessFactor; }

	void setUseSpecularGlossiness(bool use) { _useSpecularGlossiness = use; }
	bool getUseSpecularGlossiness() const { return _useSpecularGlossiness; }

	// Diffuse texture
	void setDiffuseMap(const QString& path) { _diffuseMap = path; }
	QString diffuseMap() const { return _diffuseMap; }
	bool hasDiffuseMap() const { return !_diffuseMap.isEmpty(); }
	void setDiffuseTextureId(unsigned int id) { _diffuseTextureId = id; _textures[static_cast<size_t>(TextureType::Diffuse)].id = id; }
	unsigned int diffuseTextureId() const { return _diffuseTextureId; }
	void setDiffuseTexCoord(int coord) { _diffuseTexTransform.texCoord = coord; }
	int diffuseTexCoord() const { return _diffuseTexTransform.texCoord; }
	QVector2D diffuseTexScale() const { return _diffuseTexTransform.texScale; }
	void setDiffuseTexScale(const QVector2D& scale) { _diffuseTexTransform.texScale = scale; }
	QVector2D diffuseTexOffset() const { return _diffuseTexTransform.texOffset; }
	void setDiffuseTexOffset(const QVector2D& offset) { _diffuseTexTransform.texOffset = offset; }
	float diffuseTexRotation() const { return _diffuseTexTransform.texRotation; }
	void setDiffuseTexRotation(float rotation) { _diffuseTexTransform.texRotation = rotation; }

	// Specular-Glossiness packed texture (RGB spec sRGB, A gloss linear)
	void setSpecularGlossinessMap(const QString& path) { _specularGlossinessMap = path; }
	QString specularGlossinessMap() const { return _specularGlossinessMap; }
	bool hasSpecularGlossinessMap() const { return !_specularGlossinessMap.isEmpty(); }
	void setSpecularGlossinessTextureId(unsigned int id) { _specularGlossinessTextureId = id; _textures[static_cast<size_t>(TextureType::SpecularGlossiness)].id = id; }
	unsigned int specularGlossinessTextureId() const { return _specularGlossinessTextureId; }
	void setSpecularGlossinessTexCoord(int coord) { _specularGlossinessTexTransform.texCoord = coord; }
	int specularGlossinessTexCoord() const { return _specularGlossinessTexTransform.texCoord; }
	QVector2D specularGlossinessTexScale() const { return _specularGlossinessTexTransform.texScale; }
	void setSpecularGlossinessTexScale(const QVector2D& scale) { _specularGlossinessTexTransform.texScale = scale; }
	QVector2D specularGlossinessTexOffset() const { return _specularGlossinessTexTransform.texOffset; }
	void setSpecularGlossinessTexOffset(const QVector2D& offset) { _specularGlossinessTexTransform.texOffset = offset; }
	float specularGlossinessTexRotation() const { return _specularGlossinessTexTransform.texRotation; }
	void setSpecularGlossinessTexRotation(float rotation) { _specularGlossinessTexTransform.texRotation = rotation; }

	// --- KHR_materials_anisotropy ---
	void setAnisotropyStrength(float strength) { _anisotropyStrength = strength; }
	float anisotropyStrength() const { return _anisotropyStrength; }
	void setAnisotropyRotation(float rotation) { _anisotropyRotation = rotation; }
	float anisotropyRotation() const { return _anisotropyRotation; }
	void setAnisotropyMap(const QString& path) { _anisotropyMap = path; }
	void clearAnisotropyMap() { _anisotropyMap.clear(); }
	QString anisotropyMap() const { return _anisotropyMap; }
	bool hasAnisotropyMap() const { return !_anisotropyMap.isEmpty(); }
	void setAnisotropyTextureId(unsigned int id) { _anisotropyTextureId = id; _textures[static_cast<size_t>(TextureType::Anisotropy)].id = id; }
	int anisotropyTextureId() const { return _anisotropyTextureId; }
	void setAnisotropyTexCoord(int coord) { _anisotropyTexTransform.texCoord = coord; }
	int anisotropyTexCoord() const { return _anisotropyTexTransform.texCoord; }
	void setAnisotropyTexScale(const QVector2D& scale) { _anisotropyTexTransform.texScale = scale; }
	QVector2D anisotropyTexScale() const { return _anisotropyTexTransform.texScale; }
	void setAnisotropyTexOffset(const QVector2D& offset) { _anisotropyTexTransform.texOffset = offset; }
	QVector2D anisotropyTexOffset() const { return _anisotropyTexTransform.texOffset; }
	void setAnisotropyTexRotation(float rotation) { _anisotropyTexTransform.texRotation = rotation; }
	float anisotropyTexRotation() const { return _anisotropyTexTransform.texRotation; }


	// --- KHR_materials_iridescence ---
	void setIridescenceFactor(float factor) { _iridescenceFactor = factor; }
	float iridescenceFactor() const { return _iridescenceFactor; }
	void setIridescenceIor(float ior) { _iridescenceIor = ior; }
	float iridescenceIor() const { return _iridescenceIor; }
	void setIridescenceThicknessMin(float min) { _iridescenceThicknessMin = min; }
	float iridescenceThicknessMin() const { return _iridescenceThicknessMin; }
	void setIridescenceThicknessMax(float max) { _iridescenceThicknessMax = max; }
	float iridescenceThicknessMax() const { return _iridescenceThicknessMax; }
	void setIridescenceMap(const QString& path) { _iridescenceMap = path; }
	void clearIridescenceMap() { _iridescenceMap.clear(); }
	QString iridescenceMap() const { return _iridescenceMap; }
	bool hasIridescenceMap() const { return !_iridescenceMap.isEmpty(); }
	void setIridescenceTextureId(unsigned int id) { _iridescenceTextureId = id; _textures[static_cast<size_t>(TextureType::Iridescence)].id = id; }
	int iridescenceTextureId() const { return _iridescenceTextureId; }
	void setIridescenceTexCoord(int coord) { _iridescenceTexTransform.texCoord = coord; }
	int iridescenceTexCoord() const { return _iridescenceTexTransform.texCoord; }
	QVector2D iridescenceTexScale() const { return _iridescenceTexTransform.texScale; }
	void setIridescenceTexScale(const QVector2D& scale) { _iridescenceTexTransform.texScale = scale; }
	QVector2D iridescenceTexOffset() const { return _iridescenceTexTransform.texOffset; }
	void setIridescenceTexOffset(const QVector2D& offset) { _iridescenceTexTransform.texOffset = offset; }
	float iridescenceTexRotation() const { return _iridescenceTexTransform.texRotation; }
	void setIridescenceTexRotation(float rotation) { _iridescenceTexTransform.texRotation = rotation; }

	void setIridescenceThicknessMap(const QString& path) { _iridescenceThicknessMap = path; }
	void clearIridescenceThicknessMap() { _iridescenceThicknessMap.clear(); }
	QString iridescenceThicknessMap() const { return _iridescenceThicknessMap; }
	bool hasIridescenceThicknessMap() const { return !_iridescenceThicknessMap.isEmpty(); }
	void setIridescenceThicknessTextureId(unsigned int id) { _iridescenceThicknessTextureId = id; _textures[static_cast<size_t>(TextureType::IridescenceThickness)].id = id; }
	int iridescenceThicknessTextureId() const { return _iridescenceThicknessTextureId; }
	void setIridescenceThicknessTexCoord(int coord) { _iridescenceThicknessTexTransform.texCoord = coord; }
	int iridescenceThicknessTexCoord() const { return _iridescenceThicknessTexTransform.texCoord; }
	void setIridescenceThicknessTexScale(const QVector2D& scale) { _iridescenceThicknessTexTransform.texScale = scale; }
	QVector2D iridescenceThicknessTexScale() const { return _iridescenceThicknessTexTransform.texScale; }
	void setIridescenceThicknessTexOffset(const QVector2D& offset) { _iridescenceThicknessTexTransform.texOffset = offset; }
	QVector2D iridescenceThicknessTexOffset() const { return _iridescenceThicknessTexTransform.texOffset; }
	void setIridescenceThicknessTexRotation(float rotation) { _iridescenceThicknessTexTransform.texRotation = rotation; }
	float iridescenceThicknessTexRotation() const { return _iridescenceThicknessTexTransform.texRotation; }

	// --- KHR_materials_volume ---
	void setThicknessFactor(float thickness) { _thicknessFactor = thickness; }
	float thicknessFactor() const { return _thicknessFactor; }
	void setAttenuationDistance(float distance) { _attenuationDistance = distance; }
	float attenuationDistance() const { return _attenuationDistance; }
	void setAttenuationColor(const QVector3D& color) { _attenuationColor = color; }
	QVector3D attenuationColor() const { return _attenuationColor; }
	void setThicknessMap(const QString& path) { _thicknessMap = path; }
	void clearThicknessMap() { _thicknessMap.clear(); }
	QString thicknessMap() const { return _thicknessMap; }
	bool hasThicknessMap() const { return !_thicknessMap.isEmpty(); }
	void setThicknessTextureId(unsigned int id) { _thicknessTextureId = id; _textures[static_cast<size_t>(TextureType::Thickness)].id = id; }
	int thicknessTextureId() const { return _thicknessTextureId; }
	void setThicknessTexCoord(int coord) { _thicknessTexTransform.texCoord = coord; }
	int thicknessTexCoord() const { return _thicknessTexTransform.texCoord; }
	void setThicknessTexScale(const QVector2D& scale) { _thicknessTexTransform.texScale = scale; }
	QVector2D thicknessTexScale() const { return _thicknessTexTransform.texScale; }
	void setThicknessTexOffset(const QVector2D& offset) { _thicknessTexTransform.texOffset = offset; }
	QVector2D thicknessTexOffset() const { return _thicknessTexTransform.texOffset; }
	void setThicknessTexRotation(float rotation) { _thicknessTexTransform.texRotation = rotation; }
	float thicknessTexRotation() const { return _thicknessTexTransform.texRotation; }
	void setHasThicknessAlpha(bool hasAlpha) { _hasThicknessAlpha = hasAlpha; }
	bool hasThicknessAlpha() const { return _hasThicknessAlpha; }

	// -- KHR_materials_diffuse_transmission ---
	void setDiffuseTransmissionFactor(float transmission) { _diffuseTransmissionFactor = transmission; }
	float diffuseTransmissionFactor() const { return _diffuseTransmissionFactor; }
	void setDiffuseTransmissionColorFactor(const QVector3D& color) { _diffuseTransmissionColorFactor = color; }
	QVector3D diffuseTransmissionColorFactor() const { return _diffuseTransmissionColorFactor; }
	void setDiffuseTransmissionMap(const QString& path) { _diffuseTransmissionMap = path; }
	void clearDiffuseTransmissionMap() { _diffuseTransmissionMap.clear(); }
	QString diffuseTransmissionMap() const { return _diffuseTransmissionMap; }
	bool hasDiffuseTransmissionMap() const { return !_diffuseTransmissionMap.isEmpty(); }
	void setDiffuseTransmissionTextureId(unsigned int id) { _diffuseTransmissionTextureId = id; _textures[static_cast<size_t>(TextureType::DiffuseTransmission)].id = id; }
	int diffuseTransmissionTextureId() const { return _diffuseTransmissionTextureId; }
	void setDiffuseTransmissionTexCoord(int coord) { _diffuseTransmissionTexTransform.texCoord = coord; }
	int diffuseTransmissionTexCoord() const { return _diffuseTransmissionTexTransform.texCoord; }
	QVector2D diffuseTransmissionTexScale() const { return _diffuseTransmissionTexTransform.texScale; }
	void setDiffuseTransmissionTexScale(const QVector2D& scale) { _diffuseTransmissionTexTransform.texScale = scale; }
	QVector2D diffuseTransmissionTexOffset() const { return _diffuseTransmissionTexTransform.texOffset; }
	void setDiffuseTransmissionTexOffset(const QVector2D& offset) { _diffuseTransmissionTexTransform.texOffset = offset; }
	float diffuseTransmissionTexRotation() const { return _diffuseTransmissionTexTransform.texRotation; }
	void setDiffuseTransmissionTexRotation(float rotation) { _diffuseTransmissionTexTransform.texRotation = rotation; }

	void setDiffuseTransmissionColorMap(const QString& path) { _diffuseTransmissionColorMap = path; }
	void clearDiffuseTransmissionColorMap() { _diffuseTransmissionColorMap.clear(); }
	QString diffuseTransmissionColorMap() const { return _diffuseTransmissionColorMap; }
	bool hasDiffuseTransmissionColorMap() const { return !_diffuseTransmissionColorMap.isEmpty(); }
	void setDiffuseTransmissionColorTextureId(unsigned int id) { _diffuseTransmissionColorTextureId = id; _textures[static_cast<size_t>(TextureType::DiffuseTransmissionColor)].id = id; }
	int diffuseTransmissionColorTextureId() const { return _diffuseTransmissionColorTextureId; }
	void setDiffuseTransmissionColorTexCoord(int coord) { _diffuseTransmissionColorTexTransform.texCoord = coord; }
	int diffuseTransmissionColorTexCoord() const { return _diffuseTransmissionColorTexTransform.texCoord; }
	QVector2D diffuseTransmissionColorTexScale() const { return _diffuseTransmissionColorTexTransform.texScale; }
	void setDiffuseTransmissionColorTexScale(const QVector2D& scale) { _diffuseTransmissionColorTexTransform.texScale = scale; }
	QVector2D diffuseTransmissionColorTexOffset() const { return _diffuseTransmissionColorTexTransform.texOffset; }
	void setDiffuseTransmissionColorTexOffset(const QVector2D& offset) { _diffuseTransmissionColorTexTransform.texOffset = offset; }
	float diffuseTransmissionColorTexRotation() const { return _diffuseTransmissionColorTexTransform.texRotation; }
	void setDiffuseTransmissionColorTexRotation(float rotation) { _diffuseTransmissionColorTexTransform.texRotation = rotation; }


	// --- KHR_materials_dispersion ---
	void setDispersion(float dispersion) { _dispersion = dispersion; }
	float dispersion() const { return _dispersion; }

	// --- KHR_materials_unlit ---
	void setUnlit(bool unlit) { _unlit = unlit; }
	bool isUnlit() const { return _unlit; }

	// --- KHR_materials_scattering ---
	void setMultiScatterColor(const QVector3D& color) { _multiScatterColor = color; }
	QVector3D multiScatterColor() const { return _multiScatterColor; }
	void setHasVolumeScattering(bool hasVolumeScattering) { _hasVolumeScattering = hasVolumeScattering; }
	bool hasVolumeScattering() const { return _hasVolumeScattering; }

	// --- Map path API (used by TextureMappingPanel) ---
	QString albedoMapPath() const { return _albedoMapPath; }
	void setAlbedoMap(const QString& path) { _albedoMapPath = path; /* optional: _albedoTextureId = -1; */ }
	void clearAlbedoMap() { _albedoMapPath.clear(); /* optional: _albedoTextureId = -1; */ }
	bool hasAlbedoMap() const { return !albedoMapPath().isEmpty(); }

	void setHasTextureAlpha(bool hasAlpha) { _hasTextureAlpha = hasAlpha; }
	bool hasTextureAlpha() const { return _hasTextureAlpha; } // Check if any assigned texture has alpha channel

	QString normalMapPath() const { return _normalMapPath; }
	void setNormalMap(const QString& path) { _normalMapPath = path; /* _normalTextureId = -1; */ }
	void clearNormalMap() { _normalMapPath.clear(); /* _normalTextureId = -1; */ }
	bool hasNormalMap() const { return !normalMapPath().isEmpty(); }

	QString emissiveMapPath() const { return _emissiveMapPath; }
	void setEmissiveMap(const QString& path) { _emissiveMapPath = path; /* _emissiveTextureId = -1; */ }
	void clearEmissiveMap() { _emissiveMapPath.clear(); /* _emissiveTextureId = -1; */ }
	bool hasEmissiveMap() const { return !emissiveMapPath().isEmpty(); }

	QString metallicMapPath() const { return _metallicMapPath; }
	void setMetallicMap(const QString& path)
	{
		_metallicMapPath = path;
		assignAutoPackingForPath(path);
	}
	void clearMetallicMap() { _metallicMapPath.clear(); }
	bool hasMetallicMap() const { return !metallicMapPath().isEmpty(); }

	QString roughnessMapPath() const { return _roughnessMapPath; }
	void setRoughnessMap(const QString& path)
	{
		_roughnessMapPath = path;
		assignAutoPackingForPath(path);
	}
	void clearRoughnessMap() { _roughnessMapPath.clear(); }
	bool hasRoughnessMap() const { return !roughnessMapPath().isEmpty(); }

	QString aoMapPath() const { return _aoMapPath; }
	void setAOMap(const QString& path)
	{
		_aoMapPath = path;
		assignAutoPackingForPath(path);
	}
	void clearAOMap() { _aoMapPath.clear(); }
	bool hasAOMap() const { return !aoMapPath().isEmpty(); }

	QString opacityMapPath() const { return _opacityMapPath; }
	void setOpacityMap(const QString& path)
	{
		_opacityMapPath = path;
		assignAutoPackingForPath(path);
	}
	void clearOpacityMap() { _opacityMapPath.clear(); }
	bool hasOpacityMap() const { return !opacityMapPath().isEmpty(); }
	void setInvertOpacityMap(bool invert) { _invertOpacityTexture = invert; }
	bool isOpacityMapInverted() const { return _invertOpacityTexture; }

	QString heightMapPath() const { return _heightMapPath; }
	void setHeightMap(const QString& path) { _heightMapPath = path; /* _heightTextureId = -1; */ }
	void clearHeightMap() { _heightMapPath.clear(); /* _heightTextureId = -1; */ }
	bool hasHeightMap() const { return !heightMapPath().isEmpty(); }

	QString transmissionMapPath() const { return _transmissionMapPath; }
	void setTransmissionMap(const QString& path) { _transmissionMapPath = path; /* _transmissionTextureId = -1; */ }
	void clearTransmissionMap() { _transmissionMapPath.clear(); /* _transmissionTextureId = -1; */ }
	bool hasTransmissionMap() const { return !transmissionMapPath().isEmpty(); }

	QString iorMapPath() const { return _iorMapPath; }
	void setIORMap(const QString& path) { _iorMapPath = path; }
	void clearIORMap() { _iorMapPath.clear(); }
	bool hasIORMap() const { return !iorMapPath().isEmpty(); }

	QString sheenColorMapPath() const { return _sheenColorMapPath; }
	void setSheenColorMap(const QString& path) { _sheenColorMapPath = path; }
	void clearSheenColorMap() { _sheenColorMapPath.clear(); }
	bool hasSheenColorMap() const { return !sheenColorMapPath().isEmpty(); }

	QString sheenRoughnessMapPath() const { return _sheenRoughnessMapPath; }
	void setSheenRoughnessMap(const QString& path) { _sheenRoughnessMapPath = path; }
	void clearSheenRoughnessMap() { _sheenRoughnessMapPath.clear(); }
	bool hasSheenRoughnessMap() const { return !sheenRoughnessMapPath().isEmpty(); }

	QString clearcoatColorMapPath() const { return _clearcoatColorMapPath; }
	void setClearcoatColorMap(const QString& path) { _clearcoatColorMapPath = path; /* _clearcoatTextureId = -1; */ }
	void clearClearcoatColorMap() { _clearcoatColorMapPath.clear(); }
	bool hasClearcoatColorMap() const { return !clearcoatColorMapPath().isEmpty(); }

	QString clearcoatRoughnessMapPath() const { return _clearcoatRoughnessMapPath; }
	void setClearcoatRoughnessMap(const QString& path) { _clearcoatRoughnessMapPath = path; }
	void clearClearcoatRoughnessMap() { _clearcoatRoughnessMapPath.clear(); }
	bool hasClearcoatRoughnessMap() const { return !clearcoatRoughnessMapPath().isEmpty(); }

	QString clearcoatNormalMapPath() const { return _clearcoatNormalMapPath; }
	void setClearcoatNormalMap(const QString& path) { _clearcoatNormalMapPath = path; }
	void clearClearcoatNormalMap() { _clearcoatNormalMapPath.clear(); }
	bool hasClearcoatNormalMap() const { return !clearcoatNormalMapPath().isEmpty(); }

	// --- Shared UV transform (used by the panel's UV controls) ---
	void setUVTiling(float u, float v) { _uvTilingU = u; _uvTilingV = v; }
	void setUVOffset(float u, float v) { _uvOffsetU = u; _uvOffsetV = v; }
	float uvTilingU() const { return _uvTilingU; }
	float uvTilingV() const { return _uvTilingV; }
	float uvOffsetU() const { return _uvOffsetU; }
	float uvOffsetV() const { return _uvOffsetV; }

	ChannelPacking packingFor(const QString& key) const;
	void setPackingFor(const QString& key, const ChannelPacking& p);

	void syncTextureParameters();

	// Convenience methods
	bool isMetallic() const
	{
		return _metallic;
	}

	bool isTransparent() const
	{
		// If it has transmission, it's ALWAYS transparent (exclude from FBO)
		if (_transmission > 0.0f)
			return true;
		// If it's OPAQUE, it's NOT transparent
		if (_blendMode == GLMaterial::BlendMode::Opaque)
			return false;
		// If it has BLEND mode (not MASK or OPAQUE), it's transparent
		if (_blendMode == GLMaterial::BlendMode::Alpha)  // BLEND mode
			return true;
		// If it's masked, it's NOT transparent (exclude from FBO)
		if (_blendMode == GLMaterial::BlendMode::Masked)
			return false;

		return (_opacity < 1.0) || _hasTextureAlpha || _transmission > 0.0f;
	}

	bool isEmissive() const
	{
		return _emissive.length() > 0.0f && _emissiveStrength > 0.0f;
	}

	bool hasClearcoat() const { return _clearcoat > 0.0f; }
	bool hasSheen() const { return _sheenColor.length() > 0.0f; }
	bool hasTransmission() const { return _transmission > 0.0f; }
	bool hasIOR() const { return _ior > 0.0f; }

	// Material validation
	bool isValid() const;
	void validateAndFix();

	// Conversion utilities
	void convertToBlinnPhong();
	void convertToPBR();

	// Get Fresnel reflectance at normal incidence
	QVector3D getF0() const
	{
		// F0 is the base reflectivity at normal incidence

		if (_metalness > 0.5f)
		{
			// For metals, F0 is the albedo color
			return _albedoColor;
		}
		else
		{
			// For dielectrics, F0 is typically around 0.04 (4% reflectance)
			// Can be calculated from IOR: F0 = ((IOR-1)/(IOR+1))^2
			float f0_scalar = pow((_ior - 1.0f) / (_ior + 1.0f), 2.0f);

			// For mixed materials, interpolate between dielectric F0 and albedo
			QVector3D dielectric_f0(f0_scalar, f0_scalar, f0_scalar);
			return QVector3D(
				dielectric_f0.x() * (1.0f - _metalness) + _albedoColor.x() * _metalness,
				dielectric_f0.y() * (1.0f - _metalness) + _albedoColor.y() * _metalness,
				dielectric_f0.z() * (1.0f - _metalness) + _albedoColor.z() * _metalness
			);
		}
	}

	// Utility getters for common material properties
	QVector3D getAlbedoColor() const { return albedoColor(); }
	float getMetalness() const { return metalness(); }
	float getRoughness() const { return roughness(); }
	float getOpacity() const { return opacity(); }
	float getTransmission() const { return transmission(); }

	// Static material creation methods
	static GLMaterial getPredefinedMaterial(GLMaterial::PredefinedMaterials type);

	// Some metal materials
	static GLMaterial METAL_TITANIUM();
	static GLMaterial METAL_PLATINUM();
	static GLMaterial METAL_MAGNESIUM();
	static GLMaterial METAL_ZINC();
	static GLMaterial METAL_NICKEL();
	static GLMaterial METAL_ALUMINUM();
	static GLMaterial METAL_IRON_RAW();
	static GLMaterial METAL_COBALT();
	static GLMaterial METAL_PEWTER();
	static GLMaterial METAL_TUNGSTEN();
	static GLMaterial METAL_BRASS();
	static GLMaterial METAL_BRONZE();
	static GLMaterial METAL_COPPER();
	static GLMaterial METAL_GOLD();
	static GLMaterial METAL_SILVER();
	static GLMaterial METAL_CHROME();
	static GLMaterial METAL_STEEL();

	// Brushed metals
	static GLMaterial BRUSHED_ALUMINUM();
	static GLMaterial BRUSHED_STEEL();

	// Stone materials
	static GLMaterial STONE_RUBY();
	static GLMaterial STONE_EMERALD();
	static GLMaterial STONE_TURQUOISE();
	static GLMaterial STONE_PEARL();
	static GLMaterial STONE_JADE();
	static GLMaterial STONE_GRANITE();
	static GLMaterial STONE_LIMESTONE();
	static GLMaterial STONE_MARBLE();
	static GLMaterial STONE_SLATE();
	static GLMaterial STONE_SANDSTONE();
	static GLMaterial STONE_BASALT();
	static GLMaterial STONE_OBSIDIAN();
	static GLMaterial STONE_TRAVERTINE();
	static GLMaterial STONE_QUARTZITE();
	static GLMaterial STONE_SOAPSTONE();

	// Some colored plastic and rubber materials
	static GLMaterial RED_PLASTIC();
	static GLMaterial GREEN_PLASTIC();
	static GLMaterial BLUE_PLASTIC();
	static GLMaterial CYAN_PLASTIC();
	static GLMaterial YELLOW_PLASTIC();
	static GLMaterial MAGENTA_PLASTIC();
	static GLMaterial WHITE_PLASTIC();
	static GLMaterial BLACK_PLASTIC();
	static GLMaterial RED_RUBBER();
	static GLMaterial GREEN_RUBBER();
	static GLMaterial BLUE_RUBBER();
	static GLMaterial CYAN_RUBBER();
	static GLMaterial YELLOW_RUBBER();
	static GLMaterial MAGENTA_RUBBER();
	static GLMaterial WHITE_RUBBER();
	static GLMaterial BLACK_RUBBER();
	static GLMaterial DEFAULT_MAT();

	// Sheen materials
	static GLMaterial FABRIC();
	static GLMaterial VELVET_RED();
	static GLMaterial SATIN_FABRIC();
	static GLMaterial MICROFIBER_CLOTH();

	// Leather finishes
	static GLMaterial LEATHER_BLACK();
	static GLMaterial LEATHER_BROWN();
	static GLMaterial LEATHER_RED();
	static GLMaterial LEATHER_WHITE();
	static GLMaterial LEATHER_OXBLOOD();
	static GLMaterial LEATHER_TAN();

	// Wood materials
	// Wood materials
	static GLMaterial WOOD();
	static GLMaterial WOOD_BAMBOO();
	static GLMaterial WOOD_CEDAR();
	static GLMaterial WOOD_REDWOOD();
	static GLMaterial WOOD_OAK();
	static GLMaterial WOOD_PINE();
	static GLMaterial WOOD_BIRCH();
	static GLMaterial WOOD_WALNUT();
	static GLMaterial WOOD_CHERRY();
	static GLMaterial WOOD_TEAK();
	static GLMaterial WOOD_MAPLE();


	// Advanced PBR Materials
	// === CLEARCOAT MATERIALS ===
	// ORIGINAL METALLIC FINISHES	
	static GLMaterial CAR_PAINT_METALLIC_BLUE();
	static GLMaterial CAR_PAINT_DEEP_METALLIC_BLUE();
	static GLMaterial CAR_PAINT_METALLIC_GREEN();
	static GLMaterial CAR_PAINT_METALLIC_SILVER();
	static GLMaterial CAR_PAINT_METALLIC_RED();
	static GLMaterial CAR_PAINT_METALLIC_COPPER();
	static GLMaterial CAR_PAINT_METALLIC_GOLD();
	static GLMaterial CAR_PAINT_METALLIC_PURPLE();
	static GLMaterial CAR_PAINT_PEARL();

	// ORIGINAL NON-METALLIC FINISHES
	static GLMaterial CAR_PAINT_GLOSSY_BLACK();
	static GLMaterial CAR_PAINT_GLOSSY_WHITE();
	static GLMaterial CAR_PAINT_MATTE_RED();
	static GLMaterial CAR_PAINT_GLOSSY_YELLOW();
	static GLMaterial CAR_PAINT_GLOSSY_ORANGE();
	static GLMaterial CAR_PAINT_SATIN_GRAY();
	static GLMaterial CAR_PAINT_RED();
	static GLMaterial CAR_PAINT_WHITE();

	// ORIGINAL SPECIAL FINISHES
	static GLMaterial CAR_PAINT_PEARLESCENT_BLUE();
	static GLMaterial CAR_PAINT_CANDY_APPLE_RED();

	// DARK SHADE VARIATIONS
	static GLMaterial CAR_PAINT_MIDNIGHT_BLUE();
	static GLMaterial CAR_PAINT_FOREST_GREEN();
	static GLMaterial CAR_PAINT_CHARCOAL_GRAY();
	static GLMaterial CAR_PAINT_BURGUNDY();

	// LIGHT SHADE VARIATIONS
	static GLMaterial CAR_PAINT_POWDER_BLUE();
	static GLMaterial CAR_PAINT_MINT_GREEN();
	static GLMaterial CAR_PAINT_CREAM_YELLOW();
	static GLMaterial CAR_PAINT_LAVENDER();

	// MEDIUM TONE VARIATIONS
	static GLMaterial CAR_PAINT_TEAL();
	static GLMaterial CAR_PAINT_CORAL();
	static GLMaterial CAR_PAINT_SLATE_BLUE();

	// METALLIC VARIATIONS WITH DIFFERENT SHADES
	static GLMaterial CAR_PAINT_METALLIC_CHAMPAGNE();
	static GLMaterial CAR_PAINT_METALLIC_GUNMETAL();
	static GLMaterial CAR_PAINT_METALLIC_BRONZE();

	// ADDITIONAL SPECIAL FINISHES
	static GLMaterial CAR_PAINT_IRIDESCENT_GREEN();

	static GLMaterial MATTE_GREY();
	static GLMaterial PIANO_BLACK();


	// === TRANSMISSION MATERIALS ===
	static GLMaterial GLASS();
	static GLMaterial FROSTED_GLASS();
	static GLMaterial COLORED_GLASS_GREEN();
	static GLMaterial CRYSTAL_QUARTZ();

	// === EMISSIVE MATERIALS ===
	static GLMaterial NEON_BLUE();
	static GLMaterial NEON_GREEN();
	static GLMaterial NEON_RED();
	static GLMaterial NEON_YELLOW();
	static GLMaterial LED_RED();
	static GLMaterial LED_GREEN();
	static GLMaterial LED_BLUE();
	static GLMaterial LED_YELLOW();
	static GLMaterial LED_WHITE();

	// === COMPLEX MATERIALS (Multiple Properties) ===
	static GLMaterial IRIDESCENT_SOAP_BUBBLE();
	static GLMaterial CARBON_FIBER();
	static GLMaterial WET_ASPHALT();
	//concrete materials
	static GLMaterial CONCRETE();
	static GLMaterial CONCRETE_LIGHT();
	static GLMaterial CONCRETE_DARK();
	static GLMaterial CONCRETE_POLISHED();

	// Additional predefined materials for advanced use cases    
	static GLMaterial WATER();
	static GLMaterial DIAMOND();
	static GLMaterial CERAMIC();
	static GLMaterial SKIN();
	static GLMaterial PAPER();
	static GLMaterial METAL();
	static GLMaterial PLASTIC();
	static GLMaterial STONE();
	static GLMaterial MIRROR_SILVER();


	// Function to create a material with time-varying properties (for animations)
	GLMaterial createAnimatedEmissive(const QVector3D& baseColor,
		const QVector3D& emissiveColor,
		float emissiveStrength,
		float time);

	// Function to blend two materials based on a factor (useful for layered materials)
	GLMaterial blendMaterials(const GLMaterial& mat1,
		const GLMaterial& mat2,
		float factor);



	void serialize(QDataStream& out) const;
	void deserialize(QDataStream& in);

	// ------------------------------------------------------------------
	// Serialization helpers for the MaterialRegistry
	// Construct a GLMaterial from a QVariantMap (as produced by JSON)
	static GLMaterial fromVariantMap(const QVariantMap& m);
	// Serialize the GLMaterial into a QVariantMap suitable for JSON writing
	QVariantMap toVariantMap() const;

	bool isGLTFMaterial() const { return _isGLTFMaterial; }
	void setIsGLTFMaterial(bool isGLTF) { _isGLTFMaterial = isGLTF; }

private:
	void setAlbedoFromADS();
	void updateConsistency(); // Ensure consistency between legacy and PBR properties
	void clampValues(); // Ensure all values are within valid ranges
	void ensureADSConsistency(); // Ensure ambient, diffuse, specular are consistent with albedo
	void convertPBRtoADS();
	void assignAutoPackingForPath(const QString& path);

private:
	// ============================================================================
	// Unified Texture Array (20 textures)
	// ============================================================================
	std::array<Texture, static_cast<size_t>(TextureType::Count)> _textures;

	// Legacy Phong/Blinn properties
	QVector3D _ambient;
	QVector3D _diffuse;
	QVector3D _specular;
	QVector3D _emissive;
	float _shininess;
	bool _metallic;

	// PBR properties
	QVector3D _albedoColor;
	float _metalness;
	float _roughness;
	float _opacity;

	// Advanced PBR properties
	float _ior; // Index of refraction
	float _clearcoat;
	float _clearcoatRoughness;
	QVector3D _sheenColor;
	float _sheenRoughness;
	float _transmission;

	// Rendering properties
	ShadingModel _shadingModel = ShadingModel::PBR;
	BlendMode _blendMode = BlendMode::Opaque;
	bool _twoSided = true;
	bool _wireframe = false;
	float _alphaThreshold = 0.5f; // For masked blend mode

	bool _hasTextureAlpha = false; // Whether the albedo texture has an alpha channel

	float _normalScale = 1.0f;
	float _occlusionStrength = 1.0f;

	// Texture IDs (managed externally)
	int _albedoTextureId = 0;
	int _metallicTextureId = 0;
	int _roughnessTextureId = 0;
	int _normalTextureId = 0;
	int _occlusionTextureId = 0;
	int _emissiveTextureId = 0;
	int _opacityTextureId = 0;
	int _heightTextureId = 0;
	int _sheenColorTextureId = 0;
	int _sheenRoughnessTextureId = 0;
	int _clearcoatColorTextureId = 0;
	int _clearcoatRoughnessTextureId = 0;
	int _clearcoatNormalTextureId = 0;
	int _iorTextureId = 0;
	int _transmissionTextureId = 0;

	bool _invertOpacityTexture = false;

	// Texture coordinate sets
	int _albedoTexCoord;
	int _normalTexCoord;
	int _metallicRoughnessTexCoord;

	TextureTransform _albedoTexTransform;
	TextureTransform _metallicTexTransform;
	TextureTransform _normalTexTransform;
	TextureTransform _metallicRoughnessTexTransform;
	TextureTransform _roughnessTexTransform;
	TextureTransform _occlusionTexTransform;
	TextureTransform _emissiveTexTransform;
	TextureTransform _opacityTexTransform;
	TextureTransform _heightTexTransform;
	TextureTransform _clearcoatColorTexTransform;
	TextureTransform _clearcoatRoughnessTexTransform;
	TextureTransform _clearcoatNormalTexTransform;
	TextureTransform _sheenColorTexTransform;
	TextureTransform _sheenRoughnessTexTransform;
	TextureTransform _iorTexTransform;
	TextureTransform _transmissionTexTransform;
	TextureTransform _anisotropyTexTransform;
	TextureTransform _iridescenceTexTransform;
	TextureTransform _iridescenceThicknessTexTransform;
	TextureTransform _thicknessTexTransform;
	TextureTransform _specularFactorTexTransform;
	TextureTransform _specularColorTexTransform;
	TextureTransform _diffuseTransmissionTexTransform;
	TextureTransform _diffuseTransmissionColorTexTransform;
	TextureTransform _diffuseTexTransform;
	TextureTransform _specularGlossinessTexTransform;

	// Map paths (UI-facing; your renderer/texture manager can translate these to GL)
	QString _albedoMapPath;
	QString _normalMapPath;
	QString _emissiveMapPath;
	QString _metallicMapPath;
	QString _roughnessMapPath;
	QString _aoMapPath;
	QString _opacityMapPath;
	QString _heightMapPath;
	QString _transmissionMapPath;
	QString _iorMapPath;
	QString _sheenColorMapPath;
	QString _sheenRoughnessMapPath;
	QString _clearcoatColorMapPath;
	QString _clearcoatRoughnessMapPath;
	QString _clearcoatNormalMapPath;

	// KHR_materials_specular
	float _specularFactor = 1.0f;
	QVector3D _specularColorFactor = QVector3D(1.0f, 1.0f, 1.0f);
	QString _specularFactorMap;
	unsigned int _specularFactorTextureId = 0;
	QString _specularColorMap;
	unsigned int _specularColorTextureId = 0;

	// KHR_materials_pbrSpecularGlossiness
	QVector3D _diffuseColor = QVector3D(1.0f, 1.0f, 1.0f);
	QVector3D _specularColor = QVector3D(1.0f, 1.0f, 1.0f);
	float _glossinessFactor = 1.0f;
	bool _useSpecularGlossiness = false;
	QString _diffuseMap;
	unsigned int _diffuseTextureId = 0;
	QString _specularGlossinessMap;
	unsigned int _specularGlossinessTextureId = 0;

	// KHR_materials_anisotropy
	float _anisotropyStrength = 0.0f;
	float _anisotropyRotation = 0.0f;
	QString _anisotropyMap;
	unsigned int _anisotropyTextureId = 0;

	// KHR_materials_iridescence
	float _iridescenceFactor = 0.0f;
	float _iridescenceIor = 1.3f;
	float _iridescenceThicknessMin = 100.0f;
	float _iridescenceThicknessMax = 400.0f;
	QString _iridescenceMap;
	unsigned int _iridescenceTextureId = 0;
	QString _iridescenceThicknessMap;
	unsigned int _iridescenceThicknessTextureId = 0;

	// KHR_materials_volume
	float _thicknessFactor = 0.0f;
	float _attenuationDistance = std::numeric_limits<float>::infinity();
	QVector3D _attenuationColor = QVector3D(1.0f, 1.0f, 1.0f);
	QString _thicknessMap;
	unsigned int _thicknessTextureId = 0;
	bool _hasThicknessAlpha = false;

	// KHR_materials_emissive_strength
	float _emissiveStrength = 1.0f;

	// KHR_materials_dispersion
	float _dispersion = 0.0f;

	// KHR_materials_unlit
	bool _unlit = false;

	// KHR_materials_diffuse_transmission
	float _diffuseTransmissionFactor = 0.0f;
	QVector3D _diffuseTransmissionColorFactor = QVector3D(1.0f, 1.0f, 1.0f);
	QString _diffuseTransmissionMap;
	unsigned int _diffuseTransmissionTextureId = 0;
	QString _diffuseTransmissionColorMap;
	unsigned int _diffuseTransmissionColorTextureId = 0;

	// KHR_materials_scattering
	QVector3D _multiScatterColor = QVector3D(1.0f, 1.0f, 1.0f);  // Default: white (no scattering effect)
	bool _hasVolumeScattering = false;

	// Material-wide UV transform (panel + preview)
	float _uvTilingU = 1.0f, _uvTilingV = 1.0f;
	float _uvOffsetU = 0.0f, _uvOffsetV = 0.0f;

	ChannelPacking _metallicPacking;   // default R
	ChannelPacking _roughnessPacking;  // default G
	ChannelPacking _aoPacking;         // default B
	ChannelPacking _opacityPacking;    // default A (or R if A not present)

	bool _isGLTFMaterial = false; // Whether this material was loaded from a glTF file
};

#endif // GLMATERIAL_H
