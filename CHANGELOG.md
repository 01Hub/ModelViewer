# Changelog

## [2026.7.0] - Unreleased

The largest release in the project's history — 410 commits since `1.2.3`/`Release-1.0`.
Highlights are grouped by theme below; internal refactors are summarized rather than
listed commit-by-commit.

### New Features

- **Exploded views** — automatic (radial, per-axis, or custom-vector) and manual
  gizmo-driven assembly explosion, with capture steps, reorderable sequences,
  presets, and parallel/sequential/separate animation export. Persisted in MVF.
- **Morph target (blend shape) animation** — full glTF import/export round-trip,
  playable through the Animations panel alongside skeletal clips.
- **Punctual lights (glTF lights)** — per-file, multi-model light system with
  point/spot/directional support, viewport indicators, and undo-aware deletion.
- **Interactive transform gizmo** — on-screen translate/rotate/scale handles,
  reused by exploded-view manual placement for non-destructive staging.
- **OCC B-Rep true-edge wireframe rendering** for STEP/IGES/BREP, plus
  feature-edge wireframe modes for OBJ/glTF meshes; new Mesh Edges and Shaded
  with Edges display modes.
- **Interactive view cube** overlay with switchable grid ground mode.
- **glTF camera support**, including animated tracking and Material Variants
  (`KHR_materials_variants`) with full export round-trip.
- **Texture Debugger panel** — live texture/extension overrides, Khronos-aligned
  channel isolation, geometry and texture channel inspection.
- **Unified Material Panel** — single editor merging material/texture editing,
  UUID-based material keys, unsaved-change tracking, user material library.
- **Scene-graph-aware tree widget** replacing the flat object list, with
  assembly-correct multi-select propagation, detachable navigation panel, and
  threaded/progressive model loading with GPU-safe finalization.
- **Dual-camera Z-up/Y-up system** and a full camera/shading math revamp aligned
  to the Khronos glTF reference viewer (PBR, sheen, clearcoat, transmission,
  anisotropy, iridescence, specular/spec-gloss).
- **Settings dialog** reorganized and fully wired end-to-end across every tab
  (General, Camera, Display, Rendering, Materials, Import/Export, Debug), with
  a new "Animate Progressive Fit" option.

### Improvements & Fixes

- Progressive-load fit-to-view no longer stalls mid-load; root-caused to a
  bounds-skipping fast path for scene-render-transformed meshes.
- Orientation-aware fit-to-screen using true view-space AABB projection,
  concurrent rotation + zoom, stable/tight bounding spheres for scene fitting.
- Per-viewport ray picking and multi-view ortho coherence fixes.
- Copy/cut/paste rebuilt as a tree-driven system with deferred-removal cut
  semantics; undo/redo added for mesh rename, deselection, and metadata delete.
- Numerous glTF/GLB export correctness fixes: material index remapping, ORM
  packing detection, roughness inversion, occlusion strength preservation,
  variant/vertex-color/punctual-light export, relative texture paths, embedded
  `data:` URI images, node-transform un-baking.
- STEP/IGES import: crash fixes, transfer guards, multi-body color fallback,
  multiple loading-performance passes.
- Shadow, SSS (subsurface scattering), IBL, and transmission pipeline
  correctness and performance passes; GPU memory leak fix in the animation
  loop (per-frame shadow recomputation).
- Detachable/floating panel UX overhaul (lock/reattach behavior) applied
  across all side panels.
- OpenGL initialization crash fixes on Linux/Wayland.

### Performance

- Large-assembly rendering: eliminated per-mesh shader bind/release in opaque
  and transparent passes, static-frame BVH subtree skipping, removed
  redundant GPU readbacks in view-cube drawing, MDI-safe shader program cache.
- Floor plane, shadow, and PBR shader micro-optimizations.
- AABB-based visibility culling and stencil-based capping-pass culling.

### Architecture (internal, not user-facing)

- Multi-phase mesh/render/runtime refactor: `TriangleMesh` → `RenderableMesh`,
  `AssImpMesh` → `SceneMesh`, introduction of `MeshGeometry`/`DeformableGeometry`,
  `SceneRuntime`, `SceneRenderController`, `ViewportInteractionController`,
  `AnimationRuntimeController`, `ExplodedViewRuntimeController`.
- `GLWidget` renamed to `ViewportWidget`; extensive SOLID-audit passes
  decomposing its responsibilities into dedicated controllers.
- Legacy `QDataStream` binary serialization removed; MVF3 (glTF-spec JSON +
  binary geometry chunk) is now the sole native format baseline.

### Documentation

- Quick Help dialog and all 14 existing tutorial lessons corrected for stale
  shortcuts and menu paths (`Ctrl+I`/`Ctrl+E` not `Ctrl+Shift+I/E`; Settings
  lives under Edit, not File).
- Four new tutorial lessons added: Exploded Views, Morph Target Animation,
  Node Transform Editing, and Edge & Wireframe Rendering.

### Known Gaps

- Tutorial screenshots for the four new lessons (15-18) are placeholders
  pending capture.
- Pawn UUID ordering issue in the ABeautifulGame chess scene (deferred,
  severity pending assessment).
