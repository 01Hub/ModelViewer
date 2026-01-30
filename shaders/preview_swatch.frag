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

// ----- KHR_materials_iridescence -----
uniform float uIridescence;
uniform float uIridescenceIOR;
uniform float uIridescenceThicknessMin;
uniform float uIridescenceThicknessMax;

// ----- Texture samplers -----
uniform sampler2D uAlbedoMap;
uniform sampler2D uMetalnessMap;
uniform sampler2D uRoughnessMap;
uniform sampler2D uNormalMap;
uniform sampler2D uOpacityMap;
uniform sampler2D uAOMap;
uniform sampler2D uEmissiveMap;
uniform sampler2D uHeightMap;  

// ----- Enable flags -----
uniform bool uUseAlbedoMap;
uniform bool uUseMetalnessMap;
uniform bool uUseRoughnessMap;
uniform bool uUseNormalMap;
uniform bool uUseOpacityMap;
uniform bool uOpacityInverted;
uniform bool uUseAOMap;
uniform bool uUseEmissiveMap;
uniform bool uUseHeightMap;     

// ----- Extra controls -----
uniform float uAOIntensity;
uniform vec3  uEmissiveColor;
uniform float uEmissiveStrength;
uniform vec2  uUVScale;
uniform float uNormalIntensity;
uniform float uHeightIntensity; // (e.g. 0.03 .. 0.06)

// ----- Texture transforms (scale, offset, rotation) -----
uniform vec2  uAlbedoScale;
uniform vec2  uAlbedoOffset;
uniform float uAlbedoRotation;

uniform vec2  uNormalScale;
uniform vec2  uNormalOffset;
uniform float uNormalRotation;

uniform vec2  uMetalnessScale;
uniform vec2  uMetalnessOffset;
uniform float uMetalnessRotation;

uniform vec2  uRoughnessScale;
uniform vec2  uRoughnessOffset;
uniform float uRoughnessRotation;

uniform vec2  uAOScale;
uniform vec2  uAOOffset;
uniform float uAORotation;

uniform vec2  uHeightScale;
uniform vec2  uHeightOffset;
uniform float uHeightRotation;

uniform vec2  uOpacityScale;
uniform vec2  uOpacityOffset;
uniform float uOpacityRotation;

uniform vec2  uEmissiveScale;
uniform vec2  uEmissiveOffset;
uniform float uEmissiveRotation;

// channel packing uniforms (for packed textures like ORM/AORM)
uniform int   uMetalnessChannel;
uniform int   uMetalnessChannelInvert;
uniform float uMetalnessChannelScale;
uniform float uMetalnessChannelBias;

uniform int   uRoughnessChannel;
uniform int   uRoughnessChannelInvert;
uniform float uRoughnessChannelScale;
uniform float uRoughnessChannelBias;

uniform int   uAOChannel;
uniform int   uAOChannelInvert;
uniform float uAOChannelScale;
uniform float uAOChannelBias;

uniform int   uOpacityChannel;
uniform int   uOpacityChannelInvert;
uniform float uOpacityChannelScale;
uniform float uOpacityChannelBias;


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

// Apply scale, offset, and rotation to UV coordinates
vec2 applyTextureTransform(vec2 uv, vec2 scale, vec2 offset, float rotation)
{    
    // Apply scale
    uv *= scale;
    
    // Apply rotation (assuming rotation is in radians)
    float c = cos(rotation);
    float s = sin(rotation);
    uv = mat2(c, -s, s, c) * uv; 
    
    // Apply offset
    uv += offset;

    return uv;
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
    float h = texture(uHeightMap, applyTextureTransform(uv, uHeightScale, uHeightOffset, uHeightRotation)).r;
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

// pick a single channel from a vec4 using an integer selector (0=r,1=g,2=b,3=a).
// invertFlag is 0 or 1. scale & bias allow simple remapping (scale * value + bias).
float pickChannel(vec4 v, int ch, int invertFlag, float scale, float bias)
{
    float c = 0.0;
    if (ch == 0) c = v.r;
    else if (ch == 1) c = v.g;
    else if (ch == 2) c = v.b;
    else if (ch == 3) c = v.a;
    else c = 0.0; // channel == -1 or unknown -> use 0

    if (invertFlag != 0) c = 1.0 - c;
    c = c * scale + bias;
    return clamp(c, 0.0, 1.0);
}

// ============================================================================
// IRIDESCENCE: Physics-based thin-film interference (KHR_materials_iridescence)
// ============================================================================

const float M_PI = 3.14159265359;
const mat3 XYZ_TO_REC709 = mat3(
	3.2404542, -0.9692660, 0.0556434,
	-1.5371385, 1.8760108, -0.2040259,
	-0.4985314, 0.0415560, 1.0572252
);

// Forward declaration for overloaded function
vec3 evalIridescence(float outsideIOR, float eta2, float cosTheta1,
	float thinFilmThickness, vec3 baseF0, vec3 baseF90);

// Helper: square a value
float sq(float a) { return a * a; }
vec3 sq(vec3 a) { return a * a; }

// Fresnel0 to IOR conversion
vec3 Fresnel0ToIor(vec3 fresnel0)
{
	vec3 sqrtF0 = sqrt(fresnel0);
	return (vec3(1.0) + sqrtF0) / (vec3(1.0) - sqrtF0);
}

// IOR to Fresnel0 conversion (vec3 version)
vec3 IorToFresnel0(vec3 transmittedIor, float incidentIor)
{
	return sq((transmittedIor - vec3(incidentIor)) / (transmittedIor + vec3(incidentIor)));
}

// IOR to Fresnel0 conversion (float version)
float IorToFresnel0(float transmittedIor, float incidentIor)
{
	return sq((transmittedIor - incidentIor) / (transmittedIor + incidentIor));
}

// Scalar Fresnel-Schlick with F90
float F_Schlick_Iridescence(float f0, float cosTheta, float f90)
{
	return f0 + (f90 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Vector Fresnel-Schlick with F90
vec3 F_Schlick_Iridescence(vec3 f0, float cosTheta, vec3 f90)
{
	return f0 + (f90 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Backward compatible versions (optional)
float F_Schlick_Iridescence(float f0, float cosTheta)
{
	return F_Schlick_Iridescence(f0, cosTheta, 1.0);
}

vec3 F_Schlick_Iridescence(vec3 f0, float cosTheta)
{
	return F_Schlick_Iridescence(f0, cosTheta, vec3(1.0));
}

// CRITICAL: XYZ sensitivity curves -> RGB conversion
// This maps CIE XYZ color space to sRGB, enabling accurate iridescent colors
vec3 evalSensitivity(float OPD, vec3 shift)
{
	float phase = 2.0 * M_PI * OPD * 1.0e-9;
	vec3 val = vec3(5.4856e-13, 4.4201e-13, 5.2481e-13);
	vec3 pos = vec3(1.6810e+06, 1.7953e+06, 2.2084e+06);
	vec3 var = vec3(4.3278e+09, 9.3046e+09, 6.6121e+09);

	vec3 xyz = val * sqrt(2.0 * M_PI * var) * cos(pos * phase + shift) * exp(-sq(phase) * var);
	xyz.x += 9.7470e-14 * sqrt(2.0 * M_PI * 4.5282e+09) * cos(2.2399e+06 * phase + shift[0]) * exp(-4.5282e+09 * sq(phase));
	xyz /= 1.0685e-7;

	vec3 srgb = XYZ_TO_REC709 * xyz;
	return srgb;
}

// Full thin-film interference model (KHR_materials_iridescence spec-compliant)
vec3 evalIridescence(float outsideIOR, float eta2, float cosTheta1,
	float thinFilmThickness, vec3 baseF0, vec3 baseF90)
{
	vec3 I;
	float iridescenceIor = mix(outsideIOR, eta2, smoothstep(0.0, 0.03, thinFilmThickness));
	float sinTheta2Sq = sq(outsideIOR / iridescenceIor) * (1.0 - sq(cosTheta1));
	float cosTheta2Sq = 1.0 - sinTheta2Sq;
	if (cosTheta2Sq < 0.0)
	{
		return vec3(1.0);
	}
	float cosTheta2 = sqrt(cosTheta2Sq);

	// First interface (air to iridescent film)
	// F90 at air-film interface is always 1.0
	float R0 = IorToFresnel0(iridescenceIor, outsideIOR);
	float R12 = F_Schlick_Iridescence(R0, cosTheta1, 1.0);  // F90 = 1.0 for air interface
	float R21 = R12;
	float T121 = 1.0 - R12;
	float phi12 = 0.0;
	if (iridescenceIor < outsideIOR) phi12 = M_PI;
	float phi21 = M_PI - phi12;

	// Second interface (iridescent film to base material)
	// F90 at film-base interface uses baseF90 from the base material
	vec3 baseIOR = Fresnel0ToIor(clamp(baseF0, 0.0, 0.9999));
	vec3 R1 = IorToFresnel0(baseIOR, iridescenceIor);
	vec3 R23 = F_Schlick_Iridescence(R1, cosTheta2, baseF90);  // Use baseF90 for base material
	vec3 phi23 = vec3(0.0);
	if (baseIOR[0] < iridescenceIor) phi23[0] = M_PI;
	if (baseIOR[1] < iridescenceIor) phi23[1] = M_PI;
	if (baseIOR[2] < iridescenceIor) phi23[2] = M_PI;

	// Optical path difference
	float OPD = 2.0 * iridescenceIor * thinFilmThickness * cosTheta2;
	vec3 phi = vec3(phi21) + phi23;

	// Compound terms
	vec3 R123 = clamp(R12 * R23, 1e-5, 0.9999);
	vec3 r123 = sqrt(R123);
	vec3 Rs = sq(T121) * R23 / (vec3(1.0) - R123);

	// DC term
	vec3 C0 = R12 + Rs;
	I = C0;

	// Higher order interference (2 iterations for richness)
	vec3 Cm = Rs - T121;
	for (int m = 1; m <= 2; ++m)
	{
		Cm *= r123;
		vec3 Sm = 2.0 * evalSensitivity(float(m) * OPD, float(m) * phi);
		I += Cm * Sm;
	}
	return max(I, vec3(0.0));
}

// Single-layer override (convenience) - must come AFTER full version definition
vec3 evalIridescence(float outsideIOR, float eta2, float cosTheta1,
	float thinFilmThickness, vec3 baseF0)
{
	return evalIridescence(outsideIOR, eta2, cosTheta1, thinFilmThickness, baseF0, vec3(1.0));
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
        vec2 heightUv = applyTextureTransform(uv, uHeightScale, uHeightOffset, uHeightRotation);
        uv = parallaxUV(heightUv, V, Ngeom, vTangentW);
        // Optional: keep inside 0..1 (useful for preview, avoids wrap artifacts)
        uv = clamp(uv, vec2(0.0), vec2(1.0));
    }

    // ---- Texture debug views (no lighting). 0 = All (normal shading) ----
    if (uTexViewMode != 0) {
        vec2 uv = vTexCoord * uUVScale;

        vec3 outCol = vec3(0.0);
        if (uTexViewMode == 1) {                     // Albedo
            vec3 base = uUseAlbedoMap ? texture(uAlbedoMap, applyTextureTransform(uv, uAlbedoScale, uAlbedoOffset, uAlbedoRotation)).rgb : uAlbedo;
            outCol = clamp(base, 0.0, 1.0);
        }
        else if (uTexViewMode == 2) {                // Metalness
            //float m = uUseMetalnessMap ? texture(uMetalnessMap, uv).r : uMetalness;
            vec4 metalTex = texture(uMetalnessMap, applyTextureTransform(uv, uMetalnessScale, uMetalnessOffset, uMetalnessRotation));
            float m = uUseMetalnessMap ? pickChannel(metalTex, uMetalnessChannel, uMetalnessChannelInvert, uMetalnessChannelScale, uMetalnessChannelBias) : uMetalness;
            outCol = vec3(m);
        }
        else if (uTexViewMode == 3) {                // Roughness
            //float r = uUseRoughnessMap ? texture(uRoughnessMap, uv).r : uRoughness;
            vec4 roughTex = texture(uRoughnessMap, applyTextureTransform(uv, uRoughnessScale, uRoughnessOffset, uRoughnessRotation));
            float r = uUseRoughnessMap ? pickChannel(roughTex, uRoughnessChannel, uRoughnessChannelInvert, uRoughnessChannelScale, uRoughnessChannelBias) : uRoughness;
            outCol = vec3(r);
        }
        else if (uTexViewMode == 4) {                // Normal (tangent space)
            vec3 nTS = uUseNormalMap ? (texture(uNormalMap, applyTextureTransform(uv, uNormalScale, uNormalOffset, uNormalRotation)).xyz * 2.0 - 1.0) : vec3(0,0,1);
            // visualize tangent normals mapped to 0..1
            outCol = nTS * 0.5 + 0.5;
        }
        else if (uTexViewMode == 5) {                // Ambient Occlusion
            //float ao = uUseAOMap ? texture(uAOMap, uv).r : 1.0;
            vec4 aoTex    = texture(uAOMap, applyTextureTransform(uv, uAOScale, uAOOffset, uAORotation));
            float ao    = uUseAOMap ? pickChannel(aoTex, uAOChannel, uAOChannelInvert, uAOChannelScale, uAOChannelBias) : 1.0;
            outCol = vec3(ao);
        }
        else if (uTexViewMode == 6) {                // Height
            float h = uUseHeightMap ? texture(uHeightMap, applyTextureTransform(uv, uHeightScale, uHeightOffset, uHeightRotation)).r : 0.5;
            outCol = vec3(h);
        }
        else if (uTexViewMode == 7) {                // Opacity
            float a = uUseOpacityMap ? texture(uOpacityMap, applyTextureTransform(uv, uOpacityScale, uOpacityOffset, uOpacityRotation)).r : uOpacity;
            outCol = vec3(a);
        }
        else if (uTexViewMode == 8) {                // Emissive (as color)
            vec3 e = uUseEmissiveMap ? texture(uEmissiveMap, applyTextureTransform(uv, uEmissiveScale, uEmissiveOffset, uEmissiveRotation)).rgb : vec3(0.0);
            outCol = e * uEmissiveColor * uEmissiveStrength;
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
    vec3  albedo = uUseAlbedoMap ? texture(uAlbedoMap, applyTextureTransform(uv, uAlbedoScale, uAlbedoOffset, uAlbedoRotation)).rgb : uAlbedo;
    albedo = clamp(albedo, vec3(0.01), vec3(1.0));

    vec4 metalTex = texture(uMetalnessMap, applyTextureTransform(uv, uMetalnessScale, uMetalnessOffset, uMetalnessRotation));
    vec4 roughTex = texture(uRoughnessMap, applyTextureTransform(uv, uRoughnessScale, uRoughnessOffset, uRoughnessRotation));
    vec4 aoTex    = texture(uAOMap, applyTextureTransform(uv, uAOScale, uAOOffset, uAORotation));
    vec4 opTex    = texture(uOpacityMap, applyTextureTransform(uv, uOpacityScale, uOpacityOffset, uOpacityRotation));

    float metalness = uUseMetalnessMap ? pickChannel(metalTex, uMetalnessChannel, uMetalnessChannelInvert, uMetalnessChannelScale, uMetalnessChannelBias) : uMetalness;
    float roughness = uUseRoughnessMap ? pickChannel(roughTex, uRoughnessChannel, uRoughnessChannelInvert, uRoughnessChannelScale, uRoughnessChannelBias) : uRoughness;
    float ao    = uUseAOMap ? pickChannel(aoTex, uAOChannel, uAOChannelInvert, uAOChannelScale, uAOChannelBias) : 1.0;
    float opacity = uUseOpacityMap ? pickChannel(opTex, uOpacityChannel, uOpacityChannelInvert, uOpacityChannelScale, uOpacityChannelBias) : uOpacity;

    vec3 emissive = vec3(0.0);
    if (uUseEmissiveMap) {
        emissive = texture(uEmissiveMap, applyTextureTransform(uv, uEmissiveScale, uEmissiveOffset, uEmissiveRotation)).rgb * uEmissiveColor * uEmissiveStrength;
    } else {
        emissive = uEmissiveColor * uEmissiveStrength;
    }

    // ----- Normal mapping uses parallaxed UVs -----
    vec3 N = getNormalFromMap(applyTextureTransform(uv, uNormalScale, uNormalOffset, uNormalRotation));

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

    // ----- Iridescence (thin-film interference) -----
    if (uIridescence > 0.001)
    {
        float NdotV = max(dot(N, V), 0.001);
        
        // Interpolate thickness between min and max based on viewing angle
        // Grazing angles (low NdotV) use thicker film regions (more interference)
        float thickness = mix(uIridescenceThicknessMin, uIridescenceThicknessMax, NdotV);
        
        // Calculate baseF0 from material metalness and albedo
        // Dielectrics use 0.04, metals use albedo color
        vec3 baseF0 = mix(vec3(0.04), albedo, metalness);
        
        // Evaluate physically-based iridescent Fresnel
        // This follows the KHR_materials_iridescence specification
        vec3 iridFresnel = evalIridescence(
            1.0,                        // outsideIOR (air)
            uIridescenceIOR,            // eta2 (iridescent film IOR)
            NdotV,                      // cosTheta1 (view angle)
            thickness,                  // thinFilmThickness (actual nanometers)
            baseF0                      // baseF0 (dielectric F0 or metallic albedo)
        );
        
       // Blend iridescence: stronger at grazing angles, subtle at face-on
        float iridIntensity = uIridescence * (1.0 - NdotV);
        
        // Extract the brightness of what we have
        float lum = dot(color, vec3(0.299, 0.587, 0.114));

        // Get the pure iridescent hue and apply it with the original brightness
        vec3 iridColor = normalize(iridFresnel + vec3(0.001)) * max(lum, 0.3);

        // Replace the color with the iridescence-tinted version
        color = mix(color, iridColor * 1.0, iridIntensity * 1.0);
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
