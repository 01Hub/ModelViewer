# Feature-Edge Wireframe System

**Date:** 2026-06-25
**Branch:** `feature/mesh-feature-edge-detection`

---

## Overview

ModelViewer has two separate wireframe display modes that draw only the silhouette / feature edges of a mesh rather than all triangle edges:

| Display mode | GL call | Use case |
|---|---|---|
| `WIREFRAME` | `glDrawArrays(GL_LINES)` from a dedicated edge VAO | Full wireframe — feature edges only |
| `SHADED_WITH_EDGES` | solid PBR pass + `glDrawArrays(GL_LINES)` overlay | Shaded with crease overlay |
| `MESH_EDGES` | solid PBR pass + `glPolygonMode(GL_LINE)` over all triangles | Shaded with all triangle edges |

The first two modes use the **feature-edge pipeline** described in this document. `MESH_EDGES` is a simpler full-triangle-wireframe overlay and does not use the edge VAO.

There are two independent implementations of the feature-edge pipeline, selected at runtime based on file format:

- **Heuristic classifier** — used for OBJ, glTF, FBX, and any mesh without B-Rep topology. Detects feature edges by measuring the dihedral angle between adjacent triangle faces.
- **OCC B-Rep extractor** — used for STEP, IGES, and native BREP files. Extracts exact analytical edges directly from the OpenCASCADE topology model.

When an OCC edge set is present it always takes priority over the heuristic classifier for that mesh.

---

## Heuristic Feature-Edge Classifier

### Concept

A shared mesh edge between two triangles is a *feature edge* if the angle between the two face normals exceeds a threshold (default 15°). Edges on mesh boundaries (only one adjacent triangle) are always included.

### Pre-computation

`AssImpMesh::buildAndUploadFeatureEdges(threshold)` runs after the mesh VBO is built:

```
for each unique edge in the index buffer:
    look up the one or two triangles that share this edge
    if boundary (one triangle):
        include edge
    else:
        compute dihedral angle between the two face normals
        if angle > threshold:
            include edge

upload selected edge indices to _featureEdgeIndexBuffer (GL_ELEMENT_ARRAY_BUFFER)
bind into _featureEdgeVAO sharing the same vertex positions as the main VBO
store count in _featureEdgeCount
```

The edge index buffer stores pairs of indices into the existing vertex array — no new vertex data is duplicated.

### Rendering

`AssImpMesh::renderFeatureEdgesFast(wireProg)` (OCC-absent path):

```
if _featureEdgeVAO not created or _featureEdgeCount == 0: return

set modelMatrix, baseColor uniforms on wireProg
bind _featureEdgeVAO
glDrawElements(GL_LINES, _featureEdgeCount, GL_UNSIGNED_INT, 0)
release VAO
```

---

## OCC B-Rep Edge Extractor

### Concept

For STEP/IGES/BREP files the geometry is derived from OpenCASCADE's B-Rep kernel. Every `TopoDS_Face` carries a list of bounding `TopoDS_Edge` objects that represent exact analytical curves (lines, circles, splines, etc.). These are the canonical feature edges of the solid — no angle heuristic is needed.

### Extraction

`BRepToAssimpConverter::extractEdgesFromFaceGroup(faceGroup, deflection)` runs once per mesh group during OCC conversion:

```
collect all unique TopoDS_Edge objects from every face in faceGroup
    (deduplication via TopTools_IndexedMapOfShape — two faces sharing the
     same underlying TShape pointer map to the same entry)

for each unique edge:
    skip if BRep_Tool::Degenerated (zero-length: cone apex, sphere pole)
    wrap in BRepAdaptor_Curve
    tessellate with GCPnts_TangentialDeflection(adaptor, deflection, angDefl=0.1)
    emit consecutive point pairs as line segments:
        for i in [1, NbPoints-1]:
            push p[i].xyz, p[i+1].xyz  → flat float array {x0,y0,z0, x1,y1,z1, ...}
```

The result is a flat `OccEdgeSegments = std::vector<float>` of segment endpoints in model space. Each pair of consecutive vec3 values is one line segment endpoint — there is no shared vertex structure.

### Cache

The converter stores `OccEdgeSegments` in a static map keyed by the `aiMesh*` produced for that face group:

```
s_occEdges[resultAiMesh] = move(segments)
```

`getPrecomputedEdges(aiMesh*)` retrieves the entry. `clearEdgeCache()` is called alongside `clearColorCache()` before each new file load so no segments leak across documents.

### GPU Upload

`AssImpMesh::setPrecomputedOccEdges(edgeVerts, bounds)`:

```
store edgeVerts in _occEdgeSegments    ← CPU copy retained for clone/MVF
store bounds   in _occEdgeBoundaries   ← CPU copy retained for clone/MVF/future picking
_occEdgeCount = edgeVerts.size() / 3  (number of vec3 endpoints)

create _occEdgeVertexBuffer (GL_ARRAY_BUFFER, StaticDraw)
upload raw float data

create _occEdgeVAO
  bind position-only layout: attribute "vertexPosition", 3 floats, stride 12
```

No index buffer — the segments are already ordered as endpoint pairs for `GL_LINES`.

### Rendering

`AssImpMesh::renderFeatureEdgesFast(wireProg)` — OCC-present path (takes priority):

```
if _occEdgeVAO created and _occEdgeCount > 0:
    set modelMatrix, baseColor uniforms
    bind _occEdgeVAO
    glDrawArrays(GL_LINES, 0, _occEdgeCount)
    release VAO
    return              ← heuristic path is skipped entirely
```

---

## Data Pipeline

```
                    ┌─ OBJ / glTF / FBX ─────────────────────────────────────┐
                    │                                                           │
File load           │  AssImpModelLoader::processMesh()                        │
                    │    meshData.precomputedOccEdges = empty                  │
                    │                                                           │
                    └───────────────────────────────────────────────────────────┘

                    ┌─ STEP / IGES / BREP ────────────────────────────────────┐
                    │                                                           │
File load           │  BRepToAssimpConverter::convertFaceGroupToMesh()         │
                    │    extractEdgesFromFaceGroup(faceGroup, deflection)       │
                    │    s_occEdges[aiMesh*] = segments                        │
                    │                                                           │
                    │  AssImpModelLoader::processMesh()                        │
                    │    meshData.precomputedOccEdges = *getPrecomputedEdges() │
                    │                                                           │
                    └───────────────────────────────────────────────────────────┘

                              ↓ (both paths)

GLWidget::createMeshFromMeshData()
    new AssImpMesh(...)                         ← VBO + heuristic edge VAO built here
    if meshData.precomputedOccEdges not empty:
        mesh->setPrecomputedOccEdges(...)       ← OCC edge VAO built here

render() → drawOpaqueMeshes()
    if WIREFRAME or SHADED_WITH_EDGES:
        mesh->renderFeatureEdgesFast(wireShader)
            → OCC path if _occEdgeVAO present
            → heuristic path otherwise
```

---

## Display Mode GL State

### WIREFRAME

```
glLineWidth(1.75f)
bind _wireframeShader
for each mesh:
    renderFeatureEdgesFast()   → glDrawArrays/glDrawElements(GL_LINES)
glLineWidth(1.0f)
```

No `glPolygonMode` changes — the edge VAO draws `GL_LINES` primitives directly.

### SHADED_WITH_EDGES

```
// Solid pass — push polygon depth back so edge lines pass depth test
glEnable(GL_POLYGON_OFFSET_FILL)
glPolygonOffset(+1.25, +1.25)
for each mesh: mesh->render()   → full PBR

// Edge overlay
glDisable(GL_POLYGON_OFFSET_FILL)
glLineWidth(1.5f)
bind _wireframeShader
for each mesh:
    renderFeatureEdgesFast()
glLineWidth(1.0f)
bind prog               ← explicit rebind so isWireframePass uniform targets fgShader
prog->setUniformValue("isWireframePass", false)
```

### MESH_EDGES (all triangle edges, not feature edges)

```
// Solid pass
glPolygonMode(GL_FILL)
for each mesh: mesh->render()

// Wire pass — rasterise triangle outlines
glPolygonMode(GL_LINE)
glLineWidth(1.5f)
glEnable(GL_POLYGON_OFFSET_LINE)   ← LINE not FILL; brings wire forward
glPolygonOffset(-1.0, -1.0)
bind _wireframeShader
for each mesh: mesh->renderWireframeFast()   → glDrawElements on main VAO

// Cleanup
glDisable(GL_POLYGON_OFFSET_LINE)
glPolygonMode(GL_FILL)
glLineWidth(1.0f)
bind prog
prog->setUniformValue("isWireframePass", false)
```

### Frame-start defensive reset (`render()`)

To guard against workstation GPU drivers that do not flush state changes immediately:

```
// Added to the GL state reset at the top of every render() call:
glDisable(GL_POLYGON_OFFSET_LINE)
glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)
glLineWidth(1.0f)
```

This ensures a display-mode switch always starts from a clean slate regardless of what the previous frame left behind.

---

## Clone Propagation

`AssImpMesh` retains the OCC edge data as a CPU-side `_occEdgeSegments` member (in addition to the GPU VAO/VBO). `clone()` propagates it:

```
clone():
    new mesh from _baseVertices, _indices, _textures, _material
    copy morph targets
    if _occEdgeSegments not empty:
        mesh->setPrecomputedOccEdges(_occEdgeSegments, _occEdgeBoundaries)
    return mesh
```

Without the CPU copy, cloned STEP meshes would silently fall back to the heuristic classifier.

---

## MVF Persistence

OCC edges survive an MVF save/load cycle through the standard binary geometry chunk mechanism.

### Write (`MvfSceneBuilder`)

```
if mesh has non-empty getOccEdgeSegments():
    append raw float data to geometryChunk
    create bufferView pointing to that byte range  (target = 0, no GPU hint)
    create accessor: componentType=FLOAT, type="VEC3", count=N/3
    store accessor index in primitiveExtras["occEdgeAccessor"]
```

### Read (`prepareMvfMeshes`)

```
occEdgeAccessorIndex = extras["occEdgeAccessor"]  (-1 if absent)
prepared.occEdgeSegments = readFloatStream(geometryChunk, accessors, bufferViews,
                                           occEdgeAccessorIndex)
```

`readFloatStream` returns an empty vector when the index is -1, so OBJ/glTF meshes without the key are unaffected.

### Upload (`uploadPreparedMvfMeshes` / `uploadOneMvfMesh`)

```
if pm.occEdgeSegments not empty:
    mesh->setPrecomputedOccEdges(pm.occEdgeSegments)
```

The `PreparedMvfMesh` struct carries `std::vector<float> occEdgeSegments` and `std::vector<int> occEdgeBoundaries` as plain-data fields — no GL resources, safe to pass across threads.

---

## Hover & Selection Highlighting

Hover and selection are **mesh-level** in all display modes — edges are purely a visual representation of the containing mesh's state, not independently selectable primitives. Sub-edge selection (individual topological edge pick for measurements, fillets, annotations) is reserved for a future phase; the `_occEdgeBoundaries` boundary table is already in place to support it when needed.

### Detection mechanism

Both hover and selection use the existing `SelectionManager` FBO path, which renders every mesh as a solid colour-coded quad into an offscreen framebuffer regardless of the current display mode. This means `getHoveredId()` and `getSelectedIds()` return correct mesh IDs even when the viewport shows only edges in WIREFRAME mode.

No custom edge-proximity picking loop is needed. The FBO hover update runs during `mouseMoveEvent` via the existing `_selectionManager->hoverSelect(e->pos())` call.

### Wireframe shader uniforms

`wireframe.frag` carries two boolean uniforms that `GLWidget` sets per mesh before each `renderFeatureEdgesFast` call:

| Uniform | Type | Meaning |
|---|---|---|
| `hovered` | `bool` | this mesh is the current FBO-hover target |
| `hoverColor` | `vec3` | edge replacement colour while hovered (gold `{1, 0.84, 0}`) |
| `selected` | `bool` | this mesh is in `SelectionManager::getSelectedIds()` |
| `selectedColor` | `vec3` | edge replacement colour while selected (blue `{0.25, 0.55, 1}`) |

`selected` is evaluated first; a selected mesh never shows hover colour (prevents a flickering colour fight when the cursor rests on an already-selected mesh).

### Mode-dependent colour logic

The `isWireframePass` flag (already used to choose between the two normal-colour paths) also gates colour replacement:

```glsl
// Pure WIREFRAME (isWireframePass = false):
//   edges are the sole visual signal → replace colour directly
if (!isWireframePass)
{
    if (hovered)  { fragColor = vec4(hoverColor,    1.0); return; }
    if (selected) { fragColor = vec4(selectedColor, 1.0); return; }
}
// SHADED_WITH_EDGES (isWireframePass = true):
//   solid mesh already carries hover/selection brightening from main_scene.frag
//   → edge overlay uses normal material colour; colour replacement is suppressed
```

### GLWidget render loop

For each of the four wireframe loops (opaque WIREFRAME, opaque SHADED_WITH_EDGES, transparent WIREFRAME, transparent SHADED_WITH_EDGES):

```
selIds = _selectionManager->getSelectedIds()
for each mesh id:
    isSel = selIds.contains(id)
    setUniform("selected", isSel)
    setUniform("hovered",  !isSel && hoverHighlightingEnabled
                           && id == _selectionManager->getHoveredId())
    renderFeatureEdgesFast()
reset selected = false, hovered = false
```

### Boundary table (reserved for future sub-edge features)

`BRepToAssimpConverter::extractEdgesFromFaceGroup` records the start of each topological edge in `OccEdgeData::bounds`:

```
bounds[i]    = first vec3-index of topological edge i in the flat segment array
bounds.back()= total vec3 count  (sentinel)
```

Stored in `AssImpMesh::_occEdgeBoundaries` (CPU), propagated through clone and MVF (`"occEdgeBounds"` JSON int array). When sub-edge picking is implemented (fillet, measurement, annotation), this table enables mapping a screen-space hit to the corresponding topological `TopoDS_Edge` without re-querying OCC.

---

## File Map

| File | Role |
|---|---|
| `include/BRepToAssimpConverter.h` | `OccEdgeData` struct (`segments` + `bounds`); `getPrecomputedEdges()`, `clearEdgeCache()`, `extractEdgesFromFaceGroup()` |
| `src/BRepToAssimpConverter.cpp` | Edge extraction; boundary recording; cache cleared with colour cache |
| `include/AssImpModelLoader.h` | `precomputedOccEdges` + `precomputedOccEdgeBoundaries` in `AssImpMeshData` |
| `src/AssImpModelLoader.cpp` | Transfers both fields from converter cache |
| `include/AssImpMesh.h` | `setPrecomputedOccEdges(segs, bounds)`; `_occEdgeSegments`, `_occEdgeBoundaries` CPU copies; `_occEdgeVAO`/`_occEdgeCount` GPU resources |
| `src/AssImpMesh.cpp` | `setPrecomputedOccEdges()` — uploads GPU buffer, stores CPU copies; `buildAndUploadFeatureEdges()` — heuristic path; `renderFeatureEdgesFast()` — draws all edges in one call; `clone()` propagation |
| `shaders/wireframe.frag` | `hovered`/`hoverColor`/`selected`/`selectedColor` uniforms; colour replacement gated on `!isWireframePass` |
| `src/GLWidget.cpp` | `createMeshFromMeshData()` handoff; wireframe render loops set hover/select uniforms from `SelectionManager`; MVF read/upload; `render()` defensive GL reset |
| `include/GLWidget.h` | `PreparedMvfMesh::occEdgeSegments` + `occEdgeBoundaries` |
| `src/MvfSceneBuilder.cpp` | MVF write: binary accessor for segments + JSON int array for bounds (`"occEdgeBounds"`) |
