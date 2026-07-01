#include "VisibilityComputationHelper.h"
#include "SceneMesh.h"

namespace VisibilityComputationHelper {

// ---- Frustum tests ---------------------------------------------------------

bool isBoundingBoxOutside(const BoundingBox& bb, const FrustumContext& ctx)
{
    for (int i = 0; i < 6; ++i)
    {
        const QVector4D& p = ctx.planes[i];
        const float sx = p.x() >= 0.0f ? static_cast<float>(bb.xMax()) : static_cast<float>(bb.xMin());
        const float sy = p.y() >= 0.0f ? static_cast<float>(bb.yMax()) : static_cast<float>(bb.yMin());
        const float sz = p.z() >= 0.0f ? static_cast<float>(bb.zMax()) : static_cast<float>(bb.zMin());
        if (p.x() * sx + p.y() * sy + p.z() * sz + p.w() < 0.0f)
            return true;
    }
    return false;
}

bool isMeshOutside(const SceneMesh* mesh, const FrustumContext& ctx)
{
    return mesh ? isBoundingBoxOutside(mesh->getBoundingBox(), ctx) : true;
}

bool isMeshFullyInside(const SceneMesh* mesh, const FrustumContext& ctx)
{
    if (!mesh) return false;
    const BoundingBox& bb = mesh->getBoundingBox();
    for (int i = 0; i < 6; ++i)
    {
        const QVector4D& p = ctx.planes[i];
        // Negative support point — the AABB corner least inside this plane.
        const float sx = p.x() >= 0.0f ? static_cast<float>(bb.xMin()) : static_cast<float>(bb.xMax());
        const float sy = p.y() >= 0.0f ? static_cast<float>(bb.yMin()) : static_cast<float>(bb.yMax());
        const float sz = p.z() >= 0.0f ? static_cast<float>(bb.zMin()) : static_cast<float>(bb.zMax());
        if (p.x() * sx + p.y() * sy + p.z() * sz + p.w() < 0.0f)
            return false;
    }
    return true;
}

// ---- Clip-plane tests (per axis) ------------------------------------------

bool isBoundingBoxFullyClipped_X(const BoundingBox& bb, const ClippingContext& ctx)
{
    return ctx.x.flipped
        ? static_cast<float>(bb.xMax()) < ctx.x.threshold
        : static_cast<float>(bb.xMin()) > ctx.x.threshold;
}

bool isBoundingBoxFullyClipped_Y(const BoundingBox& bb, const ClippingContext& ctx)
{
    return ctx.y.flipped
        ? static_cast<float>(bb.yMax()) < ctx.y.threshold
        : static_cast<float>(bb.yMin()) > ctx.y.threshold;
}

bool isBoundingBoxFullyClipped_Z(const BoundingBox& bb, const ClippingContext& ctx)
{
    return ctx.z.flipped
        ? static_cast<float>(bb.zMax()) < ctx.z.threshold
        : static_cast<float>(bb.zMin()) > ctx.z.threshold;
}

bool isMeshFullyClipped_X(const SceneMesh* mesh, const ClippingContext& ctx)
{
    return mesh ? isBoundingBoxFullyClipped_X(mesh->getBoundingBox(), ctx) : true;
}

bool isMeshFullyClipped_Y(const SceneMesh* mesh, const ClippingContext& ctx)
{
    return mesh ? isBoundingBoxFullyClipped_Y(mesh->getBoundingBox(), ctx) : true;
}

bool isMeshFullyClipped_Z(const SceneMesh* mesh, const ClippingContext& ctx)
{
    return mesh ? isBoundingBoxFullyClipped_Z(mesh->getBoundingBox(), ctx) : true;
}

bool isBoundingBoxFullyKept_X(const BoundingBox& bb, const ClippingContext& ctx)
{
    return ctx.x.flipped
        ? static_cast<float>(bb.xMin()) >= ctx.x.threshold
        : static_cast<float>(bb.xMax()) <= ctx.x.threshold;
}

bool isBoundingBoxFullyKept_Y(const BoundingBox& bb, const ClippingContext& ctx)
{
    return ctx.y.flipped
        ? static_cast<float>(bb.yMin()) >= ctx.y.threshold
        : static_cast<float>(bb.yMax()) <= ctx.y.threshold;
}

bool isBoundingBoxFullyKept_Z(const BoundingBox& bb, const ClippingContext& ctx)
{
    return ctx.z.flipped
        ? static_cast<float>(bb.zMin()) >= ctx.z.threshold
        : static_cast<float>(bb.zMax()) <= ctx.z.threshold;
}

bool isMeshFullyKept_X(const SceneMesh* mesh, const ClippingContext& ctx)
{
    return mesh ? isBoundingBoxFullyKept_X(mesh->getBoundingBox(), ctx) : false;
}

bool isMeshFullyKept_Y(const SceneMesh* mesh, const ClippingContext& ctx)
{
    return mesh ? isBoundingBoxFullyKept_Y(mesh->getBoundingBox(), ctx) : false;
}

bool isMeshFullyKept_Z(const SceneMesh* mesh, const ClippingContext& ctx)
{
    return mesh ? isBoundingBoxFullyKept_Z(mesh->getBoundingBox(), ctx) : false;
}

bool isBoundingBoxStraddlesCapPlane(const BoundingBox& bb, int planeIndex,
                                    const ClippingContext& ctx)
{
    switch (planeIndex)
    {
    case 0: return !isBoundingBoxFullyClipped_X(bb, ctx) && !isBoundingBoxFullyKept_X(bb, ctx);
    case 1: return !isBoundingBoxFullyClipped_Y(bb, ctx) && !isBoundingBoxFullyKept_Y(bb, ctx);
    case 2: return !isBoundingBoxFullyClipped_Z(bb, ctx) && !isBoundingBoxFullyKept_Z(bb, ctx);
    default: return true;
    }
}

bool isMeshStraddlesCapPlane(const SceneMesh* mesh, int planeIndex,
                             const ClippingContext& ctx)
{
    return mesh ? isBoundingBoxStraddlesCapPlane(mesh->getBoundingBox(), planeIndex, ctx) : false;
}

bool isBoundingBoxInvisibleInAllClipPasses(const BoundingBox& bb, const ClippingContext& ctx)
{
    if (ctx.yzEnabled && !isBoundingBoxFullyClipped_X(bb, ctx)) return false;
    if (ctx.zxEnabled && !isBoundingBoxFullyClipped_Y(bb, ctx)) return false;
    if (ctx.xyEnabled && !isBoundingBoxFullyClipped_Z(bb, ctx)) return false;
    return true;
}

bool isMeshInvisibleInAllClipPasses(const SceneMesh* mesh, const ClippingContext& ctx)
{
    return mesh ? isBoundingBoxInvisibleInAllClipPasses(mesh->getBoundingBox(), ctx) : true;
}

} // namespace VisibilityComputationHelper
