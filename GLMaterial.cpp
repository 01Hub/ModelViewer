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
		return BRASS();
		break;
	case PredefinedMaterials::BRONZE:
		return BRONZE();
		break;
	case PredefinedMaterials::COPPER:
		return COPPER();
		break;
	case PredefinedMaterials::GOLD:
		return GOLD();
		break;
	case PredefinedMaterials::SILVER:
		return SILVER();
		break;
	case PredefinedMaterials::CHROME:
		return CHROME();
		break;
	case PredefinedMaterials::RUBY:
		return RUBY();
		break;
	case PredefinedMaterials::EMERALD:
		return EMERALD();
		break;
	case PredefinedMaterials::TURQUOISE:
		return TURQUOISE();
		break;
	case PredefinedMaterials::PEARL:
		return PEARL();
		break;
	case PredefinedMaterials::JADE:
		return JADE();
		break;
	case PredefinedMaterials::OBSIDIAN:
		return OBSIDIAN();
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

GLMaterial GLMaterial::BRASS()
{
    GLMaterial mat({ 0.329412f, 0.223529f, 0.027451f },
        { 0.780392f, 0.568627f, 0.113725f },
        { 0.992157f, 0.941176f, 0.807843f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.21794872),
        true,
        1.0f);

    // Corrected PBR properties for brass
    mat.setAlbedoColor(QVector3D(0.955f, 0.638f, 0.538f)); // Proper brass F0 color
    mat.setMetalness(1.0f);
    mat.setRoughness(0.3f); // Brass is typically smoother than 0.65
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(0.47f); // Brass complex IOR (real part)

    return mat;
}

GLMaterial GLMaterial::BRONZE()
{
    GLMaterial mat({ 0.2125f, 0.1275f, 0.054f },
        { 0.714f, 0.4284f, 0.18144f },
        { 0.393548f, 0.271906f, 0.166721f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.2),
        true,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.804f, 0.498f, 0.196f)); // Proper bronze F0
    mat.setMetalness(1.0f);
    mat.setRoughness(0.4f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(1.18f);

    return mat;
}

GLMaterial GLMaterial::COPPER()
{
    GLMaterial mat({ 0.19125f, 0.0735f, 0.0225f },
        { 0.7038f, 0.27048f, 0.0828f },
        { 0.256777f, 0.137622f, 0.086014f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.1),
        true,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.955f, 0.638f, 0.538f)); // Proper copper F0
    mat.setMetalness(1.0f);
    mat.setRoughness(0.25f);
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(0.617f);

    return mat;
}

GLMaterial GLMaterial::GOLD()
{
    GLMaterial mat({ 0.24725f, 0.1995f, 0.0745f },
        { 0.75164f, 0.60648f, 0.22648f },
        { 0.628281f, 0.555802f, 0.366065f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.4),
        true,
        1.0f);

    mat.setAlbedoColor(QVector3D(1.0f, 0.766f, 0.336f)); // Proper gold F0
    mat.setMetalness(1.0f);
    mat.setRoughness(0.1f); // Gold is very smooth when polished
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(0.47f);

    return mat;
}

GLMaterial GLMaterial::SILVER()
{
    GLMaterial mat({ 0.19225f, 0.19225f, 0.19225f },
        { 0.50754f, 0.50654f, 0.50754f },
        { 0.508273f, 0.508273f, 0.508273f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.4),
        true,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.972f, 0.960f, 0.915f)); // Proper silver F0
    mat.setMetalness(1.0f);
    mat.setRoughness(0.05f); // Silver is very reflective
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(0.155f);

    return mat;
}

GLMaterial GLMaterial::CHROME()
{
    GLMaterial mat({ 0.25f, 0.25f, 0.25f },
        { 0.4f, 0.4f, 0.4f },
        { 0.774597f, 0.774597f, 0.774597f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.6),
        true,
        1.0f);

    mat.setAlbedoColor(QVector3D(0.549f, 0.556f, 0.554f)); // Proper chrome F0
    mat.setMetalness(1.0f);
    mat.setRoughness(0.02f); // Chrome is extremely smooth
    mat.setShadingModel(ShadingModel::PBR);
    mat.setIOR(4.1f);

    return mat;
}

GLMaterial GLMaterial::RUBY()
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

GLMaterial GLMaterial::EMERALD()
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

GLMaterial GLMaterial::TURQUOISE()
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

GLMaterial GLMaterial::PEARL()
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

GLMaterial GLMaterial::JADE()
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

GLMaterial GLMaterial::OBSIDIAN()
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
    GLMaterial mat;

    // Clear glass properties
    mat.setAmbient(QVector3D(0.1f, 0.1f, 0.1f));
    mat.setDiffuse(QVector3D(0.1f, 0.1f, 0.1f));
    mat.setSpecular(QVector3D(0.9f, 0.9f, 0.9f));
    mat.setEmissive(QVector3D(0.0f, 0.0f, 0.0f));
    mat.setShininess(128.0f * 0.8f);
    mat.setMetallic(false);

    // PBR properties
    mat.setAlbedoColor(QVector3D(0.95f, 0.95f, 0.95f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.05f); // Very smooth
    mat.setOpacity(0.1f); // Highly transparent
    mat.setTransmission(0.9f); // High transmission
    mat.setIOR(1.52f); // Standard glass IOR
    mat.setShadingModel(ShadingModel::PBR);
    mat.setBlendMode(BlendMode::Alpha);

    return mat;
}

GLMaterial GLMaterial::WATER()
{
    GLMaterial mat;

    mat.setAmbient(QVector3D(0.05f, 0.1f, 0.2f));
    mat.setDiffuse(QVector3D(0.1f, 0.3f, 0.5f));
    mat.setSpecular(QVector3D(0.8f, 0.9f, 1.0f));
    mat.setEmissive(QVector3D(0.0f, 0.0f, 0.0f));
    mat.setShininess(128.0f * 0.9f);
    mat.setMetallic(false);

    // PBR properties
    mat.setAlbedoColor(QVector3D(0.3f, 0.7f, 0.9f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.02f); // Very smooth surface
    mat.setOpacity(0.2f);
    mat.setTransmission(0.8f);
    mat.setIOR(1.33f); // Water IOR
    mat.setShadingModel(ShadingModel::PBR);
    mat.setBlendMode(BlendMode::Alpha);

    return mat;
}

GLMaterial GLMaterial::DIAMOND()
{
    GLMaterial mat;

    mat.setAmbient(QVector3D(0.1f, 0.1f, 0.1f));
    mat.setDiffuse(QVector3D(0.9f, 0.9f, 0.9f));
    mat.setSpecular(QVector3D(1.0f, 1.0f, 1.0f));
    mat.setEmissive(QVector3D(0.0f, 0.0f, 0.0f));
    mat.setShininess(128.0f * 0.95f);
    mat.setMetallic(false);

    // PBR properties
    mat.setAlbedoColor(QVector3D(0.98f, 0.98f, 0.98f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.01f); // Extremely smooth
    mat.setOpacity(0.8f);
    mat.setTransmission(0.2f);
    mat.setIOR(2.42f); // Diamond IOR
    mat.setShadingModel(ShadingModel::PBR);
    mat.setBlendMode(BlendMode::Alpha);

    return mat;
}

GLMaterial GLMaterial::CERAMIC()
{
    GLMaterial mat;

    mat.setAmbient(QVector3D(0.2f, 0.2f, 0.2f));
    mat.setDiffuse(QVector3D(0.8f, 0.8f, 0.8f));
    mat.setSpecular(QVector3D(0.6f, 0.6f, 0.6f));
    mat.setEmissive(QVector3D(0.0f, 0.0f, 0.0f));
    mat.setShininess(128.0f * 0.3f);
    mat.setMetallic(false);

    // PBR properties
    mat.setAlbedoColor(QVector3D(0.9f, 0.9f, 0.9f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.2f);
    mat.setOpacity(1.0f);
    mat.setIOR(1.62f); // Ceramic IOR
    mat.setShadingModel(ShadingModel::PBR);

    return mat;
}

GLMaterial GLMaterial::FABRIC()
{
    GLMaterial mat;

    mat.setAmbient(QVector3D(0.15f, 0.1f, 0.08f));
    mat.setDiffuse(QVector3D(0.6f, 0.4f, 0.3f));
    mat.setSpecular(QVector3D(0.1f, 0.1f, 0.1f));
    mat.setEmissive(QVector3D(0.0f, 0.0f, 0.0f));
    mat.setShininess(128.0f * 0.05f);
    mat.setMetallic(false);

    // PBR properties with sheen for fabric
    mat.setAlbedoColor(QVector3D(0.7f, 0.5f, 0.4f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.8f);
    mat.setOpacity(1.0f);
    mat.setIOR(1.46f);
    mat.setSheenColor(QVector3D(0.8f, 0.8f, 0.8f)); // Fabric sheen
    mat.setSheenRoughness(0.2f);
    mat.setShadingModel(ShadingModel::PBR);

    return mat;
}

GLMaterial GLMaterial::SKIN()
{
    GLMaterial mat;

    mat.setAmbient(QVector3D(0.2f, 0.15f, 0.1f));
    mat.setDiffuse(QVector3D(0.8f, 0.6f, 0.4f));
    mat.setSpecular(QVector3D(0.3f, 0.3f, 0.3f));
    mat.setEmissive(QVector3D(0.0f, 0.0f, 0.0f));
    mat.setShininess(128.0f * 0.15f);
    mat.setMetallic(false);

    // PBR properties for subsurface scattering approximation
    mat.setAlbedoColor(QVector3D(0.9f, 0.7f, 0.5f));
    mat.setMetalness(0.0f);
    mat.setRoughness(0.6f);
    mat.setOpacity(1.0f);
    mat.setTransmission(0.1f); // Slight subsurface scattering
    mat.setIOR(1.4f); // Skin IOR
    mat.setShadingModel(ShadingModel::PBR);

    return mat;
}

GLMaterial GLMaterial::PAPER()
{
	GLMaterial mat;

	// Traditional properties - paper is matte with very low specular
	mat.setAmbient(QVector3D(0.3f, 0.3f, 0.3f));
	mat.setDiffuse(QVector3D(0.9f, 0.9f, 0.85f));
	mat.setSpecular(QVector3D(0.05f, 0.05f, 0.05f));
	mat.setEmissive(QVector3D(0.0f, 0.0f, 0.0f));
	mat.setShininess(8.0f);
	mat.setMetallic(false);

	// PBR properties - paper is highly diffuse
	mat.setAlbedoColor(QVector3D(0.95f, 0.95f, 0.9f));
	mat.setMetalness(0.0f);
	mat.setRoughness(0.9f); // Very rough surface
	mat.setOpacity(0.85f); // Slightly translucent
	mat.setTransmission(0.05f); // Minimal light transmission
	mat.setIOR(1.3f); // Paper fiber IOR
	mat.setShadingModel(ShadingModel::PBR);

	return mat;
}

GLMaterial GLMaterial::WOOD()
{
	GLMaterial mat;

	mat.setAmbient(QVector3D(0.3f, 0.2f, 0.1f));
	mat.setDiffuse(QVector3D(0.6f, 0.4f, 0.2f));
	mat.setSpecular(QVector3D(0.1f, 0.1f, 0.1f));
	mat.setEmissive(QVector3D(0.0f, 0.0f, 0.0f));
	mat.setShininess(32.0f);
	mat.setMetallic(false);

	mat.setAlbedoColor(QVector3D(0.6f, 0.4f, 0.2f));
	mat.setMetalness(0.0f);
	mat.setRoughness(0.7f);
	mat.setOpacity(1.0f);
	mat.setTransmission(0.0f);
	mat.setIOR(1.4f);
	mat.setShadingModel(ShadingModel::PBR);

	return mat;
}

GLMaterial GLMaterial::DEFAULT_MAT()
{    
    GLMaterial mat({ 90 / 255.0f, 98 / 255.0f, 115 / 255.0f },
        { 175 / 255.0f, 192 / 255.0f, 224 / 255.0f },
        { 26 / 255.0f, 26 / 255.0f, 26 / 255.0f },
        { 0.0, 0.0, 0.0 },
        fabs(128.0 * 0.05f),
        false,
        1.0f);
	mat.setAlbedoColor(mat.ambient() + mat.diffuse());
	mat.setMetalness(1.0f);
	mat.setRoughness(0.7f);

	// Additional PBR properties for complete material definition
	mat.setOpacity(1.0f); // Fully opaque
	mat.setTransmission(0.0f); // No light transmission
	mat.setIOR(1.5f); // Standard dielectric IOR
	mat.setShadingModel(ShadingModel::PBR); // Use PBR shading

	return mat;
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

