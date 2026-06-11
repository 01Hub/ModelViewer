# ModelViewer-Qt: Exploded View Feature
## Architecture & Implementation Document

**Version:** 2.0 (Architecture Revision — codebase-accurate)
**Date:** 2026-06-11
**Status:** UI design phase — ready for `.ui` file authoring

---

## TABLE OF CONTENTS

1. [Executive Summary](#executive-summary)
2. [Architecture & Design](#architecture--design)
3. [Data Structures](#data-structures)
4. [Algorithms & Logic](#algorithms--logic)
5. [UI/UX Specification](#uiux-specification)
6. [Animation Recording & Export](#animation-recording--export)
7. [Implementation Phases](#implementation-phases)
8. [File Structure & Integration](#file-structure--integration)

---

## EXECUTIVE SUMMARY

Implement an exploded view system for ModelViewer-Qt that allows users to:

1. Select an assembly or sub-assembly and explode its parts radially or along an axis
2. Optionally designate an anchor mesh that stays centred while others explode around it
3. Capture the exploded state as a named TRS animation stored in the document
4. Play back captured animations via `AnimationsPanel`
5. Export animations as proper glTF TRS animation channels via `GltfPostProcessor`

**Technology stack:**
- Qt6, `GLWidget` (OpenGL 4.5), `SceneGraph` / `SceneNode` / `TriangleMesh`
- No SSBO, no shader changes — explosion is a per-draw-call `modelMatrix` translation offset
- `GltfPostProcessor` JSON surgery for glTF export (same mechanism as camera/TRS animation export)

---

## ARCHITECTURE & DESIGN

### High-Level Component Map

```
User interaction (ExplodedViewPanel overlay on GLWidget)
        │
        ▼
ExplodedViewManager
  ├─ Receives: assembly UUIDs, anchor UUID, mode, slider factor
  ├─ Computes: per-mesh world-space explosion offsets (one-time on selection/mode change)
  └─ Provides: QHash<QUuid, glm::vec3> _explosionOffsets   ← keyed by mesh UUID

GLWidget::paintMesh()
  └─ If _explosionOffsets contains meshUuid:
       modelMatrix = originalModelMatrix + translationBy(offset * sliderFactor)
     (No shader changes. Pure CPU-side matrix modification per draw call.)

Capture button
  └─ ExplodedViewAnimation created:
       per-mesh track: { t=0: original local pos, t=duration: exploded local pos }
       Added to document's explodedViewAnimations list
       Shown in AnimationsPanel (synthetic section)

glTF export
  └─ GltfPostProcessor iterates explodedViewAnimations
       Maps mesh UUID → glTF node index (from export node map)
       Injects animation.channels[] + animation.samplers[] into glTF JSON
```

### Core Classes

| Class | Location | Role |
|---|---|---|
| `ExplodedViewManager` | `include/` + `src/` | Compute offsets, hold state |
| `ExplodedViewAnimation` | `include/` + `src/` | Store per-mesh translation tracks |
| `ExplodedViewPanel` | `include/` + `src/` + `ui/` | Overlay UI on `GLWidget` |
| `GLWidget` | existing | Applies offsets at render time; owns manager |
| `AnimationsPanel` | existing — modified | Shows synthetic exploded animations |
| `GltfPostProcessor` | existing — modified | Injects animations on glTF export |
| `ModelViewer` | existing — modified | Save/load exploded animations in MVF |

### Overlay Panel Pattern

`ExplodedViewPanel` follows the `ClippingPlanesEditor` pattern exactly:
- `QWidget` subclass, parented to `GLWidget`
- Added to `GLWidget::_lowerLayout` during `GLWidget` initialisation
- Hidden by default; shown/hidden via `GLWidget::showExplodedViewPanel(bool)`
- Holds a `GLWidget*` pointer for direct calls

---

## DATA STRUCTURES

### ExplodedViewManager State

```cpp
class ExplodedViewManager {
public:
    enum class Mode { Auto, AxisX, AxisY, AxisZ, Vector };

    // Inputs (set by panel)
    QSet<QUuid>   assemblyMeshUuids;   // meshes participating in explosion
    QUuid         anchorMeshUuid;      // stays at origin; null = no anchor
    Mode          mode = Mode::Auto;
    glm::vec3     userVector{1,0,0};   // used only in Vector mode
    float         sliderFactor = 1.0f; // 0.0–2.0 (slider 0%–200%)

    // Output (consumed by GLWidget render loop)
    QHash<QUuid, glm::vec3> explosionOffsets; // per-mesh base offset at factor=1.0

    // Methods
    void recompute(const SceneGraph* sg, const QHash<QUuid, BoundingBox>& meshBounds);
    void clear();
};
```

### ExplodedViewAnimation

```cpp
struct ExplodedViewMeshTrack {
    QUuid    meshUuid;
    glm::vec3 localPosOriginal;  // local translation at t=0
    glm::vec3 localPosExploded;  // local translation at t=duration
};

class ExplodedViewAnimation {
public:
    QUuid    id;               // stable identity
    QString  name;             // user-provided, e.g. "Exploded View 1"
    float    duration;         // seconds, default 3.0, user-editable at capture
    QDateTime createdAt;

    QVector<ExplodedViewMeshTrack> tracks;  // one per exploding mesh

    QJsonObject toJson() const;
    static ExplodedViewAnimation fromJson(const QJsonObject&);
};
```

### ExplosionState (transient — not serialised)

```cpp
struct ExplosionState {
    QSet<QUuid>              assemblyMeshUuids;
    QUuid                    anchorMeshUuid;
    ExplodedViewManager::Mode mode;
    glm::vec3                userVector;
    float                    sliderFactor;
};
```

### MVF JSON Format

```json
{
  "explodedViewAnimations": [
    {
      "id": "550e8400-e29b-41d4-a716-446655440000",
      "name": "Exploded View 1",
      "duration": 3.0,
      "createdAt": "2026-06-11T10:00:00Z",
      "tracks": [
        {
          "meshUuid": "...",
          "localPosOriginal": [0.0, 0.0, 0.0],
          "localPosExploded": [1.25, 0.0, 0.0]
        }
      ]
    }
  ]
}
```

---

## ALGORITHMS & LOGIC

### Explosion Vector Computation

All modes are computed once when selection or mode changes. `sliderFactor` just scales at render time.

**Assembly centroid** = average of all participating mesh centroids (world space).

#### Auto (Radial)
```
for each mesh in assemblyMeshUuids:
    if mesh == anchorMeshUuid:
        offset = (0, 0, 0)
        continue
    dir = normalize(meshCentroid - assemblyCentroid)
    if |dir| < 1e-6: dir = (1, 0, 0)   // degenerate fallback
    dist = |meshCentroid - assemblyCentroid|
    explosionOffsets[mesh] = dir * dist  // at factor=1.0, part doubles its distance
```

#### Axis X / Y / Z
```
axis = unitVec[X|Y|Z]
for each mesh in assemblyMeshUuids:
    if mesh == anchorMeshUuid: offset = (0,0,0); continue
    projection = dot(meshCentroid - assemblyCentroid, axis)
    explosionOffsets[mesh] = axis * projection
```
(Parts on opposite sides of the centroid project to opposite signs — they move apart.)

#### Vector (user-defined)
```
dir = normalize(userVector)
for each mesh in assemblyMeshUuids:
    if mesh == anchorMeshUuid: offset = (0,0,0); continue
    projection = dot(meshCentroid - assemblyCentroid, dir)
    explosionOffsets[mesh] = dir * projection
```

### Render-Time Application (GLWidget)

```cpp
// In GLWidget::paintMesh(uuid, originalModelMatrix):
if (_explodedViewManager && _explodedViewManager->explosionOffsets.contains(uuid)) {
    glm::vec3 offset = _explodedViewManager->explosionOffsets[uuid]
                     * _explodedViewManager->sliderFactor;
    glm::mat4 explodedModel = glm::translate(originalModelMatrix, offset);
    shader.setUniform("modelMatrix", explodedModel);
} else {
    shader.setUniform("modelMatrix", originalModelMatrix);
}
```

No shader changes. No SSBO. Pure CPU matrix math per draw call.

### Local-Space Conversion for Animation Tracks

Explosion offsets are computed in world space. For animation keyframes (glTF TRS is local space):

```cpp
// For each mesh track at capture time:
glm::vec3 worldOffset = explosionOffsets[uuid] * sliderFactor;

// Get parent node world transform for this mesh
glm::mat4 parentWorld = getParentWorldTransform(uuid, sceneGraph);
glm::mat4 parentWorldInv = glm::inverse(parentWorld);

// Local-space delta
glm::vec3 localOffset = glm::vec3(parentWorldInv * glm::vec4(worldOffset, 0.0f));

track.localPosOriginal = mesh->localTranslation();       // original local pos
track.localPosExploded = track.localPosOriginal + localOffset;
```

---

## UI/UX SPECIFICATION

### Panel Presentation

`ExplodedViewPanel` is a non-modal overlay `QWidget` docked into `GLWidget::_lowerLayout`, following the `ClippingPlanesEditor` pattern. It is shown when the user activates "Exploded View" from the toolbar or menu, and hidden on close (explosion state cleared).

### Controls Layout

```
┌─────────────────────────────────────────────────────────┐
│  EXPLODED VIEW                               [✕ Close]  │
├─────────────────────────────────────────────────────────┤
│  Assembly                                               │
│  [📋 /root/GearBox — 12 meshes           ] [↩ Select]  │
│                                                         │
│  Anchor (optional)                                      │
│  [📋 Housing                             ] [↩ Select]  │
├─────────────────────────────────────────────────────────┤
│  Explosion Mode   [Auto (Radial)              ▼]        │
│                                                         │
│  Vector   X [  1.000 ↕]  Y [  0.000 ↕]  Z [  0.000 ↕] │
│           (visible only when mode = Vector)             │
├─────────────────────────────────────────────────────────┤
│  Distance                                               │
│  [━━━━━━━━━━━●━━━━━━━━━━━━━━━━━━━━━━━━━━━━] 100%       │
│   0%                100%                  200%          │
├─────────────────────────────────────────────────────────┤
│  [         Capture Animation         ]                  │
│  (disabled when slider < 10%)                           │
└─────────────────────────────────────────────────────────┘
```

### Selector Controls Detail

Each selector (Assembly and Anchor) is a pair:
- **Read-only `QLineEdit`**: shows current selection info or placeholder text
  - Assembly: `"Select assembly or meshes…"` when empty
  - Anchor: `"Select anchor mesh (optional)…"` when empty
  - Populated: sub-assembly name if all meshes share a parent, otherwise `"N meshes selected"`
  - Right-click context menu: **Clear Selection**
- **Arrow `QPushButton`** (`↩` icon): clears screen selection, enters picking mode
  - Panel enters picking mode: button stays highlighted, edit box shows `"Click mesh or node in scene…"`
  - On selection confirmed from viewport or `SceneTreeWidget`: selection captured, screen selection cleared
  - Picking mode cancelled by pressing the button again or pressing Escape

### Mode ComboBox Items

| Display | Internal enum |
|---|---|
| `Auto (Radial)` | `Mode::Auto` |
| `Axis X` | `Mode::AxisX` |
| `Axis Y` | `Mode::AxisY` |
| `Axis Z` | `Mode::AxisZ` |
| `Custom Vector` | `Mode::Vector` |

Selecting `Custom Vector` shows the three `QDoubleSpinBox` controls (X/Y/Z). All other modes hide them.

### Capture Dialog

Small inline prompt on Capture click:

```
┌──────────────────────────────────────────┐
│  Save Animation                          │
│                                          │
│  Name:     [Exploded View 1            ] │
│  Duration: [  3.0 ↕] seconds            │
│            (range 0.5 – 30.0)           │
│                                          │
│            [Cancel]    [Save]            │
└──────────────────────────────────────────┘
```

Auto-increments name: `"Exploded View 1"`, `"Exploded View 2"`, …

### AnimationsPanel Integration

`AnimationsPanel` tree gets a new synthetic section:

```
Animations
├── GearBox.gltf
│   ├── Walk Cycle
│   └── Gear Rotation
└── Exploded View Animations          ← synthetic section (hidden if empty)
    ├── ▶  Exploded View 1  [🗑]
    └── ▶  Exploded View 2  [🗑]
```

Play/pause/scrub/loop/speed controls work identically for both sections.

### State & Clearing Rules

| Action | Effect |
|---|---|
| Clear assembly selection | Clears anchor too; clears all explosion offsets; slider stays |
| Clear anchor only | Recomputes explosion offsets without anchor; assembly untouched |
| Change mode | Recomputes explosion offsets; slider stays |
| Move slider | Scales offsets live; no recompute |
| Close panel | Clears all explosion offsets; panel hides; captured animations persist |
| Capture | Creates `ExplodedViewAnimation`; adds to document and `AnimationsPanel`; panel state unchanged |

---

## ANIMATION RECORDING & EXPORT

### Capture Flow

```
User sets assembly + anchor + mode + slider position
    ↓
Clicks [Capture Animation]  (disabled if slider < 10%)
    ↓
Inline dialog: set name + duration → [Save]
    ↓
For each mesh in assemblyMeshUuids:
    worldOffset = explosionOffsets[uuid] * sliderFactor
    localOffset = inverse(parentWorldTransform) * worldOffset
    track = { uuid, localPosOriginal, localPosOriginal + localOffset }
    ↓
ExplodedViewAnimation{ name, duration, tracks[] } created
    ↓
Added to document.explodedViewAnimations[]
AnimationsPanel.addSyntheticAnimation(anim)
    ↓
Panel state unchanged (user can capture more with different settings)
```

### glTF Export Flow

```
GltfPostProcessor::process(jsonDoc, nodeUuidToGltfIndex)
    ↓
For each ExplodedViewAnimation in document:
    Create glTF animation object { name, samplers[], channels[] }
    For each track:
        gltfNodeIndex = nodeUuidToGltfIndex[track.meshUuid]
        Build input sampler: [0.0, duration]
        Build output sampler: [localPosOriginal, localPosExploded] (vec3 × 2)
        Add channel: { sampler, target: { node: gltfNodeIndex, path: "translation" } }
    Append animation to glTF animations[]
    ↓
Resulting glTF plays in Babylon.js / three.js / Blender
```

### MVF Save/Load

`ModelViewer::saveToFile()` includes:
```json
{ "explodedViewAnimations": [ … ] }
```
`ModelViewer::loadFromFile()` restores all animations and calls `AnimationsPanel::addSyntheticAnimation()` for each.

---

## IMPLEMENTATION PHASES

### Phase 1 — Core + UI + Recording + MVF persistence
- `ExplodedViewManager`: offset computation, all 5 modes
- `ExplodedViewPanel.ui` + `ExplodedViewPanel.h/cpp`: full overlay UI
- `GLWidget` modifications: manager ownership, render-time offset application, picking mode signals
- `ExplodedViewAnimation`: data structure + JSON serialisation
- `AnimationsPanel` modifications: synthetic animation section
- `ModelViewer` modifications: save/load `explodedViewAnimations[]`
- Playback via existing animation engine (lerp on translation tracks)

### Phase 2 — glTF Export
- `GltfPostProcessor` modifications: inject exploded animations as glTF TRS channels
- Validation in Babylon.js Sandbox

### Phase 3 — Axis/Vector point & line picking from viewport
- "Select point" for X/Y/Z modes (axis through point parallel to world axis)
- "Select line" for Vector mode (direction from line in scene)

---

## FILE STRUCTURE & INTEGRATION

### New Files

```
include/
├── ExplodedViewManager.h
├── ExplodedViewAnimation.h
└── ExplodedViewPanel.h

src/
├── ExplodedViewManager.cpp
├── ExplodedViewAnimation.cpp
└── ExplodedViewPanel.cpp

ui/
└── ExplodedViewPanel.ui
```

CMakeLists.txt uses `file(GLOB ...)` — no changes needed.

### Files to Modify

| File | Change |
|---|---|
| `include/GLWidget.h` | Add `ExplodedViewManager*`, picking mode state, `showExplodedViewPanel(bool)` |
| `src/GLWidget.cpp` | Instantiate panel + manager; apply offsets at render time; emit selection signals |
| `include/AnimationsPanel.h` | Add `addSyntheticAnimation()`, `removeSyntheticAnimation()` |
| `src/AnimationsPanel.cpp` | Render synthetic section in tree; wire play/delete |
| `src/ModelViewer.cpp` | Save/load `explodedViewAnimations[]`; connect toolbar action |
| `src/GltfPostProcessor.cpp` | Inject exploded animations into glTF on export |

---

## OPEN QUESTIONS (Phase 2+)

- **Q1:** Should the playback scrubber in AnimationsPanel show seconds or a 0–100% bar for exploded animations?
- **Q2:** For glTF export, should all exploded animations be exported, or only the one currently selected in AnimationsPanel?
- **Q3:** Phase 3 point/line picking — snap to mesh centroid on hover, or surface intersection point?

---

**Version 2.0 — replaces v1.0 entirely**
**All pseudocode and class names are codebase-accurate as of 2026-06-11**
