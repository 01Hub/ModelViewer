# Exploded View MVF Persistence

## Status

Implemented as of 2026-06-14 and baseline-committed together with the exploded-view preset system.

This document describes the persistence model that now exists in code, not the earlier proposal phase.

---

## Persistence Scope

Exploded-view persistence is document-level state stored in MVF session metadata.

Persisted:

- all exploded-view presets
- active preset id
- selected captured-step index for the active preset

Not persisted:

- transient Auto/Manual authoring mode
- draft preview playback state
- manual placement active/staged session state
- draft preview timer position
- panel visibility

---

## Storage Location

The data is stored in `Mvf::Document::mvfSession`.

Current keys:

```json
{
  "explodedViews": [ ... ],
  "activeExplodedViewId": "uuid-string",
  "activeExplodedViewStepIndex": 1
}
```

This was chosen because exploded-view presets are authored document assets, not per-file metadata. A single preset can legitimately reference nodes from multiple imported source files.

---

## Serialized Shape

### Preset

Each preset is serialized with:

- `id`
- `name`
- `assemblyUuids`
- `anchorUuid`
- `mode`
- `userVector`
- `factor`
- `capturedStepCounter`
- `outputMode`
- `durationSeconds`
- `loopBack`
- `capturedSteps`

Example:

```json
{
  "id": "9f5f5c87-3d1d-4cb4-8c5a-3ef0b7d82d13",
  "name": "Exploded View 2",
  "assemblyUuids": [
    "8fdb91d8-6b47-426c-8f8b-4020325b8c3a"
  ],
  "anchorUuid": "ec4e7d3d-f8f6-4419-9af2-b4fe2bc8bf25",
  "mode": "AxisX",
  "userVector": [1.0, 0.0, 0.0],
  "factor": 1.0,
  "capturedStepCounter": 3,
  "outputMode": 1,
  "durationSeconds": 3.0,
  "loopBack": true,
  "capturedSteps": [ ... ]
}
```

### Captured step

Each step stores:

- `id`
- `name`
- `tracks`

### Transform track

Each track stores:

- `meshUuid`
- `ownerNodeUuid`
- `sourceFile`
- `targetNodeName`
- `targetNodeIndex`
- `startPosition`
- `endPosition`
- `startRotation`
- `endRotation`

Quaternion layout is:

```json
[w, x, y, z]
```

This matches the current `QQuaternion` serialization helpers in `ExplodedViewPanel.cpp`.

---

## Save Path

### Entry point

`ModelViewer::buildMVFPackage()`

### Runtime flow

1. `ModelViewer` asks `GLWidget` for the active `ExplodedViewPanel`.
2. The panel exports its preset state through:
   - `presetsToJson()`
   - `activePresetId()`
   - `activeCapturedStepIndex()`
3. `ModelViewer` writes those into `package.document.mvfSession`.

### Code surface

Primary files:

- `src/ModelViewer.cpp`
- `include/ExplodedViewPanel.h`
- `src/ExplodedViewPanel.cpp`

---

## Load Path

### Entry point

`ModelViewer::loadFromFile()`

### Runtime flow

1. MVF JSON is parsed into `Mvf::Document`.
2. `ModelViewer` reads:
   - `explodedViews`
   - `activeExplodedViewId`
   - `activeExplodedViewStepIndex`
3. After scene graph, animation, and camera metadata are restored, `ModelViewer` calls:

```cpp
explodedViewPanel->restorePresetsFromJson(
    result.explodedViews,
    QUuid(result.activeExplodedViewId),
    result.activeExplodedViewStepIndex);
```

4. `ExplodedViewPanel`:
   - stops draft preview
   - cancels picking mode
   - clears manual placement state
   - rebuilds `_presets`
   - resolves the active preset by id
   - restores the selected captured step
   - refreshes panel UI state

---

## Restore Rules

### Empty or missing exploded view data

If `explodedViews` is absent or empty:

- the panel falls back to its default initialization path
- `Exploded View 1` is created

### Invalid preset ids

If a preset id is missing or invalid on load:

- a fresh UUID is generated

### Empty preset names

If a preset name is empty:

- the panel generates the next default name

### Invalid or incomplete tracks

Tracks are skipped if:

- `ownerNodeUuid` is missing
- `sourceFile` is empty

Steps with no valid tracks are dropped during restore.

### Captured-step counter normalization

If the stored `capturedStepCounter` is stale or too small, it is normalized to:

```text
capturedSteps.size() + 1
```

This preserves stable future naming like `Step 3`, `Step 4`, and so on.

---

## What Is Intentionally Not Solved Yet

The current implementation does not yet include:

- legacy MVF migration
- degraded UI badges for missing tracks or unresolved presets
- standalone JSON import/export for exploded presets
- undo/redo for preset CRUD

That is intentional. The current schema assumes the post-release exploded-view format only.

---

## Validation Performed

The following persistence flows have been validated against the current implementation:

1. Create multiple presets with captures, save MVF, reopen, and verify preset list plus active preset restore.
2. Verify selected captured-step index restores.
3. Create animation after reload and confirm it still uses the restored preset data correctly.
4. Save MVF again with the created animations.
5. Reload MVF and verify animations still play.

All of the above are currently working as expected.

---

## Practical Notes For Future Changes

If the preset model changes, update both:

- `ExplodedViewPanel::presetsToJson()`
- `ExplodedViewPanel::restorePresetsFromJson()`

If persistence is moved out of the panel later, keep the current document-level shape unless there is a strong reason to split exploded views by source file. The current document-level model matches the feature semantics better.
