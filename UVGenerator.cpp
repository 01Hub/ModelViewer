#include "UVGenerator.h"

void UVGenerator::generateUVForMesh(
    std::vector<Vertex>& vertices,
    const MeshAnalysis::AnalysisResult& analysis,
    const UVConfig& config)
{

    for (auto& vertex : vertices)
    {
        generateUVForSurface(vertex, analysis, config);
    }
}

void UVGenerator::generateUVForSurface(
    Vertex& vertex,
    const MeshAnalysis::AnalysisResult& analysis,
    const UVConfig& config)
{

    switch (analysis.surfaceType)
    {
    case MeshAnalysis::SurfaceType::SPHERICAL:
        generateSphericalUV(vertex, analysis.centroid, config);
        break;

    case MeshAnalysis::SurfaceType::CYLINDRICAL:
        if (analysis.dominantAxis.has_value())
        {
            generateCylindricalUV(vertex, analysis.dominantAxis.value(), analysis.boundingBox, config);
        }
        else
        {
            generateAdaptiveUV(vertex, analysis.boundingBox, config);
        }
        break;

    case MeshAnalysis::SurfaceType::PLANAR:
        generatePlanarUV(vertex, vertex.Normal, analysis.boundingBox, config);
        break;

    case MeshAnalysis::SurfaceType::MIXED:
    default:
        generateAdaptiveUV(vertex, analysis.boundingBox, config);
        break;
    }
}

void UVGenerator::generateSphericalUV(
    Vertex& vertex,
    const glm::vec3& center,
    const UVConfig& config)
{

    glm::vec2 sphericalUV = cartesianToSpherical(vertex.Position, center);

    // Apply scaling
    sphericalUV *= config.sphericalScale;

    // Handle seams for spherical mapping
    if (config.seamlessSpherical)
    {
        // Ensure U coordinate is in [0, 1] range
        sphericalUV.x = fmod(sphericalUV.x + 1.0f, 1.0f);
    }

    if (config.flipV)
    {
        sphericalUV.y = 1.0f - sphericalUV.y;
    }

    vertex.TexCoords = sphericalUV;
}

void UVGenerator::generateCylindricalUV(
    Vertex& vertex,
    const glm::vec3& axis,
    const MeshAnalysis::BoundingBox& bbox,
    const UVConfig& config)
{

    glm::vec2 cylindricalUV = cartesianToCylindrical(vertex.Position, axis, bbox);

    // Apply scaling and offset
    cylindricalUV.x = cylindricalUV.x * config.cylindricalScale + config.cylindricalOffset;
    cylindricalUV.y *= config.cylindricalScale;

    // Wrap U coordinate
    cylindricalUV.x = fmod(cylindricalUV.x + 1.0f, 1.0f);

    if (config.flipV)
    {
        cylindricalUV.y = 1.0f - cylindricalUV.y;
    }

    vertex.TexCoords = cylindricalUV;
}

void UVGenerator::generatePlanarUV(
    Vertex& vertex,
    const glm::vec3& normal,
    const MeshAnalysis::BoundingBox& bbox,
    const UVConfig& config)
{

    glm::vec2 planarUV = projectToPlanar(vertex.Position, normal, bbox);

    // Apply scaling
    planarUV *= config.planarScale;

    if (config.flipV)
    {
        planarUV.y = 1.0f - planarUV.y;
    }

    vertex.TexCoords = planarUV;
}

void UVGenerator::generateAdaptiveUV(
    Vertex& vertex,
    const MeshAnalysis::BoundingBox& bbox,
    const UVConfig& config)
{

    // Use bounding box projection as fallback
    glm::vec3 size = bbox.getSize();
    glm::vec3 normalizedPos = (vertex.Position - bbox.min) / size;

    // Project to the two largest dimensions
    glm::vec2 adaptiveUV;
    if (size.x >= size.y && size.x >= size.z)
    {
        if (size.y >= size.z)
        {
            adaptiveUV = glm::vec2(normalizedPos.x, normalizedPos.y);
        }
        else
        {
            adaptiveUV = glm::vec2(normalizedPos.x, normalizedPos.z);
        }
    }
    else if (size.y >= size.z)
    {
        adaptiveUV = glm::vec2(normalizedPos.y, normalizedPos.z);
    }
    else
    {
        adaptiveUV = glm::vec2(normalizedPos.x, normalizedPos.z);
    }

    if (config.flipV)
    {
        adaptiveUV.y = 1.0f - adaptiveUV.y;
    }

    vertex.TexCoords = adaptiveUV;
}

// UV Utility Functions
glm::vec2 UVGenerator::cartesianToSpherical(const glm::vec3& pos, const glm::vec3& center)
{
    glm::vec3 dir = glm::normalize(pos - center);

    float u = 0.5f + atan2f(dir.z, dir.x) / (2.0f * glm::pi<float>());
    float v = 0.5f - asinf(dir.y) / glm::pi<float>();

    return glm::vec2(u, v);
}

glm::vec2 UVGenerator::cartesianToCylindrical(const glm::vec3& pos, const glm::vec3& axis, const MeshAnalysis::BoundingBox& bbox)
{
    // Project position onto the plane perpendicular to the axis
    glm::vec3 center = bbox.getCenter();
    glm::vec3 relativePos = pos - center;

    // Remove the component along the axis
    glm::vec3 projected = relativePos - glm::dot(relativePos, axis) * axis;

    // Calculate cylindrical coordinates
    float u = 0.5f + atan2f(projected.z, projected.x) / (2.0f * glm::pi<float>());

    // V coordinate is the position along the axis
    float axisLength = glm::dot(bbox.getSize(), glm::abs(axis));
    float v = (glm::dot(relativePos, axis) + axisLength * 0.5f) / axisLength;

    return glm::vec2(u, glm::clamp(v, 0.0f, 1.0f));
}

glm::vec2 UVGenerator::projectToPlanar(const glm::vec3& pos, const glm::vec3& normal, const MeshAnalysis::BoundingBox& bbox)
{
    // Determine the best projection plane based on normal
    glm::vec3 absNormal = glm::abs(normal);
    glm::vec3 size = bbox.getSize();
    glm::vec3 relativePos = (pos - bbox.min) / size;

    if (absNormal.x > absNormal.y && absNormal.x > absNormal.z)
    {
        // Project to YZ plane
        return glm::vec2(relativePos.y, relativePos.z);
    }
    else if (absNormal.y > absNormal.z)
    {
        // Project to XZ plane
        return glm::vec2(relativePos.x, relativePos.z);
    }
    else
    {
        // Project to XY plane
        return glm::vec2(relativePos.x, relativePos.y);
    }
}
