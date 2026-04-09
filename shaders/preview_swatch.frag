#version 450 core
#define MAX_LIGHTS 8

// ----- Varyings from vertex shader -----
in vec3 vNormal;        // world-space normal (geom)
in vec3 vPos;           // world-space position
in vec2 vTexCoord;      // base UV
in vec4 vTangentW;      // world-space tangent (xyz) + handedness (w)

out vec4 FragColor;

// ----- Camera -----
uniform vec3 camPos;

// Preview profile
uniform int  previewProfile; // 0 = TextureAuthoring, 1 = MaterialShowcase

// 0=All, 1=Albedo, 2=Metalness, 3=Roughness, 4=Normal, 5=AO, 6=Height, 7=Opacity, 8=Emissive
uniform int texViewMode;

// ----- Lights -----
struct Light { vec3 position; vec3 color; };   // you use these as direction-like
uniform Light lights[MAX_LIGHTS];
uniform int   numLights;

// Environment mode (set from C++): 0=Studio, 1=Outdoor, 2=Office
uniform int  envMode;

// Hemisphere colors (set per mode)
uniform vec3 skyColor;    // color from +Y
uniform vec3 groundColor; // color from -Y

// Intensity multipliers
uniform float envDiffuseIntensity;  // e.g. 0.2
uniform float envSpecularIntensity; // keep for future (IBL), 0.0 for now

uniform float exposureEV;   // in EV stops, e.g. [-4 .. +4], default 0
uniform bool  useACES;      // optional: filmic tonemap toggle (true recommended)

// Optional: if you want a separate gamma control (or assume sRGB=2.2)
uniform float gamma;        // default 2.2

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
uniform vec3  albedo;
uniform float metalness;
uniform float roughness;
uniform float opacity;
uniform float clearcoat;
uniform float clearcoatRoughness;
uniform vec3  sheenColor;
uniform float sheenRoughness;
uniform float transmission;
uniform float IOR;
uniform float specular;

// ----- KHR_materials_iridescence -----
uniform float iridescence;
uniform float iridescenceIOR;
uniform float iridescenceThicknessMin;
uniform float iridescenceThicknessMax;

// ----- Texture samplers -----
uniform sampler2D albedoMap;
uniform sampler2D metalnessMap;
uniform sampler2D roughnessMap;
uniform sampler2D normalMap;
uniform sampler2D opacityMap;
uniform sampler2D AOMap;
uniform sampler2D emissiveMap;
uniform sampler2D heightMap;
uniform sampler2D sheenColorMap;
uniform sampler2D sheenRoughnessMap;
uniform sampler2D clearcoatColorMap;
uniform sampler2D clearcoatRoughnessMap;
uniform sampler2D clearcoatNormalMap;

// ----- Enable flags -----
uniform bool useAlbedoMap;
uniform bool useMetalnessMap;
uniform bool useRoughnessMap;
uniform bool useNormalMap;
uniform bool useOpacityMap;
uniform bool opacityInverted;
uniform bool useAOMap;
uniform bool useEmissiveMap;
uniform bool useHeightMap;
uniform bool useSheenColorMap;
uniform bool useSheenRoughnessMap;
uniform bool useClearcoatColorMap;
uniform bool useClearcoatRoughnessMap;
uniform bool useClearcoatNormalMap;

// ----- Extra controls -----
uniform float AOIntensity;
uniform vec3  emissiveColor;
uniform float emissiveStrength;
uniform vec2  UVScale;
uniform float normalIntensity;
uniform float heightIntensity; // (e.g. 0.03 .. 0.06)
uniform float clearcoatNormalIntensity;

// ----- Texture transforms (scale, offset, rotation) -----
uniform vec2  albedoScale;
uniform vec2  albedoOffset;
uniform float albedoRotation;

uniform vec2  normalScale;
uniform vec2  normalOffset;
uniform float normalRotation;

uniform vec2  metalnessScale;
uniform vec2  metalnessOffset;
uniform float metalnessRotation;

uniform vec2  roughnessScale;
uniform vec2  roughnessOffset;
uniform float roughnessRotation;

uniform vec2  AOScale;
uniform vec2  AOOffset;
uniform float AORotation;

uniform vec2  heightScale;
uniform vec2  heightOffset;
uniform float heightRotation;

uniform vec2  opacityScale;
uniform vec2  opacityOffset;
uniform float opacityRotation;

uniform vec2  emissiveScale;
uniform vec2  emissiveOffset;
uniform float emissiveRotation;

uniform vec2  sheenColorScale;
uniform vec2  sheenColorOffset;
uniform float sheenColorRotation;

uniform vec2  sheenRoughnessScale;
uniform vec2  sheenRoughnessOffset;
uniform float sheenRoughnessRotation;

uniform vec2  clearcoatColorScale;
uniform vec2  clearcoatColorOffset;
uniform float clearcoatColorRotation;

uniform vec2  clearcoatRoughnessScale;
uniform vec2  clearcoatRoughnessOffset;
uniform float clearcoatRoughnessRotation;

uniform vec2  clearcoatNormalScale;
uniform vec2  clearcoatNormalOffset;
uniform float clearcoatNormalRotation;

// channel packing uniforms (for packed textures like ORM/AORM)
uniform int   metalnessChannel;
uniform int   metalnessChannelInvert;
uniform float metalnessChannelScale;
uniform float metalnessChannelBias;

uniform int   roughnessChannel;
uniform int   roughnessChannelInvert;
uniform float roughnessChannelScale;
uniform float roughnessChannelBias;

uniform int   AOChannel;
uniform int   AOChannelInvert;
uniform float AOChannelScale;
uniform float AOChannelBias;

uniform int   opacityChannel;
uniform int   opacityChannelInvert;
uniform float opacityChannelScale;
uniform float opacityChannelBias;


// ---------- Helpers ----------

// TBN-based normal mapping (tangent-space normal -> world space)
vec3 getNormalFromMapSampler(sampler2D map, vec2 uv, float intensity)
{
    // Sample tangent-space normal in [-1,1]
    vec3 nTS = texture(map, uv).xyz * 2.0 - 1.0;
    nTS.xy *= intensity;

    // Build TBN from vertex outputs
    vec3 N = normalize(vNormal);
    vec3 T = normalize(vTangentW.xyz);
    vec3 B = normalize(cross(N, T)) * vTangentW.w; // handedness in .w
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * nTS);
}

vec3 getNormalFromMap(vec2 uv)
{
    if (!useNormalMap) return normalize(vNormal);
    return getNormalFromMapSampler(normalMap, uv, normalIntensity);
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
    float h = texture(heightMap, applyTextureTransform(uv, heightScale, heightOffset, heightRotation)).r;
    float height = (h - 0.5) * heightIntensity;

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
    vec3 V     = normalize(camPos - vPos);  // view (camera -> fragment)
    if (length(V) < 0.1) V = vec3(0,0,1);    // safety (shouldn't trigger in your preview)

    // Base UV (tiling)
    vec2 uv = vTexCoord * UVScale;

    // ----- Parallax: adjust UVs BEFORE sampling any map -----
    if (useHeightMap) {
        vec2 heightUv = applyTextureTransform(uv, heightScale, heightOffset, heightRotation);
        uv = parallaxUV(heightUv, V, Ngeom, vTangentW);
        // Optional: keep inside 0..1 (useful for preview, avoids wrap artifacts)
        uv = clamp(uv, vec2(0.0), vec2(1.0));
    }

    // ---- Texture debug views (no lighting). 0 = All (normal shading) ----
    if (texViewMode != 0) {
        vec2 uv = vTexCoord * UVScale;

        vec3 outCol = vec3(0.0);
        if (texViewMode == 1) {                     // Albedo
            vec3 base = useAlbedoMap ? texture(albedoMap, applyTextureTransform(uv, albedoScale, albedoOffset, albedoRotation)).rgb : albedo;
            outCol = clamp(base, 0.0, 1.0);
        }
        else if (texViewMode == 2) {                // Metalness
            //float m = useMetalnessMap ? texture(metalnessMap, uv).r : metalness;
            vec4 metalTex = texture(metalnessMap, applyTextureTransform(uv, metalnessScale, metalnessOffset, metalnessRotation));
            float m = useMetalnessMap ? pickChannel(metalTex, metalnessChannel, metalnessChannelInvert, metalnessChannelScale, metalnessChannelBias) : metalness;
            outCol = vec3(m);
        }
        else if (texViewMode == 3) {                // Roughness
            //float r = useRoughnessMap ? texture(roughnessMap, uv).r : roughness;
            vec4 roughTex = texture(roughnessMap, applyTextureTransform(uv, roughnessScale, roughnessOffset, roughnessRotation));
            float r = useRoughnessMap ? pickChannel(roughTex, roughnessChannel, roughnessChannelInvert, roughnessChannelScale, roughnessChannelBias) : roughness;
            outCol = vec3(r);
        }
        else if (texViewMode == 4) {                // Normal (tangent space)
            vec3 nTS = useNormalMap ? (texture(normalMap, applyTextureTransform(uv, normalScale, normalOffset, normalRotation)).xyz * 2.0 - 1.0) : vec3(0,0,1);
            // visualize tangent normals mapped to 0..1
            outCol = nTS * 0.5 + 0.5;
        }
        else if (texViewMode == 5) {                // Ambient Occlusion
            //float ao = useAOMap ? texture(AOMap, uv).r : 1.0;
            vec4 aoTex    = texture(AOMap, applyTextureTransform(uv, AOScale, AOOffset, AORotation));
            float ao    = useAOMap ? pickChannel(aoTex, AOChannel, AOChannelInvert, AOChannelScale, AOChannelBias) : 1.0;
            outCol = vec3(ao);
        }
        else if (texViewMode == 6) {                // Height
            float h = useHeightMap ? texture(heightMap, applyTextureTransform(uv, heightScale, heightOffset, heightRotation)).r : 0.5;
            outCol = vec3(h);
        }
        else if (texViewMode == 7) {                // Opacity
            float a = useOpacityMap ? texture(opacityMap, applyTextureTransform(uv, opacityScale, opacityOffset, opacityRotation)).r : opacity;
            outCol = vec3(a);
        }
        else if (texViewMode == 8) {                // Emissive (as color)
            vec3 e = useEmissiveMap ? texture(emissiveMap, applyTextureTransform(uv, emissiveScale, emissiveOffset, emissiveRotation)).rgb : vec3(0.0);
            outCol = e * emissiveColor * emissiveStrength;
        }

        // Apply exposure / tonemap for consistent preview look
        float exposure = exp2(exposureEV);
        outCol *= exposure;
        if (useACES) {
            const float a=2.51, b=0.03, c=2.43, d=0.59, e=0.14;
            outCol = clamp((outCol*(a*outCol + b)) / (outCol*(c*outCol + d) + e), 0.0, 1.0);
        } else {
            outCol = outCol / (1.0 + outCol);
        }
        float g = (gamma > 0.0) ? gamma : 2.2;
        outCol = pow(outCol, vec3(1.0 / g));

        FragColor = vec4(outCol, 1.0);
        return;
    }


    // ----- Sample or fallback using parallaxed UVs -----
    vec3  albedoVal = useAlbedoMap ? texture(albedoMap, applyTextureTransform(uv, albedoScale, albedoOffset, albedoRotation)).rgb : albedo;
    albedoVal = clamp(albedoVal, vec3(0.01), vec3(1.0));

    vec4 metalTex = texture(metalnessMap, applyTextureTransform(uv, metalnessScale, metalnessOffset, metalnessRotation));
    vec4 roughTex = texture(roughnessMap, applyTextureTransform(uv, roughnessScale, roughnessOffset, roughnessRotation));
    vec4 aoTex    = texture(AOMap, applyTextureTransform(uv, AOScale, AOOffset, AORotation));
    vec4 opTex    = texture(opacityMap, applyTextureTransform(uv, opacityScale, opacityOffset, opacityRotation));

    float metalnessVal = useMetalnessMap ? pickChannel(metalTex, metalnessChannel, metalnessChannelInvert, metalnessChannelScale, metalnessChannelBias) : metalness;
    float roughnessVal = useRoughnessMap ? pickChannel(roughTex, roughnessChannel, roughnessChannelInvert, roughnessChannelScale, roughnessChannelBias) : roughness;
    float ao    = useAOMap ? pickChannel(aoTex, AOChannel, AOChannelInvert, AOChannelScale, AOChannelBias) : 1.0;
    float opacityVal = useOpacityMap ? pickChannel(opTex, opacityChannel, opacityChannelInvert, opacityChannelScale, opacityChannelBias) : opacity;

    vec3 emissive = vec3(0.0);
    if (useEmissiveMap) {
        emissive = texture(emissiveMap, applyTextureTransform(uv, emissiveScale, emissiveOffset, emissiveRotation)).rgb * emissiveColor * emissiveStrength;
    } else {
        emissive = emissiveColor * emissiveStrength;
    }

    // ----- Normal mapping uses parallaxed UVs -----
    vec3 N = getNormalFromMap(applyTextureTransform(uv, normalScale, normalOffset, normalRotation));

    float clearcoatVal = useClearcoatColorMap
        ? texture(clearcoatColorMap, applyTextureTransform(uv, clearcoatColorScale, clearcoatColorOffset, clearcoatColorRotation)).r * clearcoat
        : clearcoat;
    clearcoatVal = clamp(clearcoatVal, 0.0, 1.0);

    float clearcoatRoughnessVal = useClearcoatRoughnessMap
        ? texture(clearcoatRoughnessMap, applyTextureTransform(uv, clearcoatRoughnessScale, clearcoatRoughnessOffset, clearcoatRoughnessRotation)).g * clearcoatRoughness
        : clearcoatRoughness;
    clearcoatRoughnessVal = clamp(clearcoatRoughnessVal, 0.0001, 1.0);

    vec3 clearcoatN = useClearcoatNormalMap
        ? getNormalFromMapSampler(
            clearcoatNormalMap,
            applyTextureTransform(uv, clearcoatNormalScale, clearcoatNormalOffset, clearcoatNormalRotation),
            clearcoatNormalIntensity)
        : N;

    vec3 sheenColorVal = useSheenColorMap
        ? texture(sheenColorMap, applyTextureTransform(uv, sheenColorScale, sheenColorOffset, sheenColorRotation)).rgb * sheenColor
        : sheenColor;

    float sheenRoughnessVal = useSheenRoughnessMap
        ? texture(sheenRoughnessMap, applyTextureTransform(uv, sheenRoughnessScale, sheenRoughnessOffset, sheenRoughnessRotation)).a * sheenRoughness
        : sheenRoughness;
    sheenRoughnessVal = clamp(sheenRoughnessVal, 0.0, 1.0);

    // ----- Metal vs dielectric split -----
    vec3 dielectricDiffuse  = albedoVal;
    vec3 dielectricSpecular = vec3(0.04);
    vec3 metallicDiffuse    = vec3(0.0);
    vec3 metallicSpecular   = albedoVal;

    vec3 diffuseColor  = mix(dielectricDiffuse,  metallicDiffuse,  metalnessVal);
    vec3 specularColor = mix(dielectricSpecular, metallicSpecular, metalnessVal);

    // ----- Direct lighting -----
    vec3 color = vec3(0.075) * albedoVal; // small base light

    for (int i = 0; i < numLights; ++i)
    {
        // Your rig: treat .position as an incoming direction
        vec3 L = normalize(lights[i].position);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL <= 0.0) continue;

        vec3 H = normalize(V + L);
        float NdotH = max(dot(N, H), 0.0);

        float specPow    = clamp(mix(8.0, 128.0, 1.0 - roughnessVal), 1.0, 256.0);
        float specFactor = pow(NdotH, specPow);

        vec3 diffuse = diffuseColor * NdotL;
        vec3 spec    = specularColor * specFactor;

        color += (diffuse + spec) * lights[i].color * NdotL;
    }

    // --- Hemisphere environment ambient (AO only on diffuse) ---
    vec3 hemi = hemisphereAmbient(normalize(N), skyColor, groundColor);
    vec3 diffuseAmbient = albedoVal * hemi * envDiffuseIntensity * ao;
    // (specular IBL can be added later; keep envSpecularIntensity for future)
    color += diffuseAmbient;

    // ----- Clearcoat (simple lobe using light 0 direction-like) -----
    if (clearcoatVal > 0.001 && numLights > 0)
    {
        vec3 Lc = normalize(lights[0].position);
        vec3 Hc = normalize(V + Lc);
        float ccNdotH   = max(dot(clearcoatN, Hc), 0.0);
        float ccSpecPow = clamp(mix(64.0, 512.0, 1.0 - clearcoatRoughnessVal), 1.0, 1024.0);
        float ccSpec    = pow(ccNdotH, ccSpecPow);
        color += vec3(0.25) * clearcoatVal * ccSpec;
    }

    // ----- Sheen -----
    if (length(sheenColorVal) > 0.001)
    {
        float NdotV = max(dot(N, V), 0.0);
        float sheen = pow(clamp(1.0 - NdotV, 0.0, 1.0), mix(3.0, 1.0, 1.0 - sheenRoughnessVal));
        color += sheenColorVal * sheen * 0.5;
    }

    // ----- Iridescence (thin-film interference) -----
    if (iridescence > 0.001)
    {
        float NdotV = max(dot(N, V), 0.001);

        // Interpolate thickness between min and max based on viewing angle
        // Grazing angles (low NdotV) use thicker film regions (more interference)
        float thickness = mix(iridescenceThicknessMin, iridescenceThicknessMax, NdotV);

        // Calculate baseF0 from material metalness and albedo
        // Dielectrics use 0.04, metals use albedo color
        vec3 baseF0 = mix(vec3(0.04), albedoVal, metalnessVal);

        // Evaluate physically-based iridescent Fresnel
        // This follows the KHR_materials_iridescence specification
        vec3 iridFresnel = evalIridescence(
            1.0,                        // outsideIOR (air)
            iridescenceIOR,             // eta2 (iridescent film IOR)
            NdotV,                      // cosTheta1 (view angle)
            thickness,                  // thinFilmThickness (actual nanometers)
            baseF0                      // baseF0 (dielectric F0 or metallic albedo)
        );

       // Blend iridescence: stronger at grazing angles, subtle at face-on
        float iridIntensity = iridescence * (1.0 - NdotV);

        // Extract the brightness of what we have
        float lum = dot(color, vec3(0.299, 0.587, 0.114));

        // Get the pure iridescent hue and apply it with the original brightness
        vec3 iridColor = normalize(iridFresnel + vec3(0.001)) * max(lum, 0.3);

        // Replace the color with the iridescence-tinted version
        color = mix(color, iridColor * 1.0, iridIntensity * 1.0);
    }

    // ----- Transmission (simple tint) -----
    if (transmission > 0.001)
    {
        vec3 glassTint = mix(albedoVal, vec3(1.0), clamp((IOR - 1.0) * 0.3, 0.0, 1.0));
        color = mix(color, glassTint, transmission);
    }

    // ----- Rim -----
    float rim = pow(clamp(1.0 - max(dot(N, V), 0.0), 0.0, 1.0), 2.0);
    color += albedoVal * rim * 0.2;

    // ----- Emissive -----
    color += emissive;


    if(previewProfile == 1) {
        // ----- Brightness tweak -----
        color *= mix(1.1, 2.5, metalnessVal);

        // ----- Final clamp -----
        color = clamp(color, vec3(0.0), vec3(20.0));
    } else {
        // --- Exposure (in EV) ---
        float exposure = exp2(exposureEV);   // 2^EV
        color *= exposure;

        // --- Tonemap ---
        if (useACES) {
            color = acesTonemap(color);
        } else {
            // Reinhard as a fallback (cheap)
            color = color / (1.0 + color);
        }

        // --- Gamma encode to sRGB (if your default framebuffer isn't sRGB)
        float g = (gamma > 0.0) ? gamma : 2.2;
        color = pow(color, vec3(1.0 / g));
    }

    FragColor = vec4(color, opacityVal);
}
