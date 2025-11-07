#ifndef GLMATERIAL_H
#define GLMATERIAL_H

#include <QVector3D>
#include <QString>

class GLMaterial
{
public:

	friend std::ostream& operator<<(std::ostream& os, const GLMaterial& m);

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

	// Legacy Phong/Blinn properties
	QVector3D ambient() const;
	void setAmbient(const QVector3D& ambient);

	QVector3D diffuse() const;
	void setDiffuse(const QVector3D& diffuse);

	QVector3D specular() const;
	void setSpecular(const QVector3D& specular);

	QVector3D emissive() const;
	void setEmissive(const QVector3D& emissive);

	float shininess() const;
	void setShininess(float shininess);

	bool metallic() const;
	void setMetallic(bool metallic);

	// PBR properties
	QVector3D albedoColor() const;
	void setAlbedoColor(const QVector3D& albedoColor);

	float metalness() const;
	void setMetalness(float metalness);

	float roughness() const;
	void setRoughness(float roughness);

	float opacity() const;
	void setOpacity(float opacity);

	// Enhanced emissive properties
	float emissiveStrength() const;
	void setEmissiveStrength(float strength);

	// Advanced PBR properties
	float ior() const;
	void setIOR(float ior);

	float clearcoat() const;
	void setClearcoat(float clearcoat);

	float clearcoatRoughness() const;
	void setClearcoatRoughness(float roughness);

	QVector3D sheenColor() const;
	void setSheenColor(const QVector3D& color);

	float sheenRoughness() const;
	void setSheenRoughness(float roughness);

	float transmission() const;
	void setTransmission(float transmission);

	// Rendering properties
	ShadingModel shadingModel() const;
	void setShadingModel(ShadingModel model);

	BlendMode blendMode() const;
	void setBlendMode(BlendMode mode);

	bool twoSided() const;
	void setTwoSided(bool twoSided);

	bool wireframe() const;
	void setWireframe(bool wireframe);

	float alphaThreshold() const;
	void setAlphaThreshold(float threshold);

	// Texture slot identifiers (for use with texture manager)
	int albedoTextureId() const;
	void setAlbedoTextureId(int id);

	int metallicTextureId() const;
	void setMetallicTextureId(int id);

	int roughnessTextureId() const;
	void setRoughnessTextureId(int id);

	int normalTextureId() const;
	void setNormalTextureId(int id);

	int emissiveTextureId() const;
	void setEmissiveTextureId(int id);

	int occlusionTextureId() const;
	void setOcclusionTextureId(int id);

	int opacityTextureId() const;
	void setOpacityTextureId(int id);

	int heightTextureId() const;
	void setHeightTextureId(int id);

	int clearcoatColorTextureId() const;
	void setClearcoatColorTextureId(int id);

	int clearcoatRoughnessTextureId() const;
	void setClearcoatRoughnessTextureId(int id);

	int clearcoatNormalTextureId() const;
	void setClearcoatNormalTextureId(int id);

	int sheenColorTextureId() const;
	void setSheenColorTextureId(int id);
	int sheenRoughnessTextureId() const;
	void setSheenRoughnessTextureId(int id);

	int iorTextureId() const;
	void setIORTextureId(int id);

	int transmissionTextureId() const;
	void setTransmissionTextureId(int id);

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
	void setIorTexCoord(int coord) { _iorTexTransform.texCoord = coord; }
	QVector2D iorTexScale() const { return _iorTexTransform.texScale; }
	void setIorTexScale(const QVector2D& scale) { _iorTexTransform.texScale = scale; }
	QVector2D iorTexOffset() const { return _iorTexTransform.texOffset; }
	void setIorTexOffset(const QVector2D& offset) { _iorTexTransform.texOffset = offset; }
	float iorTexRotation() const { return _iorTexTransform.texRotation; }
	void setIorTexRotation(float rotation) { _iorTexTransform.texRotation = rotation; }

	// Transmission texture transform
	int transmissionTexCoord() const { return _transmissionTexTransform.texCoord; }
	void setTransmissionTexCoord(int coord) { _transmissionTexTransform.texCoord = coord; }
	QVector2D transmissionTexScale() const { return _transmissionTexTransform.texScale; }
	void setTransmissionTexScale(const QVector2D& scale) { _transmissionTexTransform.texScale = scale; }
	QVector2D transmissionTexOffset() const { return _transmissionTexTransform.texOffset; }
	void setTransmissionTexOffset(const QVector2D & offset) { _transmissionTexTransform.texOffset = offset; }
	float transmissionTexRotation() const { return _transmissionTexTransform.texRotation; }
	void setTransmissionTexRotation(float rotation) { _transmissionTexTransform.texRotation = rotation; }


	// --- KHR_materials_specular ---
	void setSpecularFactor(float factor) { _specularFactor = factor; }
	float specularFactor() const { return _specularFactor; }
	void setSpecularColorFactor(const QVector3D& color) { _specularColorFactor = color; }
	QVector3D specularColorFactor() const { return _specularColorFactor; }
	
	void setSpecularFactorMap(const QString& path) { _specularFactorMap = path; }
	QString specularFactorMap() const { return _specularFactorMap; }
	bool hasSpecularFactorMap() const { return !_specularFactorMap.isEmpty(); }
	void setSpecularFactorTextureId(unsigned int id) { _specularFactorTextureId = id; }
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
	QString specularColorMap() const { return _specularColorMap; }
	bool hasSpecularColorMap() const { return !_specularColorMap.isEmpty(); }
	void setSpecularColorTextureId(unsigned int id) { _specularColorTextureId = id; }
	unsigned int specularColorTextureId() const { return _specularColorTextureId; }
	void setSpecularColorTexCoord(int coord) { _specularColorTexTransform.texCoord = coord; }
	int specularColorTexCoord() const { return _specularColorTexTransform.texCoord; }
	QVector2D specularColorTexScale() const { return _specularColorTexTransform.texScale; }
	void setSpecularColorTexScale(const QVector2D& scale) { _specularColorTexTransform.texScale = scale; }
	QVector2D specularColorTexOffset() const { return _specularColorTexTransform.texOffset; }
	void setSpecularColorTexOffset(const QVector2D& offset) { _specularColorTexTransform.texOffset = offset; }
	float specularColorTexRotation() const { return _specularColorTexTransform.texRotation; }
	void setSpecularColorTexRotation(float rotation) { _specularColorTexTransform.texRotation = rotation; }

	// --- KHR_materials_anisotropy ---
	void setAnisotropyStrength(float strength) { _anisotropyStrength = strength; }
	float anisotropyStrength() const { return _anisotropyStrength; }
	void setAnisotropyRotation(float rotation) { _anisotropyRotation = rotation; }
	float anisotropyRotation() const { return _anisotropyRotation; }
	void setAnisotropyMap(const QString& path) { _anisotropyMap = path; }
	QString anisotropyMap() const { return _anisotropyMap; }
	bool hasAnisotropyMap() const { return !_anisotropyMap.isEmpty(); }
	void setAnisotropyTextureId(unsigned int id) { _anisotropyTextureId = id; }
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
	QString iridescenceMap() const { return _iridescenceMap; }
	bool hasIridescenceMap() const { return !_iridescenceMap.isEmpty(); }
	void setIridescenceTextureId(unsigned int id) { _iridescenceTextureId = id; }
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
	QString iridescenceThicknessMap() const { return _iridescenceThicknessMap; }
	bool hasIridescenceThicknessMap() const { return !_iridescenceThicknessMap.isEmpty(); }
	void setIridescenceThicknessTextureId(unsigned int id) { _iridescenceThicknessTextureId = id; }
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
	QString thicknessMap() const { return _thicknessMap; }
	bool hasThicknessMap() const { return !_thicknessMap.isEmpty(); }
	void setThicknessTextureId(unsigned int id) { _thicknessTextureId = id; }
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

	

	
	// --- KHR_materials_dispersion ---
	void setDispersion(float dispersion) { _dispersion = dispersion; }
	float dispersion() const { return _dispersion; }

	// --- KHR_materials_unlit ---
	void setUnlit(bool unlit) { _unlit = unlit; }
	bool isUnlit() const { return _unlit; }

	// --- Map path API (used by TextureMappingPanel) ---
	QString albedoMapPath() const;
	void setAlbedoMap(const QString& path);
	void clearAlbedoMap();
	bool hasAlbedoMap() const { return !albedoMapPath().isEmpty(); }	

	void setHasTextureAlpha(bool hasAlpha) { _hasTextureAlpha = hasAlpha; }
	bool hasTextureAlpha() const { return _hasTextureAlpha; } // Check if any assigned texture has alpha channel

	QString normalMapPath() const;
	void setNormalMap(const QString& path);
	void clearNormalMap();
	bool hasNormalMap() const { return !normalMapPath().isEmpty(); }

	QString emissiveMapPath() const;
	void setEmissiveMap(const QString& path);
	void clearEmissiveMap();
	bool hasEmissiveMap() const { return !emissiveMapPath().isEmpty(); }

	QString metallicMapPath() const;
	void setMetallicMap(const QString& path);
	void clearMetallicMap();
	bool hasMetallicMap() const { return !metallicMapPath().isEmpty(); }

	QString roughnessMapPath() const;
	void setRoughnessMap(const QString& path);
	void clearRoughnessMap();
	bool hasRoughnessMap() const { return !roughnessMapPath().isEmpty(); }

	QString aoMapPath() const;
	void setAOMap(const QString& path);
	void clearAOMap();
	bool hasAOMap() const { return !aoMapPath().isEmpty(); }

	QString opacityMapPath() const;
	void setOpacityMap(const QString& path);
	void clearOpacityMap();
	bool hasOpacityMap() const { return !opacityMapPath().isEmpty(); }
	void setInvertOpacityMap(bool invert) { _invertOpacityTexture = invert; }
	bool isOpacityMapInverted() const { return _invertOpacityTexture; }
		
	QString heightMapPath() const;
	void setHeightMap(const QString& path);
	void clearHeightMap();
	bool hasHeightMap() const { return !heightMapPath().isEmpty(); }

	QString transmissionMapPath() const;
	void setTransmissionMap(const QString& path);
	void clearTransmissionMap();
	bool hasTransmissionMap() const { return !transmissionMapPath().isEmpty(); }

	QString iorMapPath() const;
	void setIORMap(const QString& path);
	void clearIORMap();
	bool hasIORMap() const { return !iorMapPath().isEmpty(); }

	QString sheenColorMapPath() const;
	void setSheenColorMap(const QString& path);
	void clearSheenColorMap();
	bool hasSheenColorMap() const { return !sheenColorMapPath().isEmpty(); }

	QString sheenRoughnessMapPath() const;
	void setSheenRoughnessMap(const QString& path);
	void clearSheenRoughnessMap();
	bool hasSheenRoughnessMap() const { return !sheenRoughnessMapPath().isEmpty(); }

	QString clearcoatColorMapPath() const;
	void setClearcoatColorMap(const QString& path);
	void clearClearcoatColorMap();
	bool hasClearcoatColorMap() const { return !clearcoatColorMapPath().isEmpty(); }

	QString clearcoatRoughnessMapPath() const;
	void setClearcoatRoughnessMap(const QString& path);
	void clearClearcoatRoughnessMap();
	bool hasClearcoatRoughnessMap() const { return !clearcoatRoughnessMapPath().isEmpty(); }

	QString clearcoatNormalMapPath() const;
	void setClearcoatNormalMap(const QString& path);
	void clearClearcoatNormalMap();
	bool hasClearcoatNormalMap() const { return !clearcoatNormalMapPath().isEmpty(); }

	// --- Shared UV transform (used by the panel's UV controls) ---
	void setUVTiling(float u, float v);
	void setUVOffset(float u, float v);
	float uvTilingU() const;
	float uvTilingV() const;
	float uvOffsetU() const;
	float uvOffsetV() const;

	ChannelPacking packingFor(const QString& key) const;
	void setPackingFor(const QString& key, const ChannelPacking& p);

	// Convenience methods
	bool isMetallic() const;
	bool isTransparent() const;
	bool isEmissive() const;
	bool hasClearcoat() const;
	bool hasSheen() const;
	bool hasTransmission() const;
	bool hasIOR() const;

	// Material validation
	bool isValid() const;
	void validateAndFix();

	// Conversion utilities
	void convertToBlinnPhong();
	void convertToPBR();
	QVector3D getF0() const; // Get Fresnel reflectance at normal incidence

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

private:
	void setAlbedoFromADS();
	void updateConsistency(); // Ensure consistency between legacy and PBR properties
	void clampValues(); // Ensure all values are within valid ranges
	void ensureADSConsistency(); // Ensure ambient, diffuse, specular are consistent with albedo
	void convertPBRtoADS();
	void assignAutoPackingForPath(const QString& path);


private:
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
	ShadingModel _shadingModel;
	BlendMode _blendMode;
	bool _twoSided;
	bool _wireframe;
	float _alphaThreshold = 0.5f; // For masked blend mode

	bool _hasTextureAlpha = false; // Whether the albedo texture has an alpha channel

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

	// Material-wide UV transform (panel + preview)
	float _uvTilingU = 1.0f, _uvTilingV = 1.0f;
	float _uvOffsetU = 0.0f, _uvOffsetV = 0.0f;

	ChannelPacking _metallicPacking;   // default R
	ChannelPacking _roughnessPacking;  // default G
	ChannelPacking _aoPacking;         // default B
	ChannelPacking _opacityPacking;    // default A (or R if A not present)
	
};

// Inline implementations for performance-critical getters
inline QVector3D GLMaterial::ambient() const { return _ambient; }
inline QVector3D GLMaterial::diffuse() const { return _diffuse; }
inline QVector3D GLMaterial::specular() const { return _specular; }
inline QVector3D GLMaterial::emissive() const { return _emissive; }
inline float GLMaterial::shininess() const { return _shininess; }
inline bool GLMaterial::metallic() const { return _metallic; }

inline QVector3D GLMaterial::albedoColor() const { return _albedoColor; }
inline float GLMaterial::metalness() const { return _metalness; }
inline float GLMaterial::roughness() const { return _roughness; }
inline float GLMaterial::opacity() const { return _opacity; }

inline float GLMaterial::emissiveStrength() const { return _emissiveStrength; }

inline void GLMaterial::setEmissiveStrength(float strength) { _emissiveStrength = strength; }

inline float GLMaterial::ior() const { return _ior; }

inline void GLMaterial::setIOR(float ior) { _ior = ior; }

inline float GLMaterial::clearcoat() const { return _clearcoat; }

inline void GLMaterial::setClearcoat(float clearcoat) { _clearcoat = clearcoat; }

inline float GLMaterial::clearcoatRoughness() const { return _clearcoatRoughness; }

inline void GLMaterial::setClearcoatRoughness(float roughness) { _clearcoatRoughness = roughness; }

inline QVector3D GLMaterial::sheenColor() const { return _sheenColor; }

inline void GLMaterial::setSheenColor(const QVector3D& color) { _sheenColor = color; }

inline float GLMaterial::sheenRoughness() const { return _sheenRoughness; }

inline void GLMaterial::setSheenRoughness(float roughness) { _sheenRoughness = roughness; }

inline float GLMaterial::transmission() const { return _transmission; }

inline void GLMaterial::setTransmission(float transmission) { _transmission = transmission; }

inline GLMaterial::ShadingModel GLMaterial::shadingModel() const { return _shadingModel; }

inline void GLMaterial::setShadingModel(ShadingModel model) { _shadingModel = model; }

inline GLMaterial::BlendMode GLMaterial::blendMode() const { return _blendMode; }

inline void GLMaterial::setBlendMode(BlendMode mode) { _blendMode = mode; }

inline bool GLMaterial::twoSided() const { return _twoSided; }

inline void GLMaterial::setTwoSided(bool twoSided) { _twoSided = twoSided; }

inline bool GLMaterial::wireframe() const { return _wireframe; }

inline void GLMaterial::setWireframe(bool wireframe) { _wireframe = wireframe; }

inline float GLMaterial::alphaThreshold() const { return _alphaThreshold; }

inline void GLMaterial::setAlphaThreshold(float threshold) { _alphaThreshold = threshold; }

inline int GLMaterial::albedoTextureId() const { return _albedoTextureId; }

inline void GLMaterial::setAlbedoTextureId(int id) { _albedoTextureId = id; }

inline int GLMaterial::normalTextureId() const { return _normalTextureId; }

inline void GLMaterial::setNormalTextureId(int id) { _normalTextureId = id; }

inline int GLMaterial::metallicTextureId() const { return _metallicTextureId; }

inline void GLMaterial::setMetallicTextureId(int id) { _metallicTextureId = id; }

inline int GLMaterial::roughnessTextureId() const { return _roughnessTextureId; }

inline void GLMaterial::setRoughnessTextureId(int id) { _roughnessTextureId = id; }

inline int GLMaterial::occlusionTextureId() const { return _occlusionTextureId; }

inline void GLMaterial::setOcclusionTextureId(int id) { _occlusionTextureId = id; }

inline int GLMaterial::opacityTextureId() const { return _opacityTextureId; }

inline void GLMaterial::setOpacityTextureId(int id) { _opacityTextureId = id; }

inline int GLMaterial::heightTextureId() const { return _heightTextureId; }

inline void GLMaterial::setHeightTextureId(int id) { _heightTextureId = id; }

inline int GLMaterial::emissiveTextureId() const { return _emissiveTextureId; }

inline void GLMaterial::setEmissiveTextureId(int id) { _emissiveTextureId = id; }

inline int GLMaterial::clearcoatColorTextureId() const { return _clearcoatColorTextureId; }

inline void GLMaterial::setClearcoatColorTextureId(int id) { _clearcoatColorTextureId = id; }

inline int GLMaterial::clearcoatRoughnessTextureId() const { return _clearcoatRoughnessTextureId; }

inline void GLMaterial::setClearcoatRoughnessTextureId(int id) { _clearcoatRoughnessTextureId = id; }

inline int GLMaterial::clearcoatNormalTextureId() const { return _clearcoatNormalTextureId; }

inline void GLMaterial::setClearcoatNormalTextureId(int id) { _clearcoatNormalTextureId = id; }

inline int GLMaterial::sheenColorTextureId() const { return _sheenColorTextureId; }

inline void GLMaterial::setSheenColorTextureId(int id) { _sheenColorTextureId = id; }

inline int GLMaterial::sheenRoughnessTextureId() const { return _sheenRoughnessTextureId; }

inline void GLMaterial::setSheenRoughnessTextureId(int id) { _sheenRoughnessTextureId = id; }

inline int GLMaterial::iorTextureId() const { return _iorTextureId; }
inline void GLMaterial::setIORTextureId(int id) { _iorTextureId = id; }

inline int GLMaterial::transmissionTextureId() const { return _transmissionTextureId; }

inline void GLMaterial::setTransmissionTextureId(int id) { _transmissionTextureId = id; }

// Albedo
inline QString GLMaterial::albedoMapPath() const { return _albedoMapPath; }
inline void GLMaterial::setAlbedoMap(const QString& path) { _albedoMapPath = path; /* optional: _albedoTextureId = -1; */ }
inline void GLMaterial::clearAlbedoMap() { _albedoMapPath.clear(); /* optional: _albedoTextureId = -1; */ }

// Metallic (separate logical slot; we also have a packed MetallicRoughness texture id)
inline QString GLMaterial::metallicMapPath() const { return _metallicMapPath; }
inline void GLMaterial::setMetallicMap(const QString& path) 
{
	_metallicMapPath = path; 
	assignAutoPackingForPath(path);
}
inline void GLMaterial::clearMetallicMap() { _metallicMapPath.clear(); }

// Roughness
inline QString GLMaterial::roughnessMapPath() const { return _roughnessMapPath; }
inline void GLMaterial::setRoughnessMap(const QString& path) 
{ 
	_roughnessMapPath = path; 
	assignAutoPackingForPath(path);
}
inline void GLMaterial::clearRoughnessMap() { _roughnessMapPath.clear(); }

// Normal
inline QString GLMaterial::normalMapPath() const { return _normalMapPath; }
inline void GLMaterial::setNormalMap(const QString& path) { _normalMapPath = path; /* _normalTextureId = -1; */ }
inline void GLMaterial::clearNormalMap() { _normalMapPath.clear(); /* _normalTextureId = -1; */ }

// AO
inline QString GLMaterial::aoMapPath() const { return _aoMapPath; }
inline void GLMaterial::setAOMap(const QString& path) 
{ 
	_aoMapPath = path; 
	assignAutoPackingForPath(path);
}
inline void GLMaterial::clearAOMap() { _aoMapPath.clear(); }

// Height (for parallax mapping, optional)
inline QString GLMaterial::heightMapPath() const { return _heightMapPath; }
inline void GLMaterial::setHeightMap(const QString& path) { _heightMapPath = path; /* _heightTextureId = -1; */ }
inline void GLMaterial::clearHeightMap() { _heightMapPath.clear(); /* _heightTextureId = -1; */ }

// Opacity
inline QString GLMaterial::opacityMapPath() const { return _opacityMapPath; }
inline void GLMaterial::setOpacityMap(const QString& path) 
{ 
	_opacityMapPath = path; 
	assignAutoPackingForPath(path);
}
inline void GLMaterial::clearOpacityMap() { _opacityMapPath.clear(); }

// Emissive
inline QString GLMaterial::emissiveMapPath() const { return _emissiveMapPath; }
inline void GLMaterial::setEmissiveMap(const QString& path) { _emissiveMapPath = path; /* _emissiveTextureId = -1; */ }
inline void GLMaterial::clearEmissiveMap() { _emissiveMapPath.clear(); /* _emissiveTextureId = -1; */ }

// Transmission
inline QString GLMaterial::transmissionMapPath() const { return _transmissionMapPath; }
inline void GLMaterial::setTransmissionMap(const QString& path) { _transmissionMapPath = path; /* _transmissionTextureId = -1; */ }
inline void GLMaterial::clearTransmissionMap() { _transmissionMapPath.clear(); /* _transmissionTextureId = -1; */ }

// IOR
inline QString GLMaterial::iorMapPath() const { return _iorMapPath; }
inline void GLMaterial::setIORMap(const QString& path) { _iorMapPath = path; }
inline void GLMaterial::clearIORMap() { _iorMapPath.clear(); }

// Sheen
inline QString GLMaterial::sheenColorMapPath() const { return _sheenColorMapPath; }
inline void GLMaterial::setSheenColorMap(const QString& path) { _sheenColorMapPath = path; }
inline void GLMaterial::clearSheenColorMap() { _sheenColorMapPath.clear(); }

inline QString GLMaterial::sheenRoughnessMapPath() const { return _sheenRoughnessMapPath; }
inline void GLMaterial::setSheenRoughnessMap(const QString& path) { _sheenRoughnessMapPath = path; }
inline void GLMaterial::clearSheenRoughnessMap() { _sheenRoughnessMapPath.clear(); }

// Clearcoat
inline QString GLMaterial::clearcoatColorMapPath() const { return _clearcoatColorMapPath; }
inline void GLMaterial::setClearcoatColorMap(const QString& path) { _clearcoatColorMapPath = path; /* _clearcoatTextureId = -1; */ }
inline void GLMaterial::clearClearcoatColorMap() { _clearcoatColorMapPath.clear(); }

inline QString GLMaterial::clearcoatRoughnessMapPath() const { return _clearcoatRoughnessMapPath; }
inline void GLMaterial::setClearcoatRoughnessMap(const QString& path) { _clearcoatRoughnessMapPath = path; }
inline void GLMaterial::clearClearcoatRoughnessMap() { _clearcoatRoughnessMapPath.clear(); }

inline QString GLMaterial::clearcoatNormalMapPath() const { return _clearcoatNormalMapPath; }
inline void GLMaterial::setClearcoatNormalMap(const QString& path) { _clearcoatNormalMapPath = path; }
inline void GLMaterial::clearClearcoatNormalMap() { _clearcoatNormalMapPath.clear(); }

inline void GLMaterial::setUVTiling(float u, float v) { _uvTilingU = u; _uvTilingV = v; }
inline void GLMaterial::setUVOffset(float u, float v) { _uvOffsetU = u; _uvOffsetV = v; }
inline float GLMaterial::uvTilingU() const { return _uvTilingU; }
inline float GLMaterial::uvTilingV() const { return _uvTilingV; }
inline float GLMaterial::uvOffsetU() const { return _uvOffsetU; }
inline float GLMaterial::uvOffsetV() const { return _uvOffsetV; }

inline bool GLMaterial::isMetallic() const
{
	return _metallic;
}

inline bool GLMaterial::isTransparent() const
{
	return _opacity < 1.0f || _transmission > 0.0f ||
		_blendMode == BlendMode::Alpha || _blendMode == BlendMode::Additive;
}

inline bool GLMaterial::isEmissive() const
{
	return _emissive.length() > 0.0f && _emissiveStrength > 0.0f;
}

inline bool GLMaterial::hasClearcoat() const { return _clearcoat > 0.0f; }
inline bool GLMaterial::hasSheen() const { return _sheenColor.length() > 0.0f; }
inline bool GLMaterial::hasTransmission() const { return _transmission > 0.0f; }
inline bool GLMaterial::hasIOR() const { return _ior > 0.0f; }

inline bool GLMaterial::isValid() const
{
	// Check if all material properties are within valid ranges

	// Check color components (should be 0.0 to 1.0)
	auto isColorValid = [](const QVector3D& color) {
		return color.x() >= 0.0f && color.x() <= 1.0f &&
			color.y() >= 0.0f && color.y() <= 1.0f &&
			color.z() >= 0.0f && color.z() <= 1.0f;
		};

	// Traditional Phong/Blinn-Phong checks
	if (!isColorValid(_ambient)) return false;
	if (!isColorValid(_diffuse)) return false;
	if (!isColorValid(_specular)) return false;
	if (!isColorValid(_emissive)) return false;

	// Shininess should be positive
	if (_shininess < 0.0f || _shininess > 128.0f) return false;

	// PBR material checks
	if (!isColorValid(_albedoColor)) return false;
	if (!isColorValid(_emissive)) return false;

	// PBR parameters should be in valid ranges
	if (_metalness < 0.0f || _metalness > 1.0f) return false;
	if (_roughness < 0.0f || _roughness > 1.0f) return false;
	if (_opacity < 0.0f || _opacity > 1.0f) return false;
	if (_transmission < 0.0f || _transmission > 1.0f) return false;
	if (_ior < 1.0f || _ior > 3.0f) return false; // Reasonable IOR range

	return true;
}

inline void GLMaterial::validateAndFix()
{
	// Clamp color components to valid range [0.0, 1.0]
	auto clampColor = [](float& component) {
		component = qBound(0.0f, component, 1.0f);
		};

	// Fix traditional material properties (assuming they are QVector3D members)
	_ambient.setX(qBound(0.0f, _ambient.x(), 1.0f));
	_ambient.setY(qBound(0.0f, _ambient.y(), 1.0f));
	_ambient.setZ(qBound(0.0f, _ambient.z(), 1.0f));

	_diffuse.setX(qBound(0.0f, _diffuse.x(), 1.0f));
	_diffuse.setY(qBound(0.0f, _diffuse.y(), 1.0f));
	_diffuse.setZ(qBound(0.0f, _diffuse.z(), 1.0f));

	_specular.setX(qBound(0.0f, _specular.x(), 1.0f));
	_specular.setY(qBound(0.0f, _specular.y(), 1.0f));
	_specular.setZ(qBound(0.0f, _specular.z(), 1.0f));

	_emissive.setX(qBound(0.0f, _emissive.x(), 1.0f));
	_emissive.setY(qBound(0.0f, _emissive.y(), 1.0f));
	_emissive.setZ(qBound(0.0f, _emissive.z(), 1.0f));

	// Fix shininess (0 to 128 for OpenGL)
	_shininess = qBound(0.0f, _shininess, 128.0f);

	// Fix PBR properties
	_albedoColor.setZ(qBound(0.0f, _albedoColor.z(), 1.0f));
	_albedoColor.setX(qBound(0.0f, _albedoColor.x(), 1.0f));
	_albedoColor.setY(qBound(0.0f, _albedoColor.y(), 1.0f));

	_emissive.setX(qBound(0.0f, _emissive.x(), 1.0f));
	_emissive.setY(qBound(0.0f, _emissive.y(), 1.0f));
	_emissive.setZ(qBound(0.0f, _emissive.z(), 1.0f));

	// Clamp PBR parameters
	_metalness = qBound(0.0f, _metalness, 1.0f);
	_roughness = qBound(0.04f, _roughness, 1.0f); // Minimum 0.04 to prevent artifacts
	_opacity = qBound(0.0f, _opacity, 1.0f);
	_transmission = qBound(0.0f, _transmission, 1.0f);
	_ior = qBound(1.0f, _ior, 3.0f);
}

inline void GLMaterial::convertToBlinnPhong()
{
	// Convert PBR material to Blinn-Phong approximation

	// Use albedo as diffuse color
	_diffuse = _albedoColor;

	// Set ambient to a fraction of diffuse (typically 0.1-0.3)
	float ambientFactor = 0.2f;
	_ambient = _diffuse * ambientFactor;

	// Convert metalness and roughness to specular properties
	if (_metalness > 0.5f)
	{
		// Metallic: use albedo as specular color, reduce diffuse
		_specular = _albedoColor * 0.9f;

		// Metals have very little diffuse reflection
		_diffuse *= (1.0f - _metalness) * 0.1f;
	}
	else
	{
		// Dielectric: white/gray specular, keep diffuse
		float specularIntensity = 0.04f + (1.0f - _roughness) * 0.96f;
		_specular = QVector3D(specularIntensity, specularIntensity, specularIntensity);
	}

	// Convert roughness to shininess (inverse relationship)
	// Roughness 0.0 = shininess 128, roughness 1.0 = shininess 1
	_shininess = (1.0f - _roughness) * 127.0f + 1.0f;

	// Set shading model to traditional
	_shadingModel = ShadingModel::BlinnPhong;
}

inline void GLMaterial::convertToPBR()
{
	// Convert Blinn-Phong material to PBR approximation

	// Use diffuse as base albedo
	_albedoColor = _diffuse;

	// Determine if material is metallic based on specular color
	float specularLuminance = 0.299f * _specular.x() + 0.587f * _specular.y() + 0.114f * _specular.z();
	float diffuseLuminance = 0.299f * _diffuse.x() + 0.587f * _diffuse.y() + 0.114f * _diffuse.z();

	if (specularLuminance > 0.9f && diffuseLuminance < 0.1f)
	{
		// Likely metallic - high specular, low diffuse
		_metalness = 1.0f;
		_albedoColor = _specular;
	}
	else if (specularLuminance > 0.5f)
	{
		// Partially metallic
		_metalness = qBound(0.0f, (specularLuminance - 0.04f) / 0.96f, 1.0f);
		// Blend between diffuse and specular for albedo
		_albedoColor = _diffuse * (1.0f - _metalness) + _specular * _metalness;
	}
	else
	{
		// Dielectric material
		_metalness = 0.0f;
		_albedoColor = _diffuse;
	}

	// Convert shininess to roughness (inverse relationship)
	// Shininess 128 = roughness 0.04, shininess 1 = roughness 1.0
	_roughness = 1.0f - ((_shininess - 1.0f) / 127.0f);
	_roughness = qBound(0.04f, _roughness, 1.0f);


	// Set reasonable defaults for other PBR properties
	_transmission = 0.0f;
	_ior = 1.5f; // Common default IOR

	// Set shading model to PBR
	_shadingModel = ShadingModel::PBR;
}

inline QVector3D GLMaterial::getF0() const
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

#endif // GLMATERIAL_H
