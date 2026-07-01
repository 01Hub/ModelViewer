# SRP Refactor — Phase 2 Plan
**Branch:** `refactor/mesh-render-runtime-separation`  
**Authors:** Claude Sonnet 4.6 (architect), Codex (implementer)

---

## Background

Phase 1 established four controllers that GLWidget delegates to:

| Controller | Owns |
|---|---|
| `SceneRenderController` (`_renderCtrl`) | Render state, shaders, FBOs, shadow map, capping, clipping, `glLights` GPU buffer |
| `ViewportInteractionController` (`_viewCtrl`) | Camera, navigation, gizmo drag flags, bounding volumes |
| `AnimationRuntimeController` (`_animCtrl`) | Animation playback, light pipeline (parsed lights, repositioned lights, file→index map) |
| `SelectionManager` (`_selectionManager`) | Selection state and logic |
| `SceneRuntime` (`_sceneRuntime`) | Mesh store (`std::vector<SceneMeshRecord>`), texture cache |

Per-mesh state lives in:
- `MeshInstanceState` — transform layers, world-space bounds, picking geometry, selection flag
- `MaterialVizState` — `GLMaterial` (PBR + ADS), texture alpha, volume cache, KHR_materials_variants, ADS GL IDs

Phase 2 addresses the remaining SRP violations: three GLWidget methods that do too much, one whose rebuild logic leaks out of `AnimationRuntimeController`, two material batch methods whose mesh-mutation half belongs in `SceneRuntime`, and one animation method (`applyAnimationPose`) whose channel-sampling stage belongs in `AnimationRuntimeController`.

---

## Items — ordered by dependency and risk

| # | Item | Risk | Scope |
|---|---|---|---|
| 1 | `updatePunctualLights` — encapsulate SceneGraph predicate in `AnimationRuntimeController` | Very low | +15 lines ARC, −10 GLWidget |
| 2 | Material batch — move mesh-mutation half to `SceneRuntime` | Low | +50 SceneRuntime, −30 GLWidget |
| 3 | `applyTransforms` — extract TRS loop + model-level query to `SceneRuntime` | Low | +30 SceneRuntime, ~40 GLWidget |
| 4 | `applyAnimationPose` — extract channel-sampling to `AnimationRuntimeController`; split application stages into named private GLWidget methods | Medium | +80 ARC, ~400 GLWidget refactored |
| 5 | `_originalParsedLights`/`_lightFileIndexMap` → `SceneRuntime` | Medium-high | Separate branch after Items 1–4 |

Do Items 1–4 in order. Item 5 is a separate branch.

---

## Item 1 — `updatePunctualLights`: encapsulate the SceneGraph-enabled predicate

### Problem

`GLWidget::updatePunctualLights()` (`src/GLWidget.cpp:4654`) builds the light-enabled predicate inline and passes it into `_animCtrl.rebuildAndBuildUploadLights()`. The SceneGraph query belongs in `AnimationRuntimeController`, not in GLWidget.

### Current code (lines 4654–4690)

```cpp
void GLWidget::updatePunctualLights()
{
    if (_animCtrl.originalParsedLights().empty())
        return;

    std::vector<GPULight> uploadLights;
    if (_viewer && _viewer->sceneGraph())
    {
        auto* sg = _viewer->sceneGraph();
        uploadLights = _animCtrl.rebuildAndBuildUploadLights(
            [this](const QString& file) {
                QMatrix4x4 m;
                userModelTransformForFile(file, m);
                return m;
            },
            [sg](const LightOrigin& origin) {
                const GltfLightData& ld = sg->lightDataForFile(origin.file);
                return origin.index < static_cast<int>(ld.lights.size()) &&
                       ld.lights[origin.index].enabled;
            });
    }
    else
    {
        uploadLights = _animCtrl.rebuildAndBuildUploadLights(
            [this](const QString& file) {
                QMatrix4x4 m;
                userModelTransformForFile(file, m);
                return m;
            });
    }
    _renderCtrl.glLights()->setLights(uploadLights);
    syncPunctualLightUniforms(static_cast<int>(uploadLights.size()), !uploadLights.empty());
}
```

### Changes

**`include/AnimationRuntimeController.h`** — add declaration after the existing `buildUploadLights` overloads:

```cpp
// Rebuild and build upload list, filtering by SceneGraph light-enabled state.
// sg may be nullptr — in that case no enabled-flag filtering is applied.
std::vector<GPULight> buildUploadLightsWithSceneGraph(
    const std::function<QMatrix4x4(const QString&)>& userTransformResolver,
    const SceneGraph* sg);
```

Note: `class SceneGraph;` is already forward-declared in `AnimationRuntimeController.h`.

**`src/AnimationRuntimeController.cpp`** — add implementation:

```cpp
std::vector<GPULight> AnimationRuntimeController::buildUploadLightsWithSceneGraph(
    const std::function<QMatrix4x4(const QString&)>& userTransformResolver,
    const SceneGraph* sg)
{
    if (!sg)
        return rebuildAndBuildUploadLights(userTransformResolver);

    return rebuildAndBuildUploadLights(
        userTransformResolver,
        [sg](const LightOrigin& origin) {
            const GltfLightData& ld = sg->lightDataForFile(origin.file);
            return origin.index < static_cast<int>(ld.lights.size()) &&
                   ld.lights[origin.index].enabled;
        });
}
```

**`src/GLWidget.cpp`** — replace the body of `updatePunctualLights`:

```cpp
void GLWidget::updatePunctualLights()
{
    if (_animCtrl.originalParsedLights().empty())
        return;

    const SceneGraph* sg = _viewer ? _viewer->sceneGraph() : nullptr;
    const std::vector<GPULight> uploadLights =
        _animCtrl.buildUploadLightsWithSceneGraph(
            [this](const QString& file) {
                QMatrix4x4 m;
                userModelTransformForFile(file, m);
                return m;
            },
            sg);

    _renderCtrl.glLights()->setLights(uploadLights);
    syncPunctualLightUniforms(static_cast<int>(uploadLights.size()), !uploadLights.empty());
}
```

**Files changed:** `include/AnimationRuntimeController.h`, `src/AnimationRuntimeController.cpp`, `src/GLWidget.cpp`

---

## Item 2 — Material batch: move mesh-mutation half to `SceneRuntime`

### Problem

`setMaterialToObjects` and `setTexturesToObjects` (`src/GLWidget.cpp:4105–4140`) iterate the mesh store from GLWidget. The mesh-mutation half belongs in `SceneRuntime`. The GL-context-dependent half (texture resolution, `glDeleteTextures`) must stay in GLWidget.

### Current code

```cpp
// Line 4105
void GLWidget::setMaterialToObjects(const std::vector<int>& ids, const GLMaterial& mat)
{
    for (int id : ids) {
        try {
            SceneMesh* mesh = _sceneRuntime.meshAt(id);
            mesh->setMaterial(mat);
            if (mat.hasTransmission() || mat.diffuseTransmissionFactor() > 0.0f)
                setTransmissionEnabled(true);
        } catch (const std::exception& ex) { ... }
    }
}

// Line 4123
void GLWidget::setTexturesToObjects(const std::vector<int>& ids, const GLMaterial& mat)
{
    for (int id : ids) {
        try {
            SceneMesh* mesh = _sceneRuntime.meshAt(id);
            GLMaterial resolved = resolveMaterialTextures(this, mat);
            mesh->setTextureMaps(resolved);
            mesh->invertOpacityADSMap(resolved.isOpacityMapInverted());
            mesh->invertOpacityPBRMap(resolved.isOpacityMapInverted());
        } catch (const std::exception& ex) { ... }
    }
}

// Line 4160
void GLWidget::clearTextureCache()
{
    for (auto& entry : _sceneRuntime.texCache()) {
        if (entry.second.lastGPUTexture != 0)
            glDeleteTextures(1, &entry.second.lastGPUTexture);
    }
    _sceneRuntime.texCache().clear();
    _sceneRuntime.texRefCount().clear();
}
```

### Changes

**`include/SceneRuntime.h`** — add three declarations in the public section:

```cpp
// Batch-apply a GLMaterial to a set of mesh-store indices.
// Returns true if any affected mesh requires transmission rendering.
bool applyMaterialToMeshes(const std::vector<int>& ids, const GLMaterial& mat);

// Apply pre-resolved texture maps (and opacity inversion flags) to a single mesh.
// Called by GLWidget::setTexturesToObjects after texture resolution.
void applyTextureMapsToMesh(int id, const GLMaterial& resolved);

// Clear the texture cache: drains all GPU texture IDs into a returned vector
// (so the caller can call glDeleteTextures), then clears the cache maps.
std::vector<unsigned int> drainTextureCacheGpuIds();
```

`GLMaterial` is already included in `SceneRuntime.h` (it is used by `SceneMeshRecord`). Verify and add `#include "GLMaterial.h"` if missing.

**`src/SceneRuntime.cpp`** — add implementations:

```cpp
bool SceneRuntime::applyMaterialToMeshes(const std::vector<int>& ids, const GLMaterial& mat)
{
    bool needsTransmission = false;
    for (int id : ids)
    {
        try
        {
            SceneMesh* mesh = meshAt(id);
            if (!mesh) continue;
            mesh->setMaterial(mat);
            if (mat.hasTransmission() || mat.diffuseTransmissionFactor() > 0.0f)
                needsTransmission = true;
        }
        catch (const std::exception& ex)
        {
            std::cout << "Exception in SceneRuntime::applyMaterialToMeshes\n" << ex.what() << std::endl;
        }
    }
    return needsTransmission;
}

void SceneRuntime::applyTextureMapsToMesh(int id, const GLMaterial& resolved)
{
    try
    {
        SceneMesh* mesh = meshAt(id);
        if (!mesh) return;
        mesh->setTextureMaps(resolved);
        mesh->invertOpacityADSMap(resolved.isOpacityMapInverted());
        mesh->invertOpacityPBRMap(resolved.isOpacityMapInverted());
    }
    catch (const std::exception& ex)
    {
        std::cout << "Exception in SceneRuntime::applyTextureMapsToMesh\n" << ex.what() << std::endl;
    }
}

std::vector<unsigned int> SceneRuntime::drainTextureCacheGpuIds()
{
    std::vector<unsigned int> gpuIds;
    for (auto& entry : _texCache)
    {
        if (entry.second.lastGPUTexture != 0)
            gpuIds.push_back(entry.second.lastGPUTexture);
    }
    _texCache.clear();
    _texRefCount.clear();
    return gpuIds;
}
```

**`src/GLWidget.cpp`** — update the three methods:

```cpp
void GLWidget::setMaterialToObjects(const std::vector<int>& ids, const GLMaterial& mat)
{
    if (_sceneRuntime.applyMaterialToMeshes(ids, mat))
        setTransmissionEnabled(true);
}

void GLWidget::setTexturesToObjects(const std::vector<int>& ids, const GLMaterial& mat)
{
    const GLMaterial resolved = resolveMaterialTextures(this, mat);
    for (int id : ids)
        _sceneRuntime.applyTextureMapsToMesh(id, resolved);
}

void GLWidget::clearTextureCache()
{
    const std::vector<unsigned int> gpuIds = _sceneRuntime.drainTextureCacheGpuIds();
    for (unsigned int id : gpuIds)
        glDeleteTextures(1, &id);
}
```

`synchronizeTextureCache` stays in GLWidget unchanged — it calls `getOrLoadTextureCached` which requires a GL context.

**Files changed:** `include/SceneRuntime.h`, `src/SceneRuntime.cpp`, `src/GLWidget.cpp`

---

## Item 3 — `applyTransforms`: extract TRS loop and model-level query to `SceneRuntime`

### Problem

`GLWidget::applyTransforms()` (`src/GLWidget.cpp:4229`) applies TRS to each mesh by iterating the mesh store directly. The mesh-store mutation belongs in `SceneRuntime`. The orchestration (stats update, explosion, lights, camera reapplication, shadow, floor, fit) legitimately stays in GLWidget.

### Current structure (lines 4229–4295)

```
applyTransforms(transforms, fitView):
  1. [lines 4237–4255] Loop: mesh->setTranslation/setRotation/setScaling   ← MOVE to SceneRuntime
  2. [line 4259]        isModelLevelTransform detection                      ← MOVE to SceneRuntime
  3. [line 4265]        recalculateVisibleSceneStats(false)
  4. [lines 4266–4267]  updateExplosion() if panel visible
  5. [line 4268]        updatePunctualLights()
  6. [lines 4269–4286]  Re-apply glTF camera if model-level                 ← EXTRACT to private helper
  7. [lines 4287–4291]  triggerShadowRecomputation(), updateFloorPlane(), fitAll()
```

### Prerequisite

`TransformState` is defined in `include/TransformCommand.h` (not inside `GLWidget.h`). Verify `SceneRuntime.h` can include it without circular dependency. If `TransformCommand.h` includes GLWidget types, those includes must be removed or the struct moved to a standalone `include/TransformState.h`.

### Changes

**`include/SceneRuntime.h`** — add declarations:

```cpp
#include "TransformCommand.h"   // for TransformState — add if not present

// Apply a map of mesh-index → TransformState to the mesh store.
void applyMeshTransforms(const QMap<int, TransformState>& transforms);

// Returns true when every mesh in the store is covered by the given transform count
// (i.e., this is a model-level transform rather than a per-part transform).
bool isModelLevelTransform(int transformCount) const;
```

**`src/SceneRuntime.cpp`** — add implementations:

```cpp
void SceneRuntime::applyMeshTransforms(const QMap<int, TransformState>& transforms)
{
    for (auto it = transforms.begin(); it != transforms.end(); ++it)
    {
        const int index = it.key();
        const TransformState& state = it.value();
        if (index < 0 || index >= static_cast<int>(_meshStore.size()))
            continue;
        SceneMesh* mesh = meshAt(index);
        if (!mesh) continue;
        mesh->setTranslation(state.translation);
        if (state.hasExactRotation)
            mesh->setRotationQuaternion(state.rotationQuat, state.rotation);
        else
            mesh->setRotation(state.rotation);
        mesh->setScaling(state.scale);
    }
}

bool SceneRuntime::isModelLevelTransform(int transformCount) const
{
    return transformCount == static_cast<int>(_meshStore.size());
}
```

**`include/GLWidget.h`** — add private declaration:

```cpp
// Re-applies the active glTF camera and animation pose after a model-level transform.
void reapplyGltfCameraAfterTransform();
```

**`src/GLWidget.cpp`** — extract the glTF camera block and rewrite `applyTransforms`:

```cpp
void GLWidget::reapplyGltfCameraAfterTransform()
{
    if (!isGltfCameraActive() || !_viewer)
        return;
    const GltfCameraData camData =
        _viewer->sceneGraph()->gltfCameraDataForFile(_animCtrl.activeGltfCameraFile());
    if (_animCtrl.activeGltfCameraIndex() < 0 ||
        _animCtrl.activeGltfCameraIndex() >= camData.cameras.size())
        return;
    const GltfCameraEntry& cam = camData.cameras[_animCtrl.activeGltfCameraIndex()];
    applyGltfCameraEntryTransform(cam);
    const bool animationOwnsThisFile =
        _animCtrl.activeAnimationFile() == _animCtrl.activeGltfCameraFile() &&
        _animCtrl.activeAnimationClip() >= 0;
    if (animationOwnsThisFile)
        applyAnimationPose(_animCtrl.activeAnimationFile(),
                           _animCtrl.activeAnimationClip(),
                           _animCtrl.animationCurrentTimeSeconds());
}

void GLWidget::applyTransforms(const QMap<int, TransformState>& transforms, bool fitView)
{
    if (transforms.isEmpty())
        return;
    makeCurrent();

    _sceneRuntime.applyMeshTransforms(transforms);
    const bool isModelLevel = _sceneRuntime.isModelLevelTransform(transforms.size());

    recalculateVisibleSceneStats(false);
    if (_explodedViewPanel && _explodedViewPanel->isVisible())
        updateExplosion();
    updatePunctualLights();
    if (isModelLevel)
        reapplyGltfCameraAfterTransform();
    triggerShadowRecomputation();
    updateFloorPlane();
    if (fitView && !isGltfCameraActive())
        fitAll();

    doneCurrent();
}
```

**Files changed:** `include/SceneRuntime.h`, `src/SceneRuntime.cpp`, `include/GLWidget.h`, `src/GLWidget.cpp`

---

## Item 4 — `applyAnimationPose`: extract sampling to `AnimationRuntimeController`; split application into named private methods

### Problem

`GLWidget::applyAnimationPose()` (`src/GLWidget.cpp:10191`, ~400 lines) is a monolithic method with two distinct phases:

1. **Channel sampling** (lines 10211–10500 approx.) — reads `GltfAnimationClip` channels, samples keyframes, produces per-frame state (node transforms, morph weights, material changes, node visibility, shadow-affected flag). This is **pure computation on `AnimationRuntimeController` data** — no mesh mutations, no GL, no side effects.

2. **State application** (lines 10500–10600 approx.) — distributes the sampled state across the mesh store, light pipeline, and camera. This is **cross-cutting** — it touches `_sceneRuntime`, `_animCtrl`, `_viewCtrl`, and GLWidget side-effects (`fitAll`, `triggerShadowRecomputation`).

### Design decision

- **Phase 1 (sampling)** → moves to `AnimationRuntimeController::sampleClip()`. Pure, stateless, no dependencies on SceneRuntime or GLWidget.
- **Phase 2 (application)** → stays in GLWidget, split into 6 named private methods. Each stage has a clear name and bounded scope. Moving them to controllers would require controllers to hold cross-references to each other, which is worse than the current design.

### Step 4a — Define `AnimationSampleResult` in `AnimationRuntimeController.h`

Add after the existing struct definitions:

```cpp
// Output of sampleClip(). Carries all per-frame sampled state needed by the
// application stages. GLWidget reads this struct; it does not write to it.
struct AnimationSampleResult
{
    // Sampled node TRS (keyed by node UUID; initialized from defaultNodeTransformsByUuid)
    QHash<QUuid, RuntimeNodeTransform>  nodeTransforms;
    // Sampled mesh-direct TRS (for meshes targeted directly, not via node hierarchy)
    QHash<QUuid, RuntimeNodeTransform>  meshTransforms;
    // Sampled morph weights (keyed by mesh UUID)
    QHash<QUuid, QVector<float>>        morphWeights;
    // Animated material state (keyed by mesh UUID; initialized from defaultMeshMaterials)
    QHash<QUuid, GLMaterial>            animatedMaterials;
    // Sampled node visibility (keyed by nodeIndex)
    QHash<int, bool>                    nodeVisibility;
    // World-space transforms computed during node-transform application (stage 2).
    // Consumed by light and camera application stages (stages 5–6).
    // IMPORTANT: populated in applyNodeTransformsToMeshes(); must not be read before that.
    QHash<QUuid, QMatrix4x4>            worldTransforms;
    // True if any sampled channel animates a node that owns mesh geometry.
    // When true, GLWidget must call triggerShadowRecomputation().
    bool                                affectsShadowCasters = false;
};
```

### Step 4b — Add `sampleClip()` to `AnimationRuntimeController`

**`include/AnimationRuntimeController.h`** — add declaration:

```cpp
// Sample all channels of the given clip at timeSeconds and return the per-frame state.
// Called by GLWidget::applyAnimationPose() before any mesh/light/camera mutations.
// sg is used only for NodeVisibility pointer-path queries (may be nullptr for non-pointer clips).
AnimationSampleResult sampleClip(
    const RuntimeAnimationFileState& runtime,
    const GltfAnimationClip& clip,
    double timeSeconds,
    const SceneGraph* sg) const;
```

**`src/AnimationRuntimeController.cpp`** — move the channel-sampling loop from `GLWidget::applyAnimationPose` lines 10211–~10500 into this method. The method:
- Initializes `result.nodeTransforms` from `runtime.defaultNodeTransformsByUuid`
- Initializes `result.morphWeights` from `runtime.defaultNodeMorphWeightsByUuid`
- Initializes `result.animatedMaterials` from `runtime.defaultMeshMaterials`
- Initializes `result.nodeVisibility` from `runtime.data.nodeVisibilityStates`
- Iterates `clip.channels`, samples keyframes using `AnimationUtils::sampleVec3Keys` etc., populates all result fields
- Sets `result.affectsShadowCasters` based on whether any animated node has mesh geometry in its subtree
- Returns the populated `AnimationSampleResult`

The `nodeAffectsShadow` lambda and `hasMeshInSubtree` helper currently inside `applyAnimationPose` move into this method (or become private static helpers in `AnimationRuntimeController.cpp`).

### Step 4c — Add 6 private methods to `GLWidget`

**`include/GLWidget.h`** — add in the `private:` section:

```cpp
// Animation pose application stages — called in sequence by applyAnimationPose().
// Each stage reads from AnimationSampleResult produced by _animCtrl.sampleClip().

void applyNodeTransformsToMeshes(const QString& sourceFile,
                                  const AnimationRuntimeController::AnimationSampleResult& result,
                                  const SceneNode* fileNode,
                                  const QMatrix4x4& importCorrection);

void applyMorphTargetWeights(const QString& sourceFile,
                              const AnimationRuntimeController::AnimationSampleResult& result);

void applyAnimatedMaterialChanges(const QString& sourceFile,
                                   const AnimationRuntimeController::AnimationSampleResult& result);

void applyAnimatedMeshVisibility(const QString& sourceFile,
                                  const AnimationRuntimeController::AnimationSampleResult& result);

void applyAnimatedLightTransforms(const QString& sourceFile,
                                   const AnimationRuntimeController::AnimationSampleResult& result);

void applyAnimatedCamera(const QString& sourceFile,
                          const AnimationRuntimeController::AnimationSampleResult& result);
```

**`src/GLWidget.cpp`** — extract the corresponding blocks from `applyAnimationPose` into each method. Move the code verbatim; do not change logic.

**Critical:** `result.worldTransforms` is populated inside `applyNodeTransformsToMeshes()` (it records the final world-space transform for each animated node as it processes the node hierarchy). It is consumed by `applyAnimatedLightTransforms()` and `applyAnimatedCamera()` to position lights and cameras in world space. The result struct must be passed by non-const reference to `applyNodeTransformsToMeshes` so it can write `worldTransforms`:

```cpp
void applyNodeTransformsToMeshes(const QString& sourceFile,
                                  AnimationRuntimeController::AnimationSampleResult& result,  // non-const
                                  const SceneNode* fileNode,
                                  const QMatrix4x4& importCorrection);
```

All other 5 methods take `const AnimationSampleResult&`.

### Step 4d — Rewrite `applyAnimationPose` as orchestrator

```cpp
void GLWidget::applyAnimationPose(const QString& sourceFile, int clipIndex, double timeSeconds)
{
    if (!_viewer || !_viewer->sceneGraph())
        return;
    SceneGraph* sg = _viewer->sceneGraph();

    RuntimeAnimationFileState& runtime = _animCtrl.runtimeAnimationsByFile()[sourceFile];
    if (runtime.data.sourceFile.isEmpty())
        runtime.data = sg->animationDataForFile(sourceFile);

    if (clipIndex < 0 || clipIndex >= runtime.data.clips.size())
    {
        resetAnimationPose(sourceFile);
        return;
    }

    SceneNode* fileNode = sg->findFileNode(sourceFile);
    if (!fileNode)
        return;

    const GltfAnimationClip& clip = runtime.data.clips[clipIndex];
    const QMatrix4x4 importCorrection = /* same as current */;

    // Phase 1: pure sampling — no side effects
    AnimationRuntimeController::AnimationSampleResult result =
        _animCtrl.sampleClip(runtime, clip, timeSeconds, sg);

    // Phase 2: distribute sampled state to each subsystem
    applyNodeTransformsToMeshes(sourceFile, result, fileNode, importCorrection);  // populates result.worldTransforms
    applyMorphTargetWeights(sourceFile, result);
    applyAnimatedMaterialChanges(sourceFile, result);
    applyAnimatedMeshVisibility(sourceFile, result);
    applyAnimatedLightTransforms(sourceFile, result);
    applyAnimatedCamera(sourceFile, result);

    if (result.affectsShadowCasters)
        triggerShadowRecomputation();
}
```

**Files changed:** `include/AnimationRuntimeController.h`, `src/AnimationRuntimeController.cpp`, `include/GLWidget.h`, `src/GLWidget.cpp`

---

## Item 5 — `_originalParsedLights` / `_lightFileIndexMap` → `SceneRuntime` (separate branch)

> **Defer to a dedicated branch after Items 1–4 are merged and stable.**

### Rationale

These fields represent scene light topology (parsed from `SceneGraph` at load), not animation-clip runtime state. They belong in `SceneRuntime`.

### Prerequisites (must be done before Item 5)

1. Promote `LightOrigin` from an inner struct of `AnimationRuntimeController` to a standalone header `include/LightOrigin.h`. Both `SceneRuntime.h` and `AnimationRuntimeController.h` will include it. This can be done as a prep commit before Item 5.

### Fields to migrate

From `AnimationRuntimeController` private section:
```cpp
std::vector<GPULight> _originalParsedLights;
std::vector<GPULight> _currentRepositionedLights;
QVector<LightOrigin>  _lightFileIndexMap;
```

### Approach

The 6 `AnimationRuntimeController` methods that currently access these fields directly (`rebuildCurrentRepositionedLights`, `rebuildAndBuildUploadLights`, `buildUploadLights` (×2), `setParsedLightsFromSingleFile`, `rebuildParsedLightsFromSceneGraph`, `clearParsedLights`, `buildGizmoLights`) must be updated to accept the light data via parameters:

```cpp
// Instead of reading _originalParsedLights directly:
void rebuildCurrentRepositionedLights(
    const std::vector<GPULight>& originalLights,
    std::vector<GPULight>& repositionedLights,
    const QHash<QString, QMatrix4x4>& userTransforms);
```

Or alternatively, pass a `SceneRuntime&` reference to these methods and let them call `sceneRuntime.originalParsedLights()`.

**Files changed:** `include/LightOrigin.h` (new), `include/SceneRuntime.h`, `src/SceneRuntime.cpp`, `include/AnimationRuntimeController.h`, `src/AnimationRuntimeController.cpp`, `src/GLWidget.cpp`

---

## Commit strategy

Each item should be a separate commit:

```
SOLID phase 2: encapsulate SceneGraph light predicate in AnimationRuntimeController
SOLID phase 2: move material batch mesh-mutation to SceneRuntime
SOLID phase 2: extract applyTransforms TRS loop to SceneRuntime
SOLID phase 2: decompose applyAnimationPose into sampleClip + named application stages
```

Item 5 goes on its own branch: `refactor/light-fields-to-scene-runtime`.

---

## Risk notes for each item

| Item | Key hazard |
|---|---|
| 1 | None — pure extraction |
| 2 | `invertOpacityADSMap`/`invertOpacityPBRMap` calls must move together with `setTextureMaps` into `applyTextureMapsToMesh` |
| 2 | `drainTextureCacheGpuIds` clears both `_texCache` and `_texRefCount` — verify `clearTextureCache` in GLWidget currently clears both (it does, lines 4169–4170) |
| 3 | Check `TransformCommand.h` include graph before adding it to `SceneRuntime.h` |
| 4 | `result.worldTransforms` lifetime: it is written by `applyNodeTransformsToMeshes` and read by `applyAnimatedLightTransforms` and `applyAnimatedCamera` — these three must be called in that order |
| 4 | The `nodeAffectsShadow` lambda currently captures `sceneGraph` by pointer and builds a local cache — move both into `sampleClip`; the cache is local to one `sampleClip` call |
| 5 | `AnimationRuntimeController` method signatures change — all callers in `GLWidget.cpp` that call `_animCtrl.rebuildCurrentRepositionedLights` etc. must be updated |

---

## Phase 3 — GLWidget Facade Reduction (full ownership audit)

This phase was audited after Phase 2 was designed. It catalogues every remaining method in GLWidget that either (a) can be cleanly moved to an existing controller or (b) warrants a new class. Phase 3 is intentionally kept as a separate branch after Phase 2 items are merged and stable.

### What genuinely stays in GLWidget

These categories have legitimate GL-context or Qt-event-loop affinity and must not move:

| Category | Examples |
|---|---|
| Qt virtual event handlers | `mousePressEvent`, `keyPressEvent`, `resizeEvent`, `closeEvent`, `wheelEvent` |
| GL lifecycle | `initializeGL`, `resizeGL`, all FBO init/resize/cleanup |
| Render passes | `paintGL`, `render`, `renderToShadowBuffer`, `renderToTransmissionBuffer`, all `draw*` |
| GL resource creation | `createShaderPrograms`, texture upload, cubemap generation, `createWhiteTexture`, `createFullscreenTriangle` |
| Animation playback orchestration | `onAnimationTick`, `applyAnimationPose`, `syncFileNodeTransforms`, `resetAnimationPose` |
| Camera/view orchestration | `fitAll`, `setView`, `animateViewChange`, `updateFloorPlane`, `splitScreen` |
| Interactive picking & gizmo drag | All gizmo drag methods, view-cube click, `sweepSelect` |
| Debug readback | `requestTextureReadback`, `applyDebugTextureState`, `setDebugTextureEnabled` |
| Shadow & environment | `updateMainLightPosition`, `calculateLightDistance`, `loadEnvMap`, HDR cubemap conversion chain |

---

### Moves to existing controllers

#### → `AnimationRuntimeController` (~25 methods)
Every method that sets or queries animation/glTF-camera state and then calls `update()`. Zero GL logic in GLWidget; it is a pure facade.

| Method(s) | Reason |
|---|---|
| `setActiveAnimation`, `seekAnimation`, `setAnimationPlaying`, `setAnimationLooping`, `setAnimationPlaybackSpeed` | 100% delegate to `_animCtrl`; only `update()` is added |
| `clearAnimationRuntimeForFile`, `syncRuntimeNodeTransforms`, `refreshAnimationMaterialState` (partial) | Clean `_animCtrl` state; no render coordination |
| `activeAnimationFile`, `activeAnimationClip`, `currentAnimationTimeSeconds`, `isAnimationPlaying`, `isAnimationLooping`, `animationPlaybackSpeed` | Pure `_animCtrl` accessors |
| `activateGltfCamera`, `resetToSystemCamera`, `isGltfCameraActive`, `activeGltfCameraFile`, `activeGltfCameraIndex` | Pure `_animCtrl` glTF camera state |
| `getParsedLights`, `getRepositionedLights`, `getLightFileIndexMap`, `setParsedLights` | `_animCtrl` light state accessors/setters |
| `setAnimatedLightVisibilityState`, `setAnimatedLightTransformState`, `clearAnimatedLightVisibilityState`, `clearAnimatedLightTransformState` | Manipulate `_animCtrl.animatedLights*` state only |

#### → `SceneRenderController` (~40 methods)
All `show*`, `set*Enabled`, `get*` accessors that forward directly to `_renderCtrl`. No GL logic in GLWidget.

| Method group | Reason |
|---|---|
| `showEnvironment`, `showSkyBox`, `blurSkyBox`, `setSkyBoxBlurPercent`, `setSkyBoxFOV`, `setSkyBoxZRotation`, `setSkyBoxTextureHDRI`, `updateEnvMapRotationMatrix` | Delegate to `_renderCtrl`; no GL call |
| `showReflections`, `getEnvironmentMap`, `getIrradianceMap`, `getPrefilterMap`, `getSheenPrefilterMap` | Pure `_renderCtrl` accessors |
| `getCurrentSkyboxFolder`, `isSkyBoxShown`, `isSkyBoxHDRIEnabled`, `getSkyBoxBlurPercent`, `getSkyBoxFOV`, `getSkyBoxZRotation` | Pure `_renderCtrl` state queries |
| `showFloor`, `setGroundMode`, `isFloorShown`, `isGridShown`, `groundMode`, `getFloorSize` | Floor/ground mode state; no GL |
| `showFloorTexture`, `setFloorTexture`, `setFloorTexRepeatS`, `setFloorTexRepeatT`, `setFloorOffsetPercent` | Floor material parameters |
| `enableHDRToneMapping`, `getHdrToneMapping`, `isHDRToneMappingEnabled`, `setHDRToneMappingMode`, `getHDRToneMappingMode` | HDR pipeline flags |
| `enableGammaCorrection`, `getGammaCorrection`, `isGammaCorrectionEnabled`, `setScreenGamma`, `getScreenGamma` | Gamma/tonemapping params |
| `setEnvMapExposure`, `getEnvMapExposure`, `setIBLExposure`, `getIBLExposure` | Exposure params |
| `setDisplayMode`, `getDisplayMode`, `isShaded`, `setRenderingMode`, `getRenderingMode` | Render mode enum state |
| `setTransmissionEnabled`, `isTransmissionEnabled` | Render feature flag |
| `setShowVertexNormals`, `isVertexNormalsShown`, `setShowFaceNormals`, `isFaceNormalsShown`, `setShowBoundingBox`, `isBoundingBoxShown` | Debug overlay state |
| `setDebugOverlayMode`, `debugOverlayMode`, `setDebugOverlayEnabled`, `isDebugOverlayEnabled`, `setDebugOverlayAvailability` | Debug overlay management |
| `showShadows`, `showSelfShadows`, `setShadowQuality` | Shadow quality/enable flags |
| `setBgTopColor`, `getBgTopColor`, `setBgBotColor`, `getBgBotColor`, `getBgGradientStyle`, `setBgGradientStyle` | Background color state |
| All clipping hatch params: `setClippingPlaneHatchMode`, `setHatchTiling`, `setHatchLineThickness`, `setHatchIntensity`, `setHatchLayers`, `setHatchLineColor`, `setHatchTexture` | Hatch material params |
| `setCappingPlanesEnabled`, `cappingPlanesEnabled`, `setSectionCapsInteractionSuppressed`, `setSectionCapsDynamicEnabled`, `disableSectionCapsInteractionSuppression` | Section caps flags |
| All clipping axis wrappers: `setYZClippingEnabled` … `clippingZCoeff` (15 methods) | Direct delegates to `_renderCtrl` |
| `setCornerAxisPosition` | Render state enum; no interaction logic |
| `isAnisotropicFilteringLevel`, `setAnisotropicFilteringLevel` | Render quality param |
| `areDefaultLightsEnabled`, `arePunctualLightsEnabled`, `areShadowsEnabled`, `areSelfShadowsEnabled`, `areReflectionsEnabled` | Pure `_renderCtrl` accessors |
| `setDefaultLightColor`, `getDefaultLightColor`, `setLightOffset`, `getLightOffset` | Light parameter state; no GPU upload |
| `useDefaultLights`, `usePunctualLights`, `useIBL` | Light mode setters; delegate to `_renderCtrl` |
| `getShader` | Accessor of `_renderCtrl.fgShader()` |

#### → `ViewportInteractionController` (~8 methods)

| Method(s) | Reason |
|---|---|
| `setRotationActive`, `setPanningActive`, `setZoomingActive` | Toggle `_viewCtrl` flags only |
| `setRotations`, `setZoomAndPan` | Pure camera state setters |
| `beginWindowZoom`, `performWindowZoom` | Interactive zoom gesture state |
| `fitBoxToScreen` | Camera positioning math; no render logic |
| `getViewMatrix`, `getProjectionMatrix`, `getModelViewMatrix`, `isMultiViewActive`, `getPerspFOV` | Pure `_viewCtrl` accessors |

#### → `SceneRuntime` (~15 methods)

| Method(s) | Reason |
|---|---|
| `addToDisplay`, `removeFromDisplay`, `setDisplayList`, `getDisplayedObjectsIds` | Display list is a SceneRuntime concern |
| `getMeshStore`, `getMeshByUuid`, `getMeshByIndex`, `getIndexByUuid`, `getUuidByIndex`, `getModelNum` | Simple mesh store accessors |
| `generateUniqueMeshName` | Name uniqueness logic; no rendering |
| `clearMeshStore` | Mesh store lifecycle |
| `invalidateRuntimeVisibilityHierarchy`, `rebuildRuntimeVisibilityHierarchy`, `ensureRuntimeVisibilityHierarchy` | BVH management |
| `refreshRuntimeVisibilityCacheForCurrentView`, `buildRuntimeVisibilityNodeRecursive`, `collectVisibleMeshIdsForPass` | BVH traversal |
| `isVisibleSwapped`, `swapVisible` | Display visibility flag |
| `userModelTransformForFile` | Per-file transform uniformity query |
| `getBoundingSphere` | Precomputed bounding data query |
| `moveToRecycleBin`, `restoreFromRecycleBin`, `permanentlyDeleteFromBin`, `isInRecycleBin`, `getRecycleBinUuids` | Recycle-bin state management |

#### → `SelectionManager` (~4 methods)

| Method(s) | Reason |
|---|---|
| `select`, `deselect`, `syncMeshSelectionVisualState` | State authority already in SelectionManager |
| `processSelection` | Color-picking ID resolution belongs with SelectionManager |
| `broadcastSelectionChanged` | Signal forwarding; just re-emits SelectionManager signal |
| `activeTransformGizmoSelectionIds` | Query of selection state |

---

### New classes warranted

#### `VisibilityComputationHelper` — pure frustum/clip math

All 20+ frustum and clip-plane culling methods have zero GL state changes — they are pure math on current camera matrices. A free function namespace or a stateless helper class works.

| Methods |
|---|
| `isBoundingBoxOutsideFrustum`, `isMeshOutsideFrustum`, `isMeshFullyInsideFrustum` |
| All per-axis clip tests: `isMeshFullyClipped_X/Y/Z`, `isMeshPartiallyClipped_X/Y/Z` |
| `collectVisibleCorners` (all overloads) |
| `computeFitViewRange` (all 4 overloads) |
| `computeSharedOrthographicMultiViewRange` |
| `fitBoxToScreen` (if not taken by ViewportInteractionController) |

#### `CoordinateSystemHelper` — pure coordinate convention math

| Methods |
|---|
| `standardViewBasis`, `standardViewRotation` |
| `sceneUpAxisIsZUp`, `sceneUpAxisLabel` |
| `groundPlaneZ`, `groundPlaneScaleFactor`, `groundPlaneExtent` |
| `floorPlaneOrientation`, `currentWorldUpVector`, `coordinateAlongCurrentWorldUp`, `setCoordinateAlongCurrentWorldUp` |
| `cameraUpAxisConventionRotation`, `transformVectorForCameraUpAxis` |
| `warnOnConflictingImportedSceneUpAxis` |

#### `MvfMeshPreparationWorker` — CPU-only, thread-safe MVF mesh prep

The static `prepareMvfMeshes()` and the `PreparedMvfMesh` struct are already thread-safe and have no GL affinity. Move them to a standalone worker class so the MVF loading path is testable without a GL context.

| Items |
|---|
| `GLWidget::prepareMvfMeshes()` (static) |
| `GLWidget::PreparedMvfMesh` (struct) |

#### `TextureCacheManager` — texture cache lifecycle

`getOrCreateTextureCached`, `getOrLoadKtx2TextureCached`, `getOrLoadTextureCached`, `retainTexture`, `releaseTexture`. The GL upload calls (`createGPUTextureFromImage`, `uploadDecodedTexture`, etc.) stay in GLWidget; the cache lookup, reference counting, and eviction belong in a dedicated manager.

#### `PickingHelper` — ID↔color encoding

`colorToIndex`, `indexToColor`, `get3dTranslationVectorFromMousePoints` — pure math; no GL state.

---

### Summary: GLWidget surface area reduction

| Destination | Methods moving out |
|---|---|
| `AnimationRuntimeController` | ~25 |
| `SceneRenderController` | ~40 |
| `ViewportInteractionController` | ~8 |
| `SceneRuntime` | ~15 |
| `SelectionManager` | ~5 |
| `VisibilityComputationHelper` (new) | ~20 |
| `CoordinateSystemHelper` (new) | ~12 |
| `MvfMeshPreparationWorker` (new) | 2 |
| `TextureCacheManager` (new) | ~5 |
| `PickingHelper` (new) | 3 |
| **Total** | **~135 methods** |

The ~30–35 methods that remain in GLWidget are the legitimate GL-context-bound, Qt-event-bound, and cross-controller orchestration methods listed in the "What genuinely stays" table above.

### Phase 3 branch strategy

Each destination should be a separate commit (or small group of commits) on a dedicated branch:

```
refactor/glwidget-animation-facade-to-animctrl
refactor/glwidget-render-state-to-renderctrl
refactor/glwidget-viewport-state-to-viewctrl
refactor/glwidget-mesh-ops-to-sceneruntime
refactor/glwidget-selection-to-selectionmanager
refactor/visibility-computation-helper
refactor/coordinate-system-helper
refactor/mvf-mesh-preparation-worker
refactor/texture-cache-manager
refactor/picking-helper
```

Priority order: start with the animation facade (25 methods, zero risk — pure delegation) and render state accessors (40 methods, same pattern). These alone remove ~65 methods from GLWidget with no architectural risk.


---

# GLWidget Method Ownership Map
**Branch:** `refactor/mesh-render-runtime-separation`  
**Source of truth:** `include/GLWidget.h` as audited on 2026-06-28

Each method is listed with its exact signature and bare method name for quick lookup.  
Legend: **STAY** = GL-context or Qt-event affinity; **MOVE** = clean extraction; **SPLIT** = partial extraction.

---

## Destination: STAY in GLWidget

### Constructor / Destructor
| Signature | Method Name | Access |
|---|---|---|
| `GLWidget(QWidget* parent = 0, const char* name = 0)` | `GLWidget` | public |
| `~GLWidget()` | `~GLWidget` | public |

### Qt i18n / UI wiring
| Signature | Method Name | Access |
|---|---|---|
| `void retranslateUI()` | `retranslateUI` | public |
| `void updateView()` | `updateView` | public |
| `void resizeView(int w, int h)` | `resizeView` | public |
| `ViewToolbar* getViewToolbar() const` | `getViewToolbar` | public |
| `void cleanUpShaders()` | `cleanUpShaders` | public |
| `SelectionManager* getSelectionManager() const` | `getSelectionManager` | public slot |
| `void showFileReadingProgress(float percent)` | `showFileReadingProgress` | public slot |
| `void showMeshLoadingProgress(float percent)` | `showMeshLoadingProgress` | public slot |
| `void showNodeMeshLoadingProgress(int processedNodes, int totalNodes, int processedMeshes, int totalMeshes, bool uvProcessed)` | `showNodeMeshLoadingProgress` | public slot |
| `void cancelAssImpModelLoading()` | `cancelAssImpModelLoading` | public slot |
| `void showContextMenu(const QPoint& pos)` | `showContextMenu` | private slot |
| `void centerDisplayList()` | `centerDisplayList` | private slot |
| `void setBackgroundColor()` | `setBackgroundColor` | private slot |
| `void updateOverlayEditorTheme()` | `updateOverlayEditorTheme` | private |
| `void applyOverlayPanelStyle(QWidget* wrapper, const QString& objectName)` | `applyOverlayPanelStyle` | protected |
| `void refreshNavigationOverlayStyle()` | `refreshNavigationOverlayStyle` | protected |
| `void loadBgColorSettings()` | `loadBgColorSettings` | private |

### Overlay panels
| Signature | Method Name | Access |
|---|---|---|
| `QWidget* attachOverlayPanel(QWidget* contentWidget, const QRect& geometry, Qt::Alignment alignment, const QString& objectName)` | `attachOverlayPanel` | public |
| `QWidget* takeOverlayPanel(QWidget* contentWidget)` | `takeOverlayPanel` | public |
| `void refreshDetachedNavigationOverlayTheme()` | `refreshDetachedNavigationOverlayTheme` | public |

### Exploded view panel UI
| Signature | Method Name | Access |
|---|---|---|
| `void showExplodedViewPanel(bool show)` | `showExplodedViewPanel` | public |
| `ExplodedViewPanel* getExplodedViewPanel() const` | `getExplodedViewPanel` | public |

### Camera & view orchestration (GL context required)
| Signature | Method Name | Access |
|---|---|---|
| `void setViewMode(ViewMode mode)` | `setViewMode` | public |
| `void setCameraUpAxisZUp(bool zUp, bool syncToolbar = true)` | `setCameraUpAxisZUp` | public |
| `void setProjection(ViewProjection proj)` | `setProjection` | public |
| `void setCameraMode(GLCamera::CameraMode mode)` | `setCameraMode` | public |
| `void setMultiView(bool active)` | `setMultiView` | public |
| `void setShowCenterAxisOverride(bool show)` | `setShowCenterAxisOverride` | public |
| `void setShowCornerAxisOverride(bool show)` | `setShowCornerAxisOverride` | public |
| `void setShowViewCubeOverride(bool show)` | `setShowViewCubeOverride` | public |
| `void showAxis(bool show)` | `showAxis` | public |
| `void showTransformGizmoForSelection(bool show)` | `showTransformGizmoForSelection` | public |
| `GltfCameraData cameraDataForMvfSave(const GltfCameraData& source) const` | `cameraDataForMvfSave` | public |
| `void centerScreen(std::vector<int> selectedIDs)` | `centerScreen` | public |
| `void animateViewChange()` | `animateViewChange` | public slot |
| `void animateFitAll()` | `animateFitAll` | public slot |
| `void animateWindowZoom()` | `animateWindowZoom` | public slot |
| `void animateCenterScreen()` | `animateCenterScreen` | public slot |
| `void fitAll()` | `fitAll` | public slot |
| `void setAutoFitViewOnUpdate(bool update)` | `setAutoFitViewOnUpdate` | public slot |
| `void performKeyboardNav()` | `performKeyboardNav` | public slot |
| `void disableLowRes()` | `disableLowRes` | public slot |
| `GLCamera* getCameraForPoint(const QPoint& pixel)` | `getCameraForPoint` | public slot |
| `void setView(QVector3D viewPos, QVector3D viewDir, QVector3D upDir, QVector3D rightDir)` | `setView` | private |
| `void animateToRotation(const QQuaternion& targetRotation)` | `animateToRotation` | private |
| `void syncCameraWorldUp()` | `syncCameraWorldUp` | private |
| `void rotateCurrentCameraAroundWorldX(float degrees)` | `rotateCurrentCameraAroundWorldX` | private |
| `void applyAutoOrientCameraConvention(SceneUpAxis sceneUpAxis)` | `applyAutoOrientCameraConvention` | private |
| `bool positionGameplayCameraForScene(GLCamera::CameraMode mode)` | `positionGameplayCameraForScene` | private |
| `void configureOrthoSubviewCamera(ViewMode viewMode, const std::vector<QVector3D>& corners, int viewportWidth, int viewportHeight, const QVector3D& sharedCenter, float sharedViewRange)` | `configureOrthoSubviewCamera` | private |
| `void splitScreen()` | `splitScreen` | private |
| `void updateZoomInLimit()` | `updateZoomInLimit` | private |
| `QRect getViewportFromPoint(const QPoint& pixel)` | `getViewportFromPoint` | private |
| `QRect getClientRectFromPoint(const QPoint& pixel)` | `getClientRectFromPoint` | private |
| `void applyGltfCameraEntryTransform(const GltfCameraEntry& cam)` | `applyGltfCameraEntryTransform` | private |

### Inertia / animation timers
| Signature | Method Name | Access |
|---|---|---|
| `void onInertiaTimer()` | `onInertiaTimer` | public slot |
| `void stopAnimations()` | `stopAnimations` | public slot |
| `void checkAndStopTimers()` | `checkAndStopTimers` | public slot |

### Shadow & lighting orchestration
| Signature | Method Name | Access |
|---|---|---|
| `void triggerShadowRecomputation()` | `triggerShadowRecomputation` | public |
| `float calculateLightDistance()` | `calculateLightDistance` | public |
| `void updateFloorPlane()` | `updateFloorPlane` | public |
| `void updateBoundingSphere()` | `updateBoundingSphere` | public |
| `void updateBoundingBox()` | `updateBoundingBox` | public |
| `void showLights(bool showLights)` | `showLights` | public slot |
| `void applyEnabledLightList(const std::vector<GPULight>& enabledLights)` | `applyEnabledLightList` | public slot |
| `void onSceneLightDataChanged()` | `onSceneLightDataChanged` | public |
| `void syncDefaultLightColorUniforms()` | `syncDefaultLightColorUniforms` | private |
| `void syncPunctualLightUniforms(int lightCount, bool hasPunctualLights)` | `syncPunctualLightUniforms` | private |
| `bool shouldUseFallbackLightForVisibleScene() const` | `shouldUseFallbackLightForVisibleScene` | private |
| `void updatePunctualLights()` | `updatePunctualLights` | private |
| `void updateMainLightPosition(float halfObjectSize)` | `updateMainLightPosition` | private |
| `QVector3D effectiveWorldLightOffset() const` | `effectiveWorldLightOffset` | private |
| `QVector3D effectiveWorldLightPosition() const` | `effectiveWorldLightPosition` | private |

### Clipping / caps orchestration
| Signature | Method Name | Access |
|---|---|---|
| `void updateClippingPlane()` | `updateClippingPlane` | public |
| `void showClippingPlaneEditor(bool show)` | `showClippingPlaneEditor` | public |
| `void updateExplosion()` | `updateExplosion` | public |

### Model loading
| Signature | Method Name | Access | Notes |
|---|---|---|---|
| `bool loadAssImpModel(const QString& fileName, const UVMethod& uvMethod, QString& error, bool progressiveLoading = false)` | `loadAssImpModel` | public | SPLIT â€” mesh store population â†’ SceneRuntime |
| `bool generateUVsForMeshes(const std::vector<int>& ids, const UVMethod& uvMethod, const UVConfig& uvConfig, QString& error)` | `generateUVsForMeshes` | public | SPLIT |
| `void onMeshBatchReady(const std::vector<AssImpMeshData>& batch)` | `onMeshBatchReady` | private | |
| `void uploadOneMvfMesh(const PreparedMvfMesh& pm)` | `uploadOneMvfMesh` | public | |
| `bool uploadPreparedMvfMeshes(const QVector<PreparedMvfMesh>& meshes)` | `uploadPreparedMvfMeshes` | public | |
| `bool loadMvfMeshes(const Mvf::Document& document, const QByteArray& geometryChunk, const QByteArray& imageChunk)` | `loadMvfMeshes` | public | SPLIT |
| `UVMethod promptLargeModelUVDecision(int totalTriangles, UVMethod currentMethod)` | `promptLargeModelUVDecision` | private | |

### Selection & picking (GL thread or event coordination)
| Signature | Method Name | Access |
|---|---|---|
| `QList<int> sweepSelect(const QPoint& pixel, bool addToSelection = false)` | `sweepSelect` | private |

### Animation playback orchestration
| Signature | Method Name | Access |
|---|---|---|
| `void onAnimationTick()` | `onAnimationTick` | public slot |
| `void syncFileNodeTransforms(const QString& sourceFile)` | `syncFileNodeTransforms` | private |
| `void applyAnimationPose(const QString& sourceFile, int clipIndex, double timeSeconds)` | `applyAnimationPose` | private |
| `void resetAnimationPose(const QString& sourceFile)` | `resetAnimationPose` | private |
| `void updateAnimatedMeshState(const QString& sourceFile, const QHash<QUuid, QMatrix4x4>& worldTransformsByNodeUuid)` | `updateAnimatedMeshState` | private |
| `void applyExplodedViewManualPlacementSessionTransform()` | `applyExplodedViewManualPlacementSessionTransform` | private |

### Gizmo interaction (mouse events + GL pick)
| Signature | Method Name | Access |
|---|---|---|
| `BoundingSphere computeTransformGizmoSelectionSphere() const` | `computeTransformGizmoSelectionSphere` | private |
| `QVector3D computeTransformGizmoPivot() const` | `computeTransformGizmoPivot` | private |
| `void syncTransformGizmoToSelection()` | `syncTransformGizmoToSelection` | private |
| `bool beginTransformGizmoDrag(TransformGizmo::Handle handle, const QPoint& pixel)` | `beginTransformGizmoDrag` | private |
| `bool beginTransformGizmoTranslationDrag(TransformGizmo::Handle handle, const QPoint& pixel)` | `beginTransformGizmoTranslationDrag` | private |
| `void updateTransformGizmoTranslationDrag(const QPoint& pixel)` | `updateTransformGizmoTranslationDrag` | private |
| `void finishTransformGizmoTranslationDrag(bool commit)` | `finishTransformGizmoTranslationDrag` | private |
| `bool beginTransformGizmoScaleDrag(TransformGizmo::Handle handle, const QPoint& pixel, bool uniformScale)` | `beginTransformGizmoScaleDrag` | private |
| `void updateTransformGizmoScaleDrag(const QPoint& pixel)` | `updateTransformGizmoScaleDrag` | private |
| `void finishTransformGizmoScaleDrag(bool commit)` | `finishTransformGizmoScaleDrag` | private |
| `bool beginTransformGizmoRotationDrag(TransformGizmo::Handle handle, const QPoint& pixel)` | `beginTransformGizmoRotationDrag` | private |
| `void updateTransformGizmoRotationDrag(const QPoint& pixel)` | `updateTransformGizmoRotationDrag` | private |
| `void finishTransformGizmoRotationDrag(bool commit)` | `finishTransformGizmoRotationDrag` | private |

### View cube (GL rendering + Qt event)
| Signature | Method Name | Access |
|---|---|---|
| `void initializeViewCubeLabels()` | `initializeViewCubeLabels` | private |
| `bool computeViewCubeRenderState(QRect& viewportRect, QMatrix4x4& viewMatrix, QMatrix4x4& projectionMatrix, QMatrix4x4& modelMatrix, float& cubeScale) const` | `computeViewCubeRenderState` | private |
| `bool pickViewCubeRegionAtPixel(const QPoint& pixel, QVector3D& outwardNormal, int* regionId = nullptr) const` | `pickViewCubeRegionAtPixel` | private |
| `bool handleViewCubeClick(const QPoint& pixel)` | `handleViewCubeClick` | private |
| `void updateViewCubeHover(const QPoint& pixel, Qt::MouseButtons buttons)` | `updateViewCubeHover` | private |
| `bool orientCameraToViewCubeNormal(const QVector3D& outwardNormal)` | `orientCameraToViewCubeNormal` | private |

### GL resource creation & render passes
| Signature | Method Name | Access |
|---|---|---|
| `void initializeGL()` | `initializeGL` | protected |
| `void createCappingPlanes()` | `createCappingPlanes` | protected |
| `void resizeGL(int width, int height)` | `resizeGL` | protected |
| `void paintGL()` | `paintGL` | protected |
| `void renderSingleView(QColor& topColor, QColor& botColor)` | `renderSingleView` | protected |
| `void renderMultiView(QColor& topColor, QColor& botColor)` | `renderMultiView` | protected |
| `void createShaderPrograms()` | `createShaderPrograms` | private |
| `void syncUniformsToFlatShader()` | `syncUniformsToFlatShader` | private |
| `void createLights()` | `createLights` | private |
| `void createFullscreenTriangle()` | `createFullscreenTriangle` | private |
| `void drawFullscreenTriangle()` | `drawFullscreenTriangle` | private |
| `void setIBLFaceBasis(QOpenGLShaderProgram* prog, int faceIndex)` | `setIBLFaceBasis` | private |
| `void loadEnvMap()` | `loadEnvMap` | private |
| `void loadIrradianceMap()` | `loadIrradianceMap` | private |
| `GLuint loadPresetEnvironmentMap(const QString& hdrFilePath)` | `loadPresetEnvironmentMap` | private |
| `bool generatePresetIBLMaps(GLuint sourceCubemap, GLuint& outIrradianceMap, GLuint& outPrefilterMap, GLuint& outSheenPrefilterMap)` | `generatePresetIBLMaps` | private |
| `void loadFloor()` | `loadFloor` | private |
| `void ensureShadowMapResources()` | `ensureShadowMapResources` | private |
| `void loadGrid()` | `loadGrid` | private |
| `void applyFloorPlaneMaterialSettings()` | `applyFloorPlaneMaterialSettings` | private |
| `void syncFloorPlaneAlbedoTexture()` | `syncFloorPlaneAlbedoTexture` | private |
| `float updateFloorGeometry()` | `updateFloorGeometry` | private |
| `void drawMesh(QOpenGLShaderProgram* prog, int activeCapPlaneIndex = -1)` | `drawMesh` | private |
| `void drawOpaqueMeshes(QOpenGLShaderProgram* prog, int activeClipPlaneIndex = -1)` | `drawOpaqueMeshes` | private |
| `void drawTransparentMeshes(QOpenGLShaderProgram* prog, int activeClipPlaneIndex = -1)` | `drawTransparentMeshes` | private |
| `void drawMeshesWithClipping(QOpenGLShaderProgram* prog, bool transparentPass)` | `drawMeshesWithClipping` | private |
| `void setCommonUniforms(QOpenGLShaderProgram* prog, GLCamera* camera)` | `setCommonUniforms` | private |
| `void extractFrustumPlanes()` | `extractFrustumPlanes` | private |
| `bool sceneHasVisibleTransmissionMaterials() const` | `sceneHasVisibleTransmissionMaterials` | private |
| `bool sceneHasVisibleSSSMaterials() const` | `sceneHasVisibleSSSMaterials` | private |
| `void drawSectionCapping()` | `drawSectionCapping` | private |
| `void drawFloor(const bool& drawReflection = true)` | `drawFloor` | private |
| `void drawGrid()` | `drawGrid` | private |
| `void drawSkyBox()` | `drawSkyBox` | private |
| `void drawVertexNormals()` | `drawVertexNormals` | private |
| `void drawFaceNormals()` | `drawFaceNormals` | private |
| `void drawBoundingBoxOverlay()` | `drawBoundingBoxOverlay` | private |
| `void drawDebugOverlay(GLCamera* camera)` | `drawDebugOverlay` | private |
| `void drawAxis(GLCamera* camera)` | `drawAxis` | private |
| `void drawCornerAxis(CornerAxisPosition position)` | `drawCornerAxis` | private |
| `void drawTransformGizmo(GLCamera* camera)` | `drawTransformGizmo` | private |
| `void drawViewCube()` | `drawViewCube` | private |
| `void drawViewCubeLabels(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix, float cubeScale)` | `drawViewCubeLabels` | private |
| `void drawLights()` | `drawLights` | private |
| `void bindIBLTextures()` | `bindIBLTextures` | private |
| `void render(GLCamera* camera)` | `render` | private |
| `void renderToShadowBuffer()` | `renderToShadowBuffer` | private |
| `void renderQuad()` | `renderQuad` | private |
| `void renderMeshWithDisplayMode(SceneMesh* mesh, DisplayMode mode)` | `renderMeshWithDisplayMode` | private |
| `void renderMeshExploded(SceneMesh* mesh, DisplayMode mode)` | `renderMeshExploded` | private |
| `void gradientBackground(float top_r, float top_g, float top_b, float top_a, float bot_r, float bot_g, float bot_b, float bot_a, int gradientStyle)` | `gradientBackground` | private |
| `void setupClippingUniforms(QOpenGLShaderProgram* prog, QVector3D pos)` | `setupClippingUniforms` | private |
| `void setSkyBoxTextureFolder(QString folder)` | `setSkyBoxTextureFolder` | public |
| `bool loadCubemapFromSingleHDR(const QString& filePath)` | `loadCubemapFromSingleHDR` | public |
| `bool convertEquirectangularToCubemap(const QString& filePath)` | `convertEquirectangularToCubemap` | public |
| `bool convertEquirectangularToCubemapQuad(const QString& filePath)` | `convertEquirectangularToCubemapQuad` | public |
| `void renderConversionCube()` | `renderConversionCube` | public |
| `void initTransmissionBuffer()` | `initTransmissionBuffer` | private |
| `void renderToTransmissionBuffer(GLCamera* camera, const QColor& topColor, const QColor& botColor)` | `renderToTransmissionBuffer` | private |
| `void cleanupTransmissionBuffer()` | `cleanupTransmissionBuffer` | private |
| `void resizeTransmissionBuffer(int width, int height)` | `resizeTransmissionBuffer` | private |
| `void initSSSBuffer()` | `initSSSBuffer` | private |
| `void renderToSSSBuffer(GLCamera* camera)` | `renderToSSSBuffer` | private |
| `void resizeSSSBuffer(int width, int height)` | `resizeSSSBuffer` | private |
| `void cleanupSSSBuffer()` | `cleanupSSSBuffer` | private |
| `void createWhiteTexture()` | `createWhiteTexture` | private |
| `void generateCubemapMipmaps(GLuint cubemapTexture)` | `generateCubemapMipmaps` | private |
| `unsigned int loadTextureFromFile(const char* path, GLenum wrapS, GLenum wrapT, GLenum minFilter, GLenum magFilter, bool flipY)` | `loadTextureFromFile` | private |
| `GLuint createGPUTextureFromImage(const QImage& image, const TextureSamplerSettings& samplers)` | `createGPUTextureFromImage` | private |
| `GLuint uploadDecodedTextureImage(const QImage& image, const TextureSamplerSettings& samplers)` | `uploadDecodedTextureImage` | private |
| `GLuint uploadKtx2TextureImage(const QString& path, const std::string& mapType, const TextureSamplerSettings& samplers)` | `uploadKtx2TextureImage` | private |
| `GLuint uploadDecodedTexture(GLMaterial::Texture& texture, const QImage& image)` | `uploadDecodedTexture` | private |
| `GLuint uploadKtx2Texture(const QString& path, const std::string& mapType, GLMaterial::Texture& texture)` | `uploadKtx2Texture` | private |

### Debug readback (GL glGetTexImage)
| Signature | Method Name | Access |
|---|---|---|
| `void requestTextureReadback(int meshId)` | `requestTextureReadback` | public slot |
| `void setDebugTextureEnabled(int meshId, int unitIndex, bool enabled)` | `setDebugTextureEnabled` | public slot |
| `void applyDebugTextureState(int meshId, const QSet<int>& enabledUnits, const QSet<int>& allUnits)` | `applyDebugTextureState` | public slot |
| `void setGlobalDebugChannel(int channelId)` | `setGlobalDebugChannel` | public slot |
| `void clearDebugTextureOverrides(int meshId)` | `clearDebugTextureOverrides` | public slot |
| `void clearAllDebugOverrides(int meshId)` | `clearAllDebugOverrides` | public slot |
| `void setDebugExtensionEnabled(int meshId, const QString& extensionKey, bool enabled)` | `setDebugExtensionEnabled` | public slot |
| `void clearDebugExtensionOverrides(int meshId)` | `clearDebugExtensionOverrides` | public slot |

### Qt event handlers
| Signature | Method Name | Access |
|---|---|---|
| `void resizeEvent(QResizeEvent* event)` | `resizeEvent` | protected |
| `void mousePressEvent(QMouseEvent*)` | `mousePressEvent` | protected |
| `void mouseReleaseEvent(QMouseEvent*)` | `mouseReleaseEvent` | protected |
| `void mouseMoveEvent(QMouseEvent*)` | `mouseMoveEvent` | protected |
| `void wheelEvent(QWheelEvent*)` | `wheelEvent` | protected |
| `void keyPressEvent(QKeyEvent* event)` | `keyPressEvent` | protected |
| `void keyReleaseEvent(QKeyEvent* event)` | `keyReleaseEvent` | protected |
| `void closeEvent(QCloseEvent* event)` | `closeEvent` | protected |

### Signals (always stay in declaring class)
| Signature | Method Name |
|---|---|
| `void windowZoomEnded()` | `windowZoomEnded` |
| `void rotationsSet()` | `rotationsSet` |
| `void zoomAndPanSet()` | `zoomAndPanSet` |
| `void viewSet()` | `viewSet` |
| `void displayListSet()` | `displayListSet` |
| `void singleSelectionDone(int)` | `singleSelectionDone` |
| `void sweepSelectionDone(QList<int>)` | `sweepSelectionDone` |
| `void floorShown(bool)` | `floorShown` |
| `void visibleSwapped(bool)` | `visibleSwapped` |
| `void loadingAssImpModelCancelled()` | `loadingAssImpModelCancelled` |
| `void displayModeChanged(int)` | `displayModeChanged` |
| `void renderingModeChanged(int)` | `renderingModeChanged` |
| `void animationStateChanged()` | `animationStateChanged` |
| `void explodedViewManualPlacementChanged()` | `explodedViewManualPlacementChanged` |
| `void backgroundColorChanged(const QColor& topColor, const QColor& bottomColor)` | `backgroundColorChanged` |
| `void selectionChanged(const QList<int>& selectedIds)` | `selectionChanged` |
| `void textureReadbackReady(QVector<TextureSlotInfo> slots, QString meshName)` | `textureReadbackReady` |
| `void cameraUpAxisChanged(bool zUp)` | `cameraUpAxisChanged` |

---

## Destination: `AnimationRuntimeController`

| Signature | Method Name | Access | Notes |
|---|---|---|---|
| `void setActiveAnimation(const QString& sourceFile, int clipIndex)` | `setActiveAnimation` | public | 100% delegates to `_animCtrl` |
| `void setAnimationPlaying(bool playing)` | `setAnimationPlaying` | public | |
| `void clearAnimationRuntimeForFile(const QString& sourceFile)` | `clearAnimationRuntimeForFile` | public | |
| `void seekAnimation(double timeSeconds)` | `seekAnimation` | public | |
| `void setAnimationLooping(bool looping)` | `setAnimationLooping` | public | |
| `void setAnimationPlaybackSpeed(double speed)` | `setAnimationPlaybackSpeed` | public | |
| `void syncRuntimeNodeTransforms(const QString& sourceFile)` | `syncRuntimeNodeTransforms` | public | |
| `void refreshAnimationMaterialState(const QString& sourceFile)` | `refreshAnimationMaterialState` | public | |
| `QString activeAnimationFile() const` | `activeAnimationFile` | public | pure accessor |
| `int activeAnimationClip() const` | `activeAnimationClip` | public | |
| `double currentAnimationTimeSeconds() const` | `currentAnimationTimeSeconds` | public | |
| `bool isAnimationPlaying() const` | `isAnimationPlaying` | public | |
| `bool isAnimationLooping() const` | `isAnimationLooping` | public | |
| `double animationPlaybackSpeed() const` | `animationPlaybackSpeed` | public | |
| `void activateGltfCamera(const QString& sourceFile, int cameraIndex)` | `activateGltfCamera` | public | |
| `void resetToSystemCamera()` | `resetToSystemCamera` | public | |
| `bool isGltfCameraActive() const` | `isGltfCameraActive` | public | |
| `QString activeGltfCameraFile() const` | `activeGltfCameraFile` | public | |
| `int activeGltfCameraIndex() const` | `activeGltfCameraIndex` | public | |
| `std::vector<GPULight> getParsedLights() const` | `getParsedLights` | public | |
| `std::vector<GPULight> getRepositionedLights() const` | `getRepositionedLights` | public | |
| `QVector<LightOrigin> getLightFileIndexMap() const` | `getLightFileIndexMap` | public | |
| `void setParsedLights(const GltfLightData& lights)` | `setParsedLights` | public | |
| `void setAnimatedLightVisibilityState(const QString& sourceFile, const QVector<bool>& visibleByParsedLight)` | `setAnimatedLightVisibilityState` | private | |
| `void setAnimatedLightTransformState(const QString& sourceFile, const std::vector<GPULight>& animatedLights)` | `setAnimatedLightTransformState` | private | |
| `void clearAnimatedLightTransformState(const QString& sourceFile)` | `clearAnimatedLightTransformState` | private | |
| `void clearAnimatedLightVisibilityState(const QString& sourceFile)` | `clearAnimatedLightVisibilityState` | private | |
| `void setAnimatedMeshVisibilityState(const QString& sourceFile, const QSet<QUuid>& hiddenMeshUuids)` | `setAnimatedMeshVisibilityState` | private | |
| `void clearAnimatedMeshVisibilityState(const QString& sourceFile)` | `clearAnimatedMeshVisibilityState` | private | |

**Type aliases to dissolve** (currently `using X = AnimationRuntimeController::X` in GLWidget):
- `using LightOrigin = AnimationRuntimeController::LightOrigin;`
- `using RuntimeNodeTransform = AnimationRuntimeController::RuntimeNodeTransform;`
- `using RuntimeAnimationFileState = AnimationRuntimeController::RuntimeAnimationFileState;`

---

## Destination: `SceneRenderController`

| Signature | Method Name | Access | Notes |
|---|---|---|---|
| `void setCornerAxisPosition(CornerAxisPosition position)` | `setCornerAxisPosition` | public | render state enum |
| `void setShadowQuality(AdaptiveShadowMapper::QualityLevel quality)` | `setShadowQuality` | public | |
| `void setClippingPlaneHatchMode(ClippingPlaneHatchMode mode)` | `setClippingPlaneHatchMode` | public | |
| `void setClippingPlaneHatchPattern(HatchPattern pattern)` | `setClippingPlaneHatchPattern` | public | |
| `void setHatchTiling(int tiling)` | `setHatchTiling` | public | |
| `void setHatchLineThickness(float width)` | `setHatchLineThickness` | public | |
| `void setHatchIntensity(float spacing)` | `setHatchIntensity` | public | |
| `void setHatchLayers(int layers)` | `setHatchLayers` | public | |
| `void setHatchLineColor(const QColor& color)` | `setHatchLineColor` | public | |
| `void setHatchTexture(const QString& path)` | `setHatchTexture` | public | |
| `void showShadows(bool show)` | `showShadows` | public | |
| `void showSelfShadows(bool show)` | `showSelfShadows` | public | |
| `void showEnvironment(bool show)` | `showEnvironment` | public | |
| `void showSkyBox(bool show)` | `showSkyBox` | public | |
| `void blurSkyBox(bool blur)` | `blurSkyBox` | public | |
| `void setSkyBoxBlurPercent(int percent)` | `setSkyBoxBlurPercent` | public | |
| `void showReflections(bool show)` | `showReflections` | public | |
| `void setGroundMode(GroundMode mode)` | `setGroundMode` | public | |
| `GroundMode groundMode() const` | `groundMode` | public | |
| `void showFloor(bool show)` | `showFloor` | public | |
| `bool isFloorShown()` | `isFloorShown` | public | |
| `bool isGridShown() const` | `isGridShown` | public | |
| `void showFloorTexture(bool show)` | `showFloorTexture` | public | |
| `void setFloorTexture(QImage img)` | `setFloorTexture` | public | state only; GL upload stays in GLWidget |
| `void setAnisotropicFilteringLevel(int level)` | `setAnisotropicFilteringLevel` | public | |
| `int getAnisotropicFilteringLevel() const` | `getAnisotropicFilteringLevel` | public | |
| `void setTransmissionEnabled(const bool& enabled)` | `setTransmissionEnabled` | public | |
| `bool isTransmissionEnabled() const` | `isTransmissionEnabled` | public | |
| `QVector4D getDefaultLightColor() const` | `getDefaultLightColor` | public | |
| `void setDefaultLightColor(const QVector4D& defaultLightColor)` | `setDefaultLightColor` | public | |
| `QVector3D getLightPosition() const` | `getLightPosition` | public | |
| `QVector3D getLightOffset() const` | `getLightOffset` | public | |
| `void setLightOffset(const QVector3D& offset)` | `setLightOffset` | public | |
| `float getFloorSize() const` | `getFloorSize` | public | |
| `bool isShaded() const` | `isShaded` | public | |
| `DisplayMode getDisplayMode() const` | `getDisplayMode` | public | |
| `void setDisplayMode(DisplayMode mode)` | `setDisplayMode` | public | |
| `bool isVertexNormalsShown() const` | `isVertexNormalsShown` | public | |
| `void setShowVertexNormals(bool showVertexNormals)` | `setShowVertexNormals` | public | |
| `bool isBoundingBoxShown() const` | `isBoundingBoxShown` | public | |
| `void setShowBoundingBox(bool showBoundingBox)` | `setShowBoundingBox` | public | |
| `DebugOverlayMode debugOverlayMode() const` | `debugOverlayMode` | public | |
| `void setDebugOverlayMode(DebugOverlayMode mode)` | `setDebugOverlayMode` | public | |
| `bool isDebugOverlayEnabled() const` | `isDebugOverlayEnabled` | public | |
| `void setDebugOverlayEnabled(bool enabled)` | `setDebugOverlayEnabled` | public | |
| `void setDebugOverlayAvailability(bool boundingBox, bool vertexNormals, bool faceNormals)` | `setDebugOverlayAvailability` | public | |
| `bool isFaceNormalsShown() const` | `isFaceNormalsShown` | public | |
| `void setShowFaceNormals(bool showFaceNormals)` | `setShowFaceNormals` | public | |
| `QColor getBgTopColor() const` | `getBgTopColor` | public | |
| `void setBgTopColor(const QColor& bgTopColor)` | `setBgTopColor` | public | |
| `QColor getBgBotColor() const` | `getBgBotColor` | public | |
| `void setBgBotColor(const QColor& bgBotColor)` | `setBgBotColor` | public | |
| `int getBgGradientStyle() const` | `getBgGradientStyle` | public | |
| `void setBgGradientStyle(int style)` | `setBgGradientStyle` | public | |
| `RenderingMode getRenderingMode() const` | `getRenderingMode` | public | |
| `void setRenderingMode(const RenderingMode& renderingMode)` | `setRenderingMode` | public | |
| `void setCappingPlanesEnabled(const bool& enabled)` | `setCappingPlanesEnabled` | public | |
| `bool cappingPlanesEnabled() const` | `cappingPlanesEnabled` | public | |
| `void setYZClippingEnabled(const bool& enabled)` | `setYZClippingEnabled` | public | |
| `bool yzClippingEnabled() const` | `yzClippingEnabled` | public | |
| `void setZXClippingEnabled(const bool& enabled)` | `setZXClippingEnabled` | public | |
| `bool zxClippingEnabled() const` | `zxClippingEnabled` | public | |
| `void setXYClippingEnabled(const bool& enabled)` | `setXYClippingEnabled` | public | |
| `bool xyClippingEnabled() const` | `xyClippingEnabled` | public | |
| `void setClippingXFlipped(const bool& flipped)` | `setClippingXFlipped` | public | |
| `bool clippingXFlipped() const` | `clippingXFlipped` | public | |
| `void setClippingYFlipped(const bool& flipped)` | `setClippingYFlipped` | public | |
| `bool clippingYFlipped() const` | `clippingYFlipped` | public | |
| `void setClippingZFlipped(const bool& flipped)` | `setClippingZFlipped` | public | |
| `bool clippingZFlipped() const` | `clippingZFlipped` | public | |
| `void setClippingXCoeff(const float& coeff)` | `setClippingXCoeff` | public | |
| `float clippingXCoeff() const` | `clippingXCoeff` | public | |
| `void setClippingYCoeff(const float& coeff)` | `setClippingYCoeff` | public | |
| `float clippingYCoeff() const` | `clippingYCoeff` | public | |
| `void setClippingZCoeff(const float& coeff)` | `setClippingZCoeff` | public | |
| `float clippingZCoeff() const` | `clippingZCoeff` | public | |
| `bool getHdrToneMapping() const` | `getHdrToneMapping` | public | |
| `bool getGammaCorrection() const` | `getGammaCorrection` | public | |
| `float getScreenGamma() const` | `getScreenGamma` | public | |
| `GLuint getEnvironmentMap(int index = 0, bool regenerate = false)` | `getEnvironmentMap` | public | |
| `GLuint getIrradianceMap(int index = 0, bool regenerate = false)` | `getIrradianceMap` | public | |
| `GLuint getPrefilterMap(int index = 0, bool regenerate = false)` | `getPrefilterMap` | public | |
| `GLuint getSheenPrefilterMap(int index = 0, bool regenerate = false)` | `getSheenPrefilterMap` | public | |
| `unsigned int getPrefilterMipLevels() const` | `getPrefilterMipLevels` | public | |
| `unsigned int getSheenPrefilterMipLevels() const` | `getSheenPrefilterMipLevels` | public | |
| `GLuint getBrdfLUT() const` | `getBrdfLUT` | public | |
| `GLuint getCharlieLUT() const` | `getCharlieLUT` | public | |
| `GLuint getSheenELUT() const` | `getSheenELUT` | public | |
| `bool isEnvironmentMapEnabled() const` | `isEnvironmentMapEnabled` | public | |
| `bool isIBLEnabled() const` | `isIBLEnabled` | public | |
| `float getIBLExposure() const` | `getIBLExposure` | public | |
| `float getEnvMapExposure() const` | `getEnvMapExposure` | public | |
| `QString getCurrentSkyboxFolder() const` | `getCurrentSkyboxFolder` | public | |
| `bool isSkyBoxShown() const` | `isSkyBoxShown` | public | |
| `bool isSkyBoxHDRIEnabled() const` | `isSkyBoxHDRIEnabled` | public | |
| `int getSkyBoxBlurPercent() const` | `getSkyBoxBlurPercent` | public | |
| `float getSkyBoxFOV() const` | `getSkyBoxFOV` | public | |
| `float getSkyBoxZRotationDegrees() const` | `getSkyBoxZRotationDegrees` | public | |
| `bool areReflectionsEnabled() const` | `areReflectionsEnabled` | public | |
| `bool isFloorTextureShown() const` | `isFloorTextureShown` | public | |
| `bool areShadowsEnabled() const` | `areShadowsEnabled` | public | |
| `bool areSelfShadowsEnabled() const` | `areSelfShadowsEnabled` | public | |
| `bool areDefaultLightsEnabled() const` | `areDefaultLightsEnabled` | public | |
| `bool arePunctualLightsEnabled() const` | `arePunctualLightsEnabled` | public | |
| `bool areLightsShown() const` | `areLightsShown` | public | |
| `ShaderProgram* getShader() const` | `getShader` | public | |
| `void setSectionCapsDynamicEnabled(bool enabled)` | `setSectionCapsDynamicEnabled` | public | |
| `void disableSectionCapsInteractionSuppression()` | `disableSectionCapsInteractionSuppression` | public slot | |
| `void setFloorTexRepeatS(double floorTexRepeatS)` | `setFloorTexRepeatS` | public slot | |
| `void setFloorTexRepeatT(double floorTexRepeatT)` | `setFloorTexRepeatT` | public slot | |
| `void setFloorOffsetPercent(double value)` | `setFloorOffsetPercent` | public slot | |
| `void setSkyBoxFOV(double fov)` | `setSkyBoxFOV` | public slot | |
| `void setSkyBoxZRotation(int index)` | `setSkyBoxZRotation` | public slot | |
| `void setSkyBoxTextureHDRI(bool hdrSet)` | `setSkyBoxTextureHDRI` | public slot | |
| `void enableHDRToneMapping(bool hdrToneMapping)` | `enableHDRToneMapping` | public slot | |
| `void enableGammaCorrection(bool gammaCorrection)` | `enableGammaCorrection` | public slot | |
| `void setScreenGamma(double screenGamma)` | `setScreenGamma` | public slot | |
| `void setHDRToneMappingMode(HDRToneMapMode mode)` | `setHDRToneMappingMode` | public slot | |
| `void setEnvMapExposure(double exposure)` | `setEnvMapExposure` | public slot | |
| `void setIBLExposure(double exposure)` | `setIBLExposure` | public slot | |
| `bool isHDRToneMappingEnabled() const` | `isHDRToneMappingEnabled` | public slot | |
| `bool isGammaCorrectionEnabled() const` | `isGammaCorrectionEnabled` | public slot | |
| `HDRToneMapMode getHDRToneMappingMode() const` | `getHDRToneMappingMode` | public slot | |
| `void useDefaultLights(bool useDefaultLights)` | `useDefaultLights` | public slot | |
| `void usePunctualLights(bool usePunctualLights)` | `usePunctualLights` | public slot | |
| `void useIBL(bool useIBL)` | `useIBL` | public slot | |
| `void updateEnvMapRotationMatrix()` | `updateEnvMapRotationMatrix` | private | env map rotation state |
| `void setSectionCapsInteractionSuppressed(bool suppressed)` | `setSectionCapsInteractionSuppressed` | private | |

---

## Destination: `ViewportInteractionController`

| Signature | Method Name | Access | Notes |
|---|---|---|---|
| `void setRotationActive(bool active)` | `setRotationActive` | public | toggles `_viewCtrl` flags only |
| `void setPanningActive(bool active)` | `setPanningActive` | public | |
| `void setZoomingActive(bool active)` | `setZoomingActive` | public | |
| `void beginWindowZoom()` | `beginWindowZoom` | public | interactive zoom gesture state |
| `void performWindowZoom()` | `performWindowZoom` | public | |
| `bool isCameraUpAxisZUp() const` | `isCameraUpAxisZUp` | public | pure `_viewCtrl` accessor |
| `GLCamera::CameraMode cameraMode() const` | `cameraMode` | public | |
| `float getPerspFOV() const` | `getPerspFOV` | public | |
| `QMatrix4x4 getViewMatrix() const` | `getViewMatrix` | public slot | |
| `QMatrix4x4 getProjectionMatrix() const` | `getProjectionMatrix` | public slot | |
| `QMatrix4x4 getModelViewMatrix() const` | `getModelViewMatrix` | public slot | |
| `bool isMultiViewActive() const` | `isMultiViewActive` | public slot | |
| `void setPerspFOV(int fovDegrees)` | `setPerspFOV` | public slot | |
| `void setRotations(float xRot, float yRot, float zRot)` | `setRotations` | private | |
| `void setZoomAndPan(float zoom, QVector3D pan)` | `setZoomAndPan` | private | |
| `void fitBoxToScreen(const BoundingBox& box)` | `fitBoxToScreen` | private | |

---

## Destination: `SceneRuntime`

| Signature | Method Name | Access | Notes |
|---|---|---|---|
| `void setDisplayList(const std::vector<int>& ids)` | `setDisplayList` | public | |
| `int getModelNum() const` | `getModelNum` | public | |
| `std::vector<SceneMesh*> getMeshStore() const` | `getMeshStore` | public | |
| `void addToDisplay(SceneMesh*)` | `addToDisplay` | public | |
| `void removeFromDisplay(int index)` | `removeFromDisplay` | public | |
| `aiScene* getAssImpScene() const` | `getAssImpScene` | public | |
| `glm::mat4 getGlobalSceneTransform() const` | `getGlobalSceneTransform` | public | |
| `void invertADSOpacityTexMap(const std::vector<int>& ids, const bool& inverted)` | `invertADSOpacityTexMap` | public | |
| `void setMaterialToObjects(const std::vector<int>& ids, const GLMaterial& mat)` | `setMaterialToObjects` | public | Phase 2 Item 2 |
| `void setTexturesToObjects(const std::vector<int>& ids, const GLMaterial& mat)` | `setTexturesToObjects` | public | Phase 2 Item 2 |
| `void clearTextureCache()` | `clearTextureCache` | public | drain GPU IDs â†’ GLWidget for `glDeleteTextures` |
| `bool userModelTransformForFile(const QString& sourceFile, QMatrix4x4& outTransform) const` | `userModelTransformForFile` | public | |
| `std::vector<int> getDisplayedObjectsIds() const` | `getDisplayedObjectsIds` | public | |
| `bool isVisibleSwapped() const` | `isVisibleSwapped` | public | |
| `BoundingSphere getBoundingSphere() const` | `getBoundingSphere` | public | |
| `void moveToRecycleBin(const QUuid& uuid, int originalIndex)` | `moveToRecycleBin` | public | |
| `bool restoreFromRecycleBin(const QUuid& uuid)` | `restoreFromRecycleBin` | public | |
| `void permanentlyDeleteFromBin(const QUuid& uuid)` | `permanentlyDeleteFromBin` | public | |
| `bool isInRecycleBin(const QUuid& uuid) const` | `isInRecycleBin` | public | |
| `QVector<QUuid> getRecycleBinUuids() const` | `getRecycleBinUuids` | public | |
| `SceneMesh* getMeshByUuid(const QUuid& uuid) const` | `getMeshByUuid` | public | |
| `SceneMesh* getMeshByIndex(int index) const` | `getMeshByIndex` | public | |
| `int getIndexByUuid(const QUuid& uuid) const` | `getIndexByUuid` | public | |
| `QUuid getUuidByIndex(int index) const` | `getUuidByIndex` | public | |
| `QString generateUniqueMeshName(const QString& baseName)` | `generateUniqueMeshName` | public | |
| `void clearMeshStore()` | `clearMeshStore` | public | |
| `void swapVisible(bool checked)` | `swapVisible` | public slot | |
| `void invalidateRuntimeVisibilityHierarchy()` | `invalidateRuntimeVisibilityHierarchy` | private | BVH management |
| `void rebuildRuntimeVisibilityHierarchy()` | `rebuildRuntimeVisibilityHierarchy` | private | |
| `bool ensureRuntimeVisibilityHierarchy()` | `ensureRuntimeVisibilityHierarchy` | private | |
| `void refreshRuntimeVisibilityCacheForCurrentView()` | `refreshRuntimeVisibilityCacheForCurrentView` | private | |
| `int buildRuntimeVisibilityNodeRecursive(const SceneNode* node, const QHash<QUuid, int>& meshIndexByUuid)` | `buildRuntimeVisibilityNodeRecursive` | private | |
| `bool refreshRuntimeVisibilityNodeBounds(int nodeIndex, const std::vector<unsigned char>& baseVisibleMask, bool refreshBounds)` | `refreshRuntimeVisibilityNodeBounds` | private | |
| `void collectVisibleMeshIdsForPass(int nodeIndex, int activeClipPlaneIndex, bool wantTransparent, std::vector<int>& out) const` | `collectVisibleMeshIdsForPass` | private | |
| `SceneMesh* createMeshFromData(const AssImpMeshData& meshData)` | `createMeshFromData` | private | pure construction; no GL affinity |
| `float highestModelZ()` | `highestModelZ` | private | bounding box query |
| `float lowestModelZ()` | `lowestModelZ` | private | bounding box query |

**TRS property accessors** â€” pending confirmation these only touch the currently selected mesh transform (not camera or shader state):

| Signature | Method Name | Access |
|---|---|---|
| `float getXTran() const` | `getXTran` | public |
| `void setXTran(const float& xTran)` | `setXTran` | public |
| `float getYTran() const` | `getYTran` | public |
| `void setYTran(const float& yTran)` | `setYTran` | public |
| `float getZTran() const` | `getZTran` | public |
| `void setZTran(const float& zTran)` | `setZTran` | public |
| `float getXRot() const` | `getXRot` | public |
| `void setXRot(const float& xRot)` | `setXRot` | public |
| `float getYRot() const` | `getYRot` | public |
| `void setYRot(const float& yRot)` | `setYRot` | public |
| `float getZRot() const` | `getZRot` | public |
| `void setZRot(const float& zRot)` | `setZRot` | public |
| `float getXScale() const` | `getXScale` | public |
| `void setXScale(const float& xScale)` | `setXScale` | public |
| `float getYScale() const` | `getYScale` | public |
| `void setYScale(const float& yScale)` | `setYScale` | public |
| `float getZScale() const` | `getZScale` | public |
| `void setZScale(const float& zScale)` | `setZScale` | public |

---

## Destination: `SelectionManager`

| Signature | Method Name | Access | Notes |
|---|---|---|---|
| `void select(int id)` | `select` | public | |
| `void deselect(int id)` | `deselect` | public | |
| `void syncMeshSelectionVisualState()` | `syncMeshSelectionVisualState` | public | |
| `int processSelection(const QPoint& pixel)` | `processSelection` | public | color-pick ID resolution |
| `void setSelectionHighlighting(bool highlight)` | `setSelectionHighlighting` | public slot | |
| `void broadcastSelectionChanged(const QList<int>& ids)` | `broadcastSelectionChanged` | public slot | just re-emits signal |
| `ShaderProgram* getSelectionShader() const` | `getSelectionShader` | public slot | dependency injection accessor |
| `std::vector<int> activeTransformGizmoSelectionIds() const` | `activeTransformGizmoSelectionIds` | private | query of selection state |

---

## Destination: `ExplodedViewRuntimeController`

| Signature | Method Name | Access |
|---|---|---|
| `bool beginExplodedViewManualPlacement(const QVector<QUuid>& selectionUuids = {})` | `beginExplodedViewManualPlacement` | public |
| `void finishExplodedViewManualPlacement()` | `finishExplodedViewManualPlacement` | public |
| `void clearExplodedViewManualPlacement()` | `clearExplodedViewManualPlacement` | public |
| `bool isExplodedViewManualPlacementActive() const` | `isExplodedViewManualPlacementActive` | public |
| `bool hasExplodedViewManualPlacement() const` | `hasExplodedViewManualPlacement` | public |
| `bool hasExplodedViewManualTransformChanges() const` | `hasExplodedViewManualTransformChanges` | public |
| `QSet<QUuid> explodedViewManualPlacementUuids() const` | `explodedViewManualPlacementUuids` | public |
| `QVector3D explodedViewManualPlacementTranslationDelta() const` | `explodedViewManualPlacementTranslationDelta` | public |
| `QVector3D explodedViewManualPlacementRotationDelta() const` | `explodedViewManualPlacementRotationDelta` | public |
| `void setExplodedViewManualPlacementTranslationDelta(const QVector3D& delta)` | `setExplodedViewManualPlacementTranslationDelta` | public |
| `void setExplodedViewManualPlacementRotationDelta(const QVector3D& delta)` | `setExplodedViewManualPlacementRotationDelta` | public |
| `QMap<QUuid, TransformState> explodedViewManualStates() const` | `explodedViewManualStates` | public |
| `void restoreExplodedViewManualStates(const QMap<QUuid, TransformState>& states)` | `restoreExplodedViewManualStates` | public |

---

## Destination: `VisibilityComputationHelper` (new)

Stateless free functions or a stateless helper class. All are pure math â€” no GL state changes.

| Signature | Method Name | Access |
|---|---|---|
| `bool isBoundingBoxOutsideFrustum(const BoundingBox& bb) const` | `isBoundingBoxOutsideFrustum` | private |
| `bool isMeshOutsideFrustum(const SceneMesh* mesh) const` | `isMeshOutsideFrustum` | private |
| `bool isMeshFullyInsideFrustum(const SceneMesh* mesh) const` | `isMeshFullyInsideFrustum` | private |
| `float computeFullyVisibleMinMeshRadius() const` | `computeFullyVisibleMinMeshRadius` | private |
| `bool isBoundingBoxFullyClipped_X(const BoundingBox& bb) const` | `isBoundingBoxFullyClipped_X` | private |
| `bool isBoundingBoxFullyClipped_Y(const BoundingBox& bb) const` | `isBoundingBoxFullyClipped_Y` | private |
| `bool isBoundingBoxFullyClipped_Z(const BoundingBox& bb) const` | `isBoundingBoxFullyClipped_Z` | private |
| `bool isBoundingBoxFullyKept_X(const BoundingBox& bb) const` | `isBoundingBoxFullyKept_X` | private |
| `bool isBoundingBoxFullyKept_Y(const BoundingBox& bb) const` | `isBoundingBoxFullyKept_Y` | private |
| `bool isBoundingBoxFullyKept_Z(const BoundingBox& bb) const` | `isBoundingBoxFullyKept_Z` | private |
| `bool isBoundingBoxStraddlesCapPlane(const BoundingBox& bb, int planeIndex) const` | `isBoundingBoxStraddlesCapPlane` | private |
| `bool isBoundingBoxInvisibleInAllClipPasses(const BoundingBox& bb) const` | `isBoundingBoxInvisibleInAllClipPasses` | private |
| `bool isMeshFullyClipped_X(const SceneMesh* mesh) const` | `isMeshFullyClipped_X` | private |
| `bool isMeshFullyClipped_Y(const SceneMesh* mesh) const` | `isMeshFullyClipped_Y` | private |
| `bool isMeshFullyClipped_Z(const SceneMesh* mesh) const` | `isMeshFullyClipped_Z` | private |
| `bool isMeshFullyKept_X(const SceneMesh* mesh) const` | `isMeshFullyKept_X` | private |
| `bool isMeshFullyKept_Y(const SceneMesh* mesh) const` | `isMeshFullyKept_Y` | private |
| `bool isMeshFullyKept_Z(const SceneMesh* mesh) const` | `isMeshFullyKept_Z` | private |
| `bool isMeshStraddlesCapPlane(const SceneMesh* mesh, int planeIndex) const` | `isMeshStraddlesCapPlane` | private |
| `bool isMeshInvisibleInAllClipPasses(const SceneMesh* mesh) const` | `isMeshInvisibleInAllClipPasses` | private |
| `bool isMeshAnimationVisible(const SceneMesh* mesh) const` | `isMeshAnimationVisible` | private |
| `bool isMeshVisible(const SceneMesh* mesh, int activeClipPlaneIndex) const` | `isMeshVisible` | private |
| `std::vector<QVector3D> collectVisibleCorners() const` | `collectVisibleCorners` | private |
| `float computeFitViewRange(const std::vector<QVector3D>& corners, const QVector3D& right, const QVector3D& up, const QVector3D& viewDir, QVector3D* outCenter = nullptr) const` | `computeFitViewRange` | private |
| `float computeFitViewRange(const QVector3D& right, const QVector3D& up, const QVector3D& viewDir, QVector3D* outCenter = nullptr) const` | `computeFitViewRange` | private |
| `float computeFitViewRange(QVector3D* outCenter = nullptr) const` | `computeFitViewRange` | private |
| `float computeOrthographicFitViewRangeForViewport(const std::vector<QVector3D>& corners, const QVector3D& right, const QVector3D& up, const QVector3D& viewDir, int viewportWidth, int viewportHeight, QVector3D* outCenter = nullptr, const QVector3D& eyePos = QVector3D(0,0,0)) const` | `computeOrthographicFitViewRangeForViewport` | private |
| `QVector3D computeVisibleWorldCenter(const std::vector<QVector3D>& corners) const` | `computeVisibleWorldCenter` | private |
| `float computeSharedOrthographicMultiViewRange(const std::vector<QVector3D>& corners, int viewportWidth, int viewportHeight, const QVector3D& eyePos = QVector3D(0,0,0)) const` | `computeSharedOrthographicMultiViewRange` | private |
| `QRect viewCubeRect() const` | `viewCubeRect` | private |
| `QRect viewCubeScreenRect() const` | `viewCubeScreenRect` | private |

---

## Destination: `CoordinateSystemHelper` (new)

Stateless free functions. Pure coordinate convention math; no GL state.

| Signature | Method Name | Access |
|---|---|---|
| `Plane::Orientation floorPlaneOrientation() const` | `floorPlaneOrientation` | private |
| `QVector3D currentWorldUpVector() const` | `currentWorldUpVector` | private |
| `float coordinateAlongCurrentWorldUp(const QVector3D& point) const` | `coordinateAlongCurrentWorldUp` | private |
| `void setCoordinateAlongCurrentWorldUp(QVector3D& point, float value) const` | `setCoordinateAlongCurrentWorldUp` | private |
| `QQuaternion cameraUpAxisConventionRotation() const` | `cameraUpAxisConventionRotation` | private |
| `QVector3D transformVectorForCameraUpAxis(const QVector3D& vector) const` | `transformVectorForCameraUpAxis` | private |
| `void standardViewBasis(ViewMode mode, QVector3D& viewDir, QVector3D& upDir, QVector3D& rightDir) const` | `standardViewBasis` | private |
| `QQuaternion standardViewRotation(ViewMode mode) const` | `standardViewRotation` | private |
| `bool sceneUpAxisIsZUp(SceneUpAxis sceneUpAxis) const` | `sceneUpAxisIsZUp` | private |
| `QString sceneUpAxisLabel(SceneUpAxis sceneUpAxis) const` | `sceneUpAxisLabel` | private |
| `void warnOnConflictingImportedSceneUpAxis(const QString& fileName, SceneUpAxis sceneUpAxis)` | `warnOnConflictingImportedSceneUpAxis` | private |
| `float groundPlaneZ()` | `groundPlaneZ` | private |
| `float groundPlaneScaleFactor() const` | `groundPlaneScaleFactor` | private |
| `float groundPlaneExtent() const` | `groundPlaneExtent` | private |

---

## Destination: `MvfMeshPreparationWorker` (new)

CPU-only, fully thread-safe, no GL affinity.

| Signature | Method Name | Access | Notes |
|---|---|---|---|
| `static QVector<PreparedMvfMesh> prepareMvfMeshes(const Mvf::Document& document, const QByteArray& geometryChunk, const QByteArray& imageChunk)` | `prepareMvfMeshes` | public | static; thread-safe |
| `struct PreparedMvfMesh` | `PreparedMvfMesh` | (inner type) | moves with the worker |

---

## Destination: `TextureCacheManager` (new)

Cache lookup and reference counting. GL upload calls stay in GLWidget.

| Signature | Method Name | Access |
|---|---|---|
| `unsigned int getOrCreateTextureCached(const QString& cacheKey, const QImage& image, const TextureSamplerSettings& samplers)` | `getOrCreateTextureCached` | public |
| `unsigned int getOrLoadKtx2TextureCached(const QString& path, const std::string& mapType, const TextureSamplerSettings& samplers)` | `getOrLoadKtx2TextureCached` | public |
| `unsigned int getOrLoadTextureCached(const QString& path, const TextureSamplerSettings& samplers)` | `getOrLoadTextureCached` | public |
| `void synchronizeTextureCache(const GLMaterial* material, GLMaterial::TextureType type)` | `synchronizeTextureCache` | public |
| `void retainTexture(unsigned int texId)` | `retainTexture` | private |
| `void releaseTexture(unsigned int texId)` | `releaseTexture` | private |

---

## Destination: `PickingHelper` (new)

Pure math; no GL state.

| Signature | Method Name | Access |
|---|---|---|
| `unsigned int colorToIndex(const QColor& color)` | `colorToIndex` | private |
| `QColor indexToColor(const unsigned int& index)` | `indexToColor` | private |
| `QVector3D get3dTranslationVectorFromMousePoints(const QPoint& start, const QPoint& end)` | `get3dTranslationVectorFromMousePoints` | private |

---

## Summary count

| Destination | Method count |
|---|---|
| STAY in GLWidget | ~115 |
| `AnimationRuntimeController` | 29 |
| `SceneRenderController` | 73 |
| `ViewportInteractionController` | 16 |
| `SceneRuntime` | 35 + 18 TRS (pending) |
| `SelectionManager` | 8 |
| `ExplodedViewRuntimeController` | 13 |
| `VisibilityComputationHelper` (new) | 31 |
| `CoordinateSystemHelper` (new) | 14 |
| `MvfMeshPreparationWorker` (new) | 1 + struct |
| `TextureCacheManager` (new) | 6 |
| `PickingHelper` (new) | 3 |
| **Total declared methods** | **~344** |

