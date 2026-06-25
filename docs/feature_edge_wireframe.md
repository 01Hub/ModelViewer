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

`AssImpMesh::setPrecomputedOccEdges(edgeVerts)`:

```
store edgeVerts in _occEdgeSegments  ← CPU copy retained for clone/MVF
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
        mesh->setPrecomputedOccEdges(_occEdgeSegments)
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

The `PreparedMvfMesh` struct carries `std::vector<float> occEdgeSegments` as a plain-data field — no GL resources, safe to pass across threads.

---

## File Map

| File | Role |
|---|---|
| `include/BRepToAssimpConverter.h` | `OccEdgeSegments` type; `getPrecomputedEdges()`, `clearEdgeCache()`, `s_occEdges` static, `extractEdgesFromFaceGroup()` |
| `src/BRepToAssimpConverter.cpp` | Edge extraction implementation; cache cleared with color cache |
| `include/AssImpModelLoader.h` | `precomputedOccEdges` field in `AssImpMeshData` |
| `src/AssImpModelLoader.cpp` | Transfers OCC edges from converter cache into `meshData` |
| `include/AssImpMesh.h` | `setPrecomputedOccEdges()`, `getOccEdgeSegments()`, GPU buffers, `_occEdgeSegments` CPU copy |
| `src/AssImpMesh.cpp` | `setPrecomputedOccEdges()` upload; `buildAndUploadFeatureEdges()` heuristic; `renderFeatureEdgesFast()` priority dispatch; `clone()` propagation |
| `src/GLWidget.cpp` | `createMeshFromMeshData()` handoff; `drawOpaqueMeshes/drawTransparentMeshes` display-mode GL state; `render()` defensive reset; MVF read/upload paths |
| `include/GLWidget.h` | `PreparedMvfMesh::occEdgeSegments` field |
| `src/MvfSceneBuilder.cpp` | MVF write path: binary chunk accessor for edge segments |
