#pragma once

// ---------------------------------------------------------------------------
// VisibilityComputationHelper
//
// Pure frustum- and clip-plane culling math, extracted from ViewportWidget so it
// can be used and tested without a GL context.
//
// Callers build a FrustumContext once per frame (after extractFrustumPlanes)
// and a ClippingContext once per frame (or whenever clipping state changes).
// Both structs are plain data — no GL, no Qt signals.
// ---------------------------------------------------------------------------

#include "BoundingBox.h"
#include <QVector4D>

class SceneMesh;

namespace VisibilityComputationHelper {

// Six normalised view-frustum planes, as produced by
// ViewportInteractionController::extractFrustumPlanes().
struct FrustumContext
{
    QVector4D planes[6] = {};
};

// Per-axis clipping state derived from SceneRenderController + scene AABB center.
struct ClipAxisContext
{
    float threshold = 0.0f;  // renderCtrl.clippingCoeff + boundingBox.center.axis
    bool  flipped   = false; // renderCtrl.clippingFlipped
};

struct ClippingContext
{
    ClipAxisContext x, y, z;
    bool yzEnabled = false;  // YZ clipping plane active (clips along X)
    bool zxEnabled = false;  // ZX clipping plane active (clips along Y)
    bool xyEnabled = false;  // XY clipping plane active (clips along Z)
};

// ---- Frustum tests ---------------------------------------------------------

bool isBoundingBoxOutside(const BoundingBox& bb, const FrustumContext& ctx);
bool isMeshOutside(const SceneMesh* mesh, const FrustumContext& ctx);
bool isMeshFullyInside(const SceneMesh* mesh, const FrustumContext& ctx);

// ---- Clip-plane tests (per axis) ------------------------------------------

bool isBoundingBoxFullyClipped_X(const BoundingBox& bb, const ClippingContext& ctx);
bool isBoundingBoxFullyClipped_Y(const BoundingBox& bb, const ClippingContext& ctx);
bool isBoundingBoxFullyClipped_Z(const BoundingBox& bb, const ClippingContext& ctx);
bool isMeshFullyClipped_X(const SceneMesh* mesh, const ClippingContext& ctx);
bool isMeshFullyClipped_Y(const SceneMesh* mesh, const ClippingContext& ctx);
bool isMeshFullyClipped_Z(const SceneMesh* mesh, const ClippingContext& ctx);

bool isBoundingBoxFullyKept_X(const BoundingBox& bb, const ClippingContext& ctx);
bool isBoundingBoxFullyKept_Y(const BoundingBox& bb, const ClippingContext& ctx);
bool isBoundingBoxFullyKept_Z(const BoundingBox& bb, const ClippingContext& ctx);
bool isMeshFullyKept_X(const SceneMesh* mesh, const ClippingContext& ctx);
bool isMeshFullyKept_Y(const SceneMesh* mesh, const ClippingContext& ctx);
bool isMeshFullyKept_Z(const SceneMesh* mesh, const ClippingContext& ctx);

// Returns true when the AABB straddles the active clipping plane for the
// given plane index (0=X, 1=Y, 2=Z): it is neither fully clipped nor fully kept.
bool isBoundingBoxStraddlesCapPlane(const BoundingBox& bb, int planeIndex,
                                    const ClippingContext& ctx);
bool isMeshStraddlesCapPlane(const SceneMesh* mesh, int planeIndex,
                             const ClippingContext& ctx);

// Returns true when the AABB is fully clipped away by EVERY enabled clipping
// plane — i.e. the mesh contributes nothing in any clip pass.
bool isBoundingBoxInvisibleInAllClipPasses(const BoundingBox& bb,
                                           const ClippingContext& ctx);
bool isMeshInvisibleInAllClipPasses(const SceneMesh* mesh, const ClippingContext& ctx);

} // namespace VisibilityComputationHelper
