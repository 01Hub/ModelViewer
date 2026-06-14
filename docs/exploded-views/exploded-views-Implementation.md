# ModelViewer-Qt Exploded View

## Status

Implemented and baseline-committed as of 2026-06-14.

The feature now supports:

- automatic explode placement with improved axis and radial behavior
- hybrid authoring with manual gizmo-based refinement
- capture of multiple exploded steps into a named preset
- draft preview without creating a glTF animation
- glTF animation creation in parallel, sequential, or separate-step modes
- loop-back generation for explode-and-collapse playback
- MVF persistence of exploded-view presets

---

## User Workflow

### 1. Author a preset

The panel always works against one active preset.

- `Exploded View 1` is created automatically
- users can `New`, `Duplicate`, `Rename`, and `Delete` presets
- each preset owns:
  - assembly selection
  - optional anchor mesh
  - explode mode
  - vector and distance settings
  - captured steps
  - animation output settings

### 2. Choose authoring method

Authoring mode is transient UI state and is not persisted.

- `Auto` uses the computed exploded offsets from `ExplodedViewManager`
- `Manual` reuses the transform gizmo to refine placements before capture

The intended workflow can be hybrid:

1. select a group
2. explode automatically
3. switch to manual
4. refine with the gizmo
5. capture

### 3. Capture steps

Each capture stores one logical exploded state inside the active preset.

Captured data includes:

- affected node and mesh identity
- source file
- start local position
- end local position
- start quaternion rotation
- end quaternion rotation

The captured-steps list supports:

- selection
- removal
- drag-and-drop reordering

### 4. Preview

The panel includes a draft preview player that animates the captured steps directly in the scene without creating an animation asset.

Preview supports:

- play/pause
- stop
- scrub
- loop
- exact WYSIWYG parity with animation generation modes

### 5. Create animation

The panel can generate glTF-style animation data from the active preset.

Supported output modes:

- `Parallel`: one clip, all final transforms applied together, last capture wins per node
- `Sequential`: one clip, captures accumulated in step order
- `Separate`: one clip per captured step

`Loop Back` adds reverse keys so the animation collapses back to origin.

Generated clip names derive from the active preset name:

- `Preset Name - Parallel`
- `Preset Name - Sequential`
- `Preset Name - Step N`

---

## Architecture

### Core runtime pieces

- `ExplodedViewPanel`
  - owns preset state
  - owns captured steps
  - owns preview state
  - drives capture and animation creation

- `ExplodedViewManager`
  - computes world-space explosion offsets for automatic modes

- `GLWidget`
  - applies render-time explosion offsets
  - hosts the exploded-view panel
  - hosts manual placement state using the transform gizmo
  - applies preview and generated animation playback

- `SceneGraph`
  - resolves file nodes, mesh ownership, and animation node bindings

- `AnimationsPanel`
  - displays created clips
  - allows playback and reset of saved animations

- `ModelViewer`
  - saves and restores exploded-view presets in MVF

### Automatic explode placement

`ExplodedViewManager` computes offsets from the active preset inputs:

- assembly mesh UUID set
- optional anchor UUID
- mode
- custom vector
- factor

Current supported modes:

- `Auto`
- `AxisX`
- `AxisY`
- `AxisZ`
- `Vector`

The current implementation includes fit-to-screen fixes for exploded state, especially for large axis-mode spreads.

### Manual placement

Manual placement intentionally does not mutate the true model state.

`GLWidget` snapshots the original TRS of the selected meshes, allows gizmo interaction, and can:

- finish placement and keep the staged pose for capture
- clear placement and restore the original TRS

This isolation prevents the animation drift and accidental permanent transform bugs that showed up during earlier iterations.

---

## Data Model

### `ExplodedViewPreset`

`ExplodedViewPanel` currently owns the preset model internally.

Each preset contains:

- `id`
- `name`
- `assemblyUuids`
- `anchorUuid`
- `mode`
- `userVector`
- `factor`
- `capturedSteps`
- `capturedStepCounter`
- `outputMode`
- `durationSeconds`
- `loopBack`

### `CapturedExplosionStep`

Each step contains:

- `id`
- `name`
- `tracks`

### `CapturedTransformTrack`

Each track contains:

- `meshUuid`
- `ownerNodeUuid`
- `sourceFile`
- `targetNodeName`
- `targetNodeIndex`
- `startPosition`
- `endPosition`
- `startRotation`
- `endRotation`

---

## Animation Generation Notes

### Local-space correctness

Exploded offsets are computed in world space, but captured and exported animation channels are stored in local node space.

The implementation converts world offsets into local parent space before capture and stores full TRS-compatible track data. This is the key reason exported glTF/glb animations now match in-app playback.

### Rotation support

Manual placement capture stores quaternion rotation as well as translation.

Interpolation uses spherical linear interpolation (`slerp`) in:

- draft preview
- generated runtime clips
- exported glTF animation data

### Parallel semantics

In `Parallel` mode, if the same node appears in multiple captured steps, the final capture overrides earlier ones for that node. This is intentional and matches the current authoring model of “last capture wins”.

---

## UI Summary

The current panel includes:

- preset combo
- `New`
- `Duplicate`
- preset actions menu for rename/delete
- authoring mode radio buttons
- assembly and anchor picking
- explode mode and vector controls
- distance slider
- animation duration
- output mode combo
- `Loop Back`
- `Capture`
- captured-step list
- preview player
- `Create Animation`
- reset

The selection workflow for assembly picking is two-step:

- selection mode toggles on
- the button changes to a commit/check state
- clicking the button again commits the current selection

---

## MVF Persistence

Exploded-view presets are now persisted in MVF session metadata.

Stored keys:

- `explodedViews`
- `activeExplodedViewId`
- `activeExplodedViewStepIndex`

See `exploded_views_persistence.md` for persistence-specific details.

---

## Files Involved

Primary implementation files:

- `include/ExplodedViewPanel.h`
- `src/ExplodedViewPanel.cpp`
- `include/ExplodedViewManager.h`
- `src/ExplodedViewManager.cpp`
- `include/GLWidget.h`
- `src/GLWidget.cpp`
- `include/AnimationsPanel.h`
- `src/AnimationsPanel.cpp`
- `src/ModelViewer.cpp`

---

## Verified Behavior

The following flows have been verified in the current implementation:

- multiple presets can be authored independently
- preset switching restores the correct explode state
- manual placement does not permanently alter model transforms
- captures can be reordered and previewed
- generated clips play correctly across output modes
- exported glTF/glb animations preserve translation and rotation correctly
- presets survive MVF save/load
- active preset and selected captured step restore correctly
- animation generation after MVF reload still uses restored preset data
- saved animations survive a second MVF save/load and remain playable

---

## Known Deliberate Constraints

- authoring mode is not persisted
- draft preview playback state is not persisted
- preset CRUD is not currently undoable
- persistence currently assumes the current MVF schema only; no legacy migration layer is implemented
