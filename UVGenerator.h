
#pragma once
#include "AssImpMesh.h"
#include "MeshAnalyzer.h"
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <utility>
#include <vector>


// Edge type (ensure vertex indices are sorted to prevent duplicates)
struct Edge
{
    uint32_t v1, v2;

    Edge(uint32_t a, uint32_t b)
    {
        if (a < b)
        {
            v1 = a;
            v2 = b;
        }
        else
        {
            v1 = b;
            v2 = a;
        }
    }

    bool operator==(const Edge& other) const
    {
        return v1 == other.v1 && v2 == other.v2;
    }
};

// Custom hash for Edge
namespace std
{
    template <>
    struct hash<Edge>
    {
        std::size_t operator()(const Edge& e) const
        {
            return std::hash<uint32_t>()(e.v1) ^ (std::hash<uint32_t>()(e.v2) << 1);
        }
    };
}

// UV Generation Configuration
struct UVConfig
{
    float sphericalScale = 1.0f;        // Scale factor for spherical UV
    float cylindricalScale = 1.0f;      // Scale factor for cylindrical UV
    glm::vec2 planarScale = glm::vec2(1.0f);  // Scale factor for planar UV
    bool flipV = false;                 // Flip V coordinate
    bool seamlessSpherical = true;      // Handle spherical UV seams
    float cylindricalOffset = 0.0f;     // Offset for cylindrical wrapping

    // Angle-based unwrapping parameters
    float angleThreshold = 60.0f;       // Angle threshold for seam detection (degrees)
    float distortionWeight = 0.5f;      // Weight for distortion vs area preservation
    bool preserveAspectRatio = true;    // Preserve triangle aspect ratios
    float seamPadding = 0.02f;          // Padding around UV islands
    bool enableRelaxation = false;
    int relaxationIterations = 10; // Default number of smoothing passes
    bool enablePacking = true;
};

struct Triangle
{
    unsigned int indices[3];
    glm::vec3 normal;
    float area;
    bool visited;
};

struct UVIsland
{
    std::vector<unsigned int> triangles;
    glm::vec2 minUV, maxUV;
    float totalArea;
};

class UVGenerator
{
public:
    // Method 1: Angle-based unwrapping (most reliable for automation)
    static bool generateAngleBased(aiMesh* mesh,
        std::vector<Vertex>& vertices,
        std::vector<unsigned int>& indices,
        const UVConfig& config = UVConfig{});

    // Method 2: Cylindrical projection (good for organic shapes)
    static bool generateCylindrical(aiMesh* mesh,
        std::vector<Vertex>& vertices,
        std::vector<unsigned int>& indices,
        const UVConfig& config = UVConfig{});

    // Method 3: Spherical projection (good for rounded objects)
    static bool generateSpherical(aiMesh* mesh,
        std::vector<Vertex>& vertices,
        std::vector<unsigned int>& indices,
        const UVConfig& config = UVConfig{});

    // Method 4: Planar projection with automatic orientation
    static bool generatePlanar(aiMesh* mesh,
        std::vector<Vertex>& vertices,
        std::vector<unsigned int>& indices,
        const UVConfig& config = UVConfig{});

    // Method 5: Hybrid approach (combines multiple methods)
    static bool generateHybrid(aiMesh* mesh,
        std::vector<Vertex>& vertices,
        std::vector<unsigned int>& indices,
        const UVConfig& config = UVConfig{});

	// Method 6: Angle-based Smart UV (similar to Blender's Smart UV)
    /*static bool generateAngleBasedSmartUV(
        aiMesh* mesh,
        std::vector<Vertex>& vertices,
        std::vector<unsigned int>& indices,
        const UVConfig& config);*/

    static bool generateAngleBasedSmartUV(
        aiMesh* mesh,
        std::vector<Vertex>& vertices,
        std::vector<unsigned int>& indices,
        const UVConfig& config);

private:
    // Helper methods for angle-based unwrapping
    static void buildTriangleList(const std::vector<Vertex>& vertices,
        const std::vector<unsigned int>& indices,
        std::vector<Triangle>& triangles);

    static void findSeams(const std::vector<Vertex>& vertices,
        const std::vector<Triangle>& triangles,
        std::vector<std::pair<unsigned int, unsigned int>>& seams,
        float angleThreshold);

    static void createUVIslands(const std::vector<Triangle>& triangles,
        const std::vector<std::pair<unsigned int, unsigned int>>& seams,
        std::vector<UVIsland>& islands);

    static void unwrapIsland(const std::vector<Vertex>& vertices,
        const std::vector<Triangle>& triangles,
        const UVIsland& island,
        std::vector<glm::vec2>& uvs);

    /*static void unwrapIslandPCA(const std::vector<Vertex>& vertices,
        const std::vector<Triangle>& triangles,
        const UVIsland& island,
        std::vector<glm::vec2>& uvs);*/

    static void unwrapIslandPCA(const std::vector<Vertex>& vertices,
        const std::vector<Triangle>& triangles,
        const UVIsland& island,
        std::unordered_map<unsigned int, std::array<glm::vec2, 3>>& triangleUVs,
        bool normalizeUVs = true);

    static void relaxUVs(
        const std::vector<Triangle>& triangles,
        std::vector<glm::vec2>& uvs,
        const std::vector<UVIsland>& islands,
        const UVConfig& config,
        int iterations);

    static void packUVIslands(std::vector<UVIsland>& islands,
        std::vector<glm::vec2>& uvs,
        float padding);

    static void packWithXAtlas(
        std::vector<glm::vec2>& uvs,
        const std::vector<unsigned int>& indices,
        const std::vector<glm::vec3>& positions);

    // Utility methods
    static void applyUVTransforms(glm::vec2& uv, const UVConfig& config);
    static glm::vec3 calculateCentroid(const std::vector<Vertex>& vertices);
    static glm::vec3 calculateBounds(const std::vector<Vertex>& vertices,
        glm::vec3& minBounds, glm::vec3& maxBounds);
    static float calculateTriangleArea(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);
    static glm::vec3 calculateTriangleNormal(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);   
    static void computeEigenDecomposition(const glm::mat3& m, glm::vec3& eigenValues, glm::mat3& eigenVectors);
};
