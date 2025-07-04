#include "TangentGenerator.h"


void TangentGenerator::generateTangentsForMesh(
    std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices,
    const MeshAnalysis::AnalysisResult& analysis)
{

    if (analysis.surfaceType == MeshAnalysis::SurfaceType::MIXED)
    {
        // For mixed surfaces, use traditional triangle-based calculation
        calculateTangentsFromTriangles(vertices, indices);
    }
    else
    {
        // For uniform surfaces, use analytical approach
        for (auto& vertex : vertices)
        {
            generateTangentsForSurface(vertex, analysis);
        }
    }
}

void TangentGenerator::generateTangentsForSurface(
    Vertex& vertex,
    const MeshAnalysis::AnalysisResult& analysis)
{

    switch (analysis.surfaceType)
    {
    case MeshAnalysis::SurfaceType::SPHERICAL:
        generateSphericalTangents(vertex, analysis.centroid);
        break;

    case MeshAnalysis::SurfaceType::CYLINDRICAL:
        if (analysis.dominantAxis.has_value())
        {
            generateCylindricalTangents(vertex, analysis.dominantAxis.value());
        }
        else
        {
            generatePlanarTangents(vertex);
        }
        break;

    case MeshAnalysis::SurfaceType::PLANAR:
        generatePlanarTangents(vertex);
        break;

    case MeshAnalysis::SurfaceType::MIXED:
    default:
        generatePlanarTangents(vertex);
        break;
    }
}

void TangentGenerator::generateSphericalTangents(
    Vertex& vertex,
    const glm::vec3& center)
{

    glm::vec3 radialDir = glm::normalize(vertex.Position - center);

    // For spherical surfaces, tangent is perpendicular to radial direction
    // Use cross product with an arbitrary vector to get tangent
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::abs(glm::dot(radialDir, up)) > 0.9f)
    {
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    vertex.Tangent = glm::normalize(glm::cross(up, radialDir));
    vertex.Bitangent = glm::normalize(glm::cross(radialDir, vertex.Tangent));

    // Ensure tangent space is orthogonal
    gramSchmidt(vertex.Tangent, vertex.Bitangent, vertex.Normal);
}

void TangentGenerator::generateCylindricalTangents(
    Vertex& vertex,
    const glm::vec3& axis)
{

    // For cylindrical surfaces, one tangent is along the axis
    // The other is perpendicular to both axis and normal
    glm::vec3 axialTangent = axis;
    glm::vec3 radialTangent = glm::normalize(glm::cross(vertex.Normal, axis));

    // Choose the tangent that's more aligned with U direction
    vertex.Tangent = radialTangent;
    vertex.Bitangent = axialTangent;

    gramSchmidt(vertex.Tangent, vertex.Bitangent, vertex.Normal);
}

void TangentGenerator::generatePlanarTangents(Vertex& vertex)
{
    // For planar surfaces, derive tangent from normal
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::abs(glm::dot(vertex.Normal, up)) > 0.9f)
    {
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    vertex.Tangent = glm::normalize(glm::cross(up, vertex.Normal));
    vertex.Bitangent = glm::normalize(glm::cross(vertex.Normal, vertex.Tangent));

    gramSchmidt(vertex.Tangent, vertex.Bitangent, vertex.Normal);
}

void TangentGenerator::calculateTangentsFromTriangles(
    std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices)
{

    // Initialize tangent accumulation
    std::vector<glm::vec3> tangentSum(vertices.size(), glm::vec3(0.0f));
    std::vector<glm::vec3> bitangentSum(vertices.size(), glm::vec3(0.0f));

    // Process each triangle
    for (size_t i = 0; i < indices.size(); i += 3)
    {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        const Vertex& v0 = vertices[i0];
        const Vertex& v1 = vertices[i1];
        const Vertex& v2 = vertices[i2];

        // Calculate edge vectors
        glm::vec3 edge1 = v1.Position - v0.Position;
        glm::vec3 edge2 = v2.Position - v0.Position;

        glm::vec2 deltaUV1 = v1.TexCoords - v0.TexCoords;
        glm::vec2 deltaUV2 = v2.TexCoords - v0.TexCoords;

        // Calculate tangent and bitangent
        float det = deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x;
        if (std::abs(det) > 1e-6f)
        {
            float invDet = 1.0f / det;
            glm::vec3 tangent = (deltaUV2.y * edge1 - deltaUV1.y * edge2) * invDet;
            glm::vec3 bitangent = (deltaUV1.x * edge2 - deltaUV2.x * edge1) * invDet;

            // Accumulate for each vertex
            tangentSum[i0] += tangent;
            tangentSum[i1] += tangent;
            tangentSum[i2] += tangent;

            bitangentSum[i0] += bitangent;
            bitangentSum[i1] += bitangent;
            bitangentSum[i2] += bitangent;
        }
    }

    // Normalize and orthogonalize
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        glm::vec3 tangent = tangentSum[i];
        glm::vec3 bitangent = bitangentSum[i];

        if (glm::length(tangent) > 1e-6f)
        {
            tangent = glm::normalize(tangent);
            bitangent = glm::normalize(bitangent);

            gramSchmidt(tangent, bitangent, vertices[i].Normal);

            vertices[i].Tangent = tangent;
            vertices[i].Bitangent = bitangent;
        }
    }
}

// Utility Functions
glm::vec3 TangentGenerator::orthogonalize(const glm::vec3& vec, const glm::vec3& normal)
{
    return glm::normalize(vec - glm::dot(vec, normal) * normal);
}

void TangentGenerator::gramSchmidt(glm::vec3& tangent, glm::vec3& bitangent, const glm::vec3& normal)
{
    // Orthogonalize tangent against normal
    tangent = orthogonalize(tangent, normal);

    // Orthogonalize bitangent against normal and tangent
    bitangent = orthogonalize(bitangent, normal);
    bitangent = orthogonalize(bitangent, tangent);

    // Ensure proper handedness
    if (glm::dot(glm::cross(normal, tangent), bitangent) < 0.0f)
    {
        bitangent = -bitangent;
    }
}
