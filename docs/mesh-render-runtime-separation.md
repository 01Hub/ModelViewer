Good. Here is the full updated assessment, grounded in the actual current code.

---

# Final Pre-Refactor Assessment — ModelViewer Mesh/Render/Runtime Architecture

---

## Part 1: What Has Changed Since Codex's Audit

Several systems have been added or modified since the original assessment. These all have refactor implications.

### Feature Edge / OCC B-Rep Edges (New — in `AssImpMesh`)
`AssImpMesh` now carries a second rendering path for edge visualization:
- `_featureEdgeIndexBuffer`, `_featureEdgeVAO`, `_featureEdgeCount` — heuristic edge VAO
- `_occEdgeVertexBuffer`, `_occEdgeVAO`, `_occEdgeCount`, `_occEdgeSegments`, `_occEdgeBoundaries` — exact B-Rep edge VAO
- `buildAndUploadFeatureEdges()`, `setPrecomputedOccEdges()`, `renderFeatureEdgesFast()`

These are GL resources, so they belong in `MeshVizAdaptor` — but the OCC edge **data** (`_occEdgeSegments`, `_occEdgeBoundaries`) is CPU geometry and belongs in either `TriangleMesh` or a separate `MeshEdgeData` struct depending on whether we want a clean geometry-layer concept. Given OCC edges are CAD-source topology (not mesh topology), they are most naturally kept alongside import provenance, i.e. `MeshImportAdaptor` for the raw segments and `MeshVizAdaptor` for the VAO.

**Refactor implication:** `MeshVizAdaptor` must handle two VAOs (main mesh VAO + edge VAO). `clone()` must copy OCC edge segments to new instance.

### Bounding Box Visibility Toggle (`show_bounding_box.png`, `hide_bounding_box.png` in QRC)
New toolbar icons suggest `_showBoundingBox` behavior has been actively used/refined. The debug overlay bounding-box draw path must be preserved during render controller extraction.

### Hover/Mesh-Level Highlight State
The recent commit "Add mesh-level hover/select highlighting for wireframe edge display modes" implies additional per-mesh state for hover (likely in `_selectionManager` forwarded to mesh render state). This needs to be located precisely before `MeshInstanceState` is designed, because it could be stored:
- In `TriangleMesh` directly (likely `_selected` + a new hover flag)
- Or forwarded from `SelectionManager` each frame

**Critical:** Confirm whether a hover flag is stored on `TriangleMesh` or only lives in `SelectionManager`. It must end up in `MeshInstanceState`.

### Normal Shader Updates (`face_normal.geom`, `face_normal.vert`, `vertex_normal.geom`, `vertex_normal.vert`)
Modified — likely related to the OCC edge or normal visualization changes. These are strictly render-path concerns. No structural refactor impact, but the draw methods `drawVertexNormals()` / `drawFaceNormals()` which consume them belong in `SceneRenderController`.

---

## Part 2: Complete Field Migration Map

### TriangleMesh → Target Classes

| Field | Target Class | Notes |
|---|---|---|
| `_indexBuffer`, `_positionBuffer`, `_normalBuffer`, `_colorBuffer`, `_texCoord[0-3]Buffer`, `_tangentBuf`, `_bitangentBuf`, `_jointIndexBuffer`, `_jointWeightBuffer`, `_coordBuf`, `_buffers`, `_vertexArrayObject` | **MeshVizAdaptor** | All GPU buffers. GL-context bound. |
| `_nVerts`, `_indices`, `_points`, `_normals`, `_colors`, `_texCoords`, `_tangents`, `_bitangents`, `_jointIndices`, `_jointWeights` | **TriangleMesh** | Pure CPU geometry. Keep here. |
| `_trsfPoints`, `_trsfNormals`, `_trsfTangents`, `_trsfBitangents` | **MeshInstanceState** | These are O(N) caches of transform-applied geometry — tied to instance transform, not to base asset. They exist to support picking and bounds recomputation. |
| `_boundingSphere`, `_boundingBox` | **MeshInstanceState** | World-space bounds, transform-dependent. |
| `_localBoundingBox` | **TriangleMesh** | Local-space, transform-independent. Keep as geometry property. |
| `_triangles` (Triangle* picking geometry) | **MeshInstanceState** | Derived from `_trsfPoints`; used for ray intersection in world space. |
| `_material` | **MaterialVizState** | |
| `_fallbackTexture`, `_fallbackTextureImage`, `_fallbackTextureBuffer` | **MeshVizAdaptor** | GL resource + image. Could be shared/static per context rather than per-mesh. |
| `_hasTextureAlpha` | **MaterialVizState** | Shader-facing material state. |
| `_sMax`, `_tMax` | **MaterialVizState** | Texture coordinate bounds used in shader. |
| `_transX/Y/Z`, `_rotateX/Y/Z`, `_rotationQuat`, `_scaleX/Y/Z`, `_transformation` | **MeshInstanceState** | User TRS. |
| `_explodedViewTransX/Y/Z`, `_explodedViewRotateX/Y/Z`, `_explodedViewRotationQuat`, `_explodedViewScaleX/Y/Z`, `_explodedViewTransformation` | **MeshInstanceState** | Exploded TRS — parallel system to user TRS. |
| `_explosionOffset` | **MeshInstanceState** | World-space auto-explode offset from `ExplodedViewManager`. |
| `_cachedCombinedRenderTransform`, `_combinedRenderTransformDirty` | **MeshInstanceState** | Cache of the composed render matrix. |
| `_primitiveMode` | **TriangleMesh** | Geometry-level property (GL_TRIANGLES, GL_POINTS, etc.). |
| `_selected` | **MeshInstanceState** | Selection/highlight flag. |
| `_hasNegativeScale` | **MeshInstanceState** | Affects face winding in render — computed from scale, read by render. |
| `_variantMappings`, `_allVariantMaterials`, `_activeVariantIndex` | **MeshInstanceState** (index only) + **MaterialVizState** (material data) | Active variant index belongs with instance state. Variant material table could stay on mesh or move to MaterialVizState depending on how we want variant switching to work. Keep for now on MaterialVizState. |
| `_skinJoints` | **MeshImportAdaptor** (definitions) | Skeleton structure — part of import provenance. |
| `_jointPalette` | **MeshAnimationState** | Runtime-mutable joint transform array. |
| `_sceneIndex`, `_originalMaterialIndex`, `_sourceFile`, `_sourceNodeName` | **MeshImportAdaptor** | Import provenance. |
| `_sceneRenderTransform` | **MeshInstanceState** | Scene-level transform separate from user TRS. |
| `_name` | **TriangleMesh** | Geometry asset identity. |
| `_textureBindingsDirty`, `_uniformsDirty` | **MeshVizAdaptor** | Render state dirty flags. |
| `_uniformLocationCache`, `_vaoConfiguredProgram` | **MeshVizAdaptor** | Shader/program caches. |
| `_debugTextureOverrides`, `_debugUniformOverrides` | **MeshVizAdaptor** | Debug tooling — render-side only. |
| `_baseThicknessFactor`, `_baseAttenuationDistance` | **MaterialVizState** | Cached material volume properties. |
| `_memorySize` | **TriangleMesh** | Asset-level stat. |
| `_hasVertexColors` | **TriangleMesh** | Geometry attribute flag. |
| Static: `_currentGlobalModelMatrix`, `_currentViewMatrix` | **SceneRenderController** (passed per-frame) | Currently static on TriangleMesh — this is a design smell. Should be passed as render context, not stored statically on geometry class. |
| Static: `_runtimeBoundsRevision` | **SceneRuntime** | Used to invalidate GLWidget's visibility BVH. Should belong to the runtime layer. |
| Static: texture binding cache, program binding cache, diagnostics | **SceneRenderController** | These are render-pass-level caches, not geometry-level. |

### AssImpMesh → Target Classes

| Field | Target Class | Notes |
|---|---|---|
| `_vertices`, `_baseVertices` | **TriangleMesh** | CPU geometry. `_baseVertices` is the morph-target base — belongs with geometry. |
| `_indices` | **TriangleMesh** | Already in base, just ensure override is handled. |
| `_textures` | **MaterialVizState** | Texture metadata list + GL IDs. |
| `_featureEdgeIndexBuffer`, `_featureEdgeVAO`, `_featureEdgeCount` | **MeshVizAdaptor** | GL edge resources. |
| `_occEdgeVertexBuffer`, `_occEdgeVAO`, `_occEdgeCount` | **MeshVizAdaptor** | GL OCC edge resources. |
| `_occEdgeSegments`, `_occEdgeBoundaries` | **MeshImportAdaptor** | CAD topology — import provenance. |
| `_morphTargets`, `_defaultMorphWeights` | **TriangleMesh** | Static morph data — geometry asset. |
| `_currentMorphWeights` | **MeshAnimationState** | Runtime per-instance weights. |
| `_textureBindings` (PrecomputedTexture array) | **MeshVizAdaptor** | Pre-computed binding optimization. |
| `_uniformStateSignatureDirty`, `_cachedUniformStateSignature` | **MeshVizAdaptor** | Render state cache. |
| Static: `_currentUniformStateShader`, `_currentUniformStateSignature`, `_currentUniformStateHadDebugOverrides` | **SceneRenderController** | Per-context shader state cache. Not mesh-level. |
| Static: `_currentBlendEnabled`, `_currentFrontFace` | **SceneRenderController** | GL blend + winding state cache. |
| `_diffuseADSMap` through `_opacityADSMap` | **MaterialVizState** | ADS texture IDs. |
| `_skipOptimization` | **MeshVizAdaptor** | Controls GPU upload behavior. |

---

## Part 3: Method Migration Map

### From TriangleMesh

| Method Group | Target | Notes |
|---|---|---|
| `clone()` | **MeshVizAdaptor** + **MeshInstanceState** | Clone must copy both. Currently pure virtual — subclass provides. |
| `setProg()` | **MeshVizAdaptor** | Program change → dirty buffers/uniforms. |
| Static render context (`setCurrentRenderContext`, `clearCurrentRenderContext`, `currentGlobalModelMatrix`, `currentViewMatrix`) | **SceneRenderController** | Should not be on geometry class. Pass render context explicitly. |
| Static texture/program caches (`bindTextureUnitCached`, `resetTextureBindingCache`, `bindProgramCached`, `notifyProgramBound`, `resetBoundProgramCache`) | **SceneRenderController** | Per-context render state caches. |
| `render()`, `renderShadow()`, `renderWireframeFast()`, `renderFeatureEdgesFast()` | **MeshVizAdaptor** | Core render methods. |
| `select()`, `deselect()` | **MeshInstanceState** | Mutate selection flag + dirty uniforms. |
| `getBoundingSphere()`, `getBoundingBox()` | **MeshInstanceState** | World-space bounds. |
| `getStableTransformCenter()`, `getStableTransformRadius()` | **MeshInstanceState** | |
| `getVAO()` | **MeshVizAdaptor** | |
| `getName()` | **TriangleMesh** | Asset identity. |
| `memorySize()` | **TriangleMesh** | Asset stat. |
| Material color/factor accessors (`ambientMaterial`, `setDiffuseMaterial`, etc.) | **MaterialVizState** | All delegate to `_material`. |
| Geometry queries (`getHighestXValue` ... `getLowestZValue`, `projectedRect`) | **MeshInstanceState** | These use world-space transformed points. |
| Transform accessors (base TRS: `getTranslation`, `setTranslation`, ..., `resetTransformations`) | **MeshInstanceState** | |
| Fast transform variants (`setTranslationFast`, `fullUpdateRuntimeBounds`) | **MeshInstanceState** | |
| Exploded view transform (`getExplodedViewTranslation` ... `resetExplodedViewTransformations`) | **MeshInstanceState** | |
| Combined render transform (`getSceneRenderTransform`, `setSceneRenderTransform`, `combinedRenderTransform`, `setExplosionOffset`, `explosionOffset`) | **MeshInstanceState** | The combined matrix logic must stay intact. |
| Data export (`getIndices`, `getPoints`, `getNormals`, `getTexCoords`, `getTrsfPoints`) | `getIndices/Points/Normals/TexCoords` → **TriangleMesh**; `getTrsfPoints` → **MeshInstanceState** | Untransformed = geometry; transformed = instance state. |
| `setPrimitiveMode`, `getPrimitiveMode` | **TriangleMesh** | Geometry property. |
| Source tracking (`setSourceFile`, `setSourceNodeName`, `setSceneIndex`, `setOriginalMaterialIndex`, `setHasNegativeScale`) | **MeshImportAdaptor** | `hasNegativeScale` is also render-relevant so **MeshInstanceState** needs to read it. |
| Material variant methods | **MaterialVizState** (material data) + **MeshInstanceState** (active index) | |
| Skinning methods (`setSkinJoints`, `hasSkinning`) | **MeshImportAdaptor** | Definitions. |
| `setJointPalette`, `jointPalette` | **MeshAnimationState** | Runtime. |
| Morph target virtuals (`hasMorphTargets`, `defaultMorphWeights`, `applyMorphWeights`, `resetMorphTargets`) | **TriangleMesh** (data + interface) + **MeshAnimationState** (current weights) | Base TriangleMesh can own default weights + target data. AnimationState owns current weights. |
| PBR/ADS texture setters/getters/clearers (40+ methods) | **MaterialVizState** | All material-state setters. |
| `getMaterial`, `setMaterial`, `setTextureMaps`, `isTransparent`, `needsDepthMaskOff`, `getRenderMaterialSortKey` | **MaterialVizState** + **MeshVizAdaptor** | Sort key is render-facing; transparent flag is used by render pass sorting. |
| Volume/thickness properties | **MaterialVizState** | |
| `getTextureSortKey` | **MeshVizAdaptor** | Render batching. |
| `intersectsWithRay` | **MeshInstanceState** | Uses `_triangles` which lives there. |
| Dirty flags (`markTexturesDirty`, `markUniformsDirty`, `updateRuntimeBounds`) | **MeshVizAdaptor** / **MeshInstanceState** | Texture/uniform dirty → viz adaptor. Bounds dirty → instance state. |
| `deleteTextures` | **MeshVizAdaptor** | GL resource cleanup. |
| Debug override methods | **MeshVizAdaptor** | |
| `initBuffers`, `buildTriangles`, `computeBounds`, `deleteBuffers`, `rebuildAbsoluteTransformation`, `rebuildExplodedViewTransformation`, `fastUpdateWorldBounds`, `invalidateCombinedRenderTransformCache`, `markRuntimeBoundsChanged`, `setupTransformation`, `setupTextures`, `setupUniforms`, `uniformLocationCached`, `clearUniformLocationCache`, `applyDebugTextureOverrides`, `applyDebugUniformOverrides` | Mostly **MeshVizAdaptor** (GL work) + **MeshInstanceState** (transform rebuild) | `buildTriangles`, `computeBounds` move to MeshInstanceState. `setupTextures`/`setupUniforms`/`initBuffers` move to MeshVizAdaptor. |
| Static diagnostics (`renderDiagnosticsEnabled`, `beginRenderDiagnosticsFrame`, `flushRenderDiagnostics`, 15x `record*()`) | **SceneRenderController** | Frame-level render profiling. |
| `currentRuntimeBoundsRevision` (static) | **SceneRuntime** | Scene-level invalidation counter. |

---

## Part 4: Transform Stack — The Non-Negotiable Contract

The current combined render transform is:

```
combinedRenderTransform =
    explodedViewTransformation   ← manual or auto explode TRS
    * transformation              ← user TRS (base mesh)
    * sceneRenderTransform        ← scene/node-level transform from file hierarchy
```

Then, before drawing, `_explosionOffset` (world-space auto-explode translation from `ExplodedViewManager`) is **pre-multiplied** as a translation.

**This order must be preserved identically in `MeshInstanceState`**. Every consumer that calls `combinedRenderTransform()` today — render, picking, gizmo, shadow, visibility culling — will need to call the equivalent method on `MeshInstanceState` instead.

Additionally:
- `_hasNegativeScale` affects GL front-face winding (`GL_CW` vs `GL_CCW`) and must be communicated from `MeshInstanceState` to `MeshVizAdaptor` at draw time.
- `_sceneRenderTransform` is set by the animation system; `MeshAnimationState` writes it, `MeshInstanceState` stores it, `MeshVizAdaptor` reads it during render.

---

## Part 5: Inheritance Chain — What Must Be Broken

```
Drawable
  └── TriangleMesh
        └── AssImpMesh
```

**Target:**
- `Drawable` relationship: `TriangleMesh` (or its replacement) no longer needs to inherit `Drawable` once rendering is in `MeshVizAdaptor`. However, `Drawable` may carry Qt signal/slot infrastructure (Q_OBJECT). Check what signals `Drawable` emits — if it's only `nameChanged` or similar, those move to `TriangleMesh` directly. If it's render-event signals, they move to `MeshVizAdaptor`.
- `AssImpMesh : TriangleMesh`: This must ultimately become composition (`SceneMeshRecord` holds both as separate members). During transition, `AssImpMesh` can remain as a compatibility shell with methods forwarding to `MeshVizAdaptor`, then be retired.

**Warning:** Many call sites in `GLWidget` hold `TriangleMesh*` and call `AssImpMesh`-specific methods via dynamic cast or through virtuals. Every such call site is a migration touchpoint in Phase 5.

---

## Part 6: Static State That Must Move

These are currently static on `TriangleMesh` or `AssImpMesh` but conceptually belong in the render controller:

| Static State | Current Home | Target |
|---|---|---|
| `_currentGlobalModelMatrix`, `_currentViewMatrix` | `TriangleMesh` | Pass as `RenderContext` struct to `MeshVizAdaptor::render()` |
| Per-context texture binding cache | `TriangleMesh` (anonymous ns) | `SceneRenderController` |
| Per-context program binding cache | `TriangleMesh` (anonymous ns) | `SceneRenderController` |
| `_runtimeBoundsRevision` | `TriangleMesh` | `SceneRuntime` |
| `_renderDiagnostics` struct | `TriangleMesh` (anonymous ns) | `SceneRenderController` |
| `_currentUniformStateShader`, `_currentUniformStateSignature`, `_currentUniformStateHadDebugOverrides` | `AssImpMesh` | `SceneRenderController` |
| `_currentBlendEnabled`, `_currentFrontFace` | `AssImpMesh` | `SceneRenderController` |

The most important change here: **`setCurrentRenderContext()` / `clearCurrentRenderContext()` should not exist as a static call on a geometry class.** This is the mechanism by which GLWidget pushes camera matrices into all meshes for the current frame. In the refactored world, `SceneRenderController` owns this context and passes it explicitly to `MeshVizAdaptor::render(RenderContext&)`.

---

## Part 7: GLWidget — Revised Extraction Map

The full member inventory confirms the Codex breakdown was correct, with these additions/refinements:

### SceneMeshRecord (new aggregate, lives in SceneRuntime)
```cpp
struct SceneMeshRecord {
    std::shared_ptr<TriangleMesh>      asset;        // CPU geometry
    std::shared_ptr<MeshImportAdaptor> import;       // source provenance, OCC edge data
    std::shared_ptr<MaterialVizState>  material;     // shader-facing material + textures
    std::shared_ptr<MeshVizAdaptor>    viz;          // VAO/VBO, draw methods
    std::shared_ptr<MeshInstanceState> instance;     // TRS, exploded TRS, selection, bounds
    std::shared_ptr<MeshAnimationState> animation;  // joint palette, morph weights
    QUuid                              uuid;
};
```

### What Stays in GLViewportWidget (thin shell only)
- `initializeGL()`, `paintGL()`, `resizeGL()` — Qt lifecycle
- `mousePressEvent()`, `mouseReleaseEvent()`, `mouseMoveEvent()`, `wheelEvent()`, `keyPressEvent()` — route to controllers
- Ownership of controller instances (as members)
- Signal relay to parent `ModelViewer`

### SceneRuntime owns
- `_meshStore` (becomes `std::vector<SceneMeshRecord>`)
- `_displayedObjectsIds`, `_hiddenObjectsIds`
- `_runtimeVisibilityNodes`, `_runtimeVisibilityRootIndex` and BVH methods
- `_recycleBin`
- `_texCache`, `_texRefCount` — texture pool
- UUID/index lookup
- `_globalScene`, `_assimpScene`, `_globalSceneTransform`
- `_pendingSceneUuids`

### SceneRenderController owns
- All shader programs (`_fgShader`, `_wireframeShader`, `_shadowMappingShader`, etc. — 25+ programs)
- `_projectionMatrix`, `_viewMatrix`, `_modelMatrix`, `_modelViewMatrix`, `_viewportMatrix`
- All render pass methods (`drawOpaqueMeshes`, `drawTransparentMeshes`, `renderToShadowBuffer`, `renderToTransmissionBuffer`, `renderToSSSBuffer`, `drawFloor`, `drawSkyBox`, etc.)
- `setCommonUniforms()`
- Static texture/program binding caches (moved from TriangleMesh statics)
- Render diagnostics
- `_frustumPlanes[6]` and frustum test methods
- Transmission/SSS FBOs + buffers
- IBL textures + generation methods
- Skybox, floor, grid resources
- Shadow map + `AdaptiveShadowMapper`
- `_quadVAO`, `_fsTriVAO`, etc. (utility geometry)
- Tone mapping / gamma parameters
- `_displayMode`, `_renderingMode`

### ViewportInteractionController owns
- Camera objects (`_primaryCamera`, `_orthoViewsCamera`)
- `_viewMode`, `_projection`, FOV, view range, rotation quaternion, translation
- Navigation timers (`_keyboardNavTimer`, `_animateViewTimer`, `_inertiaTimer`, etc.)
- Inertia velocity state
- Mouse button points, rubber band
- `_rubberBandZoomRatio`, window zoom state
- `_multiViewActive`, `_navigationViewportLocked`
- `_keys` (pressed key set)
- Fit/zoom/pan/rotate methods
- Gizmo drag state and all `beginTransformGizmoDrag*`, `updateTransformGizmo*`, `finishTransformGizmo*` methods
- `_transformGizmo` object
- Selection rubber band

### AnimationRuntimeController owns
- `_runtimeAnimationsByFile`
- `_activeAnimationFile`, `_activeAnimationClip`, `_animationCurrentTimeSeconds`
- `_animationPlaying`, `_animationLooping`, `_animationPlaybackSpeed`
- `_animationTimer`, `_animationElapsed`
- `_activeGltfCameraFile`, `_activeGltfCameraIndex`, saved camera state fields
- `applyAnimationPose()`, `syncRuntimeNodeTransforms()`, `resetAnimationPose()`, `updateAnimatedMeshState()`
- `setAnimatedLightVisibilityState()`, `clearAnimatedLightVisibilityState()`, etc.
- `setAnimatedMeshVisibilityState()`, `clearAnimatedMeshVisibilityState()`
- `refreshAnimationMaterialState()`
- `onAnimationTick()`

### ExplodedViewRuntimeController owns
- `_explodedViewManager`
- `_explodedViewManualPlacementActive`
- `_explodedViewManualOriginalStates`, `_explodedViewManualHiddenStates`, `_explodedViewManualSessionStartStates`
- `_explodedViewManualSessionTranslationDelta`, `_explodedViewManualSessionRotationEuler`, `_explodedViewManualSessionRotationQuat`
- `_cachedHintsAssemblyUuids`, `_cachedHintsAnchorUuid`, `_cachedAutoHints`, `_cachedHintsValid`
- `beginExplodedViewManualPlacement()`, `finishExplodedViewManualPlacement()`
- `applyExplodedViewTransforms()`, `explodedViewManualStates()`, `restoreExplodedViewManualStates()`
- `updateExplosion()`
- `applyExplodedViewManualPlacementSessionTransform()`

### MeshAssetUploadService (new — extracted from GLWidget)
- `prepareMvfMeshes()` (currently static)
- `uploadPreparedMvfMeshes()`
- `onMeshBatchReady()`, `createMeshFromData()`
- `getOrLoadTextureCached()`, `getOrCreateTextureCached()`
- `uploadDecodedTextureImage()`, `uploadKtx2Texture()`
- `retainTexture()`, `releaseTexture()`
- `_assimpModelLoader`

---

## Part 8: Phase Plan (Updated for Current State)

### Phase 1 — Introduce `MeshInstanceState`
**Scope:** TriangleMesh only. No GL changes.

Move into `MeshInstanceState`:
- All user TRS fields + `rebuildAbsoluteTransformation()`
- All exploded view TRS fields + `rebuildExplodedViewTransformation()`
- `_sceneRenderTransform`, `_explosionOffset`
- `_cachedCombinedRenderTransform`, `_combinedRenderTransformDirty`
- `combinedRenderTransform()` — preserve exact multiplication order
- `_selected`, `_hasNegativeScale`
- `_trsfPoints`, `_trsfNormals`, `_trsfTangents`, `_trsfBitangents` (O(N) caches)
- `_triangles` + `buildTriangles()` + `intersectsWithRay()`
- `_boundingSphere`, `_boundingBox` (world-space)
- `fastUpdateWorldBounds()`, `fullUpdateRuntimeBounds()`, `updateRuntimeBounds()`
- Fast/slow transform variants (`setTranslationFast` etc.)
- `_sceneIndex`, `_originalMaterialIndex`, `_sourceFile`, `_sourceNodeName` → hold here temporarily until `MeshImportAdaptor` exists

`TriangleMesh` keeps all public accessors as forwarding shims.

**Verification gates:**
- User TRS and gizmo drag produce identical visual result
- Exploded auto + manual transforms layer identically
- Picking/ray intersection unaffected
- Bounding sphere/box used for frustum culling unaffected
- MVF save/load restores all transform state correctly
- `combinedRenderTransform()` value is bit-identical before and after

**Hover flag check (before writing any code):** Read `SelectionManager` and `TriangleMesh` to confirm whether a hover/highlight flag exists on `TriangleMesh`. If yes, add it to `MeshInstanceState` in Phase 1.

---

### Phase 2 — Introduce `MaterialVizState`
**Scope:** TriangleMesh + AssImpMesh. No GL changes yet.

Move into `MaterialVizState`:
- `_material` (GLMaterial)
- `_hasTextureAlpha`, `_sMax`, `_tMax`
- `_baseThicknessFactor`, `_baseAttenuationDistance`
- `_allVariantMaterials`, `_variantMappings` (material data)
- `_diffuseADSMap` through `_opacityADSMap` (from AssImpMesh)
- `_textures` (from AssImpMesh)
- All PBR/ADS setters/getters/clearers (50+ methods)
- `isTransparent()`, `needsDepthMaskOff()`, `getRenderMaterialSortKey()`
- `cacheBaseVolumeProperties()`, `applyScaledVolumeProperties()`
- `setTextureMaps()`, `syncTexturesFromMaterialIfNeeded()`
- `replaceOrAppendTexture()`, `removeTexturesByType()`
- `getTextureSortKey()`

`TriangleMesh` + `AssImpMesh` keep forwarding shims.

**Verification gates:**
- ADS and PBR material editing via panel produces identical render
- KHR extension textures (sheen, clearcoat, transmission) unaffected
- Material variants switch correctly
- Transparency/transmission sorting unaffected
- Debug texture override panel still works
- Export round-trip produces same material assignments

---

### Phase 3 — Introduce `MeshVizAdaptor` and `MeshImportAdaptor`
**Scope:** AssImpMesh primarily. Major structural change.

Move into `MeshVizAdaptor` (from both TriangleMesh and AssImpMesh):
- All GL buffer fields (VAO, VBOs, EBOs)
- Feature edge + OCC edge GL resources
- `_fallbackTexture` and related
- `_uniformLocationCache`, `_vaoConfiguredProgram`
- `_textureBindingsDirty`, `_uniformsDirty`
- `_textureBindings` (PrecomputedTexture array)
- `_uniformStateSignatureDirty`, `_cachedUniformStateSignature`
- `_skipOptimization`
- `_debugTextureOverrides`, `_debugUniformOverrides`
- `initBuffers()`, `setupTextures()`, `setupUniforms()`, `setupTransformation()`
- `cacheTextureBindings()`, `bindTexturesOptimized()`, `setRenderStateOptimized()`, `setupUniformsOptimized()`
- `uniformStateSignature()`
- `render()`, `renderShadow()`, `renderWireframeFast()`, `renderFeatureEdgesFast()`
- `buildAndUploadFeatureEdges()`, `setPrecomputedOccEdges()` (GL side)
- `deleteTextures()`, `deleteBuffers()`
- `setProg()`
- All debug override methods
- Static per-context render caches (from TriangleMesh anonymous ns) — migrate to SceneRenderController or pass as context

Move into `MeshImportAdaptor`:
- `_sceneIndex`, `_originalMaterialIndex`, `_sourceFile`, `_sourceNodeName`
- `_skinJoints` (definitions)
- `_occEdgeSegments`, `_occEdgeBoundaries` (CAD topology data)
- `_vertices`, `_baseVertices` → these are intermediate import data; after mesh setup they can be released or kept for morph target base
- `_morphTargets`, `_defaultMorphWeights` → could live here as import-provided data, or stay in TriangleMesh

**Key decision needed here:** After `initBuffers()` runs, do we need `_vertices` (the `Vertex` struct array) to remain? Currently it's used by `setMeshData()` + morph target remapping. If morph targets are the only consumer, we can keep just `_baseVertices`. This needs a targeted read of `setMeshData()` and `applyMorphWeights()` before Phase 3.

**Verification gates:**
- Render output parity (ADS, PBR, wireframe, feature edges, OCC edges)
- Performance diagnostics (texture bind counts, cache hit rates) unchanged
- `clone()` correctly copies all viz adaptor state
- OCC edges render correctly after clone
- Morph targets apply correctly
- Skinning poses render correctly
- Uniform state signature caching still effective (no regression in draw call count)

---

### Phase 4 — Shrink `TriangleMesh` to Geometry Asset
After Phases 1–3, TriangleMesh should be left holding:
- `_nVerts`
- `_indices`, `_points`, `_normals`, `_colors`, `_texCoords`, `_tangents`, `_bitangents`, `_jointIndices`, `_jointWeights`
- `_localBoundingBox`
- `_primitiveMode`
- `_name`, `_memorySize`, `_hasVertexColors`
- `_morphTargets`, `_defaultMorphWeights` (or moved to MeshImportAdaptor)
- `_baseVertices` (morph base snapshot)
- Geometry queries purely on local-space data

Remove `Drawable` inheritance if `Drawable`'s only role is Qt object infrastructure — replace with `QObject` directly.

Retire `AssImpMesh : public TriangleMesh` as a class hierarchy. `AssImpMesh` becomes a factory function or a thin builder that constructs a `SceneMeshRecord`.

---

### Phase 5 — Extract `SceneRuntime` from `GLWidget`
**Highest coordination cost.** Every panel, signal, and render pass that currently holds a `TriangleMesh*` or index into `_meshStore` needs to be retargeted to `SceneRuntime`.

Key concern: `_runtimeVisibilityNodes` BVH is tightly coupled to `_meshStore` indices. Rebuilding must still work correctly after mesh add/remove/restore from recycle bin.

**Verification gates:**
- Multi-file load/unload correct
- Recycle bin delete/restore correct
- Scene graph panel synchronization correct
- MVF save/load round-trip correct (all UUIDs, indices, names preserved)
- Animation file mapping still resolves to correct meshes
- Light-to-file mapping still resolves correctly

---

### Phase 6 — Extract `AnimationRuntimeController`
Relatively self-contained. Main risk: `applyAnimationPose()` writes into `MeshInstanceState` (transforms) and `MeshAnimationState` (joint palette, morph weights). These write paths must be clean before this extraction.

**Verification gates:**
- Playback (Vec3, Quat, weights, bool, pointer channels)
- Morph targets animate correctly
- Skinning animates correctly
- Light animation (position, visibility)
- Camera animation
- glTF camera active/deactivate cycle
- Exploded animation preview

---

### Phase 7 — Extract `ExplodedViewRuntimeController`
**Verification gates:**
- All explode modes (axis, radial, assembly-aware)
- Manual placement session (start, drag, finish, cancel)
- Grouped captures / preset switching
- Combined-pose checkbox
- MVF restore of uncaptured + captured manual states

---

### Phase 8 — Extract `SceneRenderController`
Move all render pass methods and shader program ownership out of GLWidget.

**Risk:** Render state is currently held on both GLWidget (large uniforms, pass state) and individual meshes (material, texture caches). After Phases 1–4, mesh-side state is clean. This phase cleans the controller side.

---

### Phase 9 — Extract `ViewportInteractionController`
Move all camera navigation, gizmo, and interaction state. GLWidget becomes a thin event router.

---

### Phase 10 — Rename `GLWidget` → `GLViewportWidget`, remove all forwarding shims

---

## Part 9: Key Nuances Not To Miss

1. **`_trsfPoints` is used for picking, not just rendering.** It must stay accessible for ray-mesh intersection. Moving it to `MeshInstanceState` is correct, but `intersectsWithRay()` must also move there with it.

2. **`combinedRenderTransform()` is called by the culling system.** The runtime BVH in GLWidget queries bounding boxes/spheres that depend on this. The BVH must be updated to call through to `MeshInstanceState` instead of `TriangleMesh`.

3. **The morph target `applyMorphWeights()` modifies `_vertices` and calls `setMeshData()` which calls `setupMesh()` which calls `initBuffers()`.** This is a full GPU re-upload triggered by animation. In the refactored world, `AnimationRuntimeController` will write morph weights into `MeshAnimationState`, and `MeshVizAdaptor` will need to detect the weight change and trigger re-upload. The current path is deeply coupled and needs explicit thought before Phase 3.

4. **`uniformStateSignature()` is a per-draw optimization.** Moving its static cache (currently on `AssImpMesh`) to `SceneRenderController` requires that each `MeshVizAdaptor::render()` call is given the current context's cached signature so it can compare. The render controller must own the per-context signature cache and provide it as part of the `RenderContext` passed to each mesh.

5. **Texture cache (`_texCache` in GLWidget) is shared across all meshes.** It cannot live on any individual mesh. It must live in `SceneRuntime` or `MeshAssetUploadService`, and `MaterialVizState` must reference GL texture IDs that were allocated by the cache, not own them.

6. **`_runtimeBoundsRevision` (static atomic on TriangleMesh) is read by GLWidget to invalidate the visibility BVH.** Once this moves to `SceneRuntime`, the update path is: `MeshInstanceState` modifies bounds → notifies `SceneRuntime` → BVH is dirtied. The atomic counter pattern can stay if we keep it on `SceneRuntime` as an instance counter rather than a global static.

7. **`setCurrentRenderContext()` is currently called at the top of each render pass.** Every `TriangleMesh::setupUniforms()` reads the static matrices. After refactor, this becomes an explicit `RenderContext` argument to `MeshVizAdaptor::render(const RenderContext&)`. GLWidget (or `SceneRenderController`) builds and passes this per-frame, not via global statics.

8. **`clone()` must copy OCC edge segments.** The current `AssImpMesh::clone()` copies morph targets and calls `setPrecomputedOccEdges()`. After refactor, `MeshImportAdaptor::clone()` copies OCC edge data, and `MeshVizAdaptor::clone()` re-uploads the VAO on the new instance's GL context.

9. **The assembly-aware auto-placement hints cache in GLWidget** (`_cachedHintsAssemblyUuids` etc.) is a `SceneRuntime` artifact, not an `ExplodedViewRuntimeController` artifact, because it depends on the set of currently loaded meshes.

10. **`_hasNegativeScale` in `AssImpMesh::setRenderStateOptimized()` sets `glFrontFace(GL_CW)`** globally (via static `_currentFrontFace`). This is a render-pass-level GL state change. After refactor, `MeshInstanceState` owns the `hasNegativeScale` flag, `MeshVizAdaptor` reads it and calls `setRenderStateOptimized`, which sets per-draw GL front-face winding. The static per-context cache stays in `SceneRenderController`.

---

This is the complete current-state record. You have everything needed to start Phase 1 without dropping any nuance. Want me to produce the concrete Phase 1 header layout for `MeshInstanceState` now?