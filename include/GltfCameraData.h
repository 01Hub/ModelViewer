#pragma once

#include <QString>
#include <QVector>
#include <QVector3D>

// ---------------------------------------------------------------------------
// GltfCameraData
//
// Data structures for glTF camera support.
//
// A glTF file may define one or more cameras that are referenced by nodes
// in the scene graph.  Each camera carries its projection parameters and the
// world-space transform derived from its node's accumulated transform chain.
//
// These structures are populated by AssImpModelLoader::parseSceneCameras()
// during import and stored in SceneGraph keyed by source file path.
// CamerasPanel reads them to build its tree, and GLWidget uses them to
// switch the primary camera to a glTF-defined viewpoint.
// ---------------------------------------------------------------------------

enum class GltfCameraType
{
    Perspective,
    Orthographic
};

// One camera entry from the glTF file.
struct GltfCameraEntry
{
    // Name as declared in the glTF "cameras" array and referenced by the node.
    QString         name;

    // Name of the scene-graph node that hosts this camera.  Assimp sets
    // aiCamera::mName to the referencing node's name, so for glTF files
    // this equals 'name'.  Stored separately so the per-frame animation
    // update can look the node up directly in worldTransforms without
    // any extra string massaging.
    QString         nodeName;
    int             nodeIndex       = -1;

    GltfCameraType  type            = GltfCameraType::Perspective;

    // Perspective parameters (valid when type == Perspective).
    // fovYRadians is the vertical field of view in radians (glTF "yfov").
    float           fovYRadians     = 0.7854f;  // π/4 ≈ 45°
    float           zNear           = 0.01f;
    float           zFar            = 10000.0f;

    // Orthographic parameters (valid when type == Orthographic).
    // xMag / yMag are half-extents of the orthographic view box.
    float           xMag            = 1.0f;
    float           yMag            = 1.0f;

    // World-space camera orientation derived from the node's accumulated transform.
    // worldDirection is the normalised viewing direction (glTF cameras look along -Z).
    // worldUp is the normalised up vector.
    QVector3D       worldPosition;
    QVector3D       worldDirection  = { 0.0f,  0.0f, -1.0f };
    QVector3D       worldUp         = { 0.0f,  1.0f,  0.0f };
};

// All camera information for one loaded glTF/GLB source file.
struct GltfCameraData
{
    QString                    sourceFile;
    QVector<GltfCameraEntry>   cameras;

    bool isEmpty() const { return cameras.isEmpty(); }
};
