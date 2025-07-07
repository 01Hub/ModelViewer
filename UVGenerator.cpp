#include "UVGenerator.h"
#include <cstdint>   
#include <functional>
#include <queue>
#include <utility>

namespace std
{
    template<>
    struct hash<std::pair<uint32_t, uint32_t>>
    {
        size_t operator()(const std::pair<uint32_t, uint32_t>& p) const
        {
            // Combine the two integers into a 64-bit value
            return std::hash<uint64_t>()(
                (static_cast<uint64_t>(p.first) << 32) | p.second
                );
        }
    };
}


// Method 1: Angle-based unwrapping (most similar to Blender's Smart UV)
bool UVGenerator::generateAngleBased(aiMesh* mesh,
    std::vector<Vertex>& vertices,
    std::vector<unsigned int>& indices,
    const UVConfig& config)
{
    if (vertices.empty() || indices.empty()) return false;

    // Build triangle list
    std::vector<Triangle> triangles;
    buildTriangleList(vertices, indices, triangles);

    // Find seams based on angle threshold
    std::vector<std::pair<unsigned int, unsigned int>> seams;
    findSeams(vertices, triangles, seams, config.angleThreshold);

    // Create UV islands
    std::vector<UVIsland> islands;
    createUVIslands(triangles, seams, islands);

    // Unwrap each island
    std::vector<glm::vec2> uvs(vertices.size());
    for (const auto& island : islands)
    {
        unwrapIsland(vertices, triangles, island, uvs);
    }

    // Pack UV islands
    packUVIslands(const_cast<std::vector<UVIsland>&>(islands), uvs, config.seamPadding);

    // Apply transformations and update vertices
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        glm::vec2 finalUV = uvs[i];
        applyUVTransforms(finalUV, config);
        vertices[i].TexCoords = finalUV;
    }

    return true;
}


// Method 2: Cylindrical projection
bool UVGenerator::generateCylindrical(aiMesh* mesh,
    std::vector<Vertex>& vertices,
    std::vector<unsigned int>& indices,
    const UVConfig& config)
{
    if (vertices.empty()) return false;

    glm::vec3 centroid = calculateCentroid(vertices);
    glm::vec3 minBounds, maxBounds;
    calculateBounds(vertices, minBounds, maxBounds);

    float height = maxBounds.y - minBounds.y;

    for (auto& vertex : vertices)
    {
        glm::vec3 localPos = vertex.Position - centroid;

        // Cylindrical mapping
        float u = (atan2(localPos.z, localPos.x) + M_PI) / (2.0f * M_PI);
        float v = (vertex.Position.y - minBounds.y) / height;

        // Handle seam
        if (config.seamlessSpherical && u > 0.75f && localPos.x > 0)
        {
            u -= 1.0f;
        }

        u += config.cylindricalOffset;
        u = fmod(u + 1.0f, 1.0f); // Wrap to [0,1]

        glm::vec2 uv(u * config.cylindricalScale, v);
        applyUVTransforms(uv, config);
        vertex.TexCoords = uv;
    }

    return true;
}


// Method 3: Spherical projection
bool UVGenerator::generateSpherical(aiMesh* mesh,
    std::vector<Vertex>& vertices,
    std::vector<unsigned int>& indices,
    const UVConfig& config)
{
    if (vertices.empty()) return false;

    glm::vec3 centroid = calculateCentroid(vertices);

    for (auto& vertex : vertices)
    {
        glm::vec3 localPos = glm::normalize(vertex.Position - centroid);

        // Spherical mapping
        float u = (atan2(localPos.z, localPos.x) + M_PI) / (2.0f * M_PI);
        float v = (asin(localPos.y) + M_PI * 0.5f) / M_PI;

        // Handle poles and seams
        if (config.seamlessSpherical)
        {
            if (abs(localPos.y) > 0.99f)
            { // Near poles
                u = 0.5f;
            }
        }

        glm::vec2 uv(u * config.sphericalScale, v * config.sphericalScale);
        applyUVTransforms(uv, config);
        vertex.TexCoords = uv;
    }

    return true;
}


// Method 4: Planar projection with automatic orientation
bool UVGenerator::generatePlanar(aiMesh* mesh,
    std::vector<Vertex>& vertices,
    std::vector<unsigned int>& indices,
    const UVConfig& config)
{
    if (vertices.empty()) return false;

    glm::vec3 minBounds, maxBounds;
    calculateBounds(vertices, minBounds, maxBounds);

    // Find the best projection plane (largest face)
    glm::vec3 size = maxBounds - minBounds;
    int bestPlane = 0; // 0=YZ, 1=XZ, 2=XY
    if (size.x * size.y > size.y * size.z && size.x * size.y > size.x * size.z)
    {
        bestPlane = 2; // XY plane
    }
    else if (size.x * size.z > size.y * size.z)
    {
        bestPlane = 1; // XZ plane
    }

    for (auto& vertex : vertices)
    {
        glm::vec2 uv;
        switch (bestPlane)
        {
        case 0: // YZ plane
            uv.x = (vertex.Position.y - minBounds.y) / size.y;
            uv.y = (vertex.Position.z - minBounds.z) / size.z;
            break;
        case 1: // XZ plane
            uv.x = (vertex.Position.x - minBounds.x) / size.x;
            uv.y = (vertex.Position.z - minBounds.z) / size.z;
            break;
        case 2: // XY plane
            uv.x = (vertex.Position.x - minBounds.x) / size.x;
            uv.y = (vertex.Position.y - minBounds.y) / size.y;
            break;
        }

        uv *= config.planarScale;
        applyUVTransforms(uv, config);
        vertex.TexCoords = uv;
    }

    return true;
}


// Method 5: Hybrid approach
bool UVGenerator::generateHybrid(aiMesh* mesh,
    std::vector<Vertex>& vertices,
    std::vector<unsigned int>& indices,
    const UVConfig& config)
{
    if (vertices.empty()) return false;

    // Compute bounding box and size
    glm::vec3 minBounds, maxBounds;
    calculateBounds(vertices, minBounds, maxBounds);
    glm::vec3 size = maxBounds - minBounds;

    // Principal Component Analysis (for elongation and dominant axis)
    glm::vec3 mean(0.0f);
    for (const auto& v : vertices)
        mean += v.Position;
    mean /= static_cast<float>(vertices.size());

    glm::mat3 covariance(0.0f);
    for (const auto& v : vertices)
    {
        glm::vec3 p = v.Position - mean;
        covariance[0] += p.x * p;
        covariance[1] += p.y * p;
        covariance[2] += p.z * p;
    }

    covariance /= static_cast<float>(vertices.size());

    // Eigen decomposition to get principal axes
    glm::vec3 eigenValues;
    glm::mat3 eigenVectors;
    computeEigenDecomposition(covariance, eigenValues, eigenVectors);

    // Sort eigenvalues (largest = most elongated axis)
    float e0 = eigenValues[0], e1 = eigenValues[1], e2 = eigenValues[2];
    float elongation = e0 / e2; // e0 >= e1 >= e2 assumed after sort

    // Use elongation + variance to determine mapping
    if (elongation > 4.0f)
    {
        return generateCylindrical(mesh, vertices, indices, config);
    }
    else if (elongation < 1.5f)
    {
        float avg = (e0 + e1 + e2) / 3.0f;
        float var = (pow(e0 - avg, 2) + pow(e1 - avg, 2) + pow(e2 - avg, 2)) / 3.0f;

        if (var < avg * 0.05f)
            return generateSpherical(mesh, vertices, indices, config);
        else
            return generateAngleBased(mesh, vertices, indices, config);
    }
    else
    {
        return generatePlanar(mesh, vertices, indices, config);
    }
}


// Method 6: Angle-based Smart UV (similar to Blender's Smart UV)
bool UVGenerator::generateAngleBasedSmartUV(
    aiMesh* mesh,
    std::vector<Vertex>& vertices,
    std::vector<unsigned int>& indices,
    const UVConfig& config)
{
    if (vertices.empty() || indices.empty())
        return false;

    // 1. Triangle segmentation based on angle threshold
    std::vector<Triangle> triangles;
    buildTriangleList(vertices, indices, triangles);

    std::vector<std::pair<uint32_t, uint32_t>> seams;
    findSeams(vertices, triangles, seams, config.angleThreshold);

    std::vector<UVIsland> islands;
    createUVIslands(triangles, seams, islands);

    // 2. PCA-based projection per island
    std::vector<glm::vec2> uvs(vertices.size(), glm::vec2(0.0f));

    for (int i = 0; i < static_cast<int>(islands.size()); ++i)
    {
        unwrapIslandPCA(vertices, triangles, islands[i], uvs);
    }

    // 3. Optional UV relaxation (not always needed)
    if (config.enableRelaxation)
    {
        relaxUVs(triangles, uvs, islands, config, config.relaxationIterations);
    }

    // 4. Pack using xatlas (pass positions, indices, uvs)
    std::vector<glm::vec3> positions(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i)
        positions[i] = vertices[i].Position;

    packWithXAtlas(uvs, indices, positions);

    // 5. Apply final transforms
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        glm::vec2 finalUV = uvs[i];
        applyUVTransforms(finalUV, config);
        vertices[i].TexCoords = finalUV;
    }

    return true;
}


// Helper method implementations
void UVGenerator::buildTriangleList(const std::vector<Vertex>& vertices,
    const std::vector<unsigned int>& indices,
    std::vector<Triangle>& triangles)
{
    triangles.clear();
    triangles.reserve(indices.size() / 3);

    for (size_t i = 0; i < indices.size(); i += 3)
    {
        Triangle tri;
        tri.indices[0] = indices[i];
        tri.indices[1] = indices[i + 1];
        tri.indices[2] = indices[i + 2];
        tri.visited = false;

        const glm::vec3& v0 = vertices[tri.indices[0]].Position;
        const glm::vec3& v1 = vertices[tri.indices[1]].Position;
        const glm::vec3& v2 = vertices[tri.indices[2]].Position;

        tri.normal = calculateTriangleNormal(v0, v1, v2);
        tri.area = calculateTriangleArea(v0, v1, v2);

        triangles.push_back(tri);
    }
}


void UVGenerator::findSeams(const std::vector<Vertex>& vertices,
    const std::vector<Triangle>& triangles,
    std::vector<std::pair<uint32_t, uint32_t>>& seams,
    float angleThreshold)
{
    seams.clear();
        
    std::unordered_map<Edge, std::vector<uint32_t>> edgeToTriangles;

    // 1. Build edge -> triangle adjacency
    for (uint32_t i = 0; i < triangles.size(); ++i)
    {
        const Triangle& tri = triangles[i];
        for (int j = 0; j < 3; ++j)
        {
            uint32_t a = tri.indices[j];
            uint32_t b = tri.indices[(j + 1) % 3];
            edgeToTriangles[Edge(a, b)].push_back(i);
        }
    }

    const float cosThreshold = std::cos(glm::radians(angleThreshold));

    // 2. Check each edge's adjacent triangle pair(s)
    for (const auto& entry : edgeToTriangles)
    {
        const auto& adjTris = entry.second;
        if (adjTris.size() != 2)
            continue; // boundary edge

        uint32_t t0 = adjTris[0];
        uint32_t t1 = adjTris[1];

        const glm::vec3& n0 = triangles[t0].normal;
        const glm::vec3& n1 = triangles[t1].normal;

        float dot = glm::dot(n0, n1);
        if (dot < cosThreshold)
        {
            seams.emplace_back(t0, t1);
        }
    }
}


void UVGenerator::createUVIslands(const std::vector<Triangle>& triangles,
    const std::vector<std::pair<uint32_t, uint32_t>>& seams,
    std::vector<UVIsland>& islands)
{
    islands.clear();
    const size_t triangleCount = triangles.size();

    // Build fast edge -> triangle adjacency       

    std::unordered_map<Edge, std::vector<uint32_t>> edgeMap;

    for (uint32_t i = 0; i < triangleCount; ++i)
    {
        const auto& tri = triangles[i];
        edgeMap[Edge(tri.indices[0], tri.indices[1])].push_back(i);
        edgeMap[Edge(tri.indices[1], tri.indices[2])].push_back(i);
        edgeMap[Edge(tri.indices[2], tri.indices[0])].push_back(i);
    }

    // Build seam edge set for fast lookup
    std::unordered_set<Edge> seamEdges;
    for (const auto& s : seams)
    {
        const auto& t0 = triangles[s.first];
        const auto& t1 = triangles[s.second];
        for (int i = 0; i < 3; ++i)
        {
            uint32_t a = t0.indices[i];
            uint32_t b = t0.indices[(i + 1) % 3];
            Edge e = Edge(a, b);

            // Check if edge exists in both triangles
            for (int j = 0; j < 3; ++j)
            {
                uint32_t a1 = t1.indices[j];
                uint32_t b1 = t1.indices[(j + 1) % 3];
                if (Edge(a1, b1) == e)
                {
                    seamEdges.emplace(e);
                }
            }
        }
    }

    // Flood fill to build islands
    std::vector<bool> visited(triangleCount, false);
    for (uint32_t i = 0; i < triangleCount; ++i)
    {
        if (visited[i])
            continue;

        UVIsland island;
        std::queue<uint32_t> q;
        q.push(i);
        visited[i] = true;

        while (!q.empty())
        {
            uint32_t tidx = q.front();
            q.pop();
            island.triangles.push_back(tidx);
            island.totalArea += triangles[tidx].area;

            const auto& tri = triangles[tidx];
            for (int ei = 0; ei < 3; ++ei)
            {
                Edge e = Edge(tri.indices[ei], tri.indices[(ei + 1) % 3]);
                if (seamEdges.count(e)) continue;

                // Neighbors sharing this edge
                const auto& adjTris = edgeMap[e];
                for (uint32_t nidx : adjTris)
                {
                    if (!visited[nidx])
                    {
                        visited[nidx] = true;
                        q.push(nidx);
                    }
                }
            }
        }

        islands.push_back(std::move(island));
    }
}


void UVGenerator::unwrapIsland(const std::vector<Vertex>& vertices,
    const std::vector<Triangle>& triangles,
    const UVIsland& island,
    std::vector<glm::vec2>& uvs)
{
    // Simple planar unwrapping for each island
    for (unsigned int triIdx : island.triangles)
    {
        const Triangle& tri = triangles[triIdx];

        // Project triangle onto its best-fit plane
        glm::vec3 normal = tri.normal;
        glm::vec3 tangent = glm::normalize(glm::cross(normal, glm::vec3(0, 1, 0)));
        if (glm::length(tangent) < 0.1f)
        {
            tangent = glm::normalize(glm::cross(normal, glm::vec3(1, 0, 0)));
        }
        glm::vec3 bitangent = glm::cross(normal, tangent);

        for (int i = 0; i < 3; ++i)
        {
            glm::vec3 pos = vertices[tri.indices[i]].Position;
            uvs[tri.indices[i]] = glm::vec2(
                glm::dot(pos, tangent),
                glm::dot(pos, bitangent)
            );
        }
    }
}


void UVGenerator::unwrapIslandPCA(const std::vector<Vertex>& vertices,
    const std::vector<Triangle>& triangles,
    const UVIsland& island,
    std::vector<glm::vec2>& uvs)
{
    std::vector<glm::vec3> points;
    for (unsigned int triIdx : island.triangles)
    {
        const Triangle& tri = triangles[triIdx];
        for (int i = 0; i < 3; ++i)
        {
            points.push_back(vertices[tri.indices[i]].Position);
        }
    }

    // Compute centroid
    glm::vec3 centroid(0.0f);
    for (auto& p : points) centroid += p;
    centroid /= static_cast<float>(points.size());

    // Compute covariance
    glm::mat3 cov(0.0f);
    for (auto& p : points)
    {
        glm::vec3 d = p - centroid;
        cov += glm::outerProduct(d, d);
    }

    // Eigen decomposition
    glm::vec3 axis1(1, 0, 0), axis2(0, 1, 0);
    // Use PCA if you have a math lib for it, or fallback: cross with normal
    glm::vec3 normal = triangles[island.triangles[0]].normal;
    axis1 = glm::normalize(glm::cross(normal, glm::vec3(0, 1, 0)));
    if (glm::length(axis1) < 0.01f) axis1 = glm::normalize(glm::cross(normal, glm::vec3(1, 0, 0)));
    axis2 = glm::cross(normal, axis1);

    // Project to 2D plane
    for (unsigned int triIdx : island.triangles)
    {
        const Triangle& tri = triangles[triIdx];
        for (int i = 0; i < 3; ++i)
        {
            glm::vec3 pos = vertices[tri.indices[i]].Position - centroid;
            uvs[tri.indices[i]] = glm::vec2(glm::dot(pos, axis1), glm::dot(pos, axis2));
        }
    }
}

void UVGenerator::relaxUVs(
    const std::vector<Triangle>& triangles,
    std::vector<glm::vec2>& uvs,
    const std::vector<UVIsland>& islands,
    const UVConfig& config,
    int iterations)
{
    // Build adjacency map: vertex index -> unique neighbors
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> adjacency;

    for (const auto& island : islands)
    {
        for (uint32_t triIdx : island.triangles)
        {
            const Triangle& tri = triangles[triIdx];
            for (int i = 0; i < 3; ++i)
            {
                uint32_t vi = tri.indices[i];
                for (int j = 0; j < 3; ++j)
                {
                    uint32_t vj = tri.indices[j];
                    if (vi != vj)
                        adjacency[vi].insert(vj); // insert deduplicates
                }
            }
        }
    }

    std::vector<glm::vec2> newUVs = uvs;

    for (int iter = 0; iter < iterations; ++iter)
    {
        for (size_t i = 0; i < uvs.size(); ++i)
        {
            auto it = adjacency.find(static_cast<uint32_t>(i));
            if (it == adjacency.end() || it->second.empty())
                continue;

            glm::vec2 avg(0.0f);
            for (uint32_t neighbor : it->second)
                avg += uvs[neighbor];

            avg /= static_cast<float>(it->second.size());
            newUVs[i] = avg;
        }

        std::swap(uvs, newUVs); // Apply new UVs for next iteration
    }
}


void UVGenerator::packUVIslands(std::vector<UVIsland>& islands,
    std::vector<glm::vec2>& uvs,
    float padding)
{
    // Simple UV packing - normalize all UVs to [0,1] range
    if (uvs.empty()) return;

    glm::vec2 minUV = uvs[0];
    glm::vec2 maxUV = uvs[0];

    for (const auto& uv : uvs)
    {
        minUV = glm::min(minUV, uv);
        maxUV = glm::max(maxUV, uv);
    }

    glm::vec2 size = maxUV - minUV;
    if (size.x > 0 && size.y > 0)
    {
        for (auto& uv : uvs)
        {
            uv = (uv - minUV) / size;
        }
    }
}


#include <xatlas.h>
void UVGenerator::packWithXAtlas(
    std::vector<glm::vec2>& uvs,
    const std::vector<unsigned int>& indices,
    const std::vector<glm::vec3>& positions)
{
    assert(!uvs.empty());
    assert(!indices.empty());
    assert(positions.size() == uvs.size());

    xatlas::Atlas* atlas = xatlas::Create();

    xatlas::MeshDecl meshDecl{};
    meshDecl.vertexCount = static_cast<uint32_t>(positions.size());
    meshDecl.vertexPositionData = positions.data();
    meshDecl.vertexPositionStride = sizeof(glm::vec3);
    meshDecl.indexCount = static_cast<uint32_t>(indices.size());
    meshDecl.indexData = indices.data();
    meshDecl.indexFormat = xatlas::IndexFormat::UInt32;

    // Add mesh to xatlas
    xatlas::AddMeshError error = xatlas::AddMesh(atlas, meshDecl);
    if (error != xatlas::AddMeshError::Success)
    {
        printf("xatlas AddMesh failed: %s\n", xatlas::StringForEnum(error));
        xatlas::Destroy(atlas);
        return;
    }

    xatlas::ChartOptions chartOptions{};
    xatlas::PackOptions packOptions{};
    packOptions.padding = 2;
    packOptions.texelsPerUnit = 1.0f;

    xatlas::Generate(atlas, chartOptions, packOptions);

    const xatlas::Mesh& outMesh = atlas->meshes[0];
    uvs.resize(outMesh.vertexCount);

    for (uint32_t i = 0; i < outMesh.vertexCount; ++i)
    {
        const xatlas::Vertex& v = outMesh.vertexArray[i];
        uvs[v.xref] = glm::vec2(
            v.uv[0] / float(atlas->width),
            v.uv[1] / float(atlas->height));
    }

    xatlas::Destroy(atlas);
}


void UVGenerator::applyUVTransforms(glm::vec2& uv, const UVConfig& config)
{
    uv *= config.planarScale;

    if (config.flipV)
    {
        uv.y = 1.0f - uv.y;
    }

    // Ensure UVs are in [0,1] range
    uv = glm::clamp(uv, 0.0f, 1.0f);
}

// Utility methods
glm::vec3 UVGenerator::calculateCentroid(const std::vector<Vertex>& vertices)
{
    glm::vec3 centroid(0.0f);
    for (const auto& vertex : vertices)
    {
        centroid += vertex.Position;
    }
    return centroid / static_cast<float>(vertices.size());
}

glm::vec3 UVGenerator::calculateBounds(const std::vector<Vertex>& vertices,
    glm::vec3& minBounds, glm::vec3& maxBounds)
{
    if (vertices.empty()) return glm::vec3(0);

    minBounds = maxBounds = vertices[0].Position;
    for (const auto& vertex : vertices)
    {
        minBounds = glm::min(minBounds, vertex.Position);
        maxBounds = glm::max(maxBounds, vertex.Position);
    }
    return maxBounds - minBounds;
}

float UVGenerator::calculateTriangleArea(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2)
{
    return 0.5f * glm::length(glm::cross(v1 - v0, v2 - v0));
}

glm::vec3 UVGenerator::calculateTriangleNormal(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2)
{
    return glm::normalize(glm::cross(v1 - v0, v2 - v0));
}

// Utility: compute eigenvalues and eigenvectors of symmetric 3x3 matrix
// Only works correctly for symmetric matrices (like covariance)
void UVGenerator::computeEigenDecomposition(
    const glm::mat3& m,
    glm::vec3& eigenValues,
    glm::mat3& eigenVectors)
{
    const int maxIterations = 50;
    const float epsilon = 1e-10f;

    glm::mat3 A = m;
    eigenVectors = glm::mat3(1.0f); // Identity

    for (int iter = 0; iter < maxIterations; ++iter)
    {
        // Find largest off-diagonal element in A
        int p = 0, q = 1;
        float maxVal = std::abs(A[0][1]);
        for (int i = 0; i < 3; ++i)
        {
            for (int j = i + 1; j < 3; ++j)
            {
                float val = std::abs(A[i][j]);
                if (val > maxVal)
                {
                    maxVal = val;
                    p = i;
                    q = j;
                }
            }
        }

        if (maxVal < epsilon)
            break; // Converged

        float app = A[p][p];
        float aqq = A[q][q];
        float apq = A[p][q];

        float phi = 0.5f * atan2(2.0f * apq, aqq - app);
        float c = cos(phi);
        float s = sin(phi);

        // Build rotation matrix
        glm::mat3 R(1.0f);
        R[p][p] = c;
        R[q][q] = c;
        R[p][q] = s;
        R[q][p] = -s;

        // A = R^T * A * R
        A = glm::transpose(R) * A * R;
        eigenVectors = eigenVectors * R;
    }

    eigenValues = glm::vec3(A[0][0], A[1][1], A[2][2]);

    // Sort by eigenvalue magnitude (descending)
    std::array<std::pair<float, glm::vec3>, 3> sorted = {
        std::make_pair(eigenValues.x, glm::vec3(eigenVectors[0][0], eigenVectors[1][0], eigenVectors[2][0])),
        std::make_pair(eigenValues.y, glm::vec3(eigenVectors[0][1], eigenVectors[1][1], eigenVectors[2][1])),
        std::make_pair(eigenValues.z, glm::vec3(eigenVectors[0][2], eigenVectors[1][2], eigenVectors[2][2]))
    };

    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
        });

    for (int i = 0; i < 3; ++i)
    {
        eigenValues[i] = sorted[i].first;
        eigenVectors[0][i] = sorted[i].second.x;
        eigenVectors[1][i] = sorted[i].second.y;
        eigenVectors[2][i] = sorted[i].second.z;
    }
}
