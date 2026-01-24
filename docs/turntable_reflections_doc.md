# Environment Map Reflections for Turntable Camera

## Overview

This document describes the implementation of environment-mapped reflections in a CAD-style turntable viewer with a Z-up coordinate system. The solution addresses the unique challenges of computing physically plausible reflections when using a turntable camera paradigm where the object appears to rotate while the camera conceptually remains fixed.

## Problem Context

### Coordinate System
- **World Space**: Z-up (CAD convention)
  - X-axis: Right
  - Y-axis: Forward/Depth
  - Z-axis: Up (vertical)
  
- **Cubemap Space**: Y-up (OpenGL standard)
  - X-axis: Right
  - Y-axis: Up (vertical)
  - Z-axis: Forward/Depth

### Camera Model
The viewer uses a **turntable/orbit camera** where:
- The camera orbits around a fixed object center
- Users perceive the object as rotating, not themselves moving around it
- All viewpoints share a unified viewing direction toward the scene center
- Camera position changes, but the conceptual view direction remains consistent

## The Challenge

Traditional physically-based reflections compute the view vector per-fragment as:
```glsl
vec3 V = normalize(cameraPos - fragmentPosition);
```

This approach failed in our turntable viewer because:
1. Position-dependent vectors created spatially-varying reflections incompatible with the turntable paradigm
2. Coordinate space mismatches between Z-up world space and Y-up cubemap space
3. The camera's spatial position is less meaningful than its viewing direction in a turntable setup

## Solution

### Correct Implementation (Hybrid Approach)

The optimal solution uses a **hybrid approach** that combines directional stability (for rotation) with subtle positional awareness (for panning):

```glsl
// Hybrid: Direction-based with positional influence for panning
vec3 V_base = normalize(cameraDir);  // Unified view direction (for rotation)
vec3 offset = normalize(cameraPos - g_reflectionPosition);  // Positional offset (for panning)
vec3 V = normalize(V_base + offset * 0.3);  // Blend factor: 0.3 is the sweet spot

vec3 N = normalize(g_reflectionNormal);
vec3 R = reflect(V, N);
R = normalize(R);

// Sample cubemap
vec3 envColor = textureLod(envMap, R, lod).rgb;
```

**Blend Factor Guidelines:**
- **0.0**: Pure direction-based (rotation works, panning has no effect on reflections)
- **0.1-0.2**: Very subtle panning influence (product visualization)
- **0.3-0.4**: Balanced (recommended for CAD viewers) ✓
- **0.5-0.7**: Strong position awareness (architectural walkthroughs)

The value **0.3** provides the optimal balance where:
- Camera rotation updates reflections naturally and correctly
- Camera panning causes subtle, realistic reflection changes
- No coordinate space artifacts or face mapping issues

### Key Shader Uniforms

```glsl
uniform vec3 cameraDir;  // Camera's view direction (from GLCamera::_viewDir)
uniform vec3 cameraPos;  // Camera's position (from GLCamera::_position)
```

### C++ Setup

```cpp
// In rendering code, pass both camera direction and position
QVector3D camDir = _primaryCamera->getViewDir();
QVector3D camPos = _primaryCamera->getPosition();

shader->setUniformValue("cameraDir", camDir);
shader->setUniformValue("cameraPos", camPos);
```

## Why This Works

### 1. Turntable Paradigm Compatibility
In a turntable viewer, users think "the object rotates" rather than "I orbit around it." The hybrid approach respects this mental model by:
- Using a unified base direction for rotation stability
- Adding subtle positional influence for panning realism
- Avoiding the extreme spatial variation of pure position-based approaches

### 2. Coordinate Space Alignment
Both the camera's view direction (`_viewDir`) and position (`_position`) are in Z-up world space, matching:
- How normals are defined (`g_reflectionNormal`)
- How the skybox geometry is displayed (with 90° X-rotation)
- The reflection vector computation

No additional coordinate transformations are needed because everything stays in the same space.

### 3. Natural Reflection Behavior
Reflections update correctly as the camera rotates and pans:
- **Rotation**: Dominated by the unified direction (`V_base`), providing stable and correct environment mapping
- **Panning**: Influenced by the positional offset with a 0.3 blend factor, adding subtle parallax-like changes
- The blend provides the best of both approaches without the artifacts of either extreme

### 4. Adjustable Balance
The blend factor can be tuned based on application needs:
- Lower values (0.1-0.2) for applications where rotation dominates
- Higher values (0.4-0.5) for applications needing stronger position awareness
- Sweet spot at 0.3 for typical turntable CAD viewers

## What Doesn't Work

### ❌ Position-Based Reflections (Traditional Approach)
```glsl
// This creates coordinate space mismatches and artifacts
vec3 V = normalize(cameraPos - g_reflectionPosition);
vec3 R = reflect(-V, N);
R = envMapRotationMatrix * R;  // Requires complex transformations
```

**Problems:**
- Faces map to wrong cubemap locations
- 180° rotation artifacts
- Seams between faces when applying per-face corrections
- Incompatible with turntable viewing paradigm

### ❌ Coordinate Transform Matrices
```glsl
// Rotation matrices cause more problems than they solve
R = envMapRotationMatrix * R;  // Rotates the reflection, sampling wrong content
```

**Problems:**
- Rotation matrices change the **direction** vectors point
- Works only for scene-fixed positions (like light sources), not orbiting cameras
- Creates 90° or 180° rotation artifacts

## Implementation Notes

### For PBR Workflow
The same hybrid approach works for both direct lighting and IBL:

```glsl
// In calculatePBRLighting()
vec3 V_base = normalize(cameraDir);
vec3 offset = normalize(cameraPos - g_reflectionPosition);
vec3 V = normalize(V_base + offset * 0.3);  // Hybrid approach

vec3 L = normalize(lightSource.position);

// Standard PBR BRDF calculations
vec3 H = normalize(V + L);
// ... rest of PBR code

// For IBL specular
vec3 R = reflect(-V, N);  // Note: -V for incident direction
R = normalize(R);
vec3 prefilteredColor = textureLod(prefilterMap, R, lod).rgb;
```

### For ADS (Blinn-Phong) Workflow
```glsl
// In shadeBlinnPhong()
vec3 V_base = normalize(cameraDir);
vec3 offset = normalize(cameraPos - g_reflectionPosition);
vec3 V = normalize(V_base + offset * 0.3);  // Hybrid approach

vec3 L = normalize(lightSource.position - g_position);
vec3 H = normalize(L + V);

// Environment mapping
vec3 R = reflect(V, normalize(g_reflectionNormal));
vec3 envColor = textureLod(envMap, R, lod).rgb;
```

### Normal Orientation
Use `g_reflectionNormal` (from vertex shader, world-space transformed) rather than `g_normal` to avoid geometry shader artifacts that can cause reflections to rotate around face normals instead of camera axes.

## Comparison with Standard Approaches

| Aspect | Traditional (Position-Based) | Turntable (Direction-Based) |
|--------|------------------------------|----------------------------|
| View Vector | `cameraPos - fragmentPos` | `cameraViewDir` (uniform) |
| Per-Fragment | Varies spatially | Same for all fragments |
| Coordinate Transform | Required (complex) | Not needed |
| Viewing Paradigm | "I move around object" | "Object rotates for me" |
| Reflection Behavior | True view-dependent | Unified view-dependent |

## Testing & Validation

To verify correct implementation:

1. **Static Test**: Reflections should show environment content that makes sense for surface orientation
2. **Rotation Test**: As camera orbits, reflections should update smoothly without:
   - Faces appearing on wrong sides
   - 180° or 90° rotation artifacts  
   - Seams at cubemap face boundaries
3. **Material Test**: Should work identically for:
   - PBR metallic/roughness materials
   - Traditional Blinn-Phong materials
   - All geometry types (curved and flat surfaces)

## Technical Deep Dive

### Why Position-Based Approach Failed

The fundamental issue was that `cameraPos` represents the camera's **location in 3D space**, which constantly changes as you orbit. Computing per-fragment vectors from this position created:

```glsl
// Each fragment gets a different view direction based on its world position
vec3 V1 = normalize(cameraPos - fragmentPos1);  // Points one direction
vec3 V2 = normalize(cameraPos - fragmentPos2);  // Points different direction
```

This is correct for **true physical reflections** in a free-flying camera, but creates artifacts in a turntable viewer because:
- The cubemap was captured in Y-up space
- Your world is Z-up
- The varying vectors needed complex per-face coordinate transformations
- Different faces required different 180° rotations, causing seams

### Why Direction-Based Approach Works

Using `cameraViewDir` provides a **single, consistent direction** for the entire frame:

```glsl
// All fragments use the same view direction
vec3 V = normalize(cameraViewDir);  // Same for entire object
```

This works because:
- The direction is already in Z-up world space
- It matches how the skybox is displayed
- No coordinate space conversion needed
- Natural for a turntable viewer where conceptually "you look at the object from one direction"

### The Refract Workaround (Historical Note)

During development, this formula was discovered to work:
```glsl
vec3 R = refract(-I, normalize(-g_reflectionNormal), 1.0f);
```

This worked because `refract()` with IOR=1.0 and negated inputs happened to produce the correct sampling directions by accident, but it wasn't physically correct and broke proper BRDF calculations. The direction-based approach is the proper solution.

## Debugging Tips

If reflections appear incorrect:

1. **Check uniform name**: Ensure shader uniform matches C++ code (`cameraDir` in both places)
2. **Verify view direction**: Print `_viewDir` values to confirm they update with camera rotation
3. **Test with simple geometry**: Use a cube or sphere to clearly see which faces map to which environment
4. **Compare with skybox**: Reflections should match the skybox orientation
5. **Check normal source**: Use `g_reflectionNormal` not `g_normal`

Common mistakes:
```glsl
// ❌ Wrong - uses position instead of direction
vec3 V = normalize(cameraPos - g_position);

// ❌ Wrong - tries to apply unnecessary transforms
vec3 R = envMapRotationMatrix * reflect(V, N);

// ❌ Wrong - uses geometry shader normal (causes rotation artifacts)
vec3 N = normalize(g_normal);

// ✅ Correct
vec3 V = normalize(cameraDir);
vec3 N = normalize(g_reflectionNormal);
vec3 R = reflect(V, N);
```

## Future Considerations

### Hybrid Approaches
For applications needing both turntable and free-flying camera modes:

```glsl
uniform bool useTurntableReflections;

vec3 V;
if (useTurntableReflections) {
    V = normalize(cameraDir);  // Turntable mode
} else {
    V = normalize(cameraPos - g_reflectionPosition);  // Free-fly mode
}
```

### Performance
The direction-based approach is **more efficient** than position-based:
- One uniform load vs. per-fragment position calculation
- No complex coordinate transformations
- Simpler shader code

## References

This solution was developed through systematic analysis of:
- Z-up vs Y-up coordinate system transformations
- Turntable camera viewing paradigms in CAD software
- OpenGL cubemap sampling conventions
- The relationship between reflection vectors and view directions

**Related Documentation:**
- `GLCamera.cpp` - Camera implementation with Z-up coordinate system
- `main_scene.frag` - Fragment shader with reflection calculations
- Skybox rendering implementation (90° X-rotation for Z-up display)

---

**Last Updated**: 2025-10-22  
**Implementation Files**: `GLWidget.cpp`, `main_scene.frag`, `main_scene.vert`  
**Related Systems**: Skybox rendering, IBL generation, PBR pipeline, ADS lighting

**Contributors**: Developed through extensive debugging and analysis of coordinate space transformations in turntable camera systems.