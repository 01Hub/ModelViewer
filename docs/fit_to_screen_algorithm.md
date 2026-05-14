# Fit-to-Screen Algorithm

**Date:** 2026-05-14

---

## Overview

The fit-to-screen algorithm computes the minimum `viewRange` parameter that makes all visible geometry appear within the viewport, and the 3-D world point (`projCenter`) that must sit at screen centre so that equal margins surround the geometry on every side.  It works correctly for any view orientation (standard views, isometric, dimetric, trimetric, free orbit) and for both orthographic and perspective projections.

The implementation lives in three overloads of `GLWidget::computeFitViewRange` (in `src/GLWidget.cpp`) plus the helper `collectVisibleCorners`.

---

## Camera model

`GLCamera` uses a **pan-based orbit** model:

- The camera eye is placed at `_position` (world space) and looks along `_viewDir`.
- After a fit operation the camera is animated to `projCenter` — the geometric centre of the visible scene projected onto the view plane.  Placing the eye *at* the orbit target means the projection volume (ortho box or perspective frustum + Z-shift) is centred on the scene.
- **Orthographic** — `GLCamera::updateProjectionMatrix` builds an `ortho` frustum of half-height `halfRange = viewRange / 2` (landscape) or half-width `halfRange` (portrait).  The scene simply needs to fit inside `[-halfRange·aspect, +halfRange·aspect] × [-halfRange, +halfRange]`.
- **Perspective** — a standard `perspective(fovY, aspect, near, far)` matrix is built and then `translate(0, 0, shift)` is appended where `shift = −viewRange · shiftFactor` (clamped to `1.25 · viewRange`).  This Z-shift moves the entire frustum *forward* so that the camera eye can sit at `projCenter` while the geometry appears at the correct depth.

---

## Step 1 — collecting geometry samples (`collectVisibleCorners`)

```
src/GLWidget.cpp  →  GLWidget::collectVisibleCorners()
```

For every visible mesh the function reads `TriangleMesh::getTrsfPoints()` — the flat `float` array of world-space transformed vertex positions — and samples at most **1 024 vertices per mesh** with a uniform stride.  The first and last vertices are always included so that boundary extremes are never missed.

**Why vertices instead of AABB corners?**  
An axis-aligned bounding box has a "phantom corner" problem: combining the maximum-X extent from one part of the mesh with the maximum-Y extent from another part produces a corner that no actual vertex reaches.  For example, the arm tip of the AnisotropyBarnLamp reaches `xMax` while the mounting bracket top reaches `yMax`, but never at the same point.  In an isometric view where `right = (1/√2, 1/√2, 0)`, the phantom corner projects to `(xMax + yMax) / √2` — an over-estimate that causes the fit to zoom out further than necessary, leaving visible empty space at top and bottom.

Sampling actual vertices eliminates all phantom corners and gives the genuine tight silhouette.

**Fallback chain:**
1. If `_trsfPoints` is empty for a mesh → use the 8 AABB corners of that mesh.
2. If the entire sample set is empty → use the 8 corners of the scene AABB (`_boundingBox`).

---

## Step 2 — projecting onto view axes

```
src/GLWidget.cpp  →  GLWidget::computeFitViewRange(corners, right, up, viewDir, outCenter)
```

The three view-space axes are extracted from the **view matrix rows** (not from cached camera vectors, which can diverge during isometric animation):

```
right   =  V.row(0).normalized()
up      =  V.row(1).normalized()
viewDir = -V.row(2).normalized()    // camera looks along -Z in camera space
```

Each sample point `c` is dot-producted with all three axes:

```
xc = dot(c, right)
yc = dot(c, up)
zc = dot(c, viewDir)
```

Running min/max over all points yields `[xMin_v, xMax_v]`, `[yMin_v, yMax_v]`, `[zMin_v, zMax_v]` — the view-space bounding box of the geometry.

---

## Step 3 — projected visual centre

```cpp
cx = (xMin_v + xMax_v) / 2
cy = (yMin_v + yMax_v) / 2
cz = (zMin_v + zMax_v) / 2
projCenter = right·cx + up·cy + viewDir·cz
```

`projCenter` is the 3-D world point whose view-space coordinates are exactly the midpoints of the projected extents.  Setting the orbit target to `projCenter` guarantees **equal margins on all four sides** of the viewport after the camera animates there.

The caller receives `projCenter` via the optional `outCenter` out-parameter and immediately calls `_boundingSphere.setCenter(projCenter)` so that both the fit-all and rotation animations drive the camera to the correct location.

---

## Step 4 — computing `viewRange`

### Orthographic

```
halfX = (xMax_v − xMin_v) / 2
halfY = (yMax_v − yMin_v) / 2

landscape (w > h):  halfRange = max(halfX / aspect, halfY)
portrait  (w ≤ h):  halfRange = max(halfX, halfY · aspect)

viewRange = halfRange × 2 × margin          (margin = 1.05)
```

The GLCamera ortho projection in landscape mode maps `[-halfRange·aspect, +halfRange·aspect]` to the screen width and `[-halfRange, +halfRange]` to the screen height.  The formula guarantees the geometry fits without clipping on any side, with the constraining dimension filled to `1/margin ≈ 95 %`.

### Perspective

The perspective projection matrix is shifted forward by `viewRange · shiftFactor` (where `shiftFactor = min(1.05 / sin(fov/2), 1.25)`), placing the effective focal plane at that distance from the camera eye even though the eye sits at `projCenter`.

For each sample point, the minimum camera distance needed to keep that point inside the frustum is:

```
dc      = dot(c, viewDir) − cz          // depth offset from projCenter

landscape:
  req = max(|xc_rel| / aspect, |yc_rel|) / tan(fov/2) − dc

portrait:
  req = max(|xc_rel|, |yc_rel| · aspect) / tan(fov/2) − dc
```

Near-side corners (`dc < 0`) increase `req`; far-side corners (`dc > 0`) reduce it.  Taking the maximum over all points and dividing by `shiftFactor` gives the required `viewRange`:

```
viewRange = max(req) / shiftFactor × margin
```

---

## Step 5 — bounding-sphere clamp

```cpp
sphereViewRange = _boundingSphere.getRadius() × 2 × margin
return max(min(viewRange, sphereViewRange), 0.0001)
```

For perfectly spherical geometry the AABB corner approach (even per-vertex) can still over-project slightly.  The scene bounding sphere provides an orientation-independent upper bound.  Taking `min(aabb_result, sphere_result)` ensures the tighter estimate wins.

The scene bounding sphere radius is computed in `recalculateVisibleSceneStats` as:

```
bsRadius = max over meshes of: distance(meshSphereCenter, sceneBoxCenter) + meshSphereRadius
```

This is a tight enclosing sphere around the scene AABB midpoint — tighter than the box half-diagonal for compact or spherical objects.

---

## Auto-fit triggers

| Event | Mechanism |
|---|---|
| **F key / Fit All button / context menu** | `fitAll()` — computes fit for current orientation, sets `projCenter`, starts `_animateFitAllTimer` |
| **Standard view switch** (Top, Front, Left, Isometric, …) | `setViewMode()` — computes fit for the *target* orientation before the rotation animation starts, so zoom and rotation animate concurrently |
| **Ortho ↔ Perspective toggle** | `projectionToggled` lambda calls `fitAll()` |
| **Model load** | `fitAll()` called after `recalculateVisibleSceneStats` |

---

## Concurrent rotation + zoom animation

When `setViewMode()` switches to a standard view:

1. The target quaternion is formed from the Euler angles.
2. `computeFitViewRange` is called with the **target orientation's** right/up/viewDir (extracted from the quaternion rotation matrix, not from the live camera).
3. `_boundingSphere.setCenter(projCenter)` is set immediately.
4. `_animateViewTimer` starts, calling `animateViewChange()` → `setRotations()` on each tick.

Inside `setRotations()`:

```cpp
// Camera position interpolates from current to projCenter
curPos = currentTranslation·(1 − t) + boundingSphere.getCenter()·t

// viewRange interpolates toward _viewBoundingSphereDia each tick
scaleStep = (currentViewRange − targetViewRange) × slerpFrac
_viewRange -= scaleStep
```

Because both rotation *and* zoom update on every tick, the two animations run in parallel — the view reaches the correct orientation and zoom level simultaneously.

---

## Key design decisions and why

| Decision | Rationale |
|---|---|
| View axes from **view matrix rows**, not cached camera vectors | Cached vectors diverge during mid-animation isometric frames; matrix rows are always authoritative |
| **Absolute** dot products (not relative to old sphere centre) | Relative projections assume the old sphere centre is the visual centre — it isn't for asymmetric scenes, causing the fit to be off-centre |
| **projCenter = midpoint of view-space extents** | Only the midpoint guarantees equal margins; any other choice leaves the model shifted toward one side |
| Sphere clamp as **upper bound** (`min`, not replacement) | For flat or elongated objects the AABB-based result is tighter; for isotropic/spherical objects the sphere prevents the 8-corner √3 over-projection |
| Vertex sampling instead of AABB corners | Eliminates phantom AABB corners and gives the genuine tight silhouette of the actual geometry |
| `MAX_SAMPLES_PER_MESH = 1024` | Sufficient to capture all silhouette extremes of typical CAD/glTF meshes; keeps the fit O(1 024 · N_meshes) regardless of polygon count |
