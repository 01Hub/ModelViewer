#include "MeshAnalyzer.h"
#include <assimp/mesh.h>
#include <algorithm>
#include <cmath>
#include <cfloat>

MeshAnalysis::AnalysisResult MeshAnalyzer::analyzeMesh(
    const aiMesh* mesh,
    const MeshAnalysis::SamplingConfig& config)
{

    MeshAnalysis::AnalysisResult result;

    if (!validateMesh(mesh))
    {
        return result; // Returns default MIXED type with low confidence
    }

    // Calculate basic geometry properties
    result.boundingBox = calculateBoundingBox(mesh);
    result.centroid = calculateCentroid(mesh);
    result.samplesUsed = calculateSampleCount(mesh->mNumVertices, config);

    // Determine surface type through specialized analysis
    result.surfaceType = determineSurfaceType(mesh, result.boundingBox, result.centroid, config, result);

    // Always check for uniform normals
    result.hasUniformNormals = checkUniformNormals(mesh, config.normalConsistencyThreshold);

    return result;
}

MeshAnalysis::BoundingBox MeshAnalyzer::calculateBoundingBox(const aiMesh* mesh)
{
    MeshAnalysis::BoundingBox bbox;
    bbox.min = glm::vec3(FLT_MAX);
    bbox.max = glm::vec3(-FLT_MAX);

    for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
    {
        const auto& vertex = mesh->mVertices[i];
        const glm::vec3 pos(vertex.x, vertex.y, vertex.z);
        bbox.min = glm::min(bbox.min, pos);
        bbox.max = glm::max(bbox.max, pos);
    }

    return bbox;
}

glm::vec3 MeshAnalyzer::calculateCentroid(const aiMesh* mesh)
{
    glm::vec3 centroid(0.0f);

    for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
    {
        const auto& vertex = mesh->mVertices[i];
        centroid += glm::vec3(vertex.x, vertex.y, vertex.z);
    }

    return centroid / static_cast<float>(mesh->mNumVertices);
}

MeshAnalysis::SurfaceType MeshAnalyzer::determineSurfaceType(
    const aiMesh* mesh,
    const MeshAnalysis::BoundingBox& bbox,
    const glm::vec3& centroid,
    const MeshAnalysis::SamplingConfig& config,
    MeshAnalysis::AnalysisResult& result)
{

    // Try spherical analysis first (most restrictive)
    if (analyzeSphericality(mesh, centroid, config, result))
    {
        return MeshAnalysis::SurfaceType::SPHERICAL;
    }

    // Try cylindrical analysis
    if (analyzeCylindricity(mesh, bbox, config, result))
    {
        return MeshAnalysis::SurfaceType::CYLINDRICAL;
    }

    // Try planar analysis
    if (analyzePlanarity(mesh, config, result))
    {
        return MeshAnalysis::SurfaceType::PLANAR;
    }

    // Default to mixed
    result.typeConfidence = 0.0f;
    return MeshAnalysis::SurfaceType::MIXED;
}

bool MeshAnalyzer::analyzeSphericality(
    const aiMesh* mesh,
    const glm::vec3& centroid,
    const MeshAnalysis::SamplingConfig& config,
    MeshAnalysis::AnalysisResult& result)
{

    // First check: aspect ratio should be close to 1.0 for spheres
    const float minDim = result.boundingBox.getMinDimension();
    const float maxDim = result.boundingBox.getMaxDimension();

    if (maxDim == 0.0f || minDim / maxDim < config.sphericalAspectRatio)
    {
        return false;
    }

    if (!mesh->mNormals)
    {
        return false; // Need normals for spherical analysis
    }

    // Sample normals to check if they point radially outward
    const uint32_t sampleCount = result.samplesUsed;
    const uint32_t step = calculateSampleStep(mesh->mNumVertices, sampleCount);

    uint32_t radialCount = 0;
    float radiusSum = 0.0f;

    for (uint32_t i = 0; i < mesh->mNumVertices; i += step)
    {
        const glm::vec3 pos(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        const glm::vec3 normal(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);

        const glm::vec3 radialDir = safeNormalize(pos - centroid);
        const float radialDot = glm::dot(glm::normalize(normal), radialDir);

        if (radialDot > config.radialNormalThreshold)
        {
            radialCount++;
        }

        radiusSum += glm::distance(pos, centroid);
    }

    const float radialRatio = static_cast<float>(radialCount) / sampleCount;
    if (radialRatio > 0.6f)
    {
        result.avgRadius = radiusSum / sampleCount;
        result.typeConfidence = radialRatio;
        return true;
    }

    return false;
}

bool MeshAnalyzer::analyzeCylindricity(
    const aiMesh* mesh,
    const MeshAnalysis::BoundingBox& bbox,
    const MeshAnalysis::SamplingConfig& config,
    MeshAnalysis::AnalysisResult& result)
{

    const float minDim = bbox.getMinDimension();
    const float maxDim = bbox.getMaxDimension();

    // Check aspect ratio for cylindrical shape
    if (maxDim == 0.0f || maxDim / minDim < config.cylindricalAspectRatio)
    {
        return false;
    }

    if (!mesh->mNormals)
    {
        return false; // Need normals for cylindrical analysis
    }

    // Determine cylinder axis (direction of longest dimension)
    const glm::vec3 dominantAxis = getDominantAxis(bbox);

    // Sample normals to check if they're perpendicular to the dominant axis
    const uint32_t sampleCount = result.samplesUsed;
    const uint32_t step = calculateSampleStep(mesh->mNumVertices, sampleCount);

    uint32_t perpendicularCount = 0;

    for (uint32_t i = 0; i < mesh->mNumVertices; i += step)
    {
        const glm::vec3 normal(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        const float axialDot = std::abs(glm::dot(glm::normalize(normal), dominantAxis));

        if (axialDot < config.perpendicularNormalThreshold)
        {
            perpendicularCount++;
        }
    }

    const float perpendicularRatio = static_cast<float>(perpendicularCount) / sampleCount;
    if (perpendicularRatio > 0.5f)
    {
        result.dominantAxis = dominantAxis;
        result.typeConfidence = perpendicularRatio;
        return true;
    }

    return false;
}

bool MeshAnalyzer::analyzePlanarity(
    const aiMesh* mesh,
    const MeshAnalysis::SamplingConfig& config,
    MeshAnalysis::AnalysisResult& result)
{

    if (!mesh->mNormals)
    {
        return false; // Need normals for planar analysis
    }

    const uint32_t sampleCount = result.samplesUsed;
    const uint32_t step = calculateSampleStep(mesh->mNumVertices, sampleCount);

    // Calculate average normal
    glm::vec3 avgNormal(0.0f);
    for (uint32_t i = 0; i < mesh->mNumVertices; i += step)
    {
        const glm::vec3 normal(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        avgNormal += normal;
    }

    avgNormal = safeNormalize(avgNormal);

    // Check consistency of normals with average
    uint32_t consistentCount = 0;
    for (uint32_t i = 0; i < mesh->mNumVertices; i += step)
    {
        const glm::vec3 normal(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        const float consistency = glm::dot(glm::normalize(normal), avgNormal);

        if (consistency > config.normalConsistencyThreshold)
        {
            consistentCount++;
        }
    }

    const float consistencyRatio = static_cast<float>(consistentCount) / sampleCount;
    if (consistencyRatio > 0.8f)
    {
        result.typeConfidence = consistencyRatio;
        return true;
    }

    return false;
}

glm::vec3 MeshAnalyzer::getDominantAxis(const MeshAnalysis::BoundingBox& bbox)
{
    const glm::vec3 size = bbox.getSize();

    if (size.y > size.x && size.y > size.z)
    {
        return glm::vec3(0.0f, 1.0f, 0.0f);
    }
    else if (size.x > size.y && size.x > size.z)
    {
        return glm::vec3(1.0f, 0.0f, 0.0f);
    }
    else
    {
        return glm::vec3(0.0f, 0.0f, 1.0f);
    }
}

float MeshAnalyzer::calculateAverageRadius(const aiMesh* mesh, const glm::vec3& center)
{
    float radiusSum = 0.0f;

    for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
    {
        const glm::vec3 pos(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        radiusSum += glm::distance(pos, center);
    }

    return radiusSum / static_cast<float>(mesh->mNumVertices);
}

bool MeshAnalyzer::checkUniformNormals(const aiMesh* mesh, float threshold)
{
    if (!mesh->mNormals || mesh->mNumVertices < 2)
    {
        return false;
    }

    // Calculate average normal
    glm::vec3 avgNormal(0.0f);
    for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
    {
        const glm::vec3 normal(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        avgNormal += normal;
    }

    avgNormal = safeNormalize(avgNormal);

    // Check how many normals are consistent with average
    uint32_t consistentCount = 0;
    for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
    {
        const glm::vec3 normal(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        const float consistency = glm::dot(glm::normalize(normal), avgNormal);

        if (consistency > threshold)
        {
            consistentCount++;
        }
    }

    const float consistencyRatio = static_cast<float>(consistentCount) / mesh->mNumVertices;
    return consistencyRatio > 0.8f;
}

uint32_t MeshAnalyzer::calculateSampleCount(uint32_t vertexCount, const MeshAnalysis::SamplingConfig& config)
{
    const uint32_t minSamples = static_cast<uint32_t>(vertexCount * config.minSampleRatio);
    const uint32_t maxSamples = static_cast<uint32_t>(vertexCount * config.maxSampleRatio);

    return std::max(1u, std::min(config.maxSamples, std::min(maxSamples, std::max(minSamples, vertexCount / 10))));
}

uint32_t MeshAnalyzer::calculateSampleStep(uint32_t vertexCount, uint32_t sampleCount)
{
    return std::max(1u, vertexCount / sampleCount);
}

bool MeshAnalyzer::validateMesh(const aiMesh* mesh)
{
    return mesh != nullptr &&
        mesh->mVertices != nullptr &&
        mesh->mNumVertices > 0;
}

glm::vec3 MeshAnalyzer::safeNormalize(const glm::vec3& vec, const glm::vec3& fallback)
{
    const float length = glm::length(vec);
    return (length > 1e-6f) ? vec / length : fallback;
}
