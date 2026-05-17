# glTF Animation Feature Plan

## Scope

This branch adds a new `Animations` navigation tab for glTF assets that expose animation clips, along with:

- clip selection
- play/pause
- loop toggle
- timeline scrubbing
- runtime node-transform playback
- skeletal skinning on the main render path

Morph target animation is still deferred. The runtime structures were chosen so it can be added later without undoing this work.

## Architectural changes

- `AssImpModelLoader` now preserves local node-space data for animation-bearing glTF scenes instead of baking all node transforms into vertex positions.
- `SceneGraph` stores per-file `GltfAnimationData` and active clip selection alongside the existing material variant metadata.
- `GLWidget` owns runtime playback state, samples clips over time, updates node world transforms, and computes joint palettes for skinned meshes.
- `TriangleMesh` now supports:
  - scene-driven render transforms
  - skin-joint metadata
  - joint matrix palettes
  - dedicated joint index / weight vertex buffers
- `SceneUtils` now preserves bones, animation meshes, and animation channels during deep-copy and scene merge operations.

## UI changes

- Added `AnimationsPanel`, modeled after `MaterialVariantsPanel`.
- The navigation area now supports multiple optional glTF tabs instead of assuming only `Model` and `Variants`.
- The `Animations` tab appears only when at least one loaded file has actual animation clips.

## Runtime rules

- Animation state is file-local.
- One active clip is tracked per selected file.
- Playback starts paused at `t=0`.
- User-authored mesh/object transforms remain a post-animation transform.
- Non-skinned meshes use animated scene transforms directly.
- Skinned meshes use an identity scene transform plus a per-mesh joint palette in the shader path.

## Remaining follow-up

- Validate against a representative set of animated glTF samples.
- Add broader regression coverage for selection, clipping, and mixed animated/non-animated imports.
- Consider morph target playback in a follow-up branch.
