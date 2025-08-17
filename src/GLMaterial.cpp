#include "GLMaterial.h"
#include <cmath>

GLMaterial::GLMaterial()
{
	*this = DEFAULT_MAT();
}

GLMaterial::GLMaterial(QVector3D ambient, QVector3D diffuse, QVector3D specular, QVector3D emissive, float shininess, bool metallic, float opacity)
{
	_ambient.setX(std::clamp(ambient.x(), 0.0f, 1.0f));
	_ambient.setY(std::clamp(ambient.y(), 0.0f, 1.0f));
	_ambient.setZ(std::clamp(ambient.z(), 0.0f, 1.0f));

	_diffuse.setX(std::clamp(diffuse.x(), 0.0f, 1.0f));
	_diffuse.setY(std::clamp(diffuse.y(), 0.0f, 1.0f));
	_diffuse.setZ(std::clamp(diffuse.z(), 0.0f, 1.0f));

	_specular.setX(std::clamp(specular.x(), 0.0f, 1.0f));
	_specular.setY(std::clamp(specular.y(), 0.0f, 1.0f));
	_specular.setZ(std::clamp(specular.z(), 0.0f, 1.0f));

	_emissive.setX(std::clamp(emissive.x(), 0.0f, 1.0f));
	_emissive.setY(std::clamp(emissive.y(), 0.0f, 1.0f));
	_emissive.setZ(std::clamp(emissive.z(), 0.0f, 1.0f));

	_shininess = shininess;
	_metallic = metallic;
	_opacity = opacity;
	_metalness = metallic ? 1.0f : 0.0f;
	_roughness = 0.5f;
	_albedoColor = diffuse; // Use diffuse as albedo color
	_emissiveStrength = 1.0f; // Default emissive strength
	_ior = 1.5f; // Default index of refraction
	_clearcoat = 0.0f; // Default clearcoat
	_clearcoatRoughness = 0.0f; // Default clearcoat roughness
	_sheenColor = QVector3D(0.0f, 0.0f, 0.0f); // Default sheen color
	_sheenRoughness = 0.0f; // Default
	_transmission = 0.0f; // Default transmission
	_shadingModel = ShadingModel::BlinnPhong; // Default shading model
	_blendMode = BlendMode::Opaque; // Default blend mode
	_twoSided = false; // Default two-sided rendering
	_wireframe = false; // Default wireframe rendering
	_alphaThreshold = 0.5f; // Default alpha threshold for masked blend mode
	// Ensure all values are within valid ranges
	clampValues();
	// Fix shininess (0 to 128 for OpenGL)
	_shininess = std::clamp(_shininess, 0.0f, 128.0f);
	// Set albedo from diffuse
	setAlbedoFromADS();
	// Update consistency between legacy and PBR properties
	updateConsistency();
}

GLMaterial::GLMaterial(QVector3D albedo, float metalness, float roughness, float opacity)
{
	_albedoColor.setX(std::clamp(albedo.x(), 0.0f, 1.0f));
	_albedoColor.setY(std::clamp(albedo.y(), 0.0f, 1.0f));
	_albedoColor.setZ(std::clamp(albedo.z(), 0.0f, 1.0f));

	_metalness = metalness;
	_roughness = roughness;
	_opacity = opacity;
	_metallic = _metalness > 0.5f ? true : false;
	_shininess = 125 * _metalness;
	_ambient = _albedoColor * 0.2f; // Set ambient to a fraction of albedo
	_diffuse = _albedoColor; // Use albedo as diffuse color
	_specular = QVector3D(0.04f, 0.04f, 0.04f); // Default specular for dielectrics
	_emissive = QVector3D(0.0f, 0.0f, 0.0f); // Default emissive
	_emissiveStrength = 1.0f; // Default emissive strength
	_ior = 1.5f; // Default index of refraction
	_clearcoat = 0.0f; // Default clearcoat
	_clearcoatRoughness = 0.0f; // Default clearcoat roughness
	_sheenColor = QVector3D(0.0f, 0.0f, 0.0f); // Default sheen color
	_sheenRoughness = 0.0f; // Default
	_transmission = 0.0f; // Default transmission
	_shadingModel = ShadingModel::PBR; // Default shading model
	_blendMode = BlendMode::Opaque; // Default blend mode
	_twoSided = false; // Default two-sided rendering
	_wireframe = false; // Default wireframe rendering
	clampValues();
	// Fix shininess (0 to 128 for OpenGL)
	_shininess = std::clamp(_shininess, 0.0f, 128.0f);
	// Set albedo from diffuse
	setAlbedoFromADS();
	updateConsistency();
 }



void GLMaterial::setAmbient(const QVector3D& ambient)
{
	_ambient.setX(std::clamp(ambient.x(), 0.0f, 1.0f));
	_ambient.setY(std::clamp(ambient.y(), 0.0f, 1.0f));
	_ambient.setZ(std::clamp(ambient.z(), 0.0f, 1.0f));
}


void GLMaterial::setDiffuse(const QVector3D& diffuse)
{
	_diffuse.setX(std::clamp(diffuse.x(), 0.0f, 1.0f));
	_diffuse.setY(std::clamp(diffuse.y(), 0.0f, 1.0f));
	_diffuse.setZ(std::clamp(diffuse.z(), 0.0f, 1.0f));
}


void GLMaterial::setSpecular(const QVector3D& specular)
{
	_specular.setX(std::clamp(specular.x(), 0.0f, 1.0f));
	_specular.setY(std::clamp(specular.y(), 0.0f, 1.0f));
	_specular.setZ(std::clamp(specular.z(), 0.0f, 1.0f));
}


void GLMaterial::setEmissive(const QVector3D& emissive)
{
	_emissive.setX(std::clamp(emissive.x(), 0.0f, 1.0f));
	_emissive.setY(std::clamp(emissive.y(), 0.0f, 1.0f));
	_emissive.setZ(std::clamp(emissive.z(), 0.0f, 1.0f));
}


void GLMaterial::setShininess(float shininess)
{
	_shininess = shininess;
}



void GLMaterial::setMetallic(bool metallic)
{
	_metallic = metallic;
}

void GLMaterial::setAlbedoColor(const QVector3D& albedoColor)
{
	_albedoColor.setX(std::clamp(albedoColor.x(), 0.0f, 1.0f));
	_albedoColor.setY(std::clamp(albedoColor.y(), 0.0f, 1.0f));
	_albedoColor.setZ(std::clamp(albedoColor.z(), 0.0f, 1.0f));
}



void GLMaterial::setMetalness(float metalness)
{
	_metalness = metalness;
}


void GLMaterial::setRoughness(float roughness)
{
	_roughness = roughness;
}


void GLMaterial::setOpacity(float opacity)
{
	_opacity = opacity;
}

GLMaterial GLMaterial::getPredefinedMaterial(GLMaterial::PredefinedMaterials type)
{
	switch (type)
	{
	case PredefinedMaterials::BRASS:
		return METAL_BRASS();
		break;
	case PredefinedMaterials::BRONZE:
		return METAL_BRONZE();
		break;
	case PredefinedMaterials::COPPER:
		return METAL_COPPER();
		break;
	case PredefinedMaterials::GOLD:
		return METAL_GOLD();
		break;
	case PredefinedMaterials::SILVER:
		return METAL_SILVER();
		break;
	case PredefinedMaterials::CHROME:
		return METAL_CHROME();
		break;
	case PredefinedMaterials::RUBY:
		return STONE_RUBY();
		break;
	case PredefinedMaterials::EMERALD:
		return STONE_EMERALD();
		break;
	case PredefinedMaterials::TURQUOISE:
		return STONE_TURQUOISE();
		break;
	case PredefinedMaterials::PEARL:
		return STONE_PEARL();
		break;
	case PredefinedMaterials::JADE:
		return STONE_JADE();
		break;
	case PredefinedMaterials::OBSIDIAN:
		return STONE_OBSIDIAN();
		break;
	case PredefinedMaterials::RED_PLASTIC:
		return RED_PLASTIC();
		break;
	case PredefinedMaterials::GREEN_PLASTIC:
		return GREEN_PLASTIC();
		break;
	case PredefinedMaterials::CYAN_PLASTIC:
		return CYAN_PLASTIC();
		break;
	case PredefinedMaterials::YELLOW_PLASTIC:
		return YELLOW_PLASTIC();
		break;
	case PredefinedMaterials::WHITE_PLASTIC:
		return WHITE_PLASTIC();
		break;
	case PredefinedMaterials::BLACK_PLASTIC:
		return BLACK_PLASTIC();
		break;
	case PredefinedMaterials::RED_RUBBER:
		return RED_RUBBER();
		break;
	case PredefinedMaterials::GREEN_RUBBER:
		return GREEN_RUBBER();
		break;
	case PredefinedMaterials::CYAN_RUBBER:
		return CYAN_RUBBER();
		break;
	case PredefinedMaterials::YELLOW_RUBBER:
		return YELLOW_RUBBER();
		break;
	case PredefinedMaterials::WHITE_RUBBER:
		return WHITE_RUBBER();
		break;
	case PredefinedMaterials::BLACK_RUBBER:
		return BLACK_RUBBER();
		break;
	default:
		return DEFAULT_MAT();
		break;
	}
}

GLMaterial GLMaterial::METAL_BRASS()
{
    GLMaterial mat({ 0.329412f, 0.223529f, 0.027451f },
        { 0.780392f, 0.568627f, 0.113725f },
        { 0.992157f, 0.941176f, 0.807843f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.21794872),
        true,
        1.0f);

    // Corrected PBR properties for brass
    mat.setAlbedoColor(mat.diffuse());
    mat.setMetalness(1.0f);
    mat.setRoughness(0.3f); // Brass is typically smoother than 0.65
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(0.47f); // Brass complex IOR (real part)

    return mat;
}

GLMaterial GLMaterial::METAL_BRONZE()
{
    GLMaterial mat({ 0.2125f, 0.1275f, 0.054f },
        { 0.714f, 0.4284f, 0.18144f },
        { 0.393548f, 0.271906f, 0.166721f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.2),
        true,
        1.0f);

    mat.setAlbedoColor(mat.diffuse());
    mat.setMetalness(1.0f);
    mat.setRoughness(0.4f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.18f);

    return mat;
}

GLMaterial GLMaterial::METAL_COPPER()
{
    GLMaterial mat({ 0.19125f, 0.0735f, 0.0225f },
        { 0.7038f, 0.27048f, 0.0828f },
        { 0.256777f, 0.137622f, 0.086014f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.1),
        true,
        1.0f);

    mat.setAlbedoColor(mat.diffuse());
    mat.setMetalness(1.0f);
    mat.setRoughness(0.25f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(0.617f);

    return mat;
}

GLMaterial GLMaterial::METAL_GOLD()
{
    GLMaterial mat({ 0.24725f, 0.1995f, 0.0745f },
        { 0.75164f, 0.60648f, 0.22648f },
        { 0.628281f, 0.555802f, 0.366065f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.4),
        true,
        1.0f);

    mat.setAlbedoColor(mat.diffuse());
    mat.setMetalness(1.0f);
    mat.setRoughness(0.1f); // Gold is very smooth when polished
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(0.47f);

    return mat;
}

GLMaterial GLMaterial::METAL_SILVER()
{
    GLMaterial mat({ 0.19225f, 0.19225f, 0.19225f },
        { 0.50754f, 0.50654f, 0.50754f },
        { 0.508273f, 0.508273f, 0.508273f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.4),
        true,
        1.0f);

    mat.setAlbedoColor(mat.diffuse());
    mat.setMetalness(1.0f);
    mat.setRoughness(0.05f); // Silver is very reflective
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(0.155f);

    return mat;
}

GLMaterial GLMaterial::METAL_CHROME()
{
    GLMaterial mat({ 0.25f, 0.25f, 0.25f },
        { 0.4f, 0.4f, 0.4f },
        { 0.774597f, 0.774597f, 0.774597f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.6),
        true,
        1.0f);

    mat.setAlbedoColor(mat.diffuse());
    mat.setMetalness(1.0f);
    mat.setRoughness(0.02f); // Chrome is extremely smooth
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(4.1f);

    return mat;
}

GLMaterial GLMaterial::METAL_STEEL()
{
    GLMaterial mat({ 0.25f, 0.25f, 0.25f },
        { 0.4f, 0.4f, 0.4f },
        { 0.774597f, 0.774597f, 0.774597f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.6),
        true,
        1.0f);

    mat.setAlbedoColor(mat.diffuse());
    mat.setMetalness(1.0f);
    mat.setRoughness(0.25f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setOpacity(1.0f);
    mat.setTransmission(0.0f);

    return mat;
}

GLMaterial GLMaterial::STONE_RUBY()
{
    GLMaterial mat({ 0.17450f, 0.01175f, 0.01175f },
        { 0.61424f, 0.04136f, 0.04136f },
        { 0.727811f, 0.626959f, 0.626959f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.6),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.7f, 0.1f, 0.1f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.1f); // Gemstones are very smooth
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.77f); // Ruby IOR
    mat.setTransmission(0.1f); // Slight translucency

    return mat;
}

GLMaterial GLMaterial::STONE_EMERALD()
{
    GLMaterial mat({ 0.0215f, 0.1745f, 0.0215f },
        { 0.07568f, 0.61424f, 0.07568f },
        { 0.633000f, 0.727811f, 0.633000f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.6),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.1f, 0.7f, 0.1f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.1f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.57f); // Emerald IOR
    mat.setTransmission(0.15f);

    return mat;
}

GLMaterial GLMaterial::STONE_TURQUOISE()
{
    GLMaterial mat({ 0.1f, 0.18725f, 0.1745f },
        { 0.396f, 0.74151f, 0.69102f },
        { 0.297254f, 0.30829f, 0.306678f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.1),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.4f, 0.8f, 0.7f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.3f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.65f);

    return mat;
}

GLMaterial GLMaterial::STONE_PEARL()
{
    GLMaterial mat({ 0.25000f, 0.20725f, 0.20725f },
        { 1.000f, 0.829f, 0.829f },
        { 0.296648f, 0.296648f, 0.299948f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.088),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(1.0f, 0.95f, 0.9f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.2f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.53f);
    mat.setTransmission(0.05f); // Slight translucency
    mat.setSheenColor(QVector3D(0.8f, 0.8f, 0.9f)); // Pearl luster
    mat.setSheenRoughness(0.1f);

    return mat;
}

GLMaterial GLMaterial::STONE_JADE()
{
    GLMaterial mat({ 0.135f, 0.2225f, 0.1575f },
        { 0.54f, 0.89f, 0.63f },
        { 0.316228f, 0.316228f, 0.316228f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.1),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.4f, 0.8f, 0.4f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.15f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.66f);
    mat.setTransmission(0.2f);

    return mat;
}

GLMaterial GLMaterial::STONE_OBSIDIAN()
{
    GLMaterial mat({ 0.05375f, 0.05f, 0.06625f },
        { 0.18275f, 0.17f, 0.22525f },
        { 0.332741f, 0.328634f, 0.346435f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.3),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.1f, 0.1f, 0.12f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.05f); // Obsidian is very smooth
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.48f);

    return mat;
}

// Plastic materials - corrected roughness values
GLMaterial GLMaterial::RED_PLASTIC()
{
    GLMaterial mat({ 0.0f, 0.0f, 0.0f },
        { 0.5f, 0.0f, 0.0f },
        { 0.7f, 0.6f, 0.6f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.25),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.8f, 0.1f, 0.1f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.6f); // Plastics are typically rougher
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.46f);

    return mat;
}

GLMaterial GLMaterial::GREEN_PLASTIC()
{
    GLMaterial mat({ 0.0f, 0.0f, 0.0f },
        { 0.1f, 0.35f, 0.1f },
        { 0.45f, 0.55f, 0.45f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.25),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.1f, 0.8f, 0.1f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.6f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.46f);

    return mat;
}

GLMaterial GLMaterial::BLUE_PLASTIC()
{	
    GLMaterial mat({ 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.5f },
        { 0.6f, 0.6f, 0.7f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.25),
        false,
        1.0f);
    mat.setAlbedoColor(QVector3D(0.1f, 0.1f, 0.8f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.6f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.46f);
	return mat;
}

GLMaterial GLMaterial::CYAN_PLASTIC()
{
    GLMaterial mat({ 0.0f, 0.1f, 0.06f },
        { 0.0f, 0.50980392f, 0.50980392f },
        { 0.50196078f, 0.50196078f, 0.50196078f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.25),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.1f, 0.8f, 0.8f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.6f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.46f);

    return mat;
}

GLMaterial GLMaterial::YELLOW_PLASTIC()
{
    GLMaterial mat({ 0.0f, 0.0f, 0.0f },
        { 0.5f, 0.5f, 0.0f },
        { 0.6f, 0.6f, 0.5f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.25),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.8f, 0.8f, 0.1f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.6f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.46f);

    return mat;
}

GLMaterial GLMaterial::MAGENTA_PLASTIC()
{
    GLMaterial mat({ 0.0f, 0.0f, 0.0f },
        { 0.5f, 0.0f, 0.5f },
        { 0.6f, 0.5f, 0.6f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.25),
        false,
        1.0f);
    mat.setAlbedoColor(QVector3D(0.8f, 0.1f, 0.8f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.6f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.46f);
	return mat;
}

GLMaterial GLMaterial::WHITE_PLASTIC()
{
    GLMaterial mat({ 0.0f, 0.0f, 0.0f },
        { 0.55f, 0.55f, 0.55f },
        { 0.7f, 0.7f, 0.7f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.25),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.9f, 0.9f, 0.9f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.6f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.46f);

    return mat;
}

GLMaterial GLMaterial::BLACK_PLASTIC()
{
    GLMaterial mat({ 0.0f, 0.0f, 0.0f },
        { 0.01f, 0.01f, 0.01f },
        { 0.5f, 0.5f, 0.5f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.25),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.05f, 0.05f, 0.05f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.6f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.46f);

    return mat;
}

// Rubber materials - keep high roughness
GLMaterial GLMaterial::RED_RUBBER()
{
    GLMaterial mat({ 0.05f, 0.0f, 0.0f },
        { 0.7f, 0.4f, 0.4f },
        { 0.7f, 0.04f, 0.04f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.078125f),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.8f, 0.1f, 0.1f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.9f); // Rubber is very rough
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.52f);

    return mat;
}

GLMaterial GLMaterial::GREEN_RUBBER()
{
    GLMaterial mat({ 0.0f, 0.05f, 0.0f },
        { 0.4f, 0.5f, 0.4f },
        { 0.04f, 0.7f, 0.04f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.078125f),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.1f, 0.8f, 0.1f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.9f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.52f);

    return mat;
}

GLMaterial GLMaterial::BLUE_RUBBER()
{
    GLMaterial mat({ 0.0f, 0.0f, 0.05f },
        { 0.4f, 0.4f, 0.5f },
        { 0.04f, 0.04f, 0.7f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.078125f),
        false,
        1.0f);
    mat.setAlbedoColor(QVector3D(0.1f, 0.1f, 0.8f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.9f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.52f);
	return mat;
}

GLMaterial GLMaterial::CYAN_RUBBER()
{
    GLMaterial mat({ 0.0f, 0.05f, 0.05f },
        { 0.4f, 0.5f, 0.5f },
        { 0.04f, 0.7f, 0.7f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.078125f),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.1f, 0.8f, 0.8f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.9f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.52f);

    return mat;
}

GLMaterial GLMaterial::YELLOW_RUBBER()
{
    GLMaterial mat({ 0.05f, 0.05f, 0.0f },
        { 0.5f, 0.5f, 0.4f },
        { 0.7f, 0.7f, 0.04f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.078125f),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.8f, 0.8f, 0.1f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.9f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.52f);

    return mat;
}

GLMaterial GLMaterial::MAGENTA_RUBBER()
{
    GLMaterial mat({ 0.05f, 0.0f, 0.05f },
        { 0.5f, 0.4f, 0.5f },
        { 0.7f, 0.04f, 0.7f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.078125f),
        false,
        1.0f);
    mat.setAlbedoColor(QVector3D(0.8f, 0.1f, 0.8f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.9f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.52f);
	return mat;
}

GLMaterial GLMaterial::WHITE_RUBBER()
{
    GLMaterial mat({ 0.05f, 0.05f, 0.05f },
        { 0.5f, 0.5f, 0.5f },
        { 0.7f, 0.7f, 0.7f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.078125f),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.9f, 0.9f, 0.9f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.9f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.52f);

    return mat;
}

GLMaterial GLMaterial::BLACK_RUBBER()
{
    GLMaterial mat({ 0.02f, 0.02f, 0.02f },
        { 0.01f, 0.01f, 0.01f },
        { 0.4f, 0.4f, 0.4f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.078125f),
        false,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.05f, 0.05f, 0.05f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.9f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.52f);

    return mat;
}

// === NEW ADDITIONAL MATERIALS ===

GLMaterial GLMaterial::GLASS()
{
    GLMaterial mat({ 0.02f, 0.02f, 0.02f },         // ambient - very low
        { 0.1f, 0.1f, 0.1f },             // diffuse - low for transparency
        { 0.9f, 0.9f, 0.9f },             // specular - high reflectivity
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.8f),              // shininess - very shiny
        false,                            // metallic
        0.1f);                           // opacity - transparent

    mat.setAlbedoColor(QVector3D(0.95f, 0.95f, 0.95f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.05f);
    mat.setTransmission(0.9f);
    mat.setIOR(1.52f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setBlendMode(BlendMode::Alpha);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::WATER()
{
    GLMaterial mat({ 0.01f, 0.02f, 0.04f },         // ambient - very low, slight blue tint
        { 0.1f, 0.3f, 0.5f },             // diffuse - blue tint
        { 0.8f, 0.9f, 1.0f },             // specular - high with blue tint
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.9f),              // shininess - very reflective
        false,                            // metallic
        0.2f);                           // opacity - mostly transparent

    mat.setAlbedoColor(QVector3D(0.3f, 0.7f, 0.9f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.02f);
    mat.setTransmission(0.8f);
    mat.setIOR(1.33f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setBlendMode(BlendMode::Alpha);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::DIAMOND()
{
    GLMaterial mat({ 0.02f, 0.02f, 0.02f },         // ambient - very low
        { 0.9f, 0.9f, 0.9f },             // diffuse - high reflectivity
        { 1.0f, 1.0f, 1.0f },             // specular - maximum reflectivity
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.95f),             // shininess - extremely shiny
        false,                            // metallic
        0.8f);                           // opacity - semi-transparent

    mat.setAlbedoColor(QVector3D(0.98f, 0.98f, 0.98f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.01f);
    mat.setTransmission(0.2f);
    mat.setIOR(2.42f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setBlendMode(BlendMode::Alpha);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::CERAMIC()
{
    GLMaterial mat({ 0.16f, 0.16f, 0.16f },         // ambient
        { 0.8f, 0.8f, 0.8f },             // diffuse
        { 0.6f, 0.6f, 0.6f },             // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.3f),              // shininess
        false,                            // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.9f, 0.9f, 0.9f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.2f);
    mat.setIOR(1.62f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::FABRIC()
{
    GLMaterial mat({ 0.03f, 0.02f, 0.016f },        // ambient
        { 0.6f, 0.4f, 0.3f },             // diffuse
        { 0.1f, 0.1f, 0.1f },             // specular - very low for matte fabric
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.05f),             // shininess - very low
        false,                            // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.7f, 0.5f, 0.4f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.8f);
    mat.setIOR(1.46f);
    mat.setSheenColor(QVector3D(0.8f, 0.8f, 0.8f));
    mat.setSheenRoughness(0.2f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::SKIN()
{
    GLMaterial mat({ 0.16f, 0.12f, 0.08f },         // ambient - warm tones
        { 0.8f, 0.6f, 0.4f },             // diffuse - skin tone
        { 0.3f, 0.3f, 0.3f },             // specular - moderate reflectivity
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.15f),             // shininess - slight sheen
        false,                            // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.9f, 0.7f, 0.5f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.6f);
    mat.setTransmission(0.1f);
    mat.setIOR(1.4f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::PAPER()
{
    GLMaterial mat({ 0.27f, 0.27f, 0.255f },        // ambient - high for diffuse material
        { 0.9f, 0.9f, 0.85f },            // diffuse - off-white
        { 0.05f, 0.05f, 0.05f },          // specular - very low for matte
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.063f),            // shininess - very low
        false,                            // metallic
        0.85f);                          // opacity - slightly translucent

    mat.setAlbedoColor(QVector3D(0.95f, 0.95f, 0.9f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.9f);
    mat.setTransmission(0.05f);
    mat.setIOR(1.3f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::WOOD()
{
    GLMaterial mat({ 0.12f, 0.08f, 0.04f },         // ambient - brown wood tones
        { 0.6f, 0.4f, 0.2f },             // diffuse - wood color
        { 0.1f, 0.1f, 0.1f },             // specular - low reflectivity
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.25f),             // shininess - moderate
        false,                            // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.6f, 0.4f, 0.2f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.7f);
    mat.setIOR(1.4f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::METAL()
{
    GLMaterial mat({ 0.04f, 0.04f, 0.04f },         // ambient - low for metal
        { 0.2f, 0.2f, 0.2f },             // diffuse - low for metal
        { 0.8f, 0.8f, 0.8f },             // specular - high reflectivity
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 1.0f),              // shininess - maximum
        true,                             // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.7f, 0.7f, 0.7f));
    mat.setMetalness(1.0f);
    mat.setRoughness(0.1f);
    mat.setIOR(2.5f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::PLASTIC()
{
    GLMaterial mat({ 0.12f, 0.12f, 0.16f },         // ambient
        { 0.6f, 0.6f, 0.8f },             // diffuse
        { 0.5f, 0.5f, 0.5f },             // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 1.0f),              // shininess - high for glossy plastic
        false,                            // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.6f, 0.6f, 0.8f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.3f);
    mat.setIOR(1.5f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::STONE()
{
    GLMaterial mat({ 0.1f, 0.1f, 0.1f },            // ambient
        { 0.5f, 0.5f, 0.5f },             // diffuse
        { 0.05f, 0.05f, 0.05f },          // specular - very low
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.063f),            // shininess - very dull
        false,                            // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.5f, 0.5f, 0.5f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.9f);
    mat.setIOR(1.45f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::STONE_GRANITE()
{
    GLMaterial mat({ 0.12f, 0.11f, 0.11f },        // ambient
        { 0.6f, 0.55f, 0.55f },          // diffuse  
        { 0.05f, 0.05f, 0.05f },         // specular
        { 0.0f, 0.0f, 0.0f },            // emissive
        fabs(128.0f * 0.09f),             // shininess
        false,                            // metallic
        1.0f);                           // opacity

    // PBR properties
    mat.setAlbedoColor(QVector3D(0.6f, 0.55f, 0.55f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.8f);
    mat.setIOR(1.45f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::STONE_LIMESTONE()
{
    GLMaterial mat({ 0.15f, 0.146f, 0.136f },       // ambient
        { 0.75f, 0.73f, 0.68f },          // diffuse
        { 0.02f, 0.02f, 0.02f },          // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.047f),            // shininess
        false,                            // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.75f, 0.73f, 0.68f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.9f);
    mat.setIOR(1.45f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::STONE_MARBLE()
{
    GLMaterial mat({ 0.18f, 0.18f, 0.18f },         // ambient
        { 0.9f, 0.9f, 0.9f },             // diffuse
        { 0.2f, 0.2f, 0.2f },             // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.25f),             // shininess
        false,                            // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.9f, 0.9f, 0.9f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.5f);
    mat.setIOR(1.45f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::STONE_SLATE()
{
    GLMaterial mat({ 0.03f, 0.036f, 0.044f },       // ambient
        { 0.15f, 0.18f, 0.22f },          // diffuse
        { 0.01f, 0.01f, 0.01f },          // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.031f),            // shininess
        false,                            // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.15f, 0.18f, 0.22f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.95f);
    mat.setIOR(1.45f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::STONE_SANDSTONE()
{
    GLMaterial mat({ 0.152f, 0.128f, 0.096f },      // ambient
        { 0.76f, 0.64f, 0.48f },          // diffuse
        { 0.03f, 0.03f, 0.03f },          // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.078f),            // shininess
        false,                            // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.76f, 0.64f, 0.48f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.85f);
    mat.setIOR(1.45f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::STONE_BASALT()
{
    GLMaterial mat({ 0.02f, 0.02f, 0.02f },         // ambient
        { 0.1f, 0.1f, 0.1f },             // diffuse
        { 0.02f, 0.02f, 0.02f },          // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.047f),            // shininess
        false,                            // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.1f, 0.1f, 0.1f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.9f);
    mat.setIOR(1.45f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::STONE_TRAVERTINE()
{
    GLMaterial mat({ 0.17f, 0.16f, 0.14f },         // ambient
        { 0.85f, 0.8f, 0.7f },            // diffuse
        { 0.02f, 0.02f, 0.02f },          // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.063f),            // shininess
        false,                            // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.85f, 0.8f, 0.7f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.85f);
    mat.setIOR(1.45f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::STONE_QUARTZITE()
{
    GLMaterial mat({ 0.16f, 0.17f, 0.18f },         // ambient
        { 0.8f, 0.85f, 0.9f },            // diffuse
        { 0.3f, 0.3f, 0.3f },             // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.5f),              // shininess
        false,                            // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.8f, 0.85f, 0.9f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.4f);
    mat.setIOR(1.45f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::STONE_SOAPSTONE()
{
    GLMaterial mat({ 0.05f, 0.06f, 0.056f },        // ambient
        { 0.25f, 0.3f, 0.28f },           // diffuse
        { 0.01f, 0.01f, 0.01f },          // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.031f),            // shininess
        false,                            // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.25f, 0.3f, 0.28f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.95f);
    mat.setIOR(1.45f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::METAL_TITANIUM()
{
    GLMaterial mat({ 0.11f, 0.116f, 0.124f },       // ambient
        { 0.55f, 0.58f, 0.62f },          // diffuse
        { 0.7f, 0.7f, 0.7f },             // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.5f),              // shininess
        true,                             // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.55f, 0.58f, 0.62f));
    mat.setMetalness(1.0f);
    mat.setRoughness(0.3f);
    mat.setIOR(2.5f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::METAL_PLATINUM()
{
    GLMaterial mat({ 0.164f, 0.164f, 0.17f },       // ambient
        { 0.82f, 0.82f, 0.85f },          // diffuse
        { 0.9f, 0.9f, 0.9f },             // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.75f),             // shininess
        true,                             // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.82f, 0.82f, 0.85f));
    mat.setMetalness(1.0f);
    mat.setRoughness(0.2f);
    mat.setIOR(2.9f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::METAL_MAGNESIUM()
{
    GLMaterial mat({ 0.18f, 0.18f, 0.19f },         // ambient
        { 0.9f, 0.9f, 0.95f },            // diffuse
        { 0.7f, 0.7f, 0.75f },            // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.31f),             // shininess
        true,                             // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.9f, 0.9f, 0.95f));
    mat.setMetalness(1.0f);
    mat.setRoughness(0.4f);
    mat.setIOR(1.6f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::METAL_ZINC()
{
    GLMaterial mat({ 0.13f, 0.14f, 0.15f },         // ambient
        { 0.65f, 0.7f, 0.75f },           // diffuse
        { 0.7f, 0.7f, 0.7f },             // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.375f),            // shininess
        true,                             // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.65f, 0.7f, 0.75f));
    mat.setMetalness(1.0f);
    mat.setRoughness(0.35f);
    mat.setIOR(1.9f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::METAL_NICKEL()
{
    GLMaterial mat({ 0.144f, 0.144f, 0.148f },      // ambient
        { 0.72f, 0.72f, 0.74f },          // diffuse
        { 0.85f, 0.85f, 0.85f },          // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.625f),            // shininess
        true,                             // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.72f, 0.72f, 0.74f));
    mat.setMetalness(1.0f);
    mat.setRoughness(0.25f);
    mat.setIOR(2.0f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::METAL_ALUMINUM()
{
    GLMaterial mat({ 0.182f, 0.184f, 0.184f },      // ambient
        { 0.91f, 0.92f, 0.92f },          // diffuse
        { 0.95f, 0.95f, 0.95f },          // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.563f),            // shininess
        true,                             // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.91f, 0.92f, 0.92f));
    mat.setMetalness(1.0f);
    mat.setRoughness(0.2f);
    mat.setIOR(1.44f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::METAL_IRON_RAW()
{
    GLMaterial mat({ 0.09f, 0.09f, 0.094f },        // ambient
        { 0.45f, 0.45f, 0.47f },          // diffuse
        { 0.6f, 0.6f, 0.6f },             // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.25f),             // shininess
        true,                             // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.45f, 0.45f, 0.47f));
    mat.setMetalness(1.0f);
    mat.setRoughness(0.5f);
    mat.setIOR(2.25f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::METAL_COBALT()
{
    GLMaterial mat({ 0.08f, 0.09f, 0.12f },         // ambient
        { 0.4f, 0.45f, 0.6f },            // diffuse
        { 0.7f, 0.7f, 0.75f },            // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.406f),            // shininess
        true,                             // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.4f, 0.45f, 0.6f));
    mat.setMetalness(1.0f);
    mat.setRoughness(0.3f);
    mat.setIOR(2.3f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::METAL_PEWTER()
{
    GLMaterial mat({ 0.12f, 0.12f, 0.124f },        // ambient
        { 0.6f, 0.6f, 0.62f },            // diffuse
        { 0.55f, 0.55f, 0.55f },          // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.156f),            // shininess
        true,                             // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.6f, 0.6f, 0.62f));
    mat.setMetalness(1.0f);
    mat.setRoughness(0.6f);
    mat.setIOR(2.1f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}

GLMaterial GLMaterial::METAL_TUNGSTEN()
{
    GLMaterial mat({ 0.06f, 0.06f, 0.066f },        // ambient
        { 0.3f, 0.3f, 0.33f },            // diffuse
        { 0.85f, 0.85f, 0.9f },           // specular
        { 0.0f, 0.0f, 0.0f },             // emissive
        fabs(128.0f * 0.75f),             // shininess
        true,                             // metallic
        1.0f);                           // opacity

    mat.setAlbedoColor(QVector3D(0.3f, 0.3f, 0.33f));
    mat.setMetalness(1.0f);
    mat.setRoughness(0.15f);
    mat.setIOR(3.0f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.updateConsistency();
    return mat;
}


GLMaterial GLMaterial::DEFAULT_MAT()
{    
	return METAL_STEEL(); // Default material set to METAL_STEEL
 //   GLMaterial mat({ 90 / 255.0f, 98 / 255.0f, 115 / 255.0f },
 //       { 175 / 255.0f, 192 / 255.0f, 224 / 255.0f },
 //       { 26 / 255.0f, 26 / 255.0f, 26 / 255.0f },
 //       { 0.0, 0.0, 0.0 },
 //       fabs(128.0 * 0.05f),
 //       false,
 //       1.0f);
	//mat.setAlbedoColor(mat.ambient() + mat.diffuse());
	//mat.setMetalness(1.0f);
	//mat.setRoughness(0.7f);

	//// Additional PBR properties for complete material definition
	//mat.setOpacity(1.0f); // Fully opaque
	//mat.setTransmission(0.0f); // No light transmission
	//mat.setIOR(1.5f); // Standard dielectric IOR
	//mat.setShadingModel(ShadingModel::PBR); // Use PBR shading

	//return mat;
}

void GLMaterial::setAlbedoFromADS()
{
	QVector3D col;
	if (_metallic)
		col = _ambient + _diffuse;
	else
		col = _ambient + _diffuse;
	_albedoColor.setX(std::clamp(col.x(), 0.0f, 1.0f));
	_albedoColor.setY(std::clamp(col.y(), 0.0f, 1.0f));
	_albedoColor.setZ(std::clamp(col.z(), 0.0f, 1.0f));
}

void GLMaterial::updateConsistency()
{
    // Synchronize legacy and PBR properties based on current shading model
    if (_shadingModel == ShadingModel::PBR) {
        // When using PBR, update legacy properties from PBR values
        _diffuse = _albedoColor;
        _ambient = _albedoColor * 0.1f; // Typical ambient factor

        // Convert metalness to legacy metallic boolean
        _metallic = (_metalness > 0.5f);

        // Convert roughness to shininess (inverse relationship)
        _shininess = (1.0f - _roughness) * 128.0f;

        // Update specular based on metalness and albedo
        if (_metalness > 0.5f) {
            _specular = _albedoColor; // Metals use albedo as specular
        } else {
            // Dielectrics typically have low specular values
            float f0 = (_ior - 1.0f) / (_ior + 1.0f);
            f0 = f0 * f0;
            _specular = QVector3D(f0, f0, f0);
        }

        // Sync emissive properties
        _emissive = _emissive * _emissiveStrength;
    }
    else {
        // When using legacy shading, update PBR properties from legacy values
        _albedoColor = _diffuse;

        // Convert shininess to roughness (inverse relationship)
        _roughness = 1.0f - (_shininess / 128.0f);

        // Convert legacy metallic boolean to metalness value
        _metalness = _metallic ? 1.0f : 0.0f;

        // Estimate IOR from specular reflectance (assuming grayscale)
        float specularGray = (_specular.x() + _specular.y() + _specular.z()) / 3.0f;
        if (specularGray > 0.0f) {
            float sqrtF0 = sqrt(specularGray);
            _ior = (1.0f + sqrtF0) / (1.0f - sqrtF0);
        }

        // Extract emissive strength if emissive is non-zero
        float emissiveLength = _emissive.length();
        if (emissiveLength > 0.0f) {
            _emissiveStrength = emissiveLength;
            _emissive = _emissive / emissiveLength; // Normalize color
        }
    }
}

void GLMaterial::clampValues()
{
    // Clamp legacy properties
    _ambient = QVector3D(
        qBound(0.0f, _ambient.x(), 1.0f),
        qBound(0.0f, _ambient.y(), 1.0f),
        qBound(0.0f, _ambient.z(), 1.0f)
    );

    _diffuse = QVector3D(
        qBound(0.0f, _diffuse.x(), 1.0f),
        qBound(0.0f, _diffuse.y(), 1.0f),
        qBound(0.0f, _diffuse.z(), 1.0f)
    );

    _specular = QVector3D(
        qBound(0.0f, _specular.x(), 1.0f),
        qBound(0.0f, _specular.y(), 1.0f),
        qBound(0.0f, _specular.z(), 1.0f)
    );

    // Emissive can exceed 1.0 for HDR, but should be non-negative
    _emissive = QVector3D(
        qMax(0.0f, _emissive.x()),
        qMax(0.0f, _emissive.y()),
        qMax(0.0f, _emissive.z())
    );

    _shininess = qBound(0.0f, _shininess, 1000.0f);

    // Clamp PBR properties
    _albedoColor = QVector3D(
        qBound(0.0f, _albedoColor.x(), 1.0f),
        qBound(0.0f, _albedoColor.y(), 1.0f),
        qBound(0.0f, _albedoColor.z(), 1.0f)
    );

    _metalness = qBound(0.0f, _metalness, 1.0f);
    _roughness = qBound(0.0f, _roughness, 1.0f);
    _opacity = qBound(0.0f, _opacity, 1.0f);

    // Emissive strength can be > 1.0 for HDR
    _emissiveStrength = qMax(0.0f, _emissiveStrength);

    // Advanced PBR properties
    _ior = qBound(1.0f, _ior, 3.0f); // Typical range for common materials
    _clearcoat = qBound(0.0f, _clearcoat, 1.0f);
    _clearcoatRoughness = qBound(0.0f, _clearcoatRoughness, 1.0f);

    _sheenColor = QVector3D(
        qBound(0.0f, _sheenColor.x(), 1.0f),
        qBound(0.0f, _sheenColor.y(), 1.0f),
        qBound(0.0f, _sheenColor.z(), 1.0f)
    );

    _sheenRoughness = qBound(0.0f, _sheenRoughness, 1.0f);
    _transmission = qBound(0.0f, _transmission, 1.0f);

    // Rendering properties
    _alphaThreshold = qBound(0.0f, _alphaThreshold, 1.0f);

    // Texture coordinate indices should be non-negative
    _albedoTexCoord = qMax(0, _albedoTexCoord);
    _normalTexCoord = qMax(0, _normalTexCoord);
    _metallicRoughnessTexCoord = qMax(0, _metallicRoughnessTexCoord);
}

void GLMaterial::ensureADSConsistency()
{
    // This function can be called to update any material that might have
    // incomplete ADS values based on its PBR properties

    if (_shadingModel == ShadingModel::PBR)
    {
        // Update legacy ADS from PBR values
        _diffuse = _albedoColor;
        _ambient = _albedoColor * 0.2f; // Standard ambient factor

        if (_metalness > 0.5f)
        {
            // Metallic materials
            _specular = _albedoColor; // Metals use albedo as specular
            _shininess = (1.0f - _roughness) * 128.0f;
        }
        else
        {
            // Dielectric materials
            float f0 = (_ior - 1.0f) / (_ior + 1.0f);
            f0 = f0 * f0;
            _specular = QVector3D(f0, f0, f0);
            _shininess = (1.0f - _roughness) * 128.0f;
        }
    }
    else
    {
        // Update PBR from legacy ADS values
        _albedoColor = _diffuse;
        _roughness = 1.0f - (_shininess / 128.0f);
        _metalness = _metallic ? 1.0f : 0.0f;

        // Estimate IOR from specular
        float specGray = (_specular.x() + _specular.y() + _specular.z()) / 3.0f;
        if (specGray > 0.01f)
        {
            float sqrtF0 = sqrt(specGray);
            _ior = (1.0f + sqrtF0) / (1.0f - sqrtF0);
        }
    }

    // Ensure all values are properly clamped
    clampValues();
}

#include <QDataStream>
#include <QVector3D>

void GLMaterial::serialize(QDataStream& out) const
{
	// Write Phong/ADS properties
	out << ambient();
	out << diffuse();
	out << specular();
	out << emissive();
	out << shininess();
	out << opacity();

	// Write PBR properties
	out << albedoColor();
	out << metalness();
	out << roughness();

	// Write metallic flag (if you use it)
	out << _metallic;
}

void GLMaterial::deserialize(QDataStream& in)
{
	QVector3D amb, diff, spec, emis, albedo;
	float shin, opac, metal, rough;
	bool metallicFlag;

	in >> amb >> diff >> spec >> emis >> shin >> opac;
	in >> albedo >> metal >> rough;
	in >> metallicFlag;

	setAmbient(amb);
	setDiffuse(diff);
	setSpecular(spec);
	setEmissive(emis);
	setShininess(shin);
	setOpacity(opac);

	setAlbedoColor(albedo);
	setMetalness(metal);
	setRoughness(rough);

	setMetallic(metallicFlag);
}

