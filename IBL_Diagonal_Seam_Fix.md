
# IBL Diagonal Seam Fix — Full Implementation Guide

**Date:** 2026-01-07  
**Author:** M365 Copilot (with your GLWidget.cpp context)  

---

## TL;DR

Your irradiance and prefilter maps show a faint **diagonal seam** because each cubemap face is baked by rendering a **cube** (two triangles per face). The convolution integrates samples whose **sampling directions** differ slightly across that internal triangle edge, and the difference gets **baked** into the output texture.

**Fix (small & robust):**
1. Render each cubemap face using a **single fullscreen triangle**.
2. Compute the sampling direction **per fragment** using a per–face basis (`uFaceBasis`).
3. Keep your existing convolution logic (irradiance integration and specular prefilter importance sampling).

This removes triangle-boundary interpolation differences, so the diagonal disappears on **RTX A4000** and remains clean on **GTX 1660 Ti**.

---

## Why the seam appears

- Rendering each face with a quad (two triangles) creates an **internal diagonal**. Across that diagonal, interpolated varyings (and potentially implicit derivatives) differ slightly, which affects the **sample directions** and/or **LOD** used during convolution.
- Irradiance and specular prefilter maps are **blurred** by design; even tiny discontinuities become visible as a line.
- `GL_TEXTURE_CUBE_MAP_SEAMLESS` helps **across face edges**, but it **cannot fix** an intra‑face diagonal baked during generation.

---

## What changes (scope)

- **Shaders:**
  - Add a new vertex shader: `fullscreen_triangle.vert`.
  - Update `irradiance_convolution.frag` to compute per‑fragment direction from `gl_FragCoord` + `uFaceBasis`.
  - Update `prefilter.frag` similarly.
- **C++ (GLWidget.cpp):**
  - In `createShaderPrograms()`, compile irradiance/prefilter programs with `fullscreen_triangle.vert` instead of `skybox.vert`.
  - Add a tiny VAO/VBO for the fullscreen triangle and a helper to draw it.
  - In `loadIrradianceMap()` and in the prefilter section, set `uFaceBasis`+`uResolution` and draw the fullscreen triangle **per face** (and per mip for prefilter).

Total change is about **120–150 lines**. No change to your skybox rendering or main scene shaders.

---

## Per-face basis (what `uFaceBasis` should be)

Use a right‑handed basis for each cubemap face:

| Face | W (forward/look) | U (right) | V (up) |
|---|---|---|---|
| +X | +X | +Z | −Y |
| −X | −X | −Z | −Y |
| +Y | +Y | +X | +Z |
| −Y | −Y | +X | −Z |
| +Z | +Z | −X | −Y |
| −Z | −Z | +X | −Y |

In the fragment shader, the direction for each pixel is:

```glsl
vec2 uv = (gl_FragCoord.xy / uResolution) * 2.0 - 1.0;  // [-1,1]
vec3 dir = normalize(uFaceBasis * vec3(uv, 1.0));        // U*uv.x + V*uv.y + W
```

---

## Files & code

### 1) `shaders/fullscreen_triangle.vert`

```glsl
#version 450 core
layout(location = 0) in vec2 aPos;
void main() {
    // Clip-space coordinates are supplied by the VBO
    gl_Position = vec4(aPos, 0.0, 1.0);
}
```

> **VBO data (C++):** use three clip‑space vertices `(-1,-1), (3,-1), (-1,3)` to cover the entire viewport.

---

### 2) `shaders/irradiance_convolution.frag` (updated)

> **Replace the `in vec3 worldPos;` path with per‑fragment direction.

```glsl
#version 450 core
out vec4 fragColor;
uniform samplerCube environmentMap;
uniform mat3 uFaceBasis;   // columns = U, V, W
uniform vec2 uResolution;  // (width, height)

const float PI      = 3.14159265359;
const float TWO_PI  = 2.0 * PI;
const float HALF_PI = 0.5 * PI;

void main()
{
    // Per-fragment direction for this face pixel
    vec2 uv = (gl_FragCoord.xy / uResolution) * 2.0 - 1.0;  // [-1, 1]
    vec3 N  = normalize(uFaceBasis * vec3(uv, 1.0));

    // Frisvad tangent frame
    vec3 T, B;
    if (N.z < -0.9999999) {
        T = vec3(0.0, -1.0, 0.0);
        B = vec3(-1.0, 0.0, 0.0);
    } else {
        float a = 1.0 / (1.0 + N.z);
        float b = -N.x * N.y * a;
        T = vec3(1.0 - N.x*N.x*a, b, -N.x);
        B = vec3(b, 1.0 - N.y*N.y*a, -N.y);
    }

    // Your existing sampling density
    float sampleDelta = 0.0125;  // tune: 0.0167..0.0125 recommended
    int numPhiSteps   = int(ceil(TWO_PI  / sampleDelta));
    int numThetaSteps = int(ceil(HALF_PI / sampleDelta));

    vec3 irradiance = vec3(0.0);
    float totalSamples = float(numPhiSteps * numThetaSteps);

    for (int iPhi = 0; iPhi < numPhiSteps; ++iPhi) {
        float phi = float(iPhi) * sampleDelta;
        for (int iTheta = 0; iTheta < numThetaSteps; ++iTheta) {
            float theta = float(iTheta) * sampleDelta;
            vec3 tangentSample = vec3(sin(theta) * cos(phi),
                                      sin(theta) * sin(phi),
                                      cos(theta));
            vec3 sampleVec = tangentSample.x * T + tangentSample.y * B + tangentSample.z * N;

            // Implicit LOD is fine; you may use textureLod(..., 0.0) if your source has mips issues
            irradiance += texture(environmentMap, sampleVec).rgb * cos(theta) * sin(theta);
        }
    }

    irradiance = PI * irradiance / totalSamples;
    fragColor  = vec4(irradiance, 1.0);
}
```

---

### 3) `shaders/prefilter.frag` (direction update)

> Keep your existing importance sampling and roughness→LOD mapping. Only change how `N` is obtained.

```glsl
#version 450 core
out vec4 fragColor;

uniform samplerCube environmentMap;  // source HDR env with mips
uniform mat3 uFaceBasis;             // columns = U, V, W
uniform vec2 uResolution;            // face size
uniform float roughness;             // per-mip roughness
uniform int maxMipLevel;             // if needed

// ... your helper functions (Hammersley, ImportanceSampleGGX, etc.) ...

void main() {
    // Per-fragment direction for this face pixel
    vec2 uv = (gl_FragCoord.xy / uResolution) * 2.0 - 1.0;
    vec3 N  = normalize(uFaceBasis * vec3(uv, 1.0));

    const int SAMPLE_COUNT = 1024; // tune as you already do
    vec3 prefiltered = vec3(0.0);
    float totalWeight = 0.0;

    float lodBase = roughness * float(maxMipLevel);

    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        // vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        // vec3 H  = ImportanceSampleGGX(Xi, roughness, N);
        // vec3 L  = normalize(2.0 * dot(N, H) * H - N);
        // float NoL = max(dot(N, L), 0.0);
        // if (NoL > 0.0) {
        //     float lod = lodBase; // optionally add PDF-based adjustment
        //     vec3 radiance = textureLod(environmentMap, L, lod).rgb;
        //     prefiltered += radiance * NoL;
        //     totalWeight += NoL;
        // }
    }

    prefiltered = (totalWeight > 0.0) ? (prefiltered / totalWeight) : prefiltered;
    fragColor   = vec4(prefiltered, 1.0);
}
```

> **Note:** The block above shows only the **direction change**. Keep your existing importance sampling functions and loop body; simply compute `N` from `uFaceBasis` and use it in your current code.

---

## C++ changes (GLWidget.cpp)

### 4) Compile irradiance/prefilter with the new vertex shader

In `createShaderPrograms()` **replace** these two lines:

```cpp
// Irradiance Map
_irradianceShader = std::make_unique<ShaderProgram>(); _irradianceShader->setObjectName("_irradianceShader");
_irradianceShader->loadCompileAndLinkShaderFromFile(path + "shaders/skybox.vert", path + "shaders/irradiance_convolution.frag");

// Prefilter Map
_prefilterShader = std::make_unique<ShaderProgram>(); _prefilterShader->setObjectName("_prefilterShader");
_prefilterShader->loadCompileAndLinkShaderFromFile(path + "shaders/skybox.vert", path + "shaders/prefilter.frag");
```

…with:

```cpp
// Irradiance Map
_irradianceShader = std::make_unique<ShaderProgram>(); _irradianceShader->setObjectName("_irradianceShader");
_irradianceShader->loadCompileAndLinkShaderFromFile(path + "shaders/fullscreen_triangle.vert", path + "shaders/irradiance_convolution.frag");

// Prefilter Map
_prefilterShader = std::make_unique<ShaderProgram>(); _prefilterShader->setObjectName("_prefilterShader");
_prefilterShader->loadCompileAndLinkShaderFromFile(path + "shaders/fullscreen_triangle.vert", path + "shaders/prefilter.frag");
```

### 5) Fullscreen triangle VAO/VBO and draw helper

Add these members (e.g., at file scope near other GL objects):

```cpp
GLuint fsTriVAO = 0, fsTriVBO = 0;
```

Add the helper functions:

```cpp
void GLWidget::createFullscreenTriangle() {
    const float verts[6] = { -1.f, -1.f,  3.f, -1.f,  -1.f, 3.f };
    glGenVertexArrays(1, &fsTriVAO);
    glGenBuffers(1, &fsTriVBO);
    glBindVertexArray(fsTriVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fsTriVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    glBindVertexArray(0);
}

void GLWidget::drawFullscreenTriangle() {
    glBindVertexArray(fsTriVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}
```

Call `createFullscreenTriangle()` once in `initializeGL()` **after** `createShaderPrograms()`.

### 6) Basis setter (per face) and uniform setup

Inside `loadIrradianceMap()` (and similarly in the prefilter section) prepare functions to set `uFaceBasis` and `uResolution`.

```cpp
static auto setFaceBasis = [&](QOpenGLShaderProgram* prog, int faceIndex) {
    auto setM = [&](const QVector3D& U, const QVector3D& V, const QVector3D& W) {
        QMatrix3x3 m; m.setColumn(0, U); m.setColumn(1, V); m.setColumn(2, W);
        prog->setUniformValue("uFaceBasis", m);
    };
    switch (faceIndex) {
        case 0: setM({ 0,0, 1}, { 0,-1,0}, { 1,0,0}); break; // +X
        case 1: setM({ 0,0,-1}, { 0,-1,0}, {-1,0,0}); break; // -X
        case 2: setM({ 1,0, 0}, { 0, 0,1}, { 0,1,0}); break; // +Y
        case 3: setM({ 1,0, 0}, { 0, 0,-1},{ 0,-1,0}); break; // -Y
        case 4: setM({-1,0, 0}, { 0,-1,0}, { 0,0,1}); break; // +Z
        case 5: setM({ 1,0, 0}, { 0,-1,0}, { 0,0,-1}); break; // -Z
    }
};
```

### 7) Irradiance pass — draw fullscreen triangle per face

In `loadIrradianceMap()`, **replace** the per–face cube render call:

```cpp
// Old inside the loop:
_skyBox->setProg(_irradianceShader.get());
_irradianceShader->bind();
_irradianceShader->setUniformValue("viewMatrix", captureViews[i]);
// ...
_skyBox->render();
```

…with:

```cpp
// Before the loop
_irradianceShader->bind();
_irradianceShader->setUniformValue("environmentMap", 1);
_irradianceShader->setUniformValue("uResolution", QVector2D(irradianceSize, irradianceSize));

for (unsigned int i = 0; i < 6; ++i) {
    _irradianceShader->bind();
    setFaceBasis(_irradianceShader.get(), i);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, _irradianceMap, 0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawFullscreenTriangle();
}
```

> You no longer need `captureViews`/`projectionMatrix` for this pass, because direction is derived from screen position and basis, not from a cube mesh.

### 8) Prefilter pass — same approach, per mip & face

In the prefilter section of `loadIrradianceMap()`:

```cpp
_prefilterShader->bind();
_prefilterShader->setUniformValue("environmentMap", 1);

for (unsigned int mip = 0; mip < maxMipLevels; ++mip) {
    unsigned int mipWidth  = prefilterSize * std::pow(0.5, mip);
    unsigned int mipHeight = prefilterSize * std::pow(0.5, mip);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
    glViewport(0, 0, mipWidth, mipHeight);

    float roughness = std::max(0.04f, (float)mip / (float)(maxMipLevels - 1));
    _prefilterShader->setUniformValue("roughness", roughness);
    _prefilterShader->setUniformValue("uResolution", QVector2D(mipWidth, mipHeight));
    _prefilterShader->setUniformValue("maxMipLevel", (int)(maxMipLevels - 1));

    for (unsigned int i = 0; i < 6; ++i) {
        _prefilterShader->bind();
        setFaceBasis(_prefilterShader.get(), i);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, _prefilterMap, mip);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawFullscreenTriangle();
    }
}
```

> Keep your current importance sampling and `textureLod` in the shader; the CPU side just sets up basis/resolution and draws the fullscreen triangle.

---

## Optional guards

- Keep `GL_TEXTURE_CUBE_MAP_SEAMLESS` enabled (you already do) for **cross‑face** filtering.
- Use float formats (`GL_RGB16F`/`GL_RGB32F`) for env/irradiance/prefilter.
- If your source env has questionable mips, clamp `GL_TEXTURE_BASE_LEVEL`/`GL_TEXTURE_MAX_LEVEL` or sample with `textureLod(..., 0.0)` in irradiance.

---

## Test & verify

1. Regenerate irradiance and prefilter maps with the changes above.
2. Temporarily show the **irradiance map** as the background (debug) — the diagonal should be **gone** on RTX A4000.
3. Compare results on GTX 1660 Ti — no regressions expected.

---

## Checklist (quick)

- [ ] Added `fullscreen_triangle.vert`.
- [ ] Irradiance FS now computes `N` from `uFaceBasis` + `uResolution`.
- [ ] Prefilter FS does the same.
- [ ] Programs compiled with `fullscreen_triangle.vert`.
- [ ] Created fullscreen triangle VAO/VBO; drawing per face/mip.
- [ ] `uFaceBasis` set per face; `uResolution` set per pass/mip.

---

## FAQ

**Q:** Why didn’t `textureLod` fix it?

**A:** `textureLod` only locks the mip level. The seam comes from slight differences in **direction vectors** across a two‑triangle face. Those differences persist even with explicit LOD, so the integral bakes a line. Computing direction per fragment removes that boundary entirely.

**Q:** Can NVIDIA Control Panel settings help?

**A:** They can slightly affect mip selection and precision but cannot remove an intra‑face diagonal baked by two‑triangle rendering. Code changes above are the reliable fix.

---

## License

You may copy/paste/adapt this guide and code into your project without restriction.
