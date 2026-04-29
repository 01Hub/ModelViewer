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
// 9=ClearcoatColor, 10=ClearcoatRoughness, 11=ClearcoatNormal, 12=SheenColor, 13=SheenRoughness
// 14=Transmission, 15=IOR, 16=Thickness (KHR_materials_volume), 17=SpecularFactor, 18=SpecularColor
// 19=Anisotropy, 20=Iridescence, 21=IridescenceThickness, 22=DiffuseTransmission, 23=DiffuseTransmissionColor
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

// Environment tone mapping and gamma settings (matches main_scene.frag)
uniform bool  hdrToneMapping = false;
uniform bool  gammaCorrection = false;
uniform float screenGamma = 2.2;
uniform int   toneMapMode = 0;  // 0=KhronosPbrNeutral (default)

// ACES tone mapping matrices (matches main_scene.frag)
const mat3 ACESInputMat = mat3
(
	0.59719, 0.07600, 0.02840,
	0.35458, 0.90834, 0.13383,
	0.04823, 0.01566, 0.83777
);

const mat3 ACESOutputMat = mat3
(
	1.60475, -0.10208, -0.00327,
	-0.53108, 1.10813, -0.07276,
	-0.07367, -0.00605, 1.07602
);

// ACES tone map (faster approximation)
// see: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 toneMapACES_Narkowicz(vec3 color)
{
	const float A = 2.51;
	const float B = 0.03;
	const float C = 2.43;
	const float D = 0.59;
	const float E = 0.14;
	return clamp((color * (A * color + B)) / (color * (C * color + D) + E), 0.0, 1.0);
}

// ACES filmic tone map approximation helper
vec3 RRTAndODTFit(vec3 color)
{
	vec3 a = color * (color + 0.0245786) - 0.000090537;
	vec3 b = color * (0.983729 * color + 0.4329510) + 0.238081;
	return a / b;
}

// ACES Hill tone map
vec3 toneMapACES_Hill(vec3 color)
{
	color = ACESInputMat * color;
	color = RRTAndODTFit(color);
	color = ACESOutputMat * color;
	return clamp(color, 0.0, 1.0);
}

// Khronos PBR neutral tone mapping (matches main_scene.frag)
vec3 toneMap_KhronosPbrNeutral(vec3 color)
{
	const float startCompression = 0.8 - 0.04;
	const float desaturation = 0.15;

	float x = min(color.r, min(color.g, color.b));
	float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
	color -= offset;

	float peak = max(color.r, max(color.g, color.b));
	if (peak < startCompression) return color;

	const float d = 1. - startCompression;
	float newPeak = 1. - d * d / (peak + d - startCompression);
	color *= newPeak / peak;

	float g = 1. - 1. / (desaturation * (peak - newPeak) + 1.);
	return mix(color, newPeak * vec3(1, 1, 1), g);
}

// Uncharted 2 tone mapping
vec3 uncharted2ToneMapping(vec3 color)
{
	const float A = 0.15;
	const float B = 0.50;
	const float C = 0.10;
	const float D = 0.20;
	const float E = 0.02;
	const float F = 0.30;
	const float W = 11.2;

	color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
	float white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
	return color / white;
}

// Simple Reinhard tone mapping
vec3 reinhardToneMapping(vec3 color)
{
	return color / (1.0 + color);
}

// Apply tone mapping based on mode (0=KhronosPbrNeutral, 1=ACES_Narkowicz, 2=ACES_Hill, 3=AECS_Hill_Exposure_Boost, 4=Uncharted2, 5=Reinhard)
vec3 applyToneMapping(vec3 color)
{
	if (!hdrToneMapping) return color;

	if (toneMapMode == 0)
	{
		color = toneMap_KhronosPbrNeutral(color);
	}
	else if (toneMapMode == 1)
	{
		color = toneMapACES_Narkowicz(color);
	}
	else if (toneMapMode == 2)
	{
		color = toneMapACES_Hill(color);
	}
	else if (toneMapMode == 3)
	{
		// Boost exposure as discussed in https://github.com/mrdoob/three.js/pull/19621
		color /= 0.6;
		color = toneMapACES_Hill(color);
	}
	else if (toneMapMode == 4)
	{
		color = uncharted2ToneMapping(color);
	}
	else if (toneMapMode == 5)
	{
		color = reinhardToneMapping(color);
	}

	return color;
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
uniform int   renderingMode; // 0 = ADS Blinn-Phong, 1 = PBR
uniform bool  envMapEnabled;
uniform bool  useIBL;
uniform bool  unlitMaterial;
uniform vec3  adsAmbient;
uniform vec3  adsDiffuse;
uniform vec3  adsSpecular;
uniform float adsShininess;
uniform float specularFactor;
uniform vec3  specularColorFactor;
uniform float anisotropyStrength;
uniform float anisotropyRotationFactor;
uniform float occlusionStrength;
uniform float diffuseTransmissionFactor;
uniform vec3  diffuseTransmissionColorFactor;

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
uniform sampler2D transmissionMap;
uniform sampler2D iorMap;
uniform sampler2D specularFactorMap;
uniform sampler2D specularColorMap;
uniform sampler2D anisotropyMap;
uniform sampler2D iridescenceMap;
uniform sampler2D iridescenceThicknessMap;
uniform sampler2D diffuseTransmissionMap;
uniform sampler2D diffuseTransmissionColorMap;
uniform sampler2D thicknessMap;

// ----- Environment mapping (IBL) -----
uniform samplerCube envCubemap;
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;  // Prefiltered environment map with LOD miplevels
uniform sampler2D brdfLUT;  // BRDF lookup table from main viewer
uniform mat3 previewRotationMatrix;  // Rotation matrix from preview object rotation
uniform float envMapExposure;  // Exposure adjustment for environment maps

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
uniform bool useTransmissionMap;
uniform bool useIorMap;
uniform bool useSpecularFactorMap;
uniform bool useSpecularColorMap;
uniform bool useAnisotropyMap;
uniform bool useIridescenceMap;
uniform bool useIridescenceThicknessMap;
uniform bool useDiffuseTransmissionMap;
uniform bool useDiffuseTransmissionColorMap;
uniform bool useThicknessMap;

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

uniform vec2  transmissionScale;
uniform vec2  transmissionOffset;
uniform float transmissionRotation;

uniform vec2  iorScale;
uniform vec2  iorOffset;
uniform float iorRotation;

uniform vec2  specularFactorScale;
uniform vec2  specularFactorOffset;
uniform float specularFactorRotation;

uniform vec2  specularColorScale;
uniform vec2  specularColorOffset;
uniform float specularColorRotation;

uniform vec2  anisotropyScale;
uniform vec2  anisotropyOffset;
uniform float anisotropyRotation;

uniform vec2  iridescenceScale;
uniform vec2  iridescenceOffset;
uniform float iridescenceRotation;

uniform vec2  iridescenceThicknessScale;
uniform vec2  iridescenceThicknessOffset;
uniform float iridescenceThicknessRotation;

uniform vec2  diffuseTransmissionScale;
uniform vec2  diffuseTransmissionOffset;
uniform float diffuseTransmissionRotation;

uniform vec2  diffuseTransmissionColorScale;
uniform vec2  diffuseTransmissionColorOffset;
uniform float diffuseTransmissionColorRotation;

uniform vec2  thicknessScale;
uniform vec2  thicknessOffset;
uniform float thicknessRotation;

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
    uv = mat2(c, s, -s, c) * uv;

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

// Fresnel-Schlick with variable F90 (matches main_scene.frag)
vec3 fresnelSchlick(float cosTheta, vec3 F0, vec3 F90)
{
    return F0 + (F90 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

const float PI = 3.14159265359;

float distributionGGX(vec3 N, vec3 H, float perceptualRoughness)
{
    float a = perceptualRoughness * perceptualRoughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / max(PI * denom * denom, 0.001);
}

float geometrySchlickGGX(float NdotV, float perceptualRoughness)
{
    float r = perceptualRoughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / max(NdotV * (1.0 - k) + k, 0.001);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float perceptualRoughness)
{
    return geometrySchlickGGX(max(dot(N, V), 0.0), perceptualRoughness) *
           geometrySchlickGGX(max(dot(N, L), 0.0), perceptualRoughness);
}

vec3 getBitangent(vec3 N, vec3 T, float handedness)
{
    return normalize(cross(N, T)) * handedness;
}

vec3 decodeAnisotropyDirection(vec3 texel, float rotation)
{
    vec2 direction = texel.xy * 2.0 - 1.0;
    float c = cos(rotation);
    float s = sin(rotation);
    return vec3(c * direction.x - s * direction.y, s * direction.x + c * direction.y, texel.z);
}

float distributionGGXAnisotropic(vec3 N, vec3 H, vec3 T, vec3 B, float roughness, float anisotropy)
{
    float aspect = sqrt(max(1.0 - anisotropy * 0.9, 0.1));
    float ax = max(roughness * roughness / aspect, 0.001);
    float ay = max(roughness * roughness * aspect, 0.001);
    float TdotH = dot(T, H) / ax;
    float BdotH = dot(B, H) / ay;
    float NdotH = max(dot(N, H), 0.0);
    float denom = TdotH * TdotH + BdotH * BdotH + NdotH * NdotH;
    return 1.0 / max(PI * ax * ay * denom * denom, 0.001);
}

vec3 calculateSheenLayer(vec3 N, vec3 V, vec3 L, vec3 color, float rough)
{
    vec3 H = normalize(V + L);
    float NoL = max(dot(N, L), 0.0);
    float NoH = max(dot(N, H), 0.0);
    float invR = clamp(1.0 - rough, 0.0, 1.0);
    float sheen = pow(clamp(1.0 - NoH, 0.0, 1.0), mix(2.0, 0.25, invR));
    return color * sheen * NoL;
}

vec3 calculateSheenIBLLayer(vec3 N, vec3 V, vec3 color, float rough)
{
    float NoV = max(dot(N, V), 0.0);
    float grazing = pow(clamp(1.0 - NoV, 0.0, 1.0), mix(5.0, 2.0, rough));
    vec3 Nibl = previewRotationMatrix * N;
    vec3 irradiance = texture(irradianceMap, Nibl).rgb;    
    return irradiance * color * grazing;
}

vec3 calculateClearcoatLayer(vec3 N, vec3 V, vec3 L, float amount, float rough)
{
    vec3 H = normalize(V + L);
    float NoL = max(dot(N, L), 0.0);
    float NoV = max(dot(N, V), 0.0);
    float VoH = max(dot(V, H), 0.0);
    float D = distributionGGX(N, H, rough);
    float G = geometrySmith(N, V, L, rough);
    vec3 F = fresnelSchlick(VoH, vec3(0.04), vec3(1.0));
    return amount * (D * G * F) / max(4.0 * NoV * NoL, 0.001) * NoL;
}

// Specular IBL from prefiltered environment map using BRDF lookup table (matches main_scene.frag)
vec3 computeSpecularIBL(vec3 V, vec3 N, float roughness, vec3 F0, vec3 F90)
{
    // Reflect view direction
    vec3 R = reflect(-V, N);

    // Apply inverse object rotation so environment stays fixed
    R = previewRotationMatrix * R;

    // Fresnel effect: metals at 0 degrees have high reflectivity, dielectrics are low
    float dotNV = max(dot(N, V), 0.0);

    // Fresnel approximation
    vec3 Fibl = fresnelSchlick(dotNV, F0, F90);

    // Sample prefiltered environment map with roughness-based LOD
    // The prefilter map was generated with 90° X-axis rotation (see GLWidget::setIBLFaceBasis)
    // Apply 90° X-axis rotation to match the generated map orientation
    // Rotation matrix for 90° around X: (x, y, z) → (x, -z, y)
    vec3 R_prefilter = vec3(0,0,0);    
    R_prefilter = vec3(R.x, -R.z, R.y);

    // This uses the same approach as main_scene.frag lines 1885-1890
    const float MAX_REFLECTION_LOD = textureQueryLevels(prefilterMap) - 1.0;
    float lod = roughness * MAX_REFLECTION_LOD;
    lod = clamp(lod, 0.0, MAX_REFLECTION_LOD);
    vec3 prefilteredColor = textureLod(prefilterMap, R_prefilter, lod).rgb;
    prefilteredColor = max(prefilteredColor, vec3(0.0));
    prefilteredColor *= envMapExposure;

    // Use BRDF lookup table for accurate specular IBL split-sum approximation
    // LUT maps (dotNV, roughness) -> (scale, bias)
    vec2 brdfCoord = clamp(vec2(dotNV, roughness), vec2(0.0), vec2(1.0));
    vec2 brdf = texture(brdfLUT, brdfCoord).rg;
    brdf = max(brdf, vec2(0.0));

    // Apply BRDF: fresnel * scale + bias (matches main_scene.frag line 1895)
    // This is the complete PBR specular IBL calculation with no additional multipliers
    vec3 specIBL = prefilteredColor * (Fibl * brdf.x + brdf.y);

    return specIBL * envSpecularIntensity;
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
        else if (texViewMode == 9) {                // Clearcoat Color
            vec3 cc = useClearcoatColorMap ? texture(clearcoatColorMap, applyTextureTransform(uv, clearcoatColorScale, clearcoatColorOffset, clearcoatColorRotation)).rgb : vec3(0.0);
            outCol = clamp(cc, 0.0, 1.0);
        }
        else if (texViewMode == 10) {               // Clearcoat Roughness
            float cr = useClearcoatRoughnessMap ? texture(clearcoatRoughnessMap, applyTextureTransform(uv, clearcoatRoughnessScale, clearcoatRoughnessOffset, clearcoatRoughnessRotation)).r : 0.0;
            outCol = vec3(cr);
        }
        else if (texViewMode == 11) {               // Clearcoat Normal
            vec3 cnTS = useClearcoatNormalMap ? (texture(clearcoatNormalMap, applyTextureTransform(uv, clearcoatNormalScale, clearcoatNormalOffset, clearcoatNormalRotation)).xyz * 2.0 - 1.0) : vec3(0,0,1);
            outCol = cnTS * 0.5 + 0.5;
        }
        else if (texViewMode == 12) {               // Sheen Color
            vec3 sc = useSheenColorMap ? texture(sheenColorMap, applyTextureTransform(uv, sheenColorScale, sheenColorOffset, sheenColorRotation)).rgb : sheenColor;
            outCol = clamp(sc, 0.0, 1.0);
        }
        else if (texViewMode == 13) {               // Sheen Roughness
            float sr = useSheenRoughnessMap ? texture(sheenRoughnessMap, applyTextureTransform(uv, sheenRoughnessScale, sheenRoughnessOffset, sheenRoughnessRotation)).r : sheenRoughness;
            outCol = vec3(sr);
        }
        else if (texViewMode == 14) {               // Transmission (KHR_materials_volume)
            float t = useTransmissionMap ? texture(transmissionMap, applyTextureTransform(uv, transmissionScale, transmissionOffset, transmissionRotation)).r : transmission;
            outCol = vec3(t);
        }
        else if (texViewMode == 15) {               // IOR (KHR_materials_volume)
            float ior = useIorMap ? texture(iorMap, applyTextureTransform(uv, iorScale, iorOffset, iorRotation)).r : (IOR / 2.0);
            outCol = vec3(ior);
        }
        else if (texViewMode == 16) {               // Thickness (KHR_materials_volume)
            float thick = useThicknessMap ? texture(thicknessMap, applyTextureTransform(uv, thicknessScale, thicknessOffset, thicknessRotation)).r : 0.0;
            outCol = vec3(thick);
        }
        else if (texViewMode == 17) {               // Specular Factor (KHR_materials_specular)
            float sf = useSpecularFactorMap ? texture(specularFactorMap, applyTextureTransform(uv, specularFactorScale, specularFactorOffset, specularFactorRotation)).r : specular;
            outCol = vec3(sf);
        }
        else if (texViewMode == 18) {               // Specular Color (KHR_materials_specular)
            vec3 spc = useSpecularColorMap ? texture(specularColorMap, applyTextureTransform(uv, specularColorScale, specularColorOffset, specularColorRotation)).rgb : vec3(0.0);
            outCol = clamp(spc, 0.0, 1.0);
        }
        else if (texViewMode == 19) {               // Anisotropy (KHR_materials_anisotropy)
            float aniso = useAnisotropyMap ? texture(anisotropyMap, applyTextureTransform(uv, anisotropyScale, anisotropyOffset, anisotropyRotation)).r : 0.0;
            outCol = vec3(aniso);
        }
        else if (texViewMode == 20) {               // Iridescence (KHR_materials_iridescence)
            float iri = useIridescenceMap ? texture(iridescenceMap, applyTextureTransform(uv, iridescenceScale, iridescenceOffset, iridescenceRotation)).r : iridescence;
            outCol = vec3(iri);
        }
        else if (texViewMode == 21) {               // Iridescence Thickness (KHR_materials_iridescence)
            float iriThick = useIridescenceThicknessMap ? texture(iridescenceThicknessMap, applyTextureTransform(uv, iridescenceThicknessScale, iridescenceThicknessOffset, iridescenceThicknessRotation)).r : 0.5;
            outCol = vec3(iriThick);
        }
        else if (texViewMode == 22) {               // Diffuse Transmission (KHR_materials_diffuse_transmission)
            vec3 diffTrans = useDiffuseTransmissionMap ? texture(diffuseTransmissionMap, applyTextureTransform(uv, diffuseTransmissionScale, diffuseTransmissionOffset, diffuseTransmissionRotation)).rgb : vec3(0.0);
            outCol = clamp(diffTrans, 0.0, 1.0);
        }
        else if (texViewMode == 23) {               // Diffuse Transmission Color (KHR_materials_diffuse_transmission)
            vec3 diffTransCol = useDiffuseTransmissionColorMap ? texture(diffuseTransmissionColorMap, applyTextureTransform(uv, diffuseTransmissionColorScale, diffuseTransmissionColorOffset, diffuseTransmissionColorRotation)).rgb : vec3(0.0);
            outCol = clamp(diffTransCol, 0.0, 1.0);
        }

        // Apply tonemap and gamma (matches main_scene.frag environment settings)
        outCol = applyToneMapping(outCol);
        if (gammaCorrection) {
            outCol = pow(outCol, vec3(1.0 / screenGamma));
        }

        FragColor = vec4(outCol, 1.0);
        return;
    }


    // ----- Sample or fallback using parallaxed UVs -----
    // WYSIWYG: Multiply texture with scalar (if no texture, use vec3(1.0) so only scalar shows)
    vec3  albedoVal = (useAlbedoMap ? texture(albedoMap, applyTextureTransform(uv, albedoScale, albedoOffset, albedoRotation)).rgb : vec3(1.0)) * albedo;
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

    // WYSIWYG: Multiply clearcoat texture with scalar
    float clearcoatVal = (useClearcoatColorMap
        ? texture(clearcoatColorMap, applyTextureTransform(uv, clearcoatColorScale, clearcoatColorOffset, clearcoatColorRotation)).r
        : 1.0) * clearcoat;
    clearcoatVal = clamp(clearcoatVal, 0.0, 1.0);

    // WYSIWYG: Multiply clearcoat roughness texture with scalar
    float clearcoatRoughnessVal = (useClearcoatRoughnessMap
        ? texture(clearcoatRoughnessMap, applyTextureTransform(uv, clearcoatRoughnessScale, clearcoatRoughnessOffset, clearcoatRoughnessRotation)).r
        : 1.0) * clearcoatRoughness;
    clearcoatRoughnessVal = clamp(clearcoatRoughnessVal, 0.0001, 1.0);

    vec3 clearcoatN = useClearcoatNormalMap
        ? getNormalFromMapSampler(
            clearcoatNormalMap,
            applyTextureTransform(uv, clearcoatNormalScale, clearcoatNormalOffset, clearcoatNormalRotation),
            clearcoatNormalIntensity)
        : N;

    // WYSIWYG: Multiply sheen texture with scalar
    vec3 sheenColorVal = (useSheenColorMap
        ? texture(sheenColorMap, applyTextureTransform(uv, sheenColorScale, sheenColorOffset, sheenColorRotation)).rgb
        : vec3(1.0)) * sheenColor;

    float sheenRoughnessVal = useSheenRoughnessMap
        ? texture(sheenRoughnessMap, applyTextureTransform(uv, sheenRoughnessScale, sheenRoughnessOffset, sheenRoughnessRotation)).r * sheenRoughness
        : sheenRoughness;
    sheenRoughnessVal = clamp(sheenRoughnessVal, 0.0, 1.0);

    vec3 N_normalized = normalize(N);
    vec3 V_normalized = normalize(V);
    roughnessVal = clamp(roughnessVal, 0.0001, 1.0);
    metalnessVal = clamp(metalnessVal, 0.0, 1.0);
    ao = mix(1.0, ao, occlusionStrength);

    vec3 color = vec3(0.0);

    if (renderingMode == 0)
    {
        vec3 matAmbient = adsAmbient * albedoVal;
        vec3 matDiffuse = adsDiffuse * albedoVal;
        vec3 matSpecular = adsSpecular;
        color = matAmbient * envDiffuseIntensity + emissive;

        for (int i = 0; i < numLights; ++i)
        {
            vec3 L = normalize(lights[i].position);
            vec3 H = normalize(V_normalized + L);
            float NoL = max(dot(N_normalized, L), 0.0);
            float NoH = max(dot(N_normalized, H), 0.0);
            float pf = pow(NoH, max(adsShininess, 1.0));
            color += (matDiffuse * NoL + matSpecular * pf) * lights[i].color;
        }

        vec3 Nibl = previewRotationMatrix * N_normalized;
        color += albedoVal * texture(irradianceMap, Nibl).rgb * envDiffuseIntensity * ao;
    }
    else
    {
        float iorVal = useIorMap ? texture(iorMap, applyTextureTransform(uv, iorScale, iorOffset, iorRotation)).r : IOR;
        iorVal = max(iorVal, 1.001);

        float specularFactorVal = useSpecularFactorMap
            ? texture(specularFactorMap, applyTextureTransform(uv, specularFactorScale, specularFactorOffset, specularFactorRotation)).a * specularFactor
            : specularFactor;
        vec3 specularColorVal = (useSpecularColorMap
            ? texture(specularColorMap, applyTextureTransform(uv, specularColorScale, specularColorOffset, specularColorRotation)).rgb
            : vec3(1.0)) * specularColorFactor;

        vec3 F0 = vec3(pow((iorVal - 1.0) / (iorVal + 1.0), 2.0));
        F0 = max(F0 * specularColorVal * specularFactorVal, vec3(0.0));
        F0 = mix(F0, albedoVal, metalnessVal);
        vec3 F90 = mix(vec3(specularFactorVal), vec3(1.0), metalnessVal);

        float iridescenceVal = useIridescenceMap
            ? texture(iridescenceMap, applyTextureTransform(uv, iridescenceScale, iridescenceOffset, iridescenceRotation)).r * iridescence
            : iridescence;
        if (iridescenceVal > 0.001)
        {
            float thickness = iridescenceThicknessMax;
            if (useIridescenceThicknessMap)
            {
                float thicknessNorm = texture(iridescenceThicknessMap, applyTextureTransform(uv, iridescenceThicknessScale, iridescenceThicknessOffset, iridescenceThicknessRotation)).g;
                thickness = mix(iridescenceThicknessMin, iridescenceThicknessMax, thicknessNorm);
            }
            float NoV = clamp(dot(N_normalized, V_normalized), 0.0, 1.0);
            vec3 iridDielectric = evalIridescence(1.0, iridescenceIOR, NoV, thickness, F0, vec3(specularFactorVal));
            vec3 iridMetallic = evalIridescence(1.0, iridescenceIOR, NoV, thickness, albedoVal, vec3(1.0));
            vec3 iridF0 = mix(iridDielectric, iridMetallic, metalnessVal);
            F0 = mix(F0, iridF0, iridescenceVal);
            F90 = mix(F90, vec3(1.0), iridescenceVal);
        }

        vec3 T = normalize(vTangentW.xyz - dot(vTangentW.xyz, N_normalized) * N_normalized);
        vec3 B = getBitangent(N_normalized, T, vTangentW.w);
        float anisotropyVal = anisotropyStrength;
        if (useAnisotropyMap)
        {
            vec3 anisoTex = texture(anisotropyMap, applyTextureTransform(uv, anisotropyScale, anisotropyOffset, anisotropyRotation)).rgb;
            vec3 anisoData = decodeAnisotropyDirection(anisoTex, anisotropyRotationFactor);
            anisotropyVal *= anisoTex.b;
            T = normalize(anisoData.x * T + anisoData.y * B);
            B = getBitangent(N_normalized, T, vTangentW.w);
        }

        vec3 directDiffuse = vec3(0.0);
        vec3 directSpecular = vec3(0.0);
        vec3 directSheen = vec3(0.0);
        vec3 directClearcoat = vec3(0.0);
        float clearcoatAttenuation = 0.0;

        for (int i = 0; i < numLights; ++i)
        {
            vec3 L = normalize(lights[i].position);
            vec3 H = normalize(V_normalized + L);
            float NoL = max(dot(N_normalized, L), 0.0);
            float NoV = max(dot(N_normalized, V_normalized), 0.0);
            float VoH = max(dot(V_normalized, H), 0.0);
            if (NoL <= 0.0) continue;

            vec3 F = fresnelSchlick(VoH, F0, F90);
            vec3 kD = (vec3(1.0) - F) * (1.0 - metalnessVal);
            float D = anisotropyVal > 0.001
                ? distributionGGXAnisotropic(N_normalized, H, T, B, roughnessVal, anisotropyVal)
                : distributionGGX(N_normalized, H, roughnessVal);
            float G = geometrySmith(N_normalized, V_normalized, L, roughnessVal);
            vec3 specBRDF = (D * G * F) / max(4.0 * NoV * NoL, 0.001);

            directDiffuse += kD * albedoVal / PI * lights[i].color * NoL;
            directSpecular += specBRDF * lights[i].color * NoL;
            directSheen += calculateSheenLayer(N_normalized, V_normalized, L, sheenColorVal, sheenRoughnessVal) * lights[i].color;
            directClearcoat += calculateClearcoatLayer(clearcoatN, V_normalized, L, clearcoatVal, clearcoatRoughnessVal) * lights[i].color;
        }

        vec3 ambient = vec3(0.0);
        if (useIBL)
        {
            vec3 Nibl = previewRotationMatrix * N_normalized;
            vec3 irradiance = texture(irradianceMap, Nibl).rgb;
            float dotNV = max(dot(N_normalized, V_normalized), 0.0);
            vec3 Fibl = fresnelSchlick(dotNV, F0, max(vec3(1.0 - roughnessVal), F0));
            vec3 kDibl = (vec3(1.0) - Fibl) * (1.0 - metalnessVal);
            vec3 diffuseIBL = irradiance * albedoVal;
            vec3 specularIBL = envMapEnabled ? computeSpecularIBL(V_normalized, N_normalized, roughnessVal, F0, max(vec3(1.0 - roughnessVal), F0)) : vec3(0.0);
            ambient = (kDibl * diffuseIBL * envDiffuseIntensity + specularIBL) * ao;
            ambient += calculateSheenIBLLayer(N_normalized, V_normalized, sheenColorVal, sheenRoughnessVal) * ao;

            if (clearcoatVal > 0.001 && envMapEnabled)
            {
                vec3 ccR = reflect(-V_normalized, clearcoatN);
                ccR = previewRotationMatrix * ccR;
                vec3 ccPrefilterDir = vec3(ccR.x, -ccR.z, ccR.y);
                float maxLod = max(float(textureQueryLevels(prefilterMap)) - 1.0, 0.0);
                vec3 ccPrefilter = textureLod(prefilterMap, ccPrefilterDir, clamp(clearcoatRoughnessVal * maxLod, 0.0, maxLod)).rgb * envMapExposure;
                vec2 ccBrdf = texture(brdfLUT, vec2(dotNV, clearcoatRoughnessVal)).rg;
                vec3 ccF = fresnelSchlick(dotNV, vec3(0.04), vec3(1.0));
                vec3 ccIBL = ccPrefilter * (ccF * ccBrdf.x + ccBrdf.y) * clearcoatVal;
                clearcoatAttenuation = clearcoatVal * max(ccF.r, max(ccF.g, ccF.b));
                ambient = ambient * (1.0 - clearcoatAttenuation) + ccIBL;
            }
        }

        color = emissive + ambient + (directDiffuse + directSpecular) * (1.0 - clearcoatAttenuation) + directSheen + directClearcoat;

        float transmissionVal = useTransmissionMap ? texture(transmissionMap, applyTextureTransform(uv, transmissionScale, transmissionOffset, transmissionRotation)).r * transmission : transmission;
        if (transmissionVal > 0.001)
        {
            vec3 tint = mix(albedoVal, vec3(1.0), clamp((iorVal - 1.0) * 0.3, 0.0, 1.0));
            color = mix(color, tint * (ambient + directSpecular + emissive), transmissionVal);
        }
    }

    if (unlitMaterial)
    {
        color = albedoVal + emissive;
    }

    color = applyToneMapping(color);
    if (gammaCorrection) {
        color = pow(color, vec3(1.0 / screenGamma));
    }

    FragColor = vec4(color, opacityVal);
}
