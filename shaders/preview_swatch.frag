#version 450 core
#define MAX_LIGHTS 8

// ----- Varyings from vertex shader -----
in vec3 vNormal;        // world-space normal (geom)
in vec3 vPos;           // world-space position
in vec2 vTexCoord;      // base UV
in vec4 vTangentW;      // world-space tangent (xyz) + handedness (w)

out vec4 FragColor;

// ----- Camera -----
uniform vec3 uCamPos;

// Preview profile
uniform int  uPreviewProfile; // 0 = TextureAuthoring, 1 = MaterialShowcase

// 0=All, 1=Albedo, 2=Metalness, 3=Roughness, 4=Normal, 5=AO, 6=Height, 7=Opacity, 8=Emissive
uniform int uTexViewMode;

// ----- Lights -----
struct Light { vec3 position; vec3 color; };   // you use these as direction-like
uniform Light uLights[MAX_LIGHTS];
uniform int   uNumLights;

// Environment mode (set from C++): 0=Studio, 1=Outdoor, 2=Office
uniform int  uEnvMode;

// Hemisphere colors (set per mode)
uniform vec3 uSkyColor;    // color from +Y
uniform vec3 uGroundColor; // color from -Y

// Intensity multipliers
uniform float uEnvDiffuseIntensity;  // e.g. 0.2
uniform float uEnvSpecularIntensity; // keep for future (IBL), 0.0 for now

uniform float uExposureEV;   // in EV stops, e.g. [-4 .. +4], default 0
uniform bool  uUseACES;      // optional: filmic tonemap toggle (true recommended)

// Optional: if you want a separate gamma control (or assume sRGB=2.2)
uniform float uGamma;        // default 2.2

// Simple ACES approximation (Narkowicz)
vec3 acesTonemap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x*(a*x + b)) / (x*(c*x + d) + e), 0.0, 1.0);
}

// ----- Base material uniforms -----
uniform vec3  uAlbedo;
uniform float uMetalness;
uniform float uRoughness;
uniform float uOpacity;
uniform float uClearcoat;
uniform float uClearcoatRoughness;
uniform vec3  uSheenColor;
uniform float uSheenRoughness;
uniform float uTransmission;
uniform float uIOR;
uniform float uSpecular;

// ----- Texture samplers -----
uniform sampler2D uAlbedoMap;
uniform sampler2D uMetalnessMap;
uniform sampler2D uRoughnessMap;
uniform sampler2D uNormalMap;
uniform sampler2D uOpacityMap;
uniform sampler2D uAOMap;
uniform sampler2D uEmissiveMap;
uniform sampler2D uHeightMap;   // NEW

// ----- Enable flags -----
uniform bool uUseAlbedoMap;
uniform bool uUseMetalnessMap;
uniform bool uUseRoughnessMap;
uniform bool uUseNormalMap;
uniform bool uUseOpacityMap;
uniform bool uOpacityInverted;
uniform bool uUseAOMap;
uniform bool uUseEmissiveMap;
uniform bool uUseHeightMap;     // NEW

// ----- Extra controls -----
uniform float uAOIntensity;
uniform vec3  uEmissiveColor;
uniform float uEmissiveIntensity;
uniform vec2  uUVScale;
uniform float uNormalIntensity;
uniform float uHeightIntensity; // NEW (e.g. 0.03 .. 0.06)

// ---------- Helpers ----------

// TBN-based normal mapping (tangent-space normal -> world space)
vec3 getNormalFromMap(vec2 uv)
{
    if (!uUseNormalMap) return normalize(vNormal);

    // Sample tangent-space normal in [-1,1]
    vec3 nTS = texture(uNormalMap, uv).xyz * 2.0 - 1.0;
    nTS.xy *= uNormalIntensity;

    // Build TBN from vertex outputs
    vec3 N = normalize(vNormal);
    vec3 T = normalize(vTangentW.xyz);
    vec3 B = normalize(cross(N, T)) * vTangentW.w; // handedness in .w
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * nTS);
}

// Very cheap parallax (single-layer offset mapping).
// Offsets UVs along the view direction in tangent space.
vec2 parallaxUV(vec2 uv, vec3 Vworld, vec3 Nworld, vec4 tangentW)
{
    vec3 T = normalize(tangentW.xyz);
    vec3 B = normalize(cross(Nworld, T)) * tangentW.w;
    mat3 TBN = mat3(T, B, Nworld);

    // view in tangent space
    vec3 Vts = normalize(transpose(TBN) * Vworld);

    // height in [0,1] -> center around 0
    float h = texture(uHeightMap, uv).r;
    float height = (h - 0.5) * uHeightIntensity;

    // Offset proportional to view slope
    vec2 offset = (Vts.xy / max(Vts.z, 0.1)) * height;
    return uv - offset;
}

// Simple hemisphere ambient: blends ground and sky by N.y
vec3 hemisphereAmbient(vec3 N, vec3 sky, vec3 ground)
{
    float t = clamp(N.y * 0.5 + 0.5, 0.0, 1.0); // -1..+1 -> 0..1
    return mix(ground, sky, t);
}

void main()
{
    // World vectors
    vec3 Ngeom = normalize(vNormal);
    vec3 V     = normalize(uCamPos - vPos);  // view (camera -> fragment)
    if (length(V) < 0.1) V = vec3(0,0,1);    // safety (shouldn't trigger in your preview)

    // Base UV (tiling)
    vec2 uv = vTexCoord * uUVScale;

    // ----- Parallax: adjust UVs BEFORE sampling any map -----
    if (uUseHeightMap) {
        uv = parallaxUV(uv, V, Ngeom, vTangentW);
        // Optional: keep inside 0..1 (useful for preview, avoids wrap artifacts)
        uv = clamp(uv, vec2(0.0), vec2(1.0));
    }

    // ---- Texture debug views (no lighting). 0 = All (normal shading) ----
    if (uTexViewMode != 0) {
        vec2 uv = vTexCoord * uUVScale;

        vec3 outCol = vec3(0.0);
        if (uTexViewMode == 1) {                     // Albedo
            vec3 base = uUseAlbedoMap ? texture(uAlbedoMap, uv).rgb : uAlbedo;
            outCol = clamp(base, 0.0, 1.0);
        }
        else if (uTexViewMode == 2) {                // Metalness
            float m = uUseMetalnessMap ? texture(uMetalnessMap, uv).r : uMetalness;
            outCol = vec3(m);
        }
        else if (uTexViewMode == 3) {                // Roughness
            float r = uUseRoughnessMap ? texture(uRoughnessMap, uv).r : uRoughness;
            outCol = vec3(r);
        }
        else if (uTexViewMode == 4) {                // Normal (tangent space)
            vec3 nTS = uUseNormalMap ? (texture(uNormalMap, uv).xyz * 2.0 - 1.0) : vec3(0,0,1);
            // visualize tangent normals mapped to 0..1
            outCol = nTS * 0.5 + 0.5;
        }
        else if (uTexViewMode == 5) {                // Ambient Occlusion
            float ao = uUseAOMap ? texture(uAOMap, uv).r : 1.0;
            outCol = vec3(ao);
        }
        else if (uTexViewMode == 6) {                // Height
            float h = uUseHeightMap ? texture(uHeightMap, uv).r : 0.5;
            outCol = vec3(h);
        }
        else if (uTexViewMode == 7) {                // Opacity
            float a = uUseOpacityMap ? texture(uOpacityMap, uv).r : uOpacity;
            outCol = vec3(a);
        }
        else if (uTexViewMode == 8) {                // Emissive (as color)
            vec3 e = uUseEmissiveMap ? texture(uEmissiveMap, uv).rgb : vec3(0.0);
            outCol = e * uEmissiveColor * uEmissiveIntensity;
        }

        // Apply exposure / tonemap for consistent preview look
        float exposure = exp2(uExposureEV);
        outCol *= exposure;
        if (uUseACES) {
            const float a=2.51, b=0.03, c=2.43, d=0.59, e=0.14;
            outCol = clamp((outCol*(a*outCol + b)) / (outCol*(c*outCol + d) + e), 0.0, 1.0);
        } else {
            outCol = outCol / (1.0 + outCol);
        }
        float gamma = (uGamma > 0.0) ? uGamma : 2.2;
        outCol = pow(outCol, vec3(1.0 / gamma));

        FragColor = vec4(outCol, 1.0);
        return;
    }


    // ----- Sample or fallback using parallaxed UVs -----
    vec3  albedo    = uUseAlbedoMap    ? texture(uAlbedoMap,    uv).rgb : uAlbedo;
    albedo = clamp(albedo, vec3(0.01), vec3(1.0));

    float metalness = uUseMetalnessMap ? texture(uMetalnessMap, uv).r   : uMetalness;
    metalness = clamp(metalness, 0.0, 1.0);

    float roughness = uUseRoughnessMap ? texture(uRoughnessMap, uv).r   : uRoughness;
    roughness = clamp(roughness, 0.01, 1.0);

    float opacity   = uUseOpacityMap   ? texture(uOpacityMap,   uv).r   : uOpacity;
    if(uOpacityInverted) opacity = 1 - opacity;
    opacity = clamp(opacity, 0.0, 1.0);

    float ao        = uUseAOMap        ? texture(uAOMap,        uv).r   : 1.0;
    ao = mix(1.0, ao, uAOIntensity);

    vec3 emissive = vec3(0.0);
    if (uUseEmissiveMap) {
        emissive = texture(uEmissiveMap, uv).rgb * uEmissiveColor * uEmissiveIntensity;
    }

    // ----- Normal mapping uses parallaxed UVs -----
    vec3 N = getNormalFromMap(uv);

    // ----- Metal vs dielectric split -----
    vec3 dielectricDiffuse  = albedo;
    vec3 dielectricSpecular = vec3(0.04);
    vec3 metallicDiffuse    = vec3(0.0);
    vec3 metallicSpecular   = albedo;

    vec3 diffuseColor  = mix(dielectricDiffuse,  metallicDiffuse,  metalness);
    vec3 specularColor = mix(dielectricSpecular, metallicSpecular, metalness);

    // ----- Direct lighting -----
    vec3 color = vec3(0.075) * albedo; // small base light

    for (int i = 0; i < uNumLights; ++i)
    {
        // Your rig: treat .position as an incoming direction
        vec3 L = normalize(uLights[i].position);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL <= 0.0) continue;

        vec3 H = normalize(V + L);
        float NdotH = max(dot(N, H), 0.0);

        float specPow    = clamp(mix(8.0, 128.0, 1.0 - roughness), 1.0, 256.0);
        float specFactor = pow(NdotH, specPow);

        vec3 diffuse = diffuseColor * NdotL;
        vec3 spec    = specularColor * specFactor;

        color += (diffuse + spec) * uLights[i].color * NdotL;
    }

    // --- Hemisphere environment ambient (AO only on diffuse) ---
    vec3 hemi = hemisphereAmbient(normalize(N), uSkyColor, uGroundColor);
    vec3 diffuseAmbient = albedo * hemi * uEnvDiffuseIntensity * ao;
    // (specular IBL can be added later; keep uEnvSpecularIntensity for future)
    color += diffuseAmbient;

    // ----- Clearcoat (simple lobe using light 0 direction-like) -----
    if (uClearcoat > 0.001 && uNumLights > 0)
    {
        vec3 Lc = normalize(uLights[0].position);
        vec3 Hc = normalize(V + Lc);
        float ccNdotH   = max(dot(N, Hc), 0.0);
        float ccSpecPow = clamp(mix(64.0, 512.0, 1.0 - uClearcoatRoughness), 1.0, 1024.0);
        float ccSpec    = pow(ccNdotH, ccSpecPow);
        color += vec3(0.25) * uClearcoat * ccSpec;
    }

    // ----- Sheen -----
    if (length(uSheenColor) > 0.001)
    {
        float NdotV = max(dot(N, V), 0.0);
        float sheen = pow(clamp(1.0 - NdotV, 0.0, 1.0), 2.0);
        color += uSheenColor * sheen * 0.5;
    }

    // ----- Transmission (simple tint) -----
    if (uTransmission > 0.001)
    {
        vec3 glassTint = mix(albedo, vec3(1.0), clamp((uIOR - 1.0) * 0.3, 0.0, 1.0));
        color = mix(color, glassTint, uTransmission);
    }

    // ----- Rim -----
    float rim = pow(clamp(1.0 - max(dot(N, V), 0.0), 0.0, 1.0), 2.0);
    color += albedo * rim * 0.2;

    // ----- Emissive -----
    color += emissive;


    if(uPreviewProfile == 1) {
        // ----- Brightness tweak -----
        color *= mix(1.1, 2.5, metalness);

        // ----- Final clamp -----
        color = clamp(color, vec3(0.0), vec3(20.0));
    } else {
        // --- Exposure (in EV) ---
        float exposure = exp2(uExposureEV);   // 2^EV
        color *= exposure;

        // --- Tonemap ---
        if (uUseACES) {
            color = acesTonemap(color);
        } else {
            // Reinhard as a fallback (cheap)
            color = color / (1.0 + color);
        }

        // --- Gamma encode to sRGB (if your default framebuffer isn't sRGB)
        float gamma = (uGamma > 0.0) ? uGamma : 2.2;
        color = pow(color, vec3(1.0 / gamma));
    }

    FragColor = vec4(color, opacity);
}
