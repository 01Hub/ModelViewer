#include "ExplodedViewManager.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

void ExplodedViewManager::reset()
{
    _baseOffsets.clear();
    _scaledOffsets.clear();
    _factor = 1.0f;
}

QVector3D ExplodedViewManager::offsetForMesh(const QUuid& uuid) const
{
    return _scaledOffsets.value(uuid, QVector3D());
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Returns the (min, max) projection of an AABB onto a unit axis direction.
// Uses the AABB support function: for each component, pick min or max based on
// the sign of axisDir.  Works for any axis direction, not just canonical axes.
static std::pair<float, float>
aabbAxisExtent(const QPair<QVector3D, QVector3D>& box, const QVector3D& axisDir)
{
    const QVector3D& bMin = box.first;
    const QVector3D& bMax = box.second;

    float pMin = 0.0f, pMax = 0.0f;
    const float dx = axisDir.x(), dy = axisDir.y(), dz = axisDir.z();
    pMin = std::min(dx * bMin.x(), dx * bMax.x())
         + std::min(dy * bMin.y(), dy * bMax.y())
         + std::min(dz * bMin.z(), dz * bMax.z());
    pMax = std::max(dx * bMin.x(), dx * bMax.x())
         + std::max(dy * bMin.y(), dy * bMax.y())
         + std::max(dz * bMin.z(), dz * bMax.z());
    return {pMin, pMax};
}

static QVector3D sectorDirection(int sector)
{
    switch (sector)
    {
    case 0: return QVector3D(1, 0, 0);
    case 1: return QVector3D(-1, 0, 0);
    case 2: return QVector3D(0, 1, 0);
    case 3: return QVector3D(0, -1, 0);
    case 4: return QVector3D(0, 0, 1);
    default: return QVector3D(0, 0, -1);
    }
}

static int dominantAxisSector(const QUuid& uuid,
                              const QVector3D& origin,
                              const QVector3D& center,
                              const QHash<QUuid, QPair<QVector3D, QVector3D>>& worldBoxes)
{
    const QVector3D delta = center - origin;
    const float absX = std::abs(delta.x());
    const float absY = std::abs(delta.y());
    const float absZ = std::abs(delta.z());

    if (absX > 1e-4f || absY > 1e-4f || absZ > 1e-4f)
    {
        if (absX >= absY && absX >= absZ)
            return delta.x() >= 0.0f ? 0 : 1;
        if (absY >= absX && absY >= absZ)
            return delta.y() >= 0.0f ? 2 : 3;
        return delta.z() >= 0.0f ? 4 : 5;
    }

    if (worldBoxes.contains(uuid))
    {
        const auto& box = worldBoxes.value(uuid);
        const QVector3D& bMin = box.first;
        const QVector3D& bMax = box.second;

        const float scores[6] = {
            bMax.x() - origin.x(),
            origin.x() - bMin.x(),
            bMax.y() - origin.y(),
            origin.y() - bMin.y(),
            bMax.z() - origin.z(),
            origin.z() - bMin.z()
        };

        int bestIdx = 0;
        float bestScore = scores[0];
        for (int i = 1; i < 6; ++i)
        {
            if (scores[i] > bestScore)
            {
                bestScore = scores[i];
                bestIdx = i;
            }
        }
        if (bestScore > 1e-4f)
            return bestIdx;
    }

    return static_cast<int>(qHash(uuid) % 6u);
}

// ---------------------------------------------------------------------------
// recompute
// ---------------------------------------------------------------------------
void ExplodedViewManager::recompute(
    const QSet<QUuid>&                          assemblyUuids,
    const QUuid&                                anchorUuid,
    Mode                                        mode,
    const QVector3D&                            userVector,
    float                                       factor,
    const QHash<QUuid, QVector3D>&              worldCentroids,
    const QHash<QUuid, QPair<QVector3D,QVector3D>>& worldBoxes)
{
    reset();

    if (assemblyUuids.isEmpty())
        return;

    _factor = factor;

    // --- Assembly centroid (used as explosion origin for Auto/no-anchor) ---
    QVector3D origin;
    {
        int count = 0;
        for (const QUuid& uuid : assemblyUuids)
        {
            if (!worldCentroids.contains(uuid)) continue;
            origin += worldCentroids.value(uuid);
            ++count;
        }
        if (count > 0) origin /= static_cast<float>(count);
    }
    if (!anchorUuid.isNull() && worldCentroids.contains(anchorUuid))
        origin = worldCentroids.value(anchorUuid);

    // --- Assembly bounding radius (used for gap sizing in all modes) ---
    float assemblyRadius = 0.0f;
    for (const QUuid& uuid : assemblyUuids)
    {
        if (!worldCentroids.contains(uuid)) continue;
        assemblyRadius = std::max(assemblyRadius,
            (worldCentroids.value(uuid) - origin).length());
    }
    if (assemblyRadius < 1e-6f) assemblyRadius = 1.0f;

    // --- Axis direction ---
    QVector3D axisDir;
    bool fixedAxis = false;
    switch (mode)
    {
    case Mode::AxisX:  axisDir = {1,0,0}; fixedAxis = true; break;
    case Mode::AxisY:  axisDir = {0,1,0}; fixedAxis = true; break;
    case Mode::AxisZ:  axisDir = {0,0,1}; fixedAxis = true; break;
    case Mode::Vector:
        if (userVector.lengthSquared() > 1e-9f)
        { axisDir = userVector.normalized(); fixedAxis = true; }
        break;
    default: break;
    }

    // ======================================================================
    // AXIAL MODE  —  bbox-clearance-based placement
    // ======================================================================
    if (fixedAxis)
    {
        // Collect per-mesh data: centroid projection + bbox min/max along axis.
        struct MeshData
        {
            QUuid  uuid;
            float  centerProj; // centroid projected onto axisDir
            float  projMin;    // bbox min projected onto axisDir
            float  projMax;    // bbox max projected onto axisDir
            float  size() const { return projMax - projMin; }
        };

        std::vector<MeshData> parts;
        parts.reserve(static_cast<size_t>(assemblyUuids.size()));

        for (const QUuid& uuid : assemblyUuids)
        {
            if (!worldCentroids.contains(uuid)) continue;

            const float cp = QVector3D::dotProduct(worldCentroids.value(uuid), axisDir);
            float pMin = cp, pMax = cp; // fallback: point extent

            if (worldBoxes.contains(uuid))
            {
                auto [mn, mx] = aabbAxisExtent(worldBoxes.value(uuid), axisDir);
                pMin = mn; pMax = mx;
            }
            parts.push_back({uuid, cp, pMin, pMax});
        }

        if (parts.empty()) return;

        std::sort(parts.begin(), parts.end(),
                  [](const MeshData& a, const MeshData& b)
                  { return a.centerProj < b.centerProj; });

        const int n = static_cast<int>(parts.size());

        // Gap between adjacent bounding boxes at factor = 1.
        // Use the average part size along the axis so that the gap is
        // proportional to part geometry, not the overall assembly extent.
        float totalSize = 0.0f;
        for (const auto& p : parts) totalSize += p.size();
        const float avgSize  = totalSize / n;
        const float baseGap  = std::max(avgSize * 0.5f, assemblyRadius * 0.05f);

        // ----------------------------------------------------------------
        // WITH ANCHOR  —  anchor pinned, parts placed by bbox-clearance
        // ----------------------------------------------------------------
        if (!anchorUuid.isNull() && worldCentroids.contains(anchorUuid))
        {
            // Find anchor's rank in sorted list.
            int refIdx = 0;
            for (int i = 0; i < n; ++i)
                if (parts[i].uuid == anchorUuid) { refIdx = i; break; }

            const float anchorProjMin = parts[refIdx].projMin;
            const float anchorProjMax = parts[refIdx].projMax;

            // Parts above anchor (higher projection → move in +axisDir).
            float prevMax = anchorProjMax;
            for (int i = refIdx + 1; i < n; ++i)
            {
                const float targetMin = prevMax + baseGap;
                const float disp      = targetMin - parts[i].projMin;
                _baseOffsets.insert(parts[i].uuid, axisDir * disp);
                prevMax = targetMin + parts[i].size();
            }

            // Parts below anchor (lower projection → move in -axisDir).
            float prevMin = anchorProjMin;
            for (int i = refIdx - 1; i >= 0; --i)
            {
                const float targetMax = prevMin - baseGap;
                const float disp      = targetMax - parts[i].projMax;
                _baseOffsets.insert(parts[i].uuid, axisDir * disp);
                prevMin = targetMax - parts[i].size();
            }
        }
        // ----------------------------------------------------------------
        // NO ANCHOR  —  layout centred at assembly centroid projection
        // ----------------------------------------------------------------
        else
        {
            // Total layout width = sum of all part sizes + (n-1) gaps.
            const float layoutWidth = totalSize + (n - 1) * baseGap;
            const float centroidProj = QVector3D::dotProduct(origin, axisDir);

            // Start placing from the left edge of the centred layout.
            float cursor = centroidProj - layoutWidth * 0.5f;
            for (int i = 0; i < n; ++i)
            {
                const float targetMin = cursor;
                const float disp      = targetMin - parts[i].projMin;
                if (std::abs(disp) > 1e-9f)
                    _baseOffsets.insert(parts[i].uuid, axisDir * disp);
                cursor += parts[i].size() + baseGap;
            }
        }

        applyFactor();
        return;
    }

    // ======================================================================
    // AUTO MODE  —  assign each part to a dominant canonical sector, then
    //               pack outward using bbox-clearance placement per sector.
    // ======================================================================
    struct MeshData
    {
        QUuid  uuid;
        float  centerProj;
        float  projMin;
        float  projMax;
        float  size() const { return projMax - projMin; }
    };

    std::vector<MeshData> sectors[6];
    for (const QUuid& uuid : assemblyUuids)
    {
        if (uuid == anchorUuid || !worldCentroids.contains(uuid))
            continue;

        const int sector = dominantAxisSector(uuid, origin, worldCentroids.value(uuid), worldBoxes);
        const QVector3D dir = sectorDirection(sector);

        const float cp = QVector3D::dotProduct(worldCentroids.value(uuid), dir);
        float pMin = cp;
        float pMax = cp;
        if (worldBoxes.contains(uuid))
        {
            auto [mn, mx] = aabbAxisExtent(worldBoxes.value(uuid), dir);
            pMin = mn;
            pMax = mx;
        }
        sectors[sector].push_back({uuid, cp, pMin, pMax});
    }

    for (int sector = 0; sector < 6; ++sector)
    {
        auto& parts = sectors[sector];
        if (parts.empty())
            continue;

        const QVector3D dir = sectorDirection(sector);
        const float originProj = QVector3D::dotProduct(origin, dir);

        std::sort(parts.begin(), parts.end(),
                  [](const MeshData& a, const MeshData& b)
                  { return a.centerProj < b.centerProj; });

        float totalSize = 0.0f;
        for (const auto& p : parts)
            totalSize += p.size();

        const float avgSize = totalSize / static_cast<float>(parts.size());
        const float baseGap = std::max(avgSize * 0.35f, assemblyRadius * 0.05f);

        float cursor = baseGap;
        if (!anchorUuid.isNull() && worldBoxes.contains(anchorUuid))
        {
            auto [anchorMin, anchorMax] = aabbAxisExtent(worldBoxes.value(anchorUuid), dir);
            Q_UNUSED(anchorMin);
            cursor = std::max(cursor, (anchorMax - originProj) + baseGap);
        }

        for (const auto& part : parts)
        {
            const float relMin = part.projMin - originProj;
            const float targetMin = std::max(cursor, relMin);
            const float disp = targetMin - relMin;

            if (disp > 1e-6f)
                _baseOffsets.insert(part.uuid, dir * disp);

            cursor = targetMin + part.size() + baseGap;
        }
    }

    applyFactor();
}

void ExplodedViewManager::applyFactor()
{
    _scaledOffsets.clear();
    for (auto it = _baseOffsets.cbegin(); it != _baseOffsets.cend(); ++it)
        _scaledOffsets.insert(it.key(), it.value() * _factor);
}
