# Threaded Model Loading Migration

## Purpose

This document records the staged migration from a mostly UI-thread-bound model loading pipeline to a worker-thread-based implementation with explicit CPU/GPU responsibility separation.

The goal of the effort was not only to make loading faster, but to make the architecture safer:

- CPU-heavy work should run off the UI thread.
- All OpenGL work should remain on the UI thread.
- Progressive loading should continue to work.
- Cancellation should be graceful.
- Complex texture paths such as embedded GLB images and KTX2 should remain correct.


## Starting Point

The original loading pipeline mixed several responsibilities in one path:

- file read and parsing
- scene traversal
- CPU mesh conversion
- material interpretation
- OpenGL texture upload
- OpenGL mesh creation
- UI progress updates

That design worked functionally, but it created several problems:

- loading blocked the UI thread
- progress reporting was noisy and sometimes misleading
- cancellation was only partially effective
- OpenGL context ownership was not clearly separated from parsing responsibilities
- moving the loader to a thread directly would have been unsafe


## High-Level Migration Strategy

The migration was done incrementally rather than by moving everything to a worker at once.

The sequence was:

1. Stabilize and improve loading UX.
2. Separate mesh parsing from OpenGL mesh creation.
3. Separate material/texture decision logic from OpenGL texture upload.
4. Introduce a worker-thread load path once the CPU/GPU boundary was clear.
5. Restore progressive loading safely.
6. Fix texture correctness, memory behavior, and CAD-thread issues.
7. Clean up temporary diagnostics after the pipeline was stable.


## Stage 1: Progress and Cancellation Improvements

Before threading, the loading UX was cleaned up so the later changes had a better foundation.

### Progress reporting

The loader originally exposed only partial progress information. We changed it to report:

- global node progress
- global mesh progress

This replaced local child-loop progress that could not accurately represent whole-scene loading.

### Cancellation flow

Cancellation was changed from a failure-like experience to a normal user action:

- cancel button changes to a cancelling state
- cancellation no longer shows a critical error dialog
- partial progressive results are preserved when applicable

### Assimp cancellation behavior

Cancellation checkpoints were added after `ReadFile()` and before expensive post-read stages such as:

- scene statistics gathering
- transforms
- UV generation prompts
- recursive traversal

This ensured that formats like OBJ, which may not interrupt mid-read, still stop cleanly before later work continues.

### OpenCASCADE cancellation behavior

The OpenCASCADE/XCAF path was also updated to honor cancellation through progress indicator hooks, so STEP and IGES imports could stop without being treated as ordinary failures.


## Stage 2: Mesh Responsibility Separation

The first architectural seam was introduced on the mesh side.

### Before

`AssImpModelLoader` created GL-backed mesh objects directly.

That was unsafe for threading because mesh construction ultimately leads to OpenGL buffer setup.

### After

The loader now produces CPU-only mesh payloads:

- vertices
- indices
- material data
- texture descriptors
- primitive mode
- negative-scale information

These are carried in `AssImpMeshData`.

`GLWidget` became the owner of converting those CPU payloads into renderable `AssImpMesh` instances on the UI thread.

### Outcome

This established the first clean boundary:

- loader: CPU mesh preparation
- `GLWidget`: GPU mesh creation


## Stage 3: Material and Texture Responsibility Separation

The second seam was introduced on the material/texture side.

### Important design discovery

The material path was more tightly coupled than it first appeared.

`GLMaterial::Texture` was not just metadata. It also carried:

- texture id
- type binding name
- UV transform metadata
- sampler settings
- alpha information
- embedded image payloads

The `_loadedTextures` cache also carried more than paths. It was effectively a resolved texture record cache used by:

- regular texture reuse
- glTF extension maps
- ADS alias synthesis
- GLB embedded image reconstruction

### Safe separation approach

Instead of trying to make `MaterialProcessor` purely metadata-only in one step, the safer split was:

- keep texture record and cache semantics intact
- move only actual GPU upload responsibility out of `MaterialProcessor`

### Result

`MaterialProcessor` now handles:

- material interpretation
- texture slot mapping
- image decoding
- texture descriptor construction
- cache reuse decisions

`GLWidget` now owns:

- decoded image upload
- KTX2 upload
- OpenGL texture caching

This was the essential foundation required before threading.


## Stage 4: Worker-Thread Loading

With the mesh and texture seams in place, the loader could be moved off the UI thread more safely.

### Thread ownership model

The final intended split is:

- worker thread
  - file read
  - Assimp/OpenCASCADE parsing
  - scene traversal
  - CPU mesh conversion
  - CPU material/texture preparation
- UI thread
  - OpenGL texture upload
  - `AssImpMesh` creation
  - scene insertion
  - widgets and status/progress UI

### Implementation shape

`GLWidget::loadAssImpModel()` now creates a temporary `AssImpModelLoader` worker on a `QThread`.

The worker:

- receives the file path and UV method
- runs `loadModel(...)` off the UI thread
- emits progress signals and mesh batches

The UI thread:

- receives progress through queued signals
- creates meshes and textures safely inside `GLWidget`
- waits for worker completion through a nested event loop

### Lifecycle reliability

`AssImpModelLoader::loadingFinished` was made reliable across:

- success
- failure
- cancellation

This was important to avoid hanging worker cleanup paths.


## Stage 5: Crash and Reentrancy Fixes

Several stability issues were discovered during the migration.

### Reentrant progress UI crash

`MainWindow::showStatusMessage()` and `MainWindow::setProgressValue()` previously called `qApp->processEvents()`.

With threaded queued progress updates, this caused reentrant event pumping and eventually a stack overflow in text/layout code.

The fix was to remove those `processEvents()` calls entirely.

### Unsafe cross-thread mutation

Earlier worker-thread texture upload callbacks were mutating worker-owned texture structs across thread boundaries.

This was replaced with a safer design where the UI thread only receives plain values and returns texture ids, or where upload is deferred fully to UI-thread finalization.

### STEP/IGES thread-affinity crash

The XCAF CAD readers still called static `MainWindow` UI helpers directly from the worker thread.

This triggered Qt thread-affinity assertions such as:

- cannot send events to objects owned by a different thread

The fix was to make the `MainWindow` static progress/status helper methods marshal themselves to the UI thread using `QMetaObject::invokeMethod(...)` when needed.


## Stage 6: Progressive Loading Restoration

Progressive loading was intentionally disabled temporarily while stabilizing the worker path.

### Why it was disabled

The initial worker-thread progressive handoff risked:

- race conditions
- excessive queued mesh payload buildup
- crashes on large imports

### Safe restoration

Progressive loading was reintroduced with these constraints:

- the worker again emits `meshBatchReady(...)`
- the UI receives batches with `Qt::BlockingQueuedConnection`
- the worker waits for batch processing before continuing
- in progressive mode, the final "add all meshes again" path is skipped to avoid duplication

### Outcome

This restored progressive scene growth without reintroducing the earlier crash behavior.


## Stage 7: Texture and Material Fixes Found During Threading

Most of the difficult issues discovered during the migration were not generic threading bugs, but boundary bugs in texture ownership and reconstruction.

### GLB embedded images losing `imageData`

Problem:

- cached GLB textures were reused as aliases for specific material slots
- those alias copies preserved path/type/id but dropped decoded `imageData`
- UI-thread finalization then had no embedded image payload to upload

Fix:

- all alias creation paths were updated to preserve `imageData`

### Duplicate uploads and memory blow-up

Problem:

- embedded images and KTX2 textures were being uploaded repeatedly during UI-side mesh creation
- shared textures across many meshes could explode memory usage

Fix:

- cache-backed UI finalization was added for:
  - decoded embedded images
  - KTX2 textures

This made shared-texture reuse behave more like normal file-texture reuse.

### KTX2 regression in progressive safety pass

Problem:

- the progressive-only material re-resolution path still used plain image loading for every path
- `.ktx2` textures bypassed the KTX2 loader in that pass

Fix:

- `resolveMaterialTextures(...)` now dispatches by path
  - `.ktx2` -> KTX2 cache/loader path
  - others -> regular texture cache

### Advanced glTF extension maps dropped by progressive rebuild

Problem:

- advanced maps such as `anisotropyMap` were correctly resolved in `createMeshFromData()`
- but the progressive-only `setTextureMaps(...)` rebuild path dropped them afterward

Two separate fixes were needed:

1. `GLWidget::createMeshFromData()` had to copy advanced texture slots into `resolvedMaterial`.
2. `AssImpMesh::setTextureMaps()` had to rebuild advanced map entries into `_textures`.

Affected advanced slots included:

- anisotropy
- iridescence
- iridescence thickness
- specular factor
- specular color
- thickness
- diffuse
- diffuse transmission
- diffuse transmission color
- specular glossiness


## Final Architecture

After the migration, the effective responsibility split is:

### `AssImpModelLoader`

- file loading
- format routing
- Assimp/XCAF parsing
- scene traversal
- CPU mesh conversion
- CPU material processing
- progress and cancellation state
- progressive batch emission

### `MaterialProcessor`

- material interpretation
- glTF extension handling
- texture descriptor construction
- texture cache reuse logic
- image decoding
- GLB scene texture synchronization

### `GLWidget`

- UI-thread OpenGL texture upload
- UI-thread mesh creation
- UI-thread scene insertion
- UI-thread progressive batch handling
- final material/texture resolution
- KTX2 upload path

### `MainWindow`

- status/progress UI
- thread-safe static helper routing
- cancel button / cancel request state


## Why Loading Feels Faster Now

The observed speedup is real and expected.

Even when total wall-clock time is not dramatically reduced, the app feels faster because:

- the UI thread is no longer blocked by parsing and traversal
- progress and paint events are no longer starved
- texture and mesh finalization happen in a more controlled way
- release builds benefit more from optimized CPU-side work once it is cleanly separated


## Known Design Choices

Some behaviors are intentional tradeoffs rather than accidents.

### Progressive batch delivery uses blocking queued connections

This was chosen deliberately so the worker cannot outrun the UI and queue excessive mesh batches.

### CAD reader UI calls are still written in static `MainWindow` style

Rather than rewriting all CAD reader call sites immediately, `MainWindow` helpers were made thread-safe. This keeps the existing code style usable while preserving correctness.

### Progressive loading still uses a final material safety pass

The progressive path performs an extra material resolution pass to ensure textures that depend on final UI-thread resolution remain correct. This favors robustness over minimalism.


## Suggested Future Optimization Work

These are not required for correctness, but they are reasonable follow-up areas:

- reduce remaining duplicate CPU texture payload retention for very large shared-texture scenes
- tune progressive batch size heuristics dynamically
- reduce repeated material safety-pass work if profiling shows measurable cost
- centralize texture path resolution helpers further to reduce duplicated routing logic
- document worker-thread assumptions in more code comments near the actual seams


## Validation Summary

The migration was validated across:

- progressive and non-progressive loading
- standard external textures
- embedded GLB textures
- KTX2 textures
- glTF extension textures including anisotropy
- large models with many meshes and shared textures
- cancellation behavior
- STEP / IGES / BREP CAD paths

The final stabilized result is a worker-thread-based loading implementation that preserves GPU safety and format correctness while materially improving perceived loading responsiveness.
