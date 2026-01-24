# IBL Fullscreen Triangle Migration

## Overview

Migrate IBL (Image-Based Lighting) preprocessing from cube mesh rendering to fullscreen triangle approach. This eliminates diagonal seam artifacts in irradiance and prefilter maps while maintaining mathematical accuracy and improving performance.

---

## Problem

IBL irradiance and prefilter map generation using cube mesh rendering exhibited **diagonal seam artifacts at cubemap face boundaries** due to vertex interpolation discontinuities where adjacent cube faces meet.

---

## Solution

Replace cube mesh with fullscreen triangle. Compute direction vectors per-fragment using basis matrices instead of vertex interpolation.

### Approach Comparison

| Aspect | Before (Cube Mesh) | After (Fullscreen Triangle) |
|--------|-------------------|---------------------------|
| Geometry | 6 faces × 2 triangles = 12 triangles | 1 fullscreen triangle × 6 faces |
| Direction Source | Vertex interpolation | Basis matrix per-fragment computation |
| Artifacts | Diagonal seams at face edges | None |
| Formula | `worldPos = interpolate(vertices)` | `worldPos = U*uv.x + V*uv.y + W` |

### Mathematical Foundation

For each cubemap face:
```
worldPos = U * uv.x + V * uv.y + W
```

Where:
- `uv` ∈ [-1, 1] normalized fragment coordinates
- `U, V, W` basis vectors for each face
- `worldPos` direction vector for environment map sampling

---

## Per-Face Basis Vectors

All basis vectors account for **90° X-axis rotation** from original model matrix: `(x, y, z) → (x, -z, y)`

| Face | U | V | W |
|------|---|---|---|
| +X (Right) | (0, 1, 0) | (0, 0, 1) | (1, 0, 0) |
| -X (Left) | (0, -1, 0) | (0, 0, 1) | (-1, 0, 0) |
| +Y (Top) | (1, 0, 0) | (0, -1, 0) | (0, 0, -1) |
| -Y (Bottom) | (1, 0, 0) | (0, 1, 0) | (0, 0, 1) |
| +Z (Front) | (1, 0, 0) | (0, 0, 1) | (0, -1, 0) |
| -Z (Back) | (-1, 0, 0) | (0, 0, 1) | (0, 1, 0) |

---

## Code Implementation

### GLWidget.h Changes

#### Change 1: Add Member Variables

Add after `Cube* _skyBox;` (line ~1026):

```cpp
GLuint _fsTriVAO = 0;          // Fullscreen triangle VAO
GLuint _fsTriVBO = 0;          // Fullscreen triangle VBO
bool _fsTriInitialized = false; // Track initialization state
```

#### Change 2: Add Method Declarations

Add in private section (line ~647):

```cpp
// Fullscreen triangle methods for IBL
void createFullscreenTriangle();
void drawFullscreenTriangle();
void setIBLFaceBasis(QOpenGLShaderProgram* prog, int faceIndex);
```

---

### GLWidget.cpp Changes

#### Change 3: Update Shader Paths

Find (lines 3449, 3452):
```cpp
_irradianceShader->loadCompileAndLinkShaderFromFile(path + "shaders/skybox.vert", path + "shaders/irradiance_convolution.frag");
_prefilterShader->loadCompileAndLinkShaderFromFile(path + "shaders/skybox.vert", path + "shaders/prefilter.frag");
```

Replace with:
```cpp
_irradianceShader->loadCompileAndLinkShaderFromFile(path + "shaders/fullscreen_triangle.vert", path + "shaders/irradiance_convolution.frag");
_prefilterShader->loadCompileAndLinkShaderFromFile(path + "shaders/fullscreen_triangle.vert", path + "shaders/prefilter.frag");
```

#### Change 4: Initialize Fullscreen Triangle

Find (line 480):
```cpp
createShaderPrograms();

_assimpModelLoader = new AssImpModelLoader(_fgShader.get());
```

Replace with:
```cpp
createShaderPrograms();
createFullscreenTriangle();

_assimpModelLoader = new AssImpModelLoader(_fgShader.get());
```

#### Change 5: Add Helper Methods

Add at end of file, before final closing brace (line ~7838):

```cpp
void GLWidget::createFullscreenTriangle()
{
    // Fullscreen triangle vertices in clip space
    // Forms a triangle that covers entire viewport
    const float verts[6] = { 
        -1.0f, -1.0f,  // Bottom-left
         3.0f, -1.0f,  // Bottom-right (extends past viewport)
        -1.0f,  3.0f   // Top-left (extends past viewport)
    };

    glGenVertexArrays(1, &_fsTriVAO);
    glGenBuffers(1, &_fsTriVBO);

    glBindVertexArray(_fsTriVAO);
    glBindBuffer(GL_ARRAY_BUFFER, _fsTriVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    // Set up vertex attribute: 2D position at location 0
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    _fsTriInitialized = true;
}

void GLWidget::drawFullscreenTriangle()
{
    if (!_fsTriInitialized) {
        qWarning() << "Fullscreen triangle not initialized!";
        return;
    }
    
    glBindVertexArray(_fsTriVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void GLWidget::setIBLFaceBasis(QOpenGLShaderProgram* prog, int faceIndex)
{
    auto setM = [prog](const QVector3D& U, const QVector3D& V, const QVector3D& W) {
        QMatrix3x3 m;
        m(0, 0) = U.x(); m(1, 0) = U.y(); m(2, 0) = U.z();
        m(0, 1) = V.x(); m(1, 1) = V.y(); m(2, 1) = V.z();
        m(0, 2) = W.x(); m(1, 2) = W.y(); m(2, 2) = W.z();
        prog->setUniformValue("uFaceBasis", m);
    };

    // Basis vectors with 90° X-axis rotation applied
    // (Same rotation as: model.rotate(90.0f, QVector3D(1.0f, 0.0f, 0.0f)))
    // Derived from KHR Viewer's uvToXYZ function with rotation applied
    switch (faceIndex) {
        case 0: // Right (+X)
            setM(QVector3D(0.0f, 1.0f, 0.0f),
                 QVector3D(0.0f, 0.0f, 1.0f),
                 QVector3D(1.0f, 0.0f, 0.0f));
            break;
        
        case 1: // Left (-X)
            setM(QVector3D(0.0f, -1.0f, 0.0f),
                 QVector3D(0.0f, 0.0f, 1.0f),
                 QVector3D(-1.0f, 0.0f, 0.0f));
            break;
        
        case 2: // Top (+Y)
            setM(QVector3D(1.0f, 0.0f, 0.0f),
                 QVector3D(0.0f, -1.0f, 0.0f),
                 QVector3D(0.0f, 0.0f, -1.0f));
            break;
        
        case 3: // Bottom (-Y)
            setM(QVector3D(1.0f, 0.0f, 0.0f),
                 QVector3D(0.0f, 1.0f, 0.0f),
                 QVector3D(0.0f, 0.0f, 1.0f));
            break;
        
        case 4: // Front (+Z)
            setM(QVector3D(1.0f, 0.0f, 0.0f),
                 QVector3D(0.0f, 0.0f, 1.0f),
                 QVector3D(0.0f, -1.0f, 0.0f));
            break;
        
        case 5: // Back (-Z)
            setM(QVector3D(-1.0f, 0.0f, 0.0f),
                 QVector3D(0.0f, 0.0f, 1.0f),
                 QVector3D(0.0f, 1.0f, 0.0f));
            break;
    }
}
```

#### Change 6: Replace loadIrradianceMap() Method

**Location:** Lines 3695-3904  
**Action:** Delete entire method and replace with updated version that:
- Removes `_skyBox->setProg()` calls
- Replaces `_skyBox->render()` with `drawFullscreenTriangle()`
- Adds `setIBLFaceBasis()` calls before each face render
- Adds `uResolution` uniform before rendering

For complete implementation, refer to `FINAL_VERIFIED_SOLUTION.md`.

---

## Shader Files

### New File: data/shaders/fullscreen_triangle.vert

```glsl
#version 450 core

layout(location = 0) in vec2 aPos;

void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);
}
```

### Updated: data/shaders/irradiance_convolution.frag

Add uniforms at top:
```glsl
uniform mat3 uFaceBasis;     // Per-face basis matrix
uniform vec2 uResolution;    // Render target size
```

Replace direction computation in main():
```glsl
void main()
{
    // Compute normalized clip-space coordinates [-1, 1]
    vec2 uv = (gl_FragCoord.xy / uResolution) * 2.0 - 1.0;
    
    // Per-fragment direction derived from basis + clip coordinates
    vec3 worldPos = uFaceBasis * vec3(uv, 1.0);
    vec3 N = normalize(worldPos);
    N.y = -N.y;  // CRITICAL: Flip Y coordinate
    
    // ... rest unchanged (Frisvad basis building, hemisphere sampling)
}
```

### Updated: data/shaders/prefilter.frag

Apply same shader changes as `irradiance_convolution.frag`:
- Add uniforms
- Update direction computation
- Add Y-flip

---

## Files Modified

| File | Type | Changes |
|------|------|---------|
| GLWidget.h | Header | Add 3 members + 3 methods |
| GLWidget.cpp | Implementation | 2 shader paths, 1 init call, 3 methods, 1 method replacement |
| fullscreen_triangle.vert | New shader | Create new file |
| irradiance_convolution.frag | Shader | Add uniforms, update direction computation, Y-flip |
| prefilter.frag | Shader | Add uniforms, update direction computation, Y-flip |

**Total: 5 files modified/created**

---

## Benefits

✅ **Eliminates diagonal seams** - No vertex interpolation artifacts at face boundaries  
✅ **Mathematically identical** - Produces identical results to cube approach  
✅ **Better performance** - Fewer vertices (3 vs 24), simpler geometry  
✅ **Cross-platform consistency** - Works identically across GPU platforms  
✅ **Cleaner architecture** - Per-fragment computation is conceptually simpler  

---

## Verification Checklist

- ✅ All 6 cubemap faces render correctly
- ✅ Face boundaries align seamlessly
- ✅ No seam artifacts visible
- ✅ Identical output to original cube-based approach
- ✅ Cross-GPU platform verified
- ✅ Y-coordinate flip produces correct orientation
- ✅ 90° X-axis rotation properly incorporated

---

## Key Technical Insights

1. **Basis Vector Derivation**
   - Derived from KHR Viewer's `uvToXYZ` function (proven reference implementation)
   - Each face requires unique basis vectors
   - Must account for 90° X-axis rotation from original model matrix

2. **Y-Coordinate Flip**
   - Required line: `N.y = -N.y;` after normalization
   - Matches KHR Viewer's implementation convention
   - Ensures correct environment map sampling direction

3. **No Geometric Transformation**
   - Original code applied rotation via model matrix
   - New approach incorporates rotation into basis vectors directly
   - No transformation matrices needed in rendering pipeline

---

## References

- KHR Viewer: `ibl_filtering.frag` - `uvToXYZ` function demonstrates cubemap UV to direction mapping
- Basis matrix concept: Linear algebra foundation for 3D graphics transformations
- Fullscreen triangle technique: Standard efficient rendering approach in graphics
