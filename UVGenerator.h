
#pragma once
#include "MeshAnalyzer.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <cmath>
#include "AssImpMesh.h"

// UV Generation Configuration
struct UVConfig
{
    float sphericalScale = 1.0f;        // Scale factor for spherical UV
    float cylindricalScale = 1.0f;      // Scale factor for cylindrical UV
    glm::vec2 planarScale = glm::vec2(1.0f);  // Scale factor for planar UV
    bool flipV = false;                 // Flip V coordinate
    bool seamlessSpherical = true;      // Handle spherical UV seams
    float cylindricalOffset = 0.0f;     // Offset for cylindrical wrapping
};

class UVGenerator
{
public:
    static void generateUVForMesh(
        std::vector<Vertex>& vertices,
        const MeshAnalysis::AnalysisResult& analysis,
        const UVConfig& config = UVConfig{}
    );

    static void generateUVForSurface(
        Vertex& vertex,
        const MeshAnalysis::AnalysisResult& analysis,
        const UVConfig& config = UVConfig{}
    );

private:
    static void generateSphericalUV(
        Vertex& vertex,
        const glm::vec3& center,
        const UVConfig& config
    );

    static void generateCylindricalUV(
        Vertex& vertex,
        const glm::vec3& axis,
        const MeshAnalysis::BoundingBox& bbox,
        const UVConfig& config
    );

    static void generatePlanarUV(
        Vertex& vertex,
        const glm::vec3& normal,
        const MeshAnalysis::BoundingBox& bbox,
        const UVConfig& config
    );

    static void generateAdaptiveUV(
        Vertex& vertex,
        const MeshAnalysis::BoundingBox& bbox,
        const UVConfig& config
    );

    // Utility functions
    static glm::vec2 cartesianToSpherical(const glm::vec3& pos, const glm::vec3& center);
    static glm::vec2 cartesianToCylindrical(const glm::vec3& pos, const glm::vec3& axis, const MeshAnalysis::BoundingBox& bbox);
    static glm::vec2 projectToPlanar(const glm::vec3& pos, const glm::vec3& normal, const MeshAnalysis::BoundingBox& bbox);    
};
