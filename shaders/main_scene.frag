#version 450 core
#extension GL_OES_standard_derivatives : enable

// Adpated from https://learnopengl.com/

in vec3 v_position;
in vec3 v_normal;
in vec4 v_color;
in vec2 v_texCoord0;
in vec2 v_texCoord1;
in vec2 v_texCoord2;
in vec2 v_texCoord3;
in vec3 v_tangent;
in vec3 v_bitangent;
in vec3 v_reflectionPosition;
in vec3 v_reflectionNormal;
in vec3 v_tangentLightPos;
in vec3 v_tangentViewPos;
in vec3 v_tangentFragPos;

in VS_OUT_SHADOW{
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
	vec4 FragPosLightSpace;
	vec3 cameraPos;
	vec3 lightPos;
} fs_in_shadow;

uniform bool hasVertexColors;
uniform bool hasNegativeScale;

uniform int primitiveMode;  // 0=POINTS, 1=LINES, 2=LINE_LOOP, 3=LINE_STRIP, 4+=TRIANGLES

uniform float opacity;
uniform bool texEnabled;
uniform sampler2D texUnit;

// ADS light maps
uniform sampler2D texture_diffuse;
uniform sampler2D texture_specular;
uniform sampler2D texture_emissive;
uniform sampler2D texture_normal;
uniform sampler2D texture_height;
uniform sampler2D texture_opacity;
uniform bool hasDiffuseTexture = false;
uniform bool hasSpecularTexture = false;
uniform bool hasEmissiveTexture = false;
uniform bool hasNormalTexture = false;
uniform bool hasHeightTexture = false;
uniform bool hasOpacityTexture = false;
uniform bool opacityTextureInverted = false;

uniform samplerCube envMap;
uniform sampler2D shadowMap;
uniform float shadowSoftness; // Adjustable softness factor
uniform float lightFarPlane; // Far plane of the light's perspective
uniform int shadowMaxKernelSize;
uniform float shadowSoftnessScale;
uniform float shadowMaxSoftnessClamp;
uniform float shadowBiasMin;
uniform float shadowBiasMax;
uniform float shadowTransitionRange;
uniform float shadowGammaCorrection;
uniform float shadowSizeScale;

// Transmission map
uniform sampler2D transmissionSceneTexture;  // _transmissionColorTexture
uniform sampler2D transmissionDepthTexture;  // _transmissionDepthTexture
uniform vec2	  transmissionFramebufferSize;

uniform float floorAlpha = 0.95;
uniform float floorSpecularScale = 0.6;  // scale specular on floor [0..1]
uniform float floorFresnelDampen = 0.5;  // how much to dampen spec at normal incidence [0..1]


// IBL
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;
uniform bool useIBL;

// material parameters
uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D heightMap;
uniform sampler2D aoMap;
uniform sampler2D emissiveMap;
uniform sampler2D opacityMap;
uniform bool hasAlbedoMap;
uniform bool hasMetallicMap;
uniform bool hasRoughnessMap;
uniform bool hasNormalMap;
uniform bool hasAOMap;
uniform bool hasEmissiveMap;
uniform bool hasHeightMap;
uniform float heightScale = 0.08;
uniform float clearcoatNormalScale = 1.0;
uniform bool hasOpacityMap;
uniform bool opacityMapInverted = false;
uniform int blendMode; // 0 = additive, 1 = multiplicative, 2 = overlay
uniform float alphaThreshold; // Threshold for alpha testing

// packing uniforms
uniform int   metallicChannel;
uniform int   metallicInvert;
uniform float metallicScale;
uniform float metallicBias;

uniform int   roughnessChannel;
uniform int   roughnessInvert;
uniform float roughnessScale;
uniform float roughnessBias;

uniform int   aoChannel;
uniform int   aoInvert;
uniform float aoScale;
uniform float aoBias;

uniform int   opacityChannel;
uniform int   opacityInvert;
uniform float opacityScale;
uniform float opacityBias;

uniform bool twoSided;

// Advanced PBR Material Properties
uniform sampler2D transmissionMap;
uniform sampler2D iorMap;
uniform sampler2D sheenColorMap;
uniform sampler2D sheenRoughnessMap;
uniform sampler2D clearcoatColorMap;
uniform sampler2D clearcoatRoughnessMap;
uniform sampler2D clearcoatNormalMap;

uniform bool hasTransmissionMap = false;
uniform bool hasIORMap = false;
uniform bool hasSheenColorMap = false;
uniform bool hasSheenRoughnessMap = false;
uniform bool hasClearcoatMap = false;
uniform bool hasClearcoatRoughnessMap = false;
uniform bool hasClearcoatNormalMap = false;

// KHR_materials_specular
uniform sampler2D specularFactorMap;
uniform sampler2D specularColorMap;
uniform bool hasSpecularFactorMap = false;
uniform bool hasSpecularColorMap = false;

// KHR_materials_pbrSpecularGlossiness
uniform sampler2D diffuseMap;
uniform sampler2D specularGlossinessMap;
uniform bool hasDiffuseMap = false;
uniform bool hasSpecularGlossinessMap = false;
// Flag to indicate this material uses spec-glossiness workflow
uniform bool useSpecularGlossiness = false;
uniform vec3 diffuseFactor;
uniform vec3 specularColor;
uniform float glossinessFactor;

// KHR_materials_anisotropy
uniform sampler2D anisotropyMap;
uniform bool hasAnisotropyMap = false;

// KHR_materials_iridescence
uniform sampler2D iridescenceMap;
uniform sampler2D iridescenceThicknessMap;
uniform bool hasIridescenceMap = false;
uniform bool hasIridescenceThicknessMap = false;

// KHR_materials_diffuse_transmission
uniform sampler2D diffuseTransmissionMap;
uniform sampler2D diffuseTransmissionColorMap;
uniform bool hasDiffuseTransmissionMap = false;
uniform bool hasDiffuseTransmissionColorMap = false;

// KHR_materials_volume
uniform sampler2D thicknessMap;
uniform bool hasThicknessMap = false;
uniform bool hasThicknessAlpha = false;

// KHR_materials_scatter
uniform vec3 multiScatterColor;
uniform bool hasVolumeScattering;

struct TextureTransform
{
	vec2 scale;
	vec2 offset;
	float rotation;
	int texCoordIndex;  // Which of the 4 texCoords to use
};

// Transform uniforms
uniform TextureTransform albedoTexTransform;
uniform TextureTransform metallicTexTransform;
uniform TextureTransform roughnessTexTransform;
uniform TextureTransform normalTexTransform;
uniform TextureTransform heightTexTransform;
uniform TextureTransform aoTexTransform;
uniform TextureTransform opacityTexTransform;
uniform TextureTransform emissiveTexTransform;
uniform TextureTransform transmissionTexTransform;
uniform TextureTransform iorTexTransform;
uniform TextureTransform sheenColorTexTransform;
uniform TextureTransform sheenRoughnessTexTransform;
uniform TextureTransform clearcoatTexTransform;
uniform TextureTransform clearcoatRoughnessTexTransform;
uniform TextureTransform clearcoatNormalTexTransform;
uniform TextureTransform specularFactorTexTransform;
uniform TextureTransform specularColorTexTransform;
uniform TextureTransform anisotropyTexTransform;
uniform TextureTransform iridescenceTexTransform;
uniform TextureTransform iridescenceThicknessTexTransform;
uniform TextureTransform diffuseTransmissionTexTransform;
uniform TextureTransform diffuseTransmissionColorTexTransform;
uniform TextureTransform thicknessTexTransform;
uniform TextureTransform diffuseTexTransform;
uniform TextureTransform specularGlossinessTexTransform;

// Legacy ADS texture transforms
uniform TextureTransform diffuseTextureTransform;
uniform TextureTransform specularTextureTransform;
uniform TextureTransform emissiveTextureTransform;
uniform TextureTransform normalTextureTransform;
uniform TextureTransform heightTextureTransform;
uniform TextureTransform opacityTextureTransform;

uniform bool isGLTFMaterial;

uniform bool envMapEnabled;
uniform mat3 envMapRotationMatrix;
uniform bool shadowsEnabled;
uniform bool selfShadowsEnabled;
uniform float shadowSamples;
uniform vec3 cameraPos;
uniform vec3 cameraDir;
uniform mat4 viewMatrix;
uniform mat4 modelMatrix;
uniform mat4 projectionMatrix;
uniform bool sectionActive;
uniform int displayMode;
uniform int renderingMode;
uniform bool isWireframePass;
uniform bool selected;
uniform bool selectionHighlighting;
uniform bool hovered = false;  // Is this mesh currently hovered?
uniform bool hoverHighlighting = false;  // Is hover highlighting enabled?
uniform vec3 hoverColor = vec3(1.0, 0.84, 0.0);  // Gold/yellow default hover color
uniform vec4 reflectColor;
uniform bool floorRendering;
uniform bool hdrToneMapping = false;
uniform bool gammaCorrection = false;
uniform float screenGamma = 2.2;

uniform float envMapExposure = 1.0;
uniform float iblExposure = 1.0;

// 0=KhronosPbrNeutral, 1=ACES_Narkowicz, 2=ACES_Hill, 3=AECS_Hill_Exposure_Boost,
// 4=Uncharted2, 5=Reinhard(Linear)
uniform int toneMapMode = 0;

uniform bool skyBoxEnabled;
uniform sampler2D skyboxColorTexture;

uniform vec4 topColor;
uniform vec4 botColor;
uniform int gradientStyle;
uniform vec2 screenSize;
uniform vec3 screenCenter;
uniform float floorSize;
uniform bool isReflectedPass;

struct LineInfo
{
	float Width;
	vec4 Color;
};

uniform LineInfo Line;

struct LightSource
{
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
	vec3 position;
};
uniform LightSource lightSource;

struct LightModel
{
	vec3 ambient;
};
uniform LightModel lightModel;

const int LightType_Directional = 0;
const int LightType_Point = 1;
const int LightType_Spot = 2;
const int MAX_LIGHTS = 16;

uniform int lightCount;
uniform bool hasPunctualLights;
uniform bool useDefaultLights;
uniform bool usePunctualLights;

struct PunctualLight
{
	vec3 direction;
	float range;
	vec3 color;
	float intensity;
	vec3 position;
	float innerConeCos;
	float outerConeCos;
	int type;
};

layout(std140, binding = 3) uniform LightBlock
{
	PunctualLight lights[MAX_LIGHTS];
};

struct Material
{
	vec3  emission;
	vec3  ambient;
	vec3  diffuse;
	vec3  specular;
	float shininess;
	bool  metallic;
};
uniform Material material;

struct PBRLighting
{
	vec3 albedo;
	float metallic;
	float roughness;
	float normalScale;
	float ambientOcclusion;
	float occlusionStrength;
	// Advanced PBR Properties
	float transmission;
	float ior;
	vec3 sheenColor;
	float sheenRoughness;
	float clearcoat;
	float clearcoatRoughness;

	// KHR_materials_specular
	float specularFactor;
	vec3 specularColorFactor;

	// KHR_materials_anisotropy
	float anisotropyStrength;
	float anisotropyRotation;

	// KHR_materials_iridescence
	float iridescenceFactor;
	float iridescenceIor;
	float iridescenceThicknessMin;
	float iridescenceThicknessMax;

	// KHR_materials_volume
	float thicknessFactor;
	float attenuationDistance;
	vec3 attenuationColor;

	// KHR_materials_dispersion
	float dispersion;

	// KHR_materials_diffuse_transmission
	float diffuseTransmissionFactor;
	vec3 diffuseTransmissionColorFactor;

	// KHR_materials_unlit
	bool unlit;

	// KHR_emissive_strength
	float emissiveStrength;
};
uniform PBRLighting pbrLighting;

uniform int   tintMode = 1;     // 0=Off, 1=AutoGray, 2=ForceGray, 3=LerpMask
uniform float tintStrength = 1.0;
uniform float grayEpsilon = 0.02; // grayscale detection threshold in sRGB
uniform bool  useVertexColor = false; // include vtx color
uniform int   tintMaskChannel = 0; // 0=R, 1=G, 2=B, 3=A

const float PI = 3.14159265359;

layout(location = 0) out vec4 fragColor;

float	guardFactorScalar(float factor, float threshold);
vec3	guardFactorColor(vec3 factor, float threshold);

vec2	getTransformedUV(int texCoordIndex, TextureTransform transform);

vec2    getAlbedoUV();
vec2    getMetallicUV();
vec2    getRoughnessUV();
vec2    getNormalUV();
vec2    getHeightUV();
vec2    getAOUV();
vec2    getOpacityUV();
vec2	getEmissiveUV();
vec2    getTransmissionUV();
vec2    getIORUV();
vec2    getSheenColorUV();
vec2    getSheenRoughnessUV();
vec2    getClearcoatUV();
vec2    getClearcoatRoughnessUV();
vec2    getClearcoatNormalUV();
vec2    getSpecularFactorUV();
vec2    getSpecularColorUV();
vec2    getAnisotropyUV();
vec2    getIridescenceUV();
vec2    getIridescenceThicknessUV();
vec2    getThicknessUV();
vec2    getDiffuseTransmissionUV();
vec2    getDiffuseTransmissionColorUV();

// KHR pbrSpecularGlossiness
vec2	getDiffuseUV();
vec2	getSpecularGlossinessUV();



// Legacy ADS
vec2    getDiffuseTextureUV();
vec2    getSpecularTextureUV();
vec2    getEmissiveTextureUV();
vec2    getNormalTextureUV();
vec2    getHeightTextureUV();
vec2    getOpacityTextureUV();

vec4    shadeBlinnPhong(LightSource source, LightModel model, Material mat, vec3 position, vec3 normal);
vec4    calculatePBRLighting(int renderMode, float side);
vec4    calculatePBRLightingRewritten(int renderMode, float side);

float	samplePackedChannelValue(sampler2D tex, bool hasTexture, vec2 uv,
	int channel, int invert, float scale, float bias,
	float fallback);

vec3    getNormalFromMap(sampler2D map);
mat3    getTBNFromMap(sampler2D map);
float   distributionGGX(vec3 N, vec3 H, float roughness);
float   geometrySchlickGGX(float NdotV, float roughness);
float   geometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3    fresnelSchlick(float cosTheta, vec3 F0);
vec3    fresnelSchlick(float cosTheta, vec3 F0, vec3 F90);
vec2    parallaxOcclusionMapping(vec2 texCoords, vec3 viewDir, sampler2D heightMap, float heightScale);
vec2	applyParallaxMapping(vec2 baseUV, sampler2D heightMap, float heightScale, bool enabled);
vec3    calcBumpedNormal(sampler2D map, vec2 texCoord);

// Advanced PBR Functions
vec3    fresnelSchlickIOR(float cosTheta, float ior);
float   distributionCharlie(vec3 N, vec3 H, float roughness);
float   geometryCharlie(float NdotV, float roughness);
float	D_Charlie(float roughness, float NoH);

float	max3(vec3 v);
float	V_Sheen(float NdotL, float NdotV, float sheenRoughness);
float	lambdaSheen(float cosTheta, float alphaG);
float	lambdaSheenNumericHelper(float x, float alphaG);
vec3    calculateSheen(vec3 N, vec3 V, vec3 L, vec3 sheenColor, float sheenRoughness);
vec3    calculateSheenIBL(vec3 N, vec3 V, float sheenRoughness, vec3 sheenColor);
vec3    calculateClearcoat(vec3 N, vec3 V, vec3 L, float clearcoat, float clearcoatRoughness, vec3 clearcoatNormal);
vec3    calculateClearcoatIBL(vec3 N, vec3 V, vec3 clearcoatNormal, float clearcoatRoughness, float clearcoat);

// ==== NEW glTF EXTENSION FUNCTIONS ====

// ==== KHR Anisotropy
float	V_GGX_anisotropic(float NdotL, float NdotV, float BdotV, float TdotV, float TdotL, float BdotL, float at, float ab);
float	D_GGX_anisotropic(float NdotH, float TdotH, float BdotH, float at, float ab);
vec3	calculateAnisotropy(vec3 N, vec3 V, vec3 L, vec3 T, vec3 B, float anisotropyStrength, float anisotropyRotation, float roughness, vec3 F0);

// ============================================================================
// HELPER: Decode anisotropy texture data
// Call this BEFORE calling calculateAnisotropy
// ============================================================================
struct AnisotropyData
{
	float strength;      // Final strength (texture x uniform)
	float rotation;      // Final rotation (texture direction rotated by uniform)
	vec2 direction;      // Texture direction in [-1,1] (before rotation)
};

// ============================================================================
// NEW PBR REWRITE SCAFFOLDING
// These structs define the internal contract for the replacement shading path.
// The existing uniforms/samplers remain the external interface.
// ============================================================================
struct SurfaceFrame
{
	vec3 V;      // surface -> camera
	vec3 I;      // camera -> surface
	vec3 L;      // surface -> active light
	vec3 Ng;     // geometric normal
	vec3 N;      // shaded base normal
	vec3 Ncoat;  // shaded clearcoat normal
	vec3 T;      // tangent
	vec3 B;      // bitangent
};

struct MaterialParams
{
	vec3  baseColor;
	vec3  emissive;
	vec3  F0;
	vec3  F90;
	vec3  dielectricF0;
	float metallic;
	float roughness;
	float ambientOcclusion;
	float transmission;
	float ior;
	float clearcoat;
	float clearcoatRoughness;
	float sheenRoughness;
	vec3  sheenColor;
	float anisotropyStrength;
	float anisotropyRotation;
	float iridescenceFactor;
	float iridescenceIor;
	float iridescenceThickness;
	float thickness;
	float diffuseTransmissionFactor;
	vec3  diffuseTransmissionColor;
	vec3  attenuationColor;
	float attenuationDistance;
	float specularFactor;
	vec3  specularColor;
	bool  useSpecGloss;
	bool  unlit;
};

struct LayerContributions
{
	vec3 baseDirectDiffuse;
	vec3 baseDirectSpecular;
	vec3 baseDiffuseIBL;
	vec3 baseSpecularIBL;
	vec3 sheenDirect;
	vec3 sheenIBL;
	vec3 clearcoatDirect;
	vec3 clearcoatIBL;
	vec3 transmission;
	vec3 emissive;
};

AnisotropyData decodeAnisotropyTexture(
	vec3 texelRGB,                           // Raw texture data
	float uniformStrength,                   // Uniform strength parameter
	float uniformRotation,                   // Uniform rotation in radians
	bool hasTexture
);

// KHR Iridescence
vec3	evalIridescence(float outsideIOR, float eta2, float cosTheta1, float thinFilmThickness, vec3 baseF0);
vec3	evalIridescence(float outsideIOR, float eta2, float cosTheta1, float thinFilmThickness, vec3 baseF0, vec3 baseF90);
vec3    computeIridescentFresnel(float cosTheta,
	vec3 baseF0,
	vec3 baseF90,
	float iridescenceFactor,
	float iridescenceIor,
	float iridescenceThickness);

// KHR IOR, Transmission, and Volume
vec3    calculateTransmission(vec3 N, vec3 V, vec3 L, float transmission, float ior, vec3 albedo);
vec3	calculateVolumeAttenuation(vec3 transmittedLight, float distance, float thickness, vec3 attenuationColor, float attenuationDistance);
float	iorToF0(float ior);
float	fresnelVolumeEntering(float f0, float NdotH);
float	fresnelVolumeExiting(float f0, float etaRatio, float NdotH);
float	calculateVolumeFresnel(float ior, float iorIncident, float NdotV);
float	applyIorToRoughness(float roughness, float ior);
vec3	getVolumeTransmissionRay(vec3 n, vec3 v, float thickness, float ior, mat4 modelMatrix);
vec3	getTransmissionSample(vec2 fragCoord, float roughness, float ior);
vec3	getIBLVolumeRefraction(vec3 n, vec3 v, float perceptualRoughness, vec3 baseColor,
	vec3 position, mat4 modelMatrix, float ior, float thickness,
	vec3 attenuationColor, float attenuationDistance);
vec3	getIBLVolumeRefractionPerChannel(vec3 n, vec3 v, float perceptualRoughness, vec3 baseColor,
	vec3 position, mat4 modelMatrix, vec3 iors, float thickness,
	vec3 attenuationColor, float attenuationDistance);

// KHR scatter
vec3	multiToSingleScatter();

// Shadow
float   calculateShadow(vec4 fragPosLightSpace);
// Function to fetch shadow value with variable kernel size
float   calculateShadowVariableKernel(vec4 fragPosLightSpace, vec3 fragPos, vec3 lightPos);

vec2    calculateBackgroundUV();
vec3    calculateBackgroundColor();

float pickChannel(vec4 v, int ch, int invertFlag, float scale, float bias);
float sampleOpacityMap(vec2 uv);
float sampleFallbackOpacity(vec2 uv);

vec3 acesToneMapping(vec3 color);
vec3 uncharted2ToneMapping(vec3 color);
vec3 applyToneMapping(vec3 color);

vec3 sRGBToLinear(vec3 c);
vec3 linearTosRGB(vec3 c);
float sRGBSaturation(vec3 c);
float readMaskChannel(vec4 texel, int channel);
SurfaceFrame buildRewriteSurfaceFrame(float side, vec2 normalUV, vec2 clearcoatNormalUV, vec3 lightDirection);
MaterialParams gatherRewriteMaterialParams();
vec3 computeRewriteDielectricF0(float ior, float specularFactor, vec3 specularColor, bool useSpecGloss, vec3 specGlossSpecular);
vec3 computeRewriteF90(float metallic, float specularFactor);
LayerContributions makeEmptyLayerContributions();
vec3 sampleRewriteMappedNormal(sampler2D map, vec2 texCoord, float normalScale, vec3 Ng, vec3 T, vec3 B);
void evaluateRewriteBaseDirect(in SurfaceFrame frame, in MaterialParams params, float lightFactor, out vec3 diffuseOut, out vec3 specularOut);
void evaluateRewriteBaseIBL(in SurfaceFrame frame, in MaterialParams params, out vec3 diffuseIBLOut, out vec3 specularIBLOut);
void evaluateRewriteClearcoatDirect(in SurfaceFrame frame, in MaterialParams params, float lightFactor, out vec3 clearcoatOut);
vec3 evaluateRewriteClearcoatIBL(in SurfaceFrame frame, in MaterialParams params);
vec3 composeRewriteBaseLayer(in MaterialParams params, in LayerContributions layers);
vec3 composeRewriteLayeredPBR(in SurfaceFrame frame, in MaterialParams params, in LayerContributions layers);
float computeRewriteSheenScaling(float Ndot, float sheenRoughness);
vec3 computeBaseColor(vec2 uv,
	vec3 matBaseColor_sRGB,   // material.diffuse in sRGB
	sampler2D albedoTex,
	bool hasAlbedoTex,
	vec3 vertexColor_sRGB,    // pass v_color.rgb
	bool useVertexColor);

const bool kUseRewrittenBasePBR = true;
const bool kDebugRewriteClearcoatOnly = false;
const int kRewriteDebugBaseMode = 0; // 0=full base, 1=base IBL only, 2=base direct only, 3=specular IBL only, 4=diffuse IBL only
const int kRewriteClearcoatMode = 1; // 0=full clearcoat, 1=IBL/composition only

float floorRadius = floorSize * 0.5; // Adjust radius based on floor size
float fadeStart = floorRadius * 0.65;   // Start fading 
float fadeEnd = floorRadius * 1.025;     // Fully faded


void main()
{
	vec4 v_color_front;
	vec4 v_color_back;
	vec4 v_color;

	// Discard backfaces if not twoSided
	bool isFrontFacing = gl_FrontFacing;

	if (hasNegativeScale)
	{
		isFrontFacing = !isFrontFacing;  // Invert because negative scale reverses winding
	}

	if (!twoSided && !isFrontFacing && !floorRendering)
	{
		discard;
	}

	// Early discard for reflected pass beyond fade start
	if (isReflectedPass)
	{
		float distance = length(v_position - screenCenter);
		if (distance > fadeStart)
			discard;
	}

	// Choose rendering path - ADS vs PBR
	if (renderingMode == 0)
	{
		v_color_front = shadeBlinnPhong(lightSource, lightModel, material, v_position, v_normal);
		v_color_back = shadeBlinnPhong(lightSource, lightModel, material, v_position, -v_normal);
	}
	else
	{
		v_color_front = calculatePBRLighting(renderingMode, 1.0f);
		v_color_back = calculatePBRLighting(renderingMode, -1.0f);
	}

	// Two-sided coloring
	if (gl_FrontFacing)
	{
		v_color = v_color_front;
	}
	else
	{
		if (sectionActive) // lighten backfaces in section mode
			v_color = v_color_back + 0.15f;
		else
			v_color = v_color_back;
	}

	fragColor = v_color; // Start with default shaded color	

	// Display modes - this needs to be computed before the alpha
	if (displayMode == 1) // wireframe
	{
		fragColor = vec4(v_color.rgb, 0.75f); // semi-transparent shaded
	}
	else if (displayMode == 2) // wireshaded
	{
		fragColor = v_color;
		if (isWireframePass)
		{
			float brightness = dot(fragColor.rgb, vec3(0.2126, 0.7152, 0.0722));
			vec3 overlayColor;
			if (brightness < 0.2)
			{
				overlayColor = fragColor.rgb + vec3(0.6);
			}
			else if (brightness > 0.8)
			{
				overlayColor = fragColor.rgb * 0.3;
			}
			else
			{
				overlayColor = brightness > 0.5 ? fragColor.rgb * 0.5 : fragColor.rgb + vec3(0.4);
			}
			overlayColor = clamp(overlayColor, 0.0, 1.0);
			fragColor = vec4(overlayColor, 1.0);
		}		
	}

	// UNIFIED BLEND MODE AWARE OPACITY CALCULATION
	// Works for both ADS and PBR pipelines with dynamic texture availability
	// Skip for floor rendering - it handles its own alpha
	float finalAlpha = fragColor.a; // Start with whatever alpha the rendering functions set

	if (!floorRendering) // Bypass alpha for floor
	{		
		if (blendMode == 0)
		{
			// OPAQUE: ignore alpha maps, always fully opaque
			finalAlpha = 1.0;
		}
		else if (blendMode == 1)
		{
			// MASK: cutout alpha test. Compute testAlpha from material scalar and textures.
			float testAlpha = opacity; // material scalar

			// Priority: dedicated opacity map > fallbacks
			if (hasOpacityMap)
			{
				float opVal = sampleOpacityMap(getOpacityUV());
				testAlpha *= opVal;
			}
			else
			{
				// fallback to albedo/diffuse alpha or legacy opacity texture
				float fallback = sampleFallbackOpacity(getAlbedoUV());
				testAlpha *= fallback;
			}

			// Alpha test
			if (testAlpha < alphaThreshold) discard;
			finalAlpha = 1.0; // cutout either opaque or discarded
		}
		else
		{ // blendMode == 2 (BLEND) - standard transparency
			 // Compute finalAlpha as material scalar * dedicated opacity map * fallback alpha
			float alphaVal = opacity;

			if (hasOpacityMap)
			{
				alphaVal *= sampleOpacityMap(getOpacityUV());
			}
			else
			{
				alphaVal *= sampleFallbackOpacity(getAlbedoUV());
			}

			// clamp and optionally apply any opacityScale/bias global (if desired)
			finalAlpha = clamp(alphaVal, 0.0, 1.0);

			// Note: do NOT discard here. Transparent fragments are blended.
			// Depth write/disabling and render-order must be handled on the GL side.
		}
	}

	// Apply the final alpha (outside floorRendering block)
	fragColor.a = finalAlpha;

	// Apply vertex color alpha modulation
	if (hasVertexColors)
		fragColor.a *= v_color.a;

	// Premultiply for blending (non-floor; floor path already premultiplies)
	if (!floorRendering)
	{
		fragColor.rgb *= finalAlpha;
	}

	// Selection highlighting
	if (selected && selectionHighlighting) // with glow
	{
		// Compute lighting
		vec3 norm = normalize(gl_FrontFacing ? v_normal : -v_normal);
		vec3 lightDir = normalize(lightSource.position);
		float diff = max(dot(norm, lightDir), 0.0);

		vec3 viewDir = normalize(cameraDir);
		vec3 reflectDir = reflect(-lightDir, norm);
		float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

		// Base color from fragColor
		vec3 baseColor = fragColor.rgb;

		// Lighten the color with lighting + subtle spec
		vec3 lightened = baseColor + vec3(0.3) * diff + vec3(0.2) * spec + vec3(0.1);
		lightened = clamp(lightened, 0.0, 1.0);

		// Apply a subtle transparency
		float alpha = fragColor.a * 0.99;

		// Add glow effect
		vec3 glowColor = lightened * 1.2; // Make the glow a bit brighter
		glowColor = clamp(glowColor, 0.0, 1.0);

		// Mix base color with the glow
		vec3 finalColor = mix(lightened, glowColor, 0.5); // blend base and glow color

		fragColor = vec4(finalColor, alpha);
	}

	// Hover highlighting (visual preview, non-destructive)
	// Only applied if not already selected (selection takes priority)
	if (hovered && hoverHighlighting && !selected)
	{
		// Compute lighting (similar to selection but more subtle)
		vec3 norm = normalize(gl_FrontFacing ? v_normal : -v_normal);
		vec3 lightDir = normalize(lightSource.position);
		float diff = max(dot(norm, lightDir), 0.0);

		vec3 viewDir = normalize(cameraDir);
		vec3 reflectDir = reflect(-lightDir, norm);
		float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

		// Base color from fragColor
		vec3 baseColor = fragColor.rgb;

		// Subtle brightening (less aggressive than selection - use 50% of selection intensity)
		vec3 brightened = baseColor + vec3(0.15) * diff + vec3(0.1) * spec + vec3(0.05);
		brightened = clamp(brightened, 0.0, 1.0);

		// Apply subtle glow (less pronounced than selection)
		vec3 glowColor = brightened * 1.1;  // Minimal glow boost
		glowColor = clamp(glowColor, 0.0, 1.0);

		// Mix with slight glow (30% blend vs selection's 50%)
		vec3 finalColor = mix(brightened, glowColor, 0.3);

		fragColor = vec4(finalColor, fragColor.a);
	}

	// Finally, handle floor rendering fade-out and background blending
	if (floorRendering)
	{
		if (texEnabled == true)
			fragColor = v_color * texture2D(texUnit, v_texCoord0);
		// Compute distance-based blending factor
		float distance = length(v_position - screenCenter);

		// Set fade parameters first, before any calculations
		// Early discard for pixels beyond fade range
		if (distance > fadeEnd)
			discard;

		// Calculate fade factor
		float fadeFactor = smoothstep(fadeStart, fadeEnd, distance);
		if (fadeFactor >= 1.0)
			discard;
		// Blend floor color with the background gradient
		// View-angle modulation: reduce background mix when looking straight down
		// NdotV in world (front/back already handled above)
		vec3 N_main = normalize(gl_FrontFacing ? v_normal : -v_normal);
		vec3 V_main = normalize(cameraDir);
		float NdotV_main = clamp(dot(N_main, V_main), 0.0, 1.0);

		// Reduce background contribution when NdotV is high (top view).
		// When looking straight down (NdotV~1), bgMix gets smaller -> floor doesn't wash out.
		// Reduce background mixing when looking straight down (NdotV ~ 1)
		float angleMod = mix(1.0, 0.25, NdotV_main);
		// at grazing -> 1.0, at top-down -> 0.25
		float bgMix = fadeFactor * angleMod;

		// Get background color
		vec3 backgroundColor = vec3(1.0);
		if (skyBoxEnabled)
		{
			vec3 N = normalize(v_reflectionNormal);
			vec3 V = normalize(cameraDir);

			// Refract ray into environment
			float ior = 1.5; // IOR of glass
			vec3 R = refract(V, N, 1.0 / ior);

			// Sample environment
			vec3 backgroundColor = texture(envMap, R).rgb;
		}
		else if (texEnabled == true)
		{
			// Interpolate background gradient color			
			backgroundColor = texture2D(texUnit, v_texCoord0).rgb;			
		}

		// Blend floor color with background gradient
		fragColor.rgb = mix(fragColor.rgb, backgroundColor, clamp(bgMix, 0.0, 1.0));
		fragColor.a *= (1.0 - fadeFactor) * opacity;
	}
}

// ========== LEGACY BLINN-PHONG SHADING FUNCTION ==========
vec4 shadeBlinnPhong(LightSource source, LightModel model, Material mat, vec3 position, vec3 normal)
{
	vec2 clippedTexCoord = v_texCoord0;

	// --- Normal / Parallax (same as before) ---
	if (hasNormalTexture)
		normal = calcBumpedNormal(texture_normal, getNormalTextureUV());

	if (hasHeightTexture)
	{
		clippedTexCoord = applyParallaxMapping(getHeightUV(), texture_height, heightScale, hasHeightTexture);
		normal = calcBumpedNormal(normalMap, clippedTexCoord);
	}

	// --- Lighting vectors ---
	vec3 lightDir, viewDir;

	viewDir = normalize(vec3(0, 0, 1));
	lightDir = normalize(source.position - v_position);

	vec3 halfVector = normalize(lightDir + viewDir);
	float nDotVP = max(dot(normal, normalize(lightDir + viewDir)), 0.0);
	float nDotHV = max(0.0, dot(normal, halfVector));
	float pf = pow(nDotHV, mat.shininess);

	// --- Material terms ---
	vec3 matAmbient = mat.ambient;
	vec3 matDiffuse = mat.diffuse;
	vec3 matSpecular = mat.specular * pf;
	vec3 matEmissive = mat.emission;

	if (hasDiffuseTexture)
	{
		vec4 d = texture(texture_diffuse, getDiffuseTextureUV());
		matAmbient *= d.rgb;
		matDiffuse *= d.rgb;
	}

	if (hasVertexColors)
		matDiffuse *= v_color.rgb;

	if (length(normal) < 0.01)
	{		
		return vec4(matDiffuse, 1.0);  // Actual object color, unlit
	}

	// Geometry term
	matDiffuse  *= nDotVP;

	if (hasSpecularTexture)
	{
		// ADS specular is cascaded from metallic texture
		// Combine with roughness for proper metallic-roughness behavior:
		// specular = metallic × (1 - roughness)
		float metallicValue = texture(texture_specular, getSpecularTextureUV()).r;
		float roughnessValue = 0.5; // Default if no roughness texture

		// If roughness texture is available, use it to modulate specularity
		if (hasRoughnessMap)
		{
			roughnessValue = texture(roughnessMap, getRoughnessUV()).r;
		}

		// Combine: metallic determines if reflective, roughness determines intensity
		float specularity = metallicValue * (1.0 - roughnessValue);
		matSpecular = vec3(specularity) * pf;
	}
	if (hasEmissiveTexture)
		matEmissive = texture(texture_emissive, getEmissiveTextureUV()).rgb;

	// --- Build lighting buckets ---
	vec3 ambient = source.ambient * matAmbient * model.ambient;
	vec3 diffuse = vec3(0.0);
	vec3 specular = vec3(0.0);
	
	if (useDefaultLights || floorRendering)
	{
		diffuse = source.diffuse * matDiffuse;
		specular = source.specular * matSpecular;
	}

	vec3 baseNoSpec, specOnly;
	vec3 sceneColor = matEmissive + ambient;

	if (useDefaultLights && shadowsEnabled && displayMode == 3 && (selfShadowsEnabled || floorRendering))
	{
		float shadowFactor = calculateShadowVariableKernel(
			fs_in_shadow.FragPosLightSpace,
			fs_in_shadow.FragPos,
			fs_in_shadow.lightPos
		);
		shadowFactor = clamp(shadowFactor, 0.0, 0.7);
		float lightFactor = 1.0 - shadowFactor;

		vec3 ambientContrib = ambient * 0.6;
		vec3 directDiffuse = lightFactor * diffuse;
		vec3 directSpecular = lightFactor * specular;

		baseNoSpec = sceneColor + ambientContrib + directDiffuse;
		specOnly = directSpecular;
	}
	else
	{
		baseNoSpec = sceneColor + diffuse;
		specOnly = specular;
	}

	// --- Floor override (non-reflected) ---
	// Ensure the floor is actually translucent even if its material is OPAQUE
	if (floorRendering && !isReflectedPass)
	{
		float fa = clamp(floorAlpha, 0.0, 1.0);

		// View-angle term to avoid "whiteout" when looking straight down
		vec3 Nf = normalize(gl_FrontFacing ? v_normal : -v_normal);
		vec3 Vf = normalize(cameraPos - v_position);
		float NdotVf = clamp(dot(Nf, Vf), 0.0, 1.0);
		// Fresnel-like dampening of spec when NdotV is high (looking straight down)
		float fresDampen = mix(1.0 - floorFresnelDampen, 1.0, pow(1.0 - NdotVf, 5.0));

		// Scale specular; keep non-spec premultiplied
		vec3 floorRGB = baseNoSpec * fa + specOnly * (floorSpecularScale * fresDampen);

		if (hdrToneMapping) floorRGB = applyToneMapping(floorRGB);
		if (gammaCorrection) floorRGB = pow(floorRGB, vec3(1.0 / screenGamma));
		return vec4(floorRGB, fa);
	}

	vec3 composed;
	composed = baseNoSpec + specOnly;

	// ADS reflection with exposure
	if (useIBL && envMapEnabled)
	{
		vec3 I = normalize(cameraDir);
		vec3 N = normalize(v_reflectionNormal);
		vec3 offset = normalize(cameraPos - v_reflectionPosition);
		vec3 I_offset = normalize(I - offset * 0.3);  // Blend factor adjustable
		vec3 R = reflect(-I_offset, N);
		R = envMapRotationMatrix * -R;
		R = normalize(R);

		float specLum = dot(material.specular, vec3(0.299, 0.587, 0.114));
		float diffuseLum = dot(material.diffuse, vec3(0.299, 0.587, 0.114));

		// Derive metallic-like factor from ADS: metals have high spec/low diff
		float adsMetallic = specLum / (specLum + diffuseLum + 0.001);
		adsMetallic = clamp(adsMetallic, 0.0, 1.0);

		float NdotV = max(dot(-I, N), 0.0);

		// Gentler fresnel powers
		float nonMetallicFresnelPower = mix(2.0, 3.5, 1.0 - specLum); // Reduced
		float metallicFresnelPower = 1.2; // Reduced
		float fresnelPower = mix(nonMetallicFresnelPower, metallicFresnelPower, adsMetallic);
		float fresnel = pow(1.0 - NdotV, fresnelPower);

		// Limit grazing angle effect
		float grazingLimit = mix(0.6, 0.9, adsMetallic); // Metals can handle more
		fresnel = clamp(fresnel, 0.0, grazingLimit);

		float surfaceRoughness = 1.0 - (material.shininess / 128.0);
		float roughnessReduction = pow(1.0 - surfaceRoughness, 2.0);

		// Reduced base strengths
		float metallicStrength = mix(0.2, 0.4, specLum);
		float glossyStrength = mix(0.25, 0.5, specLum);
		float diffuseStrength = mix(0.02, 0.12, specLum);

		bool isHighSpecular = specLum > 0.5;
		bool isDiffuseDominant = dot(material.diffuse, vec3(0.299, 0.587, 0.114)) > specLum * 2.0;
		float nonMetallicStrength = isHighSpecular && !isDiffuseDominant ? glossyStrength : diffuseStrength;
		float baseReflectionStrength = mix(nonMetallicStrength, metallicStrength, adsMetallic);

		// Additional roughness damping for very rough surfaces
		float roughnessDamping = mix(0.3, 1.0, 1.0 - surfaceRoughness);
		float reflectionStrength = clamp(baseReflectionStrength * fresnel * roughnessReduction * roughnessDamping, 0.0, 0.5);

		// === IMPROVED GRAZING LOD ===
		// Material-aware grazing influence: smooth metals/mirrors avoid extra blur
		float grazingInfluence = surfaceRoughness * (1.0 - adsMetallic * 0.8);

		// Grazing factor for LOD adjustment
		float grazingFactor = pow(1.0 - NdotV, mix(1.8, 2.5, surfaceRoughness));

		// Base LOD from roughness
		float baseLOD = surfaceRoughness * 7.0;

		// Extra grazing blur, but only for rough dielectrics
		float grazingLOD = grazingFactor * grazingInfluence * 4.0; // Scaled to match 7.0 max

		// Combine: mirrors/smooth metals keep sharp reflections
		float envLOD = clamp(baseLOD + grazingLOD, 0.0, 7.0);
		// ========================

		vec3 envColor = textureLod(envMap, R, envLOD).rgb;
		composed += envColor * reflectionStrength;
	}

	// --- Tone/gamma ---
	if (hdrToneMapping)
	{
		composed *= envMapExposure;
		composed = applyToneMapping(composed);
	}
	if (gammaCorrection)
		composed = pow(composed, vec3(1.0 / screenGamma));

	return vec4(composed, 1.0);
}

vec4 calculatePBRLightingRewritten(int renderMode, float side)
{
	vec2 rewriteNormalUV = getNormalUV();
	if (hasHeightMap)
	{
		rewriteNormalUV = applyParallaxMapping(getNormalUV(), heightMap, heightScale, hasHeightMap);
	}

	vec2 rewriteClearcoatNormalUV = getClearcoatNormalUV();
	if (hasHeightMap)
	{
		rewriteClearcoatNormalUV = applyParallaxMapping(getClearcoatNormalUV(), heightMap, heightScale, hasHeightMap);
	}

	MaterialParams params = gatherRewriteMaterialParams();
	SurfaceFrame frame = buildRewriteSurfaceFrame(side, rewriteNormalUV, rewriteClearcoatNormalUV, lightSource.position - v_position);
	LayerContributions layers = makeEmptyLayerContributions();
	layers.emissive = params.emissive;

	if (params.unlit)
	{
		vec3 unlitColor = params.baseColor + params.emissive;
		if (hdrToneMapping) unlitColor = applyToneMapping(unlitColor);
		if (gammaCorrection) unlitColor = pow(unlitColor, vec3(1.0 / screenGamma));
		return vec4(unlitColor, 1.0);
	}

	float lightShadowFactor = 0.0;
	if (useDefaultLights && shadowsEnabled && displayMode == 3 && (selfShadowsEnabled || floorRendering))
	{
		float s = calculateShadowVariableKernel(
			fs_in_shadow.FragPosLightSpace,
			fs_in_shadow.FragPos,
			fs_in_shadow.lightPos
		);
		lightShadowFactor = clamp(s, 0.0, 0.85);
	}
	float lightFactor = 1.0 - lightShadowFactor;

	evaluateRewriteBaseDirect(frame, params, lightFactor, layers.baseDirectDiffuse, layers.baseDirectSpecular);
	evaluateRewriteBaseIBL(frame, params, layers.baseDiffuseIBL, layers.baseSpecularIBL);

	vec3 outRGB = layers.emissive +
		layers.baseDirectDiffuse +
		layers.baseDirectSpecular +
		layers.baseDiffuseIBL +
		layers.baseSpecularIBL;

	if (kRewriteDebugBaseMode == 1)
	{
		outRGB = layers.baseDiffuseIBL + layers.baseSpecularIBL;
	}
	else if (kRewriteDebugBaseMode == 2)
	{
		outRGB = layers.baseDirectDiffuse + layers.baseDirectSpecular;
	}
	else if (kRewriteDebugBaseMode == 3)
	{
		outRGB = layers.baseSpecularIBL;
	}
	else if (kRewriteDebugBaseMode == 4)
	{
		outRGB = layers.baseDiffuseIBL;
	}

	if (floorRendering && !isReflectedPass)
	{
		float fa = clamp(floorAlpha, 0.0, 1.0);
		float NdotVf = clamp(dot(frame.Ng, frame.V), 0.0, 1.0);
		float fresDampen = mix(1.0 - floorFresnelDampen, 1.0, pow(1.0 - NdotVf, 5.0));
		vec3 floorBase = (layers.emissive + layers.baseDirectDiffuse + layers.baseDiffuseIBL) * fa;
		vec3 floorSpec = (layers.baseDirectSpecular + layers.baseSpecularIBL) * (floorSpecularScale * fresDampen);
		vec3 floorRGB = floorBase + floorSpec;
		if (hdrToneMapping) floorRGB = applyToneMapping(floorRGB);
		if (gammaCorrection) floorRGB = pow(floorRGB, vec3(1.0 / screenGamma));
		return vec4(floorRGB, fa);
	}

	if (hdrToneMapping) outRGB = applyToneMapping(outRGB);
	if (gammaCorrection) outRGB = pow(outRGB, vec3(1.0 / screenGamma));
	return vec4(outRGB, 1.0);
}


SurfaceFrame buildRewriteSurfaceFrame(float side, vec2 normalUV, vec2 clearcoatNormalUV, vec3 lightDirection)
{
	SurfaceFrame frame;

	frame.V = normalize(cameraPos - v_position);
	frame.I = -frame.V;
	frame.L = normalize(lightDirection);
	frame.Ng = normalize(v_reflectionNormal * side);

	vec3 Q1 = dFdx(v_position);
	vec3 Q2 = dFdy(v_position);
	vec2 st1 = dFdx(normalUV);
	vec2 st2 = dFdy(normalUV);

	vec3 tangent = Q1 * st2.t - Q2 * st1.t;
	if (length(tangent) < 0.0001)
	{
		vec3 fallback = abs(frame.Ng.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
		tangent = cross(fallback, frame.Ng);
	}
	tangent = normalize(tangent - dot(tangent, frame.Ng) * frame.Ng);
	vec3 bitangent = -normalize(cross(frame.Ng, tangent));

	frame.T = tangent;
	frame.B = bitangent;

	frame.N = frame.Ng;
	if (hasNormalMap)
	{
		frame.N = sampleRewriteMappedNormal(
			normalMap,
			normalUV,
			pbrLighting.normalScale,
			frame.Ng,
			frame.T,
			frame.B
		);
	}

	frame.Ncoat = frame.N;
	if (hasClearcoatNormalMap)
	{
		frame.Ncoat = sampleRewriteMappedNormal(
			clearcoatNormalMap,
			clearcoatNormalUV,
			clearcoatNormalScale,
			frame.Ng,
			frame.T,
			frame.B
		);
	}

	return frame;
}

vec3 computeRewriteDielectricF0(float ior, float specularFactor, vec3 specularColor, bool useSpecGloss, vec3 specGlossSpecular)
{
	if (useSpecGloss)
	{
		return clamp(specGlossSpecular, vec3(0.0), vec3(1.0));
	}

	float f0_from_ior = pow((ior - 1.0) / (ior + 1.0), 2.0);
	vec3 dielectricF0 = vec3(f0_from_ior);
	if (specularFactor > 0.0)
	{
		dielectricF0 *= specularColor * specularFactor;
	}
	return clamp(dielectricF0, vec3(0.0), vec3(1.0));
}

vec3 computeRewriteF90(float metallic, float specularFactor)
{
	vec3 F90_dielectric = vec3(specularFactor);
	return mix(F90_dielectric, vec3(1.0), metallic);
}

LayerContributions makeEmptyLayerContributions()
{
	LayerContributions layers;
	layers.baseDirectDiffuse = vec3(0.0);
	layers.baseDirectSpecular = vec3(0.0);
	layers.baseDiffuseIBL = vec3(0.0);
	layers.baseSpecularIBL = vec3(0.0);
	layers.sheenDirect = vec3(0.0);
	layers.sheenIBL = vec3(0.0);
	layers.clearcoatDirect = vec3(0.0);
	layers.clearcoatIBL = vec3(0.0);
	layers.transmission = vec3(0.0);
	layers.emissive = vec3(0.0);
	return layers;
}

vec3 sampleRewriteMappedNormal(sampler2D map, vec2 texCoord, float normalScale, vec3 Ng, vec3 T, vec3 B)
{
	vec3 tangentNormal = texture(map, texCoord).rgb * 2.0 - 1.0;
	tangentNormal.xy *= normalScale;
	tangentNormal = normalize(tangentNormal);
	return normalize(mat3(T, B, Ng) * tangentNormal);
}

vec3 rewriteTransformNormalForIBL(vec3 normal)
{
	return normalize(envMapRotationMatrix * normal);
}

vec3 rewriteToPrefilterDirection(vec3 v)
{
	return vec3(v.x, -v.z, v.y);
}

void evaluateRewriteBaseDirect(in SurfaceFrame frame, in MaterialParams params, float lightFactor, out vec3 diffuseOut, out vec3 specularOut)
{
	float NdotL = max(dot(frame.N, frame.L), 0.0);
	if (NdotL <= 0.0)
	{
		diffuseOut = vec3(0.0);
		specularOut = vec3(0.0);
		return;
	}

	vec3 H = normalize(frame.V + frame.L);
	float NDF = distributionGGX(frame.N, H, params.roughness);
	float G = geometrySmith(frame.N, frame.V, frame.L, params.roughness);
	vec3 F = fresnelSchlick(clamp(dot(H, frame.V), 0.0, 1.0), params.F0, params.F90);
	vec3 specBRDF = (NDF * G * F) / max(4.0 * max(dot(frame.N, frame.V), 0.0) * NdotL, 0.001);

	vec3 kS = F;
	vec3 kD = (vec3(1.0) - kS) * (1.0 - params.metallic);

	vec3 lightIntensity = lightSource.ambient + lightSource.diffuse + lightSource.specular;
	diffuseOut = kD * params.baseColor / PI * lightIntensity * NdotL * lightFactor;
	specularOut = specBRDF * lightIntensity * NdotL * lightFactor;
}

void evaluateRewriteBaseIBL(in SurfaceFrame frame, in MaterialParams params, out vec3 diffuseIBLOut, out vec3 specularIBLOut)
{
	diffuseIBLOut = vec3(0.0);
	specularIBLOut = vec3(0.0);

	if (!useIBL)
	{
		return;
	}

	vec3 Nibl = rewriteTransformNormalForIBL(normalize(frame.N));
	vec3 irradiance = texture(irradianceMap, Nibl).rgb;
	diffuseIBLOut = irradiance * params.baseColor;

	if (!envMapEnabled)
	{
		return;
	}

	float dotNV = max(dot(frame.N, frame.V), 0.0);
	vec3 F90_effective = max(vec3(1.0 - params.roughness), params.F0);
	vec3 Fibl = fresnelSchlick(dotNV, params.F0, F90_effective);
	vec3 kSibl = Fibl;
	vec3 kDibl = (vec3(1.0) - kSibl) * (1.0 - params.metallic);

	vec3 R = reflect(frame.I, frame.N);
	R = normalize(envMapRotationMatrix * R);
	vec3 Rprefilter = rewriteToPrefilterDirection(R);
	float maxReflectionLod = max(textureQueryLevels(prefilterMap) - 1.0, 0.0);
	float lod = clamp(params.roughness * maxReflectionLod, 0.0, maxReflectionLod);
	vec3 prefilteredColor = textureLod(prefilterMap, Rprefilter, lod).rgb;
	prefilteredColor = max(prefilteredColor, vec3(0.0));
	prefilteredColor *= envMapExposure;

	vec2 brdf = texture(brdfLUT, vec2(dotNV, params.roughness)).rg;
	brdf = max(brdf, vec2(0.0));

	diffuseIBLOut = kDibl * diffuseIBLOut * params.ambientOcclusion;
	specularIBLOut = prefilteredColor * (Fibl * brdf.x + brdf.y) * params.ambientOcclusion;
}

void evaluateRewriteClearcoatDirect(in SurfaceFrame frame, in MaterialParams params, float lightFactor, out vec3 clearcoatOut)
{
	clearcoatOut = vec3(0.0);
	if (params.clearcoat <= 0.0)
	{
		return;
	}

	vec3 Vdirect = normalize(vec3(0.0, 0.0, 1.0));
	float NdotL = max(dot(frame.Ncoat, frame.L), 0.0);
	float NdotV = max(dot(frame.Ncoat, Vdirect), 0.0);
	if (NdotL <= 0.0 || NdotV <= 0.0)
	{
		return;
	}

	vec3 H = normalize(Vdirect + frame.L);
	float VdotH = max(dot(Vdirect, H), 0.0);
	float D = distributionGGX(frame.Ncoat, H, params.clearcoatRoughness);
	float G = geometrySmith(frame.Ncoat, Vdirect, frame.L, params.clearcoatRoughness);
	vec3 F = fresnelSchlick(VdotH, vec3(0.04), vec3(1.0));
	vec3 clearcoatBRDF = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
	vec3 lightIntensity = lightSource.specular;
	clearcoatOut = clearcoatBRDF * lightIntensity * NdotL * lightFactor * params.clearcoat;
}

vec3 evaluateRewriteClearcoatIBL(in SurfaceFrame frame, in MaterialParams params)
{
	if (params.clearcoat <= 0.0 || !useIBL || !envMapEnabled)
	{
		return vec3(0.0);
	}

	vec3 iblCoatNormal = hasClearcoatNormalMap ? frame.Ncoat : normalize(v_reflectionNormal);
	vec3 R = reflect(frame.I, iblCoatNormal);

	float maxReflectionLod = max(textureQueryLevels(prefilterMap) - 1.0, 0.0);
	float lod = clamp(params.clearcoatRoughness * maxReflectionLod, 0.0, maxReflectionLod);
	vec3 prefilteredColor = textureLod(prefilterMap, R, lod).rgb;
	prefilteredColor = max(prefilteredColor, vec3(0.0));
	prefilteredColor *= envMapExposure;

	vec3 Vdirect = normalize(vec3(0.0, 0.0, 1.0));
	float dotNV = max(dot(frame.N, Vdirect), 0.0);
	vec2 brdf = texture(brdfLUT, vec2(dotNV, params.clearcoatRoughness)).rg;
	brdf = max(brdf, vec2(0.0));
	vec3 clearcoatF = fresnelSchlick(dotNV, vec3(0.04), vec3(1.0));
	return prefilteredColor * (clearcoatF * brdf.x + brdf.y) * params.clearcoat;
}

vec3 composeRewriteBaseLayer(in MaterialParams params, in LayerContributions layers)
{
	return layers.emissive +
		layers.baseDirectDiffuse +
		layers.baseDirectSpecular +
		layers.baseDiffuseIBL +
		layers.baseSpecularIBL +
		layers.sheenDirect +
		layers.sheenIBL;
}

vec3 composeRewriteLayeredPBR(in SurfaceFrame frame, in MaterialParams params, in LayerContributions layers)
{
	vec3 directBase = layers.baseDirectDiffuse + layers.baseDirectSpecular;
	vec3 directSheen = layers.sheenDirect;
	vec3 ambientLayer = layers.baseDiffuseIBL + layers.baseSpecularIBL + layers.sheenIBL;
	vec3 outRGB = layers.emissive + ambientLayer + directBase + directSheen;

	if (params.clearcoat <= 0.0)
	{
		return outRGB;
	}

	vec3 Vdirect = normalize(vec3(0.0, 0.0, 1.0));
	float NdotVcoat = max(dot(frame.Ncoat, Vdirect), 0.0);
	vec3 clearcoatFresnel = fresnelSchlick(NdotVcoat, vec3(0.04), vec3(1.0));
	float clearcoatWeight = params.clearcoat * max(clearcoatFresnel.r, max(clearcoatFresnel.g, clearcoatFresnel.b));
	vec3 attenuatedAmbient = ambientLayer * (1.0 - clearcoatWeight);
	vec3 attenuatedDirectBase = directBase * (1.0 - clearcoatWeight);
	vec3 clearcoatLayer = layers.clearcoatDirect + layers.clearcoatIBL;
	return layers.emissive + attenuatedAmbient + attenuatedDirectBase + directSheen + clearcoatLayer;
}

MaterialParams gatherRewriteMaterialParams()
{
	MaterialParams params;

	params.baseColor = pbrLighting.albedo;
	params.emissive = material.emission * pbrLighting.emissiveStrength;
	params.metallic = clamp(pbrLighting.metallic, 0.0, 1.0);
	params.roughness = clamp(pbrLighting.roughness, 0.0001, 1.0);
	params.ambientOcclusion = clamp(pbrLighting.ambientOcclusion, 0.0001, 1.0);
	params.transmission = pbrLighting.transmission;
	params.ior = pbrLighting.ior;
	params.clearcoat = clamp(pbrLighting.clearcoat, 0.0, 1.0);
	params.clearcoatRoughness = clamp(pbrLighting.clearcoatRoughness, 0.0001, 1.0);
	params.sheenColor = clamp(pbrLighting.sheenColor, vec3(0.0), vec3(1.0));
	params.sheenRoughness = clamp(pbrLighting.sheenRoughness, 0.0001, 1.0);
	params.anisotropyStrength = pbrLighting.anisotropyStrength;
	params.anisotropyRotation = pbrLighting.anisotropyRotation;
	params.iridescenceFactor = pbrLighting.iridescenceFactor;
	params.iridescenceIor = pbrLighting.iridescenceIor;
	params.iridescenceThickness = pbrLighting.iridescenceThicknessMax;
	params.thickness = pbrLighting.thicknessFactor;
	params.diffuseTransmissionFactor = pbrLighting.diffuseTransmissionFactor;
	params.diffuseTransmissionColor = pbrLighting.diffuseTransmissionColorFactor;
	params.attenuationColor = pbrLighting.attenuationColor;
	params.attenuationDistance = pbrLighting.attenuationDistance;
	params.specularFactor = pbrLighting.specularFactor;
	params.specularColor = pbrLighting.specularColorFactor;
	params.useSpecGloss = useSpecularGlossiness;
	params.unlit = pbrLighting.unlit;

	params.baseColor = computeBaseColor(
		getAlbedoUV(),
		pbrLighting.albedo,
		albedoMap,
		hasAlbedoMap,
		v_color.rgb,
		hasVertexColors
	);
	if (hasEmissiveMap)
	{
		vec3 emissionFactor = guardFactorColor(material.emission, 0.01);
		params.emissive = texture(emissiveMap, getEmissiveUV()).rgb * emissionFactor * pbrLighting.emissiveStrength;
	}
	if (hasAOMap)
	{
		float texAO = samplePackedChannelValue(aoMap, hasAOMap, getAOUV(),
			aoChannel, aoInvert, aoScale, aoBias, pbrLighting.ambientOcclusion);
		params.ambientOcclusion = clamp(mix(1.0, texAO, pbrLighting.occlusionStrength), 0.0001, 1.0);
	}
	if (hasTransmissionMap)
	{
		params.transmission *= texture(transmissionMap, getTransmissionUV()).r;
	}
	if (hasIORMap)
	{
		params.ior = texture(iorMap, getIORUV()).r;
	}
	if (hasSheenColorMap)
	{
		params.sheenColor *= texture(sheenColorMap, getSheenColorUV()).rgb;
	}
	if (hasSheenRoughnessMap)
	{
		params.sheenRoughness *= texture(sheenRoughnessMap, getSheenRoughnessUV()).r;
	}
	if (hasClearcoatMap)
	{
		params.clearcoat *= texture(clearcoatColorMap, getClearcoatUV()).r;
	}
	if (hasClearcoatRoughnessMap)
	{
		params.clearcoatRoughness *= texture(clearcoatRoughnessMap, getClearcoatRoughnessUV()).g;
	}
	if (hasSpecularFactorMap)
	{
		params.specularFactor *= texture(specularFactorMap, getSpecularFactorUV()).a;
	}
	if (hasSpecularColorMap)
	{
		params.specularColor *= texture(specularColorMap, getSpecularColorUV()).rgb;
	}
	if (hasIridescenceMap)
	{
		params.iridescenceFactor *= texture(iridescenceMap, getIridescenceUV()).r;
	}
	if (hasIridescenceThicknessMap)
	{
		float thicknessNorm = texture(iridescenceThicknessMap, getIridescenceThicknessUV()).g;
		params.iridescenceThickness = mix(pbrLighting.iridescenceThicknessMin, pbrLighting.iridescenceThicknessMax, thicknessNorm);
	}
	if (hasThicknessMap)
	{
		vec4 thicknessTexel = texture(thicknessMap, getThicknessUV());
		float thicknessSample = hasThicknessAlpha ? thicknessTexel.a : thicknessTexel.g;
		params.thickness *= thicknessSample;
	}
	if (hasDiffuseTransmissionMap)
	{
		params.diffuseTransmissionFactor *= texture(diffuseTransmissionMap, getDiffuseTransmissionUV()).a;
	}
	if (hasDiffuseTransmissionColorMap)
	{
		params.diffuseTransmissionColor *= texture(diffuseTransmissionColorMap, getDiffuseTransmissionColorUV()).rgb;
	}

	params.baseColor = clamp(params.baseColor, vec3(0.0), vec3(1.0));
	params.sheenColor = clamp(params.sheenColor, vec3(0.0), vec3(1.0));
	params.specularColor = clamp(params.specularColor, vec3(0.0), vec3(1.0));
	params.clearcoat = clamp(params.clearcoat, 0.0, 1.0);
	params.clearcoatRoughness = clamp(params.clearcoatRoughness, 0.0001, 1.0);
	params.sheenRoughness = clamp(params.sheenRoughness, 0.0001, 1.0);
	params.roughness = clamp(params.roughness, 0.0001, 1.0);
	params.metallic = clamp(params.metallic, 0.0, 1.0);

	vec3 dielectricF0 = computeRewriteDielectricF0(
		params.ior,
		params.specularFactor,
		params.specularColor,
		params.useSpecGloss,
		specularColor
	);
	params.dielectricF0 = dielectricF0;
	params.F0 = mix(dielectricF0, params.baseColor, params.metallic);
	params.F90 = computeRewriteF90(params.metallic, params.specularFactor);

	return params;
}


// ----------------------------------------------------------------------------
// Calculate PBR lighting based on the render mode
vec4 calculatePBRLighting(int renderMode, float side) // side 1 = front, -1 = back
{
	if (kUseRewrittenBasePBR)
	{
		return calculatePBRLightingRewritten(renderMode, side);
	}

	vec3	normal = v_normal * side;

	vec3	albedo;
	float	metallic;
	float	roughness;
	float	normalScale;
	float	ambientOcclusion;
	float	occlusionStrength;
	float	transmission;
	float	ior;
	vec3	sheenColor;
	float	sheenRoughness;
	float	clearcoat;
	float	clearcoatRoughness;
	vec3	clearcoatNormal;

	// New glTF extension parameters
	float	specularFactor;
	vec3	specularColorFactor;
	float	anisotropyStrength;
	float	anisotropyRotation;
	float	iridescenceFactor;
	float	iridescenceIor;
	float	iridescenceThickness;
	float	thicknessFactor;
	float	dispersion;
	float	attenuationDistance;
	vec3	attenuationColor;
	bool	unlit;
	float	diffuseTransmissionFactor;
	vec3	diffuseTransmissionColorFactor;
	float	emissiveStrength;

	vec3	N;
	vec3	V_direct;
	vec3	L;

	// Pre-compute reflection view (used by IBL, clearcoat, transmission)
	vec3 V_reflect_base = normalize(cameraDir);
	vec3 V_reflect_offset = normalize(cameraPos - v_reflectionPosition);
	vec3 V_reflect = normalize(V_reflect_base - V_reflect_offset * 0.3);

	V_direct = normalize(vec3(0, 0, 1));
	L = normalize(lightSource.position - v_position);

	// Optional shadows affecting direct terms
	float lightShadowFactor = 0.0;
	if (useDefaultLights && shadowsEnabled && displayMode == 3 && (selfShadowsEnabled || floorRendering))
	{
		float s = calculateShadowVariableKernel(
			fs_in_shadow.FragPosLightSpace,
			fs_in_shadow.FragPos,
			fs_in_shadow.lightPos
		);
		lightShadowFactor = clamp(s, 0.0, 0.85); // a bit stronger on direct light
	}
	float lightFactor = 1.0 - lightShadowFactor;

	vec2 clippedTexCoord = v_texCoord0;
	vec4 textureColor = vec4(1.0);

	float blendFactor = float(isGLTFMaterial);

	// --- Initialize material properties from uniforms ---	
	N = normalize(normal);
	albedo = pbrLighting.albedo;
	metallic = pbrLighting.metallic;
	roughness = clamp(pbrLighting.roughness, 0.001, 1.0);
	normalScale = pbrLighting.normalScale;
	ambientOcclusion = pbrLighting.ambientOcclusion;
	occlusionStrength = pbrLighting.occlusionStrength;
	transmission = pbrLighting.transmission;
	ior = pbrLighting.ior;
	sheenColor = pbrLighting.sheenColor;
	sheenRoughness = pbrLighting.sheenRoughness;
	clearcoat = pbrLighting.clearcoat;
	clearcoatRoughness = pbrLighting.clearcoatRoughness;
	clearcoatNormal = normalize(v_reflectionNormal);
	specularFactor = pbrLighting.specularFactor;
	specularColorFactor = pbrLighting.specularColorFactor;
	anisotropyStrength = pbrLighting.anisotropyStrength;
	anisotropyRotation = pbrLighting.anisotropyRotation;
	iridescenceFactor = pbrLighting.iridescenceFactor;
	iridescenceIor = pbrLighting.iridescenceIor;
	iridescenceThickness = (pbrLighting.iridescenceThicknessMin + pbrLighting.iridescenceThicknessMax) * 0.5;
	thicknessFactor = pbrLighting.thicknessFactor;
	dispersion = pbrLighting.dispersion;
	attenuationDistance = pbrLighting.attenuationDistance;
	attenuationColor = pbrLighting.attenuationColor;
	unlit = pbrLighting.unlit;
	diffuseTransmissionFactor = pbrLighting.diffuseTransmissionFactor;
	diffuseTransmissionColorFactor = pbrLighting.diffuseTransmissionColorFactor;
	emissiveStrength = pbrLighting.emissiveStrength;


	// Normal map / Parallax
	if (hasNormalMap)  N = calcBumpedNormal(normalMap, getNormalUV()) * side;
	else               N = normalize(normal);

	 // Apply scale to XY only
     N.xy *= normalScale;
     N = normalize(N);

	if (hasHeightMap)
	{
		clippedTexCoord = applyParallaxMapping(getHeightUV(), heightMap, heightScale, hasHeightMap);
		N = calcBumpedNormal(normalMap, clippedTexCoord) * side;
	}

	// Albedo (grayscale-tint logic via computeBaseColor)
	if (hasAlbedoMap)
	{
		textureColor = texture(albedoMap, getAlbedoUV());
		vec3 texRGB_L = pow(textureColor.rgb, vec3(2.2));
		float colorDeviation = length(pbrLighting.albedo - vec3(1.0));
		if (colorDeviation < 0.1)
		{
			albedo = texRGB_L;
		}
		else
		{
			albedo = computeBaseColor(getAlbedoUV(),
				pbrLighting.albedo, // sRGB in function; it converts internally
				albedoMap,
				hasAlbedoMap,
				vec3(1.0), false);
		}
	}
	else
	{
		albedo = pbrLighting.albedo;
	}

	if (hasVertexColors)
		albedo *= v_color.rgb;


	if (length(v_normal) < 0.01)
	{
		// Return actual base color without PBR lighting
		return vec4(albedo, 1.0);
	}

	vec3 specularGlossSpecular = vec3(1.0);
	if (useSpecularGlossiness)
	{
		specularGlossSpecular = specularColor;
		float glossinessVal = glossinessFactor;
	
		// Sample diffuse with proper conversions
		if (hasDiffuseMap)
		{
			albedo = texture(diffuseMap, getDiffuseUV()).rgb;
		}
		else
		{
			albedo = diffuseFactor;
		}
	
		if (hasSpecularGlossinessMap)
		{
			vec4 sampledSpecGloss = texture(specularGlossinessMap, getSpecularGlossinessUV());			
			specularGlossSpecular *= sampledSpecGloss.rgb;			
			glossinessVal *= sampledSpecGloss.a;
		}
	
		metallic = 0.0;
		roughness = 1.0 - glossinessVal;
		roughness = clamp(roughness, 0.0, 1.0);
	}
	else  // Metallic-Roughness workflow
	{
		// --- packed-channel aware PBR sampling ---
		// Metallic
		float texMetallic = samplePackedChannelValue(metallicMap, hasMetallicMap, getMetallicUV(),
			metallicChannel, metallicInvert,
			metallicScale, metallicBias,
			isGLTFMaterial ? 1.0 : pbrLighting.metallic
		);
		float metallicFactor = blendFactor > 0.5 ? pbrLighting.metallic : guardFactorScalar(pbrLighting.metallic, 0.01);
		metallic = mix(texMetallic, metallicFactor * texMetallic, blendFactor);
		metallic = clamp(metallic, 0.0, 1.0);

		// Roughness
		float texRoughness = samplePackedChannelValue(roughnessMap, hasRoughnessMap, getRoughnessUV(),
			roughnessChannel, roughnessInvert,
			roughnessScale, roughnessBias,
			isGLTFMaterial ? 1.0 : pbrLighting.roughness
		);
		roughness = mix(texRoughness, pbrLighting.roughness * texRoughness, blendFactor);
		roughness = clamp(roughness, 0.0001, 1.0);

		// Specular (KHR_materials_specular)
		float texSpecularFactor = hasSpecularFactorMap ? texture(specularFactorMap, getSpecularFactorUV()).a : 1.0;
		specularFactor = mix(texSpecularFactor, pbrLighting.specularFactor * texSpecularFactor, blendFactor);

		vec3 texSpecularColor = hasSpecularColorMap ? texture(specularColorMap, getSpecularColorUV()).rgb : vec3(1.0);
		specularColorFactor = mix(texSpecularColor, pbrLighting.specularColorFactor * texSpecularColor, blendFactor);
	}

	// Ambient Occlusion (applies to both)
	float texAO = samplePackedChannelValue(aoMap, hasAOMap, getAOUV(),
		aoChannel, aoInvert, aoScale, aoBias,
		isGLTFMaterial ? 1.0 : pbrLighting.ambientOcclusion);
	ambientOcclusion = mix(texAO, pbrLighting.ambientOcclusion * texAO, blendFactor);
	ambientOcclusion = clamp(ambientOcclusion, 0.0001, 1.0);
	ambientOcclusion = mix(1.0, ambientOcclusion, occlusionStrength);

	// Anisotropy (KHR_materials_anisotropy)
	AnisotropyData anisoData;
	if (hasAnisotropyMap)
	{
		vec3 anisoTexel = texture(anisotropyMap, getAnisotropyUV()).rgb;
		anisoData = decodeAnisotropyTexture(
			anisoTexel,
			pbrLighting.anisotropyStrength,
			pbrLighting.anisotropyRotation,
			true
		);
	}
	else
	{
		anisoData = decodeAnisotropyTexture(
			vec3(1.0, 0.5, 1.0),  // Default texture value
			pbrLighting.anisotropyStrength,
			pbrLighting.anisotropyRotation,
			false
		);
	}

	anisotropyStrength = anisoData.strength;
	anisotropyRotation = anisoData.rotation;


	// Early out for unlit materials
	if (unlit)
	{
		vec3 unlitColor = albedo;

		// Apply emissive
		vec3 emissive_L = material.emission;
		if (hasEmissiveMap)
		{
			vec3 emission = material.emission;
			vec3 emissionFactor = guardFactorColor(emission, 0.01);
			emissive_L = texture(emissiveMap, getEmissiveUV()).rgb * emissionFactor;
		}

		// Apply emissive strength
		emissive_L *= emissiveStrength;

		unlitColor += emissive_L;

		if (hdrToneMapping) unlitColor = applyToneMapping(unlitColor);
		if (gammaCorrection) unlitColor = pow(unlitColor, vec3(1.0 / screenGamma));

		return vec4(unlitColor, 1.0);
	}

	// ============================================================================
	// LIGHT ACCUMULATION VARIABLES
	// ============================================================================
	vec3 directDiffuse_L = vec3(0.0);
	vec3 directSpecular_L = vec3(0.0);
	vec3 transmission_L = vec3(0.0);
	vec3 sheen_L = vec3(0.0);
	vec3 sheenIBL_L = vec3(0.0);
	vec3 clearcoat_L = vec3(0.0);
	vec3 clearcoatIBL_L = vec3(0.0);
	vec3 iridescence_BRDF_L = vec3(0.0);
	vec3 iridescence_IBL_L = vec3(0.0);
	vec3 ambient_L = vec3(0.0);
	vec3 emissive_L = vec3(0.0);
	float clearcoatAttenuation = 0.0;

	// ============================================================================
	// BASE LAYER - F0 and Material Foundation
	// ============================================================================
	vec3 F0 = vec3(0.04);

	// Apply KHR_materials_ior and KHR_materials_specular for dielectrics
	if (metallic < 0.02)  // Only for non-metallic/dielectric materials
	{
		// First compute the base dielectric F0 from IOR
		float f0_from_ior = pow((ior - 1.0) / (ior + 1.0), 2.0);

		if (useSpecularGlossiness)
		{
			// For spec-gloss: use specular directly as F0
			F0 = clamp(specularGlossSpecular, vec3(0.0), vec3(1.0));
		}
		else if (specularFactor > 0.0)  // KHR_materials_specular is present
		{
			// Apply specular extension: multiply base F0 by color and factor
			F0 = vec3(f0_from_ior) * specularColorFactor * specularFactor;
		}
		else  // No KHR_materials_specular, use IOR directly
		{
			F0 = vec3(f0_from_ior);
			F0 = max(F0, vec3(0.04));
		}
	}

	// Mix with albedo for metals
	F0 = mix(F0, albedo, metallic);

	// ============================================================================
	// COMPUTE F90 FOR FRESNEL (KHR_materials_specular)
	// ============================================================================
	vec3 F90_dielectric = vec3(specularFactor);  // Already contains factor * texture.a
	vec3 F90 = mix(F90_dielectric, vec3(1.0), metallic);

	// Setup tangent space for anisotropy
	vec3 T = normalize(v_tangent - dot(v_tangent, N) * N);
	vec3 B = normalize(v_bitangent - dot(v_bitangent, N) * N);

	// Keep the anisotropy frame orthonormal while preserving the imported tangent basis.
	if (length(B) < 0.0001)
	{
		B = normalize(cross(N, T));
	}
	else
	{
		B = normalize(B - dot(B, T) * T);
		if (dot(cross(T, B), N) < 0.0)
		{
			B = -B;
		}
	}

	// ============================================================================
	// PRE-LAYER: IRIDESCENCE - Compute F0/F90 before using in BRDFs
	// ============================================================================
	vec3 F0_iridescent = F0;
	vec3 F90_iridescent = F90;

	iridescenceFactor = hasIridescenceMap ? texture(iridescenceMap, getIridescenceUV()).r : pbrLighting.iridescenceFactor;
	iridescenceIor = pbrLighting.iridescenceIor;

	// Use MAX thickness per KHR spec, not average
	if (hasIridescenceThicknessMap)
	{
		float thicknessNorm = texture(iridescenceThicknessMap, getIridescenceThicknessUV()).g;
		iridescenceThickness = mix(pbrLighting.iridescenceThicknessMin, pbrLighting.iridescenceThicknessMax, thicknessNorm);
	}
	else
	{
		iridescenceThickness = pbrLighting.iridescenceThicknessMax;
	}

	if (iridescenceFactor > 0.001)
	{
		float NdotV_view = clamp(dot(N, normalize(V_direct)), 0.0, 1.0);

		vec3 iridFresnel_dielectric = evalIridescence(
			1.0,                              // outsideIOR (air)
			iridescenceIor,
			NdotV_view,
			iridescenceThickness,
			F0,								  // dielectric F0
			F90_dielectric                    // F90
		);

		vec3 iridFresnel_metallic = evalIridescence(
			1.0,                              // outsideIOR (air)
			iridescenceIor,
			NdotV_view,
			iridescenceThickness,
			albedo,							  // metallic F0
			vec3(1.0)                         // F90
		);

		vec3 F0_irid = mix(iridFresnel_dielectric, iridFresnel_metallic, metallic);
		F0_iridescent = mix(F0, F0_irid, iridescenceFactor);
		F90_iridescent = mix(F90, vec3(1.0), iridescenceFactor);
	}

	// ============================================================================
	// DIFFUSE & SPECULAR BASE LAYER (DIRECT LIGHTING)
	// ============================================================================
	vec3 H = normalize(V_direct + L);

	vec3 specBRDF;
	if (anisotropyStrength > 0.0)
	{
		specBRDF = calculateAnisotropy(N, V_direct, L, T, B, anisotropyStrength, anisotropyRotation, roughness, F0_iridescent);
	}
	else
	{
		float NDF = distributionGGX(N, H, roughness);
		float G = geometrySmith(N, V_direct, L, roughness);
		vec3 F = fresnelSchlick(clamp(dot(H, V_direct), 0.0, 1.0), F0_iridescent, F90_iridescent);
		specBRDF = (NDF * G * F) / max(4.0 * max(dot(N, V_direct), 0.0) * max(dot(N, L), 0.0), 0.001);
	}

	vec3 kS = fresnelSchlick(clamp(dot(H, V_direct), 0.0, 1.0), F0_iridescent, F90_iridescent);
	vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

	float NdotL = max(dot(N, L), 0.0);


	// Calculate the light source component - single light as of now.
	vec3 lightSourceComponent = lightSource.ambient + lightSource.diffuse + lightSource.specular;

	// Sample diffuse transmission factor and color
	float diffuseTrans_factor = diffuseTransmissionFactor;
	vec3 diffuseTrans_color = diffuseTransmissionColorFactor;
	if (hasDiffuseTransmissionMap)
	{
		float dtf = guardFactorScalar(diffuseTransmissionFactor, 0.01);
		diffuseTrans_factor = dtf * texture(diffuseTransmissionMap, getDiffuseTransmissionUV()).a;
	}
	if (hasDiffuseTransmissionColorMap)
	{
		vec3 dtc = guardFactorColor(diffuseTransmissionColorFactor, 0.01);
		diffuseTrans_color = dtc * texture(diffuseTransmissionColorMap, getDiffuseTransmissionColorUV()).rgb;
	}

	// ============================================================================
	// TRANSMISSION - Sample once, use for both punctual and fallback
	// ============================================================================
	transmission = hasTransmissionMap ? texture(transmissionMap, getTransmissionUV()).r : pbrLighting.transmission;
	ior = hasIORMap ? texture(iorMap, getIORUV()).r : pbrLighting.ior;

	// KHR_materials_volume thickness is thicknessFactor modulated by thicknessTexture.
	// Compute it once and reuse it consistently across all transmission/volume paths.
	float volumeThickness = thicknessFactor;
	if (hasThicknessMap)
	{
		vec4 thicknessTexel = texture(thicknessMap, getThicknessUV());
		float thicknessSample = hasThicknessAlpha ? thicknessTexel.a : thicknessTexel.g;
		float thicknessFactor_safe = guardFactorScalar(thicknessFactor, 0.01);
		volumeThickness = thicknessFactor_safe * thicknessSample;
	}

	// === Punctual lights (KHR_lights_punctual) ===
	if (hasPunctualLights && usePunctualLights)
	{
		for (int i = 0; i < lightCount; ++i)
		{
			PunctualLight light = lights[i];

			// ====================================================================
			// Calculate light direction and attenuation
			// ====================================================================
			vec3 pointToLight;
			vec3 lightIntensity;

			if (light.type != LightType_Directional)
			{
				// Point or Spot light
				vec3 fragPosView = vec3(viewMatrix * vec4(v_position, 1.0));
				vec3 lightPosView = vec3(viewMatrix * vec4(light.position, 1.0));
				pointToLight = lightPosView - fragPosView;
				float distance = length(pointToLight);

				// Range attenuation
				float rangeAttenuation = 1.0;
				if (light.range > 0.0)
				{
					float distAttenuation = 1.0 - pow(distance / light.range, 4.0);
					rangeAttenuation = max(min(distAttenuation, 1.0), 0.0) / (distance * distance);
				}
				else
				{
					rangeAttenuation = 1.0 / (distance * distance);
				}

				// Spot cone attenuation
				float spotAttenuation = 1.0;
				if (light.type == LightType_Spot)
				{
					vec3 lightDirView = normalize(mat3(viewMatrix) * light.direction);
					float actualCos = dot(lightDirView, normalize(-pointToLight));
					if (actualCos > light.outerConeCos)
					{
						if (actualCos < light.innerConeCos)
						{
							float angularAtten = (actualCos - light.outerConeCos) / (light.innerConeCos - light.outerConeCos);
							spotAttenuation = angularAtten * angularAtten;
						}
						else
						{
							spotAttenuation = 1.0;
						}
					}
					else
					{
						spotAttenuation = 0.0;
					}
				}

				lightIntensity = rangeAttenuation * spotAttenuation * light.intensity * light.color;
			}
			else
			{
				// Directional light
				pointToLight = -normalize(mat3(viewMatrix) * light.direction);
				lightIntensity = light.intensity * light.color;
			}

			vec3 l_dir = normalize(pointToLight);
			float NdotL_light = max(dot(N, l_dir), 0.0);

			// ====================================================================
			// DIFFUSE with Transmission & Scattering (same logic as single light)
			// ====================================================================
			vec3 l_diffuse = vec3(0.0);

			if (hasVolumeScattering && volumeThickness > 0.0)
			{
				// Scattering approach: REPLACE front diffuse with transmission color
				vec3 singleScatter = multiToSingleScatter();
				vec3 l_diffuse_front = lightIntensity * NdotL_light * (diffuseTrans_color / PI) * lightFactor * singleScatter;

				vec3 l_diffuse_btdf = vec3(0.0);
				if (dot(N, l_dir) < 0.0)
				{
					float diffuseNdotL = max(dot(-N, l_dir), 0.0);
					l_diffuse_btdf = lightIntensity * diffuseNdotL * (diffuseTrans_color / PI) * lightFactor;

					if (volumeThickness > 0.0)
					{
						vec3 refractDir = refract(-V_reflect, N, 1.0 / ior);
						vec3 transmissionRay = refractDir * volumeThickness;
						float transmissionDistance = length(transmissionRay);
						vec3 transmittance = pow(attenuationColor, vec3(transmissionDistance / max(attenuationDistance, 0.0001)));
						l_diffuse_btdf *= transmittance;
					}

					l_diffuse_front += l_diffuse_btdf * (1.0 - singleScatter) * singleScatter;
				}
				l_diffuse = l_diffuse_front * diffuseTrans_factor;

			}
			else
			{
				// PBR approach: standard diffuse + transmission BTDF
				vec3 l_diffuse_front = kD * albedo / PI * lightIntensity * NdotL_light * lightFactor;
				l_diffuse_front = l_diffuse_front * (1.0 - diffuseTrans_factor);

				vec3 l_diffuse_btdf = vec3(0.0);
				if (dot(N, l_dir) < 0.0)
				{
					float diffuseNdotL = max(dot(-N, l_dir), 0.0);
					l_diffuse_btdf = lightIntensity * diffuseNdotL * (diffuseTrans_color / PI) * lightFactor;

					if (volumeThickness > 0.0)
					{
						vec3 refractDir = refract(-V_reflect, N, 1.0 / ior);
						vec3 transmissionRay = refractDir * volumeThickness;
						float transmissionDistance = length(transmissionRay);
						vec3 transmittance = pow(attenuationColor, vec3(transmissionDistance / max(attenuationDistance, 0.0001)));
						l_diffuse_btdf *= transmittance;
					}

					l_diffuse_front += l_diffuse_btdf * diffuseTrans_factor;
				}
				l_diffuse = l_diffuse_front;
			}

			// Apply transmission blending
			if (transmission > 0.0)
			{
				l_diffuse = mix(l_diffuse, kD * albedo / PI * lightIntensity * NdotL_light * lightFactor, transmission);
			}

			directDiffuse_L += l_diffuse;

			// ====================================================================
			// SPECULAR
			// ====================================================================
			vec3 l_specular = vec3(0.0);
			if (NdotL_light > 0.0)
			{
				vec3 h_light = normalize(l_dir + V_direct);
				float NdotH_light = max(dot(N, h_light), 0.0);
				float VdotH_light = max(dot(V_direct, h_light), 0.0);

				float NDF = distributionGGX(N, h_light, roughness);
				float G = geometrySmith(N, V_direct, l_dir, roughness);
				vec3 F = fresnelSchlick(clamp(VdotH_light, 0.0, 1.0), F0_iridescent, F90_iridescent);
				vec3 specBRDF_light = (NDF * G * F) / max(4.0 * max(dot(N, V_direct), 0.0) * NdotL_light, 0.001);

				l_specular = specBRDF_light * lightIntensity * NdotL_light * lightFactor;
			}

			directSpecular_L += l_specular;

			// ====================================================================
			// TRANSMISSION (KHR_materials_transmission) - Per Light
			// ====================================================================
					
			// Calculate transmission for this light
			if (transmission > 0.0)
			{
				vec3 l_transmission = calculateTransmission(N, V_direct, l_dir, transmission, ior, albedo);
				l_transmission *= lightIntensity * lightFactor;
				transmission_L += l_transmission;
			}
		}
	}
	else if(useDefaultLights || floorRendering)
	{
		// ========================================================================
		// FALLBACK: Single legacy light - Apply SAME logic as punctual lights
		// ========================================================================

		// Use the existing L direction (already calculated before this section)
		vec3 l_dir = L;
		float NdotL_light = NdotL;
		vec3 lightSourceComponent = lightSource.ambient + lightSource.diffuse + lightSource.specular;
		vec3 lightIntensity = lightSourceComponent;

		// ====================================================================
		// DIFFUSE with Transmission & Scattering (same logic as punctual)
		// ====================================================================
		vec3 l_diffuse = vec3(0.0);

		if (hasVolumeScattering && volumeThickness > 0.0)
		{
			// Scattering approach: REPLACE front diffuse with transmission color
			vec3 singleScatter = multiToSingleScatter();
			vec3 l_diffuse_front = lightIntensity * NdotL_light * (diffuseTrans_color / PI) * lightFactor * singleScatter;

			vec3 l_diffuse_btdf = vec3(0.0);
			if (dot(N, l_dir) < 0.0)
			{
				float diffuseNdotL = max(dot(-N, l_dir), 0.0);
				l_diffuse_btdf = lightIntensity * diffuseNdotL * (diffuseTrans_color / PI) * lightFactor;

				if (volumeThickness > 0.0)
				{
					vec3 refractDir = refract(-V_reflect, N, 1.0 / ior);
					vec3 transmissionRay = refractDir * volumeThickness;
					float transmissionDistance = length(transmissionRay);
					vec3 transmittance = pow(attenuationColor, vec3(transmissionDistance / max(attenuationDistance, 0.0001)));
					l_diffuse_btdf *= transmittance;
				}

				l_diffuse_front += l_diffuse_btdf * (1.0 - singleScatter) * singleScatter;
			}
			l_diffuse = l_diffuse_front * diffuseTrans_factor;

		}
		else
		{
			// PBR approach: standard diffuse + transmission BTDF
			vec3 l_diffuse_front = kD * albedo / PI * lightIntensity * NdotL_light * lightFactor;
			l_diffuse_front = l_diffuse_front * (1.0 - diffuseTrans_factor);

			vec3 l_diffuse_btdf = vec3(0.0);
			if (dot(N, l_dir) < 0.0)
			{
				float diffuseNdotL = max(dot(-N, l_dir), 0.0);
				l_diffuse_btdf = lightIntensity * diffuseNdotL * (diffuseTrans_color / PI) * lightFactor;

				if (volumeThickness > 0.0)
				{
					vec3 refractDir = refract(-V_reflect, N, 1.0 / ior);
					vec3 transmissionRay = refractDir * volumeThickness;
					float transmissionDistance = length(transmissionRay);
					vec3 transmittance = pow(attenuationColor, vec3(transmissionDistance / max(attenuationDistance, 0.0001)));
					l_diffuse_btdf *= transmittance;
				}

				l_diffuse_front += l_diffuse_btdf * diffuseTrans_factor;
			}
			l_diffuse = l_diffuse_front;
		}

		// Apply transmission blending
		if (transmission > 0.0)
		{
			l_diffuse = mix(l_diffuse, kD * albedo / PI * lightIntensity * NdotL_light * lightFactor, transmission);
		}

		directDiffuse_L = l_diffuse;

		// ====================================================================
		// SPECULAR (same as punctual)
		// ====================================================================
		directSpecular_L = specBRDF * lightIntensity * NdotL_light * lightFactor;

		// ====================================================================
		// TRANSMISSION (same as punctual)
		// ====================================================================
		if (transmission > 0.0)
		{
			transmission_L = calculateTransmission(N, V_direct, L, transmission, ior, albedo);
		}
	}

	// ============================================================================
	// SHEEN
	// ============================================================================
	if (hasSheenColorMap)
	{
		vec3 sc = texture(sheenColorMap, getSheenColorUV()).rgb;
		vec3 sheenFactor = guardFactorColor(pbrLighting.sheenColor, 0.01);
		sheenColor = sc * sheenFactor;
	}
	else
	{
		sheenColor = pbrLighting.sheenColor;
	}
	sheenColor = clamp(sheenColor, vec3(0.0), vec3(1.0));
	
	if (hasSheenRoughnessMap)
	{
		float sr = texture(sheenRoughnessMap, getSheenRoughnessUV()).r;
		float sheenRoughFactor = guardFactorScalar(pbrLighting.sheenRoughness, 0.01);
		sheenRoughness = sr * sheenRoughFactor;
	}
	else
	{
		sheenRoughness = pbrLighting.sheenRoughness;
	}
	sheenRoughness = clamp(sheenRoughness, 0.0001, 1.0);

	if (length(sheenColor) > 0.0)
	{
		// Direct sheen
		sheen_L = calculateSheen(N, V_direct, L, sheenColor, sheenRoughness);

		// Sheen IBL
		sheenIBL_L = calculateSheenIBL(v_reflectionNormal, V_reflect, sheenRoughness, sheenColor);
	}

	// ============================================================================
	// CLEARCOAT
	// ============================================================================
	if (hasClearcoatMap)
	{
		float cc = texture(clearcoatColorMap, getClearcoatUV()).r;
		float clearcoatFactor = guardFactorScalar(pbrLighting.clearcoat, 0.01);
		clearcoat = cc * clearcoatFactor;
	}
	else
	{
		clearcoat = pbrLighting.clearcoat;
	}
	clearcoat = clamp(clearcoat, 0.0, 1.0);

	if (hasClearcoatRoughnessMap)
	{
		float ccr = texture(clearcoatRoughnessMap, getClearcoatRoughnessUV()).g;
		float ccRoughFactor = guardFactorScalar(pbrLighting.clearcoatRoughness, 0.01);
		clearcoatRoughness = ccr * ccRoughFactor;
	}
	else
	{
		clearcoatRoughness = pbrLighting.clearcoatRoughness;
	}
	clearcoatRoughness = clamp(clearcoatRoughness, 0.0001, 1.0);

	if (hasClearcoatNormalMap)
	{
		clearcoatNormal = calcBumpedNormal(clearcoatNormalMap, getClearcoatNormalUV()) * side;
		clearcoatNormal.xy *= clearcoatNormalScale;
		clearcoatNormal = normalize(clearcoatNormal);
	}
	else
	{
		clearcoatNormal = N;
	}

	if (clearcoat > 0.0)
	{
		clearcoat_L = calculateClearcoat(N, V_direct, L, clearcoat, clearcoatRoughness, clearcoatNormal);
		clearcoatIBL_L = calculateClearcoatIBL(v_reflectionNormal, V_reflect, clearcoatNormal, clearcoatRoughness, clearcoat);

		// Calculate clearcoat Fresnel for proper layering (per KHR spec line 140)
		vec3 F0_clearcoat = vec3(0.04);
		float NdotV_coat = clamp(dot(clearcoatNormal, normalize(V_direct)), 0.0, 1.0);
		vec3 F_coat = F0_clearcoat + (vec3(1.0) - F0_clearcoat) * pow(clamp(1.0 - NdotV_coat, 0.0, 1.0), 5.0);

		// Weight for layering: clearcoat_weight = clearcoat * fresnel
		// (Use max of RGB to get a single scalar weight, or average - either works)
		clearcoatAttenuation = clearcoat * max(F_coat.r, max(F_coat.g, F_coat.b));
	}

	// Add clearcoat direct specular (will be composited with proper weighting later)
	directSpecular_L += clearcoat_L * lightSource.specular;


	// ============================================================================
	// IMAGE BASED LIGHTING (IBL) - AMBIENT
	// ============================================================================
	if (useIBL)
	{
		vec3 irradiance = texture(irradianceMap, N).rgb;
		vec3 diffuseIBL_L = irradiance * albedo;

		// Diffuse transmission IBL
		if (diffuseTransmissionFactor > 0.0)
		{
			vec3 diffuseTransmissionIBL_back = texture(irradianceMap, -N).rgb * diffuseTransmissionColorFactor;
			vec3 diffuseTransmissionIBL_front = texture(irradianceMap, N).rgb * diffuseTransmissionColorFactor;

			diffuseTransmissionIBL_back *= diffuseTrans_color;
			diffuseTransmissionIBL_front *= diffuseTrans_color;

			// Apply volume attenuation to back only
			if (volumeThickness > 0.0)
			{
				vec3 refractDir = refract(-V_reflect, N, 1.0 / ior);
				vec3 transmissionRay = refractDir * volumeThickness;
				float transmissionDistance = length(transmissionRay);
				vec3 transmittance = pow(attenuationColor, vec3(transmissionDistance / max(attenuationDistance, 0.0001)));
				diffuseTransmissionIBL_back *= transmittance;
			}

			if (hasVolumeScattering && volumeThickness > 0.0)
			{
				vec3 singleScatter = multiToSingleScatter();
				diffuseIBL_L = diffuseTransmissionIBL_front * singleScatter + diffuseTransmissionIBL_back * (1.0 - singleScatter) * singleScatter;
				diffuseIBL_L = diffuseIBL_L * diffuseTrans_factor;
			}
			else
			{
				diffuseIBL_L = mix(diffuseIBL_L, diffuseTransmissionIBL_back, diffuseTrans_factor);
			}
		}

		vec3 specIBL_L = vec3(0.0);

		if (envMapEnabled)
		{
			float dotNV = max(dot(N, V_direct), 0.0);
			vec3 F90_effective = max(vec3(1.0 - roughness), F0_iridescent);
			vec3 Fibl = fresnelSchlick(dotNV, F0_iridescent, F90_effective);
			vec3 kSibl = Fibl;
			vec3 kDibl = (vec3(1.0) - kSibl) * (1.0 - metallic);

			vec3 iblNormal = normalize(v_reflectionNormal);
			vec3 R = reflect(V_reflect, iblNormal);
			float iblRoughness = roughness;

			if (anisotropyStrength > 0.0)
			{
				float c_aniso = cos(anisotropyRotation);
				float s_aniso = sin(anisotropyRotation);
				vec3 T_ibl = normalize(c_aniso * T + s_aniso * B);
				vec3 B_ibl = normalize(-s_aniso * T + c_aniso * B);
				vec3 V_ibl = normalize(-V_direct);

				// Approximate anisotropic IBL by bending the reflection normal along the
				// authored anisotropy frame. This preserves the existing IBL pipeline
				// while making environment highlights rotate and stretch more plausibly.
				vec3 bentNormal = normalize(cross(cross(V_ibl, B_ibl), B_ibl));
				float bendMix = pow(clamp(1.0 - anisotropyStrength * (1.0 - roughness), 0.0, 1.0), 2.0);
				bentNormal = normalize(mix(bentNormal, iblNormal, bendMix));
				R = reflect(V_reflect, bentNormal);
				iblRoughness = clamp(roughness + anisotropyStrength * (1.0 - roughness) * 0.35, 0.0, 1.0);
			}

			const float MAX_REFLECTION_LOD = textureQueryLevels(prefilterMap) - 1.0;
			float lod = iblRoughness * MAX_REFLECTION_LOD;
			lod = clamp(lod, 0.0, MAX_REFLECTION_LOD);
			vec3 prefilteredColor = textureLod(prefilterMap, R, lod).rgb;
			prefilteredColor = max(prefilteredColor, vec3(0.0));
			prefilteredColor *= envMapExposure;

			vec2 brdf = texture(brdfLUT, vec2(dotNV, roughness)).rg;
			brdf = max(brdf, vec2(0.0));

			specIBL_L = prefilteredColor * (Fibl * brdf.x + brdf.y);
			
			// Multiply ambient occlusion
			ambient_L = (kDibl * diffuseIBL_L + specIBL_L) * ambientOcclusion;
		}
		else
		{
			vec3 kS0 = fresnelSchlick(max(dot(N, V_direct), 0.0), F0, vec3(1.0));
			vec3 kD0 = (vec3(1.0) - kS0) * (1.0 - metallic);			
			// Multiply ambient occlusion
			ambient_L = (kD0 * diffuseIBL_L) * ambientOcclusion;
		}

		// Apply clearcoat layering to ambient
		vec3 ambient_base = ambient_L * (1.0 - clearcoatAttenuation) + clearcoatIBL_L * clearcoatAttenuation;
		ambient_L = ambient_base + sheenIBL_L;
	}
	else
	{
		// No IBL - only direct lighting
		ambient_L = vec3(0.0);		
	}

	// ============================================================================
	// EMISSION
	// ============================================================================
	emissive_L = material.emission;
	if (hasEmissiveMap) 
	{
		vec3 emission = material.emission;
		vec3 emissionFactor = guardFactorColor(emission, 0.01);
		emissive_L = texture(emissiveMap, getEmissiveUV()).rgb * emissionFactor;
	}

	// Apply emissive strength
	emissive_L *= emissiveStrength;

	// Attenuate emission by clearcoat layer (per KHR spec line 152)
	// Only apply if clearcoat is active
	if (clearcoat > 0.0)
	{
		emissive_L *= (1.0 - clearcoatAttenuation);
	}

	// ============================================================================
	// CONSOLIDATE: COMBINE ALL LIGHT LAYERS
	// ============================================================================
	// Apply clearcoat weighting to all base contributions (except already-composited ambient_L)
	vec3 baseNoSpec_L = emissive_L + ambient_L + directDiffuse_L * (1.0 - clearcoatAttenuation) +
		transmission_L * (1.0 - clearcoatAttenuation) + sheen_L * (1.0 - clearcoatAttenuation);
	vec3 specOnly_L = directSpecular_L * (1.0 - clearcoatAttenuation); // Apply attenuation to specular

	// --- Floor override (non-reflected) ---
	if (floorRendering && !isReflectedPass)
	{
		float fa = clamp(floorAlpha, 0.0, 1.0);
		vec3 Nf = normalize(gl_FrontFacing ? v_normal : -v_normal);
		vec3 Vf = normalize(cameraPos - v_position);
		float NdotVf = clamp(dot(Nf, Vf), 0.0, 1.0);
		float fresDampen = mix(1.0 - floorFresnelDampen, 1.0, pow(1.0 - NdotVf, 5.0));

		vec3 floorRGB_L = baseNoSpec_L * fa + specOnly_L * (floorSpecularScale * fresDampen);

		if (hdrToneMapping) floorRGB_L = applyToneMapping(floorRGB_L);
		if (gammaCorrection) floorRGB_L = pow(floorRGB_L, vec3(1.0 / screenGamma));
		return vec4(floorRGB_L, fa);
	}

	vec3 outRGB = baseNoSpec_L + specOnly_L + clearcoat_L * lightSource.specular * clearcoatAttenuation;

	// ============================================================================
	// TRANSMISSION VOLUME & REFRACTION WITH DISPERSION (Mipmapped)
	// ============================================================================
	float transmissionFactor = pbrLighting.transmission;
	if (hasTransmissionMap)
	{
		float mapVal = texture(transmissionMap, getTransmissionUV()).r;
		transmissionFactor *= mapVal;
	}

	if (transmissionFactor > 0.0)
	{
		vec3 N_trans = normalize(v_reflectionNormal);

		if (dot(N_trans, -V_reflect) < 0.0)
		{
			N_trans = -N_trans;  // Flip normal to face viewer
		}

		float ior_trans = pbrLighting.ior;

		// --- THICKNESS CALCULATION ---
		float thickness = volumeThickness;

		// --- DISPERSION PARAMETER ---                
		vec3 transmittedLight;

		if (dispersion > 0.0)
		{
			// --- DISPERSION MODE: Per-channel refraction with different IORs ---
			float halfSpread = (ior_trans - 1.0) * 0.025 * dispersion;
			vec3 iors = vec3(
				ior_trans - halfSpread,  // Red (lowest IOR)
				ior_trans,               // Green (medium IOR)
				ior_trans + halfSpread   // Blue (highest IOR)
			);
			iors = max(iors, vec3(1.001));  // Clamp to valid range

			// Get per-channel refraction with proper 3D projection
			transmittedLight = getIBLVolumeRefractionPerChannel(
				N_trans,
				normalize(-V_reflect),  // View direction
				roughness,
				albedo,
				v_position,             // World position
				modelMatrix,
				iors,
				thickness,
				attenuationColor,
				attenuationDistance
			);
		}
		else
		{
			// --- NO DISPERSION: Single channel with 3D projection ---
			transmittedLight = getIBLVolumeRefraction(
				N_trans,
				normalize(-V_reflect),  // View direction
				roughness,
				albedo,
				v_position,             // World position
				modelMatrix,
				ior_trans,
				thickness,
				attenuationColor,
				attenuationDistance
			);
		}

		// --- FRESNEL BLENDING WITH IRIDESCENCE ---
		float NdotV = max(dot(N_trans, -V_reflect), 0.001);

		vec3 blendFresnel = vec3(1.0);  // Blending Fresnel (1.0 = full reflection, 0.0 = full transmission)

		if (iridescenceFactor > 0.001)
		{
			// Use iridescent Fresnel for blending - matches KHR spec
			// This makes iridescence act as a wavelength-dependent reflectance
			vec3 iridFresnel_dielectric = evalIridescence(
				1.0,                    // outsideIOR (air)
				iridescenceIor,
				NdotV,
				iridescenceThickness,
				vec3(pow((ior_trans - 1.0) / (ior_trans + 1.0), 2.0))  // base dielectric F0
			);

			// Blend between volume Fresnel and iridescent Fresnel based on iridescence strength
			float volumeFresnel = calculateVolumeFresnel(ior_trans, 1.0, NdotV);
			blendFresnel = mix(vec3(volumeFresnel), iridFresnel_dielectric, iridescenceFactor);
		}
		else
		{
			// No iridescence - use standard volume Fresnel
			float volumeFresnel = calculateVolumeFresnel(ior_trans, 1.0, NdotV);
			blendFresnel = vec3(volumeFresnel);
		}

		// Add an explicit environment reflection term for transmissive surfaces.
		// Using outRGB alone here makes glass read mostly as refraction because it does
		// not provide a distinct env-map reflection contribution in this path.
		vec3 transmissionEnvReflection = vec3(0.0);
		if (envMapEnabled)
		{
			float reflectionDotNV = clamp(dot(N_trans, normalize(-V_reflect)), 0.0, 1.0);
			vec3 transmissionReflectDir = normalize(reflect(V_reflect, N_trans));
			float maxReflectionLod = max(textureQueryLevels(prefilterMap) - 1.0, 0.0);
			float reflectionLod = clamp(roughness * maxReflectionLod, 0.0, maxReflectionLod);
			vec3 transmissionPrefilter = textureLod(prefilterMap, transmissionReflectDir, reflectionLod).rgb;
			transmissionPrefilter = max(transmissionPrefilter, vec3(0.0));
			transmissionPrefilter *= envMapExposure;

			vec2 transmissionBrdf = texture(brdfLUT, vec2(reflectionDotNV, roughness)).rg;
			transmissionBrdf = max(transmissionBrdf, vec2(0.0));

			// Keep the effect subtle and Fresnel-driven so glass gets a believable
			// environment sheen without overwhelming the transmitted image.
			transmissionEnvReflection =
				transmissionPrefilter *
				(blendFresnel * transmissionBrdf.x + transmissionBrdf.y) *
				ambientOcclusion *
				0.5;
		}

		// Blend reflection and transmission using the Fresnel (which may be iridescent)
		vec3 reflectionColor = outRGB;
		vec3 finalTransmission = mix(transmittedLight, reflectionColor, blendFresnel);
		finalTransmission += transmissionEnvReflection;

		// Apply transmission factor
		outRGB = mix(outRGB, finalTransmission, transmissionFactor);
	}

	// ============================================================================
	// TONE MAPPING & GAMMA CORRECTION
	// ============================================================================
	if (hdrToneMapping)
	{
		outRGB *= envMapExposure;
		outRGB = applyToneMapping(outRGB);
	}
	if (gammaCorrection)
	{
		outRGB = pow(outRGB, vec3(1.0 / screenGamma));
	}

	return vec4(outRGB, 1.0);
}

// ============================================================================
// GUARD HELPERS: Prevent texture nullification by zero factors
// ============================================================================

// Guard a scalar factor (e.g., roughness, metallic, sheen roughness)
// Returns the factor if non-zero, otherwise returns 1.0 (neutral)
float guardFactorScalar(float factor, float threshold)
{
    return factor < threshold ? 1.0 : factor;
}

// Guard a color/vector factor (e.g., sheenColor, specularColor)
// Returns the factor if non-black, otherwise returns white (neutral)
vec3 guardFactorColor(vec3 factor, float threshold)
{
    return length(factor) < threshold ? vec3(1.0) : factor;
}

// ========== CORE TEXTURE TRANSFORM FUNCTION ==========
vec2 getTransformedUV(int texCoordIndex, TextureTransform transform)
{
	// Step 1: Select the appropriate base UV coordinate set
	vec2 uv;
	switch (texCoordIndex)
	{
	case 1: uv = v_texCoord1; break;
	case 2: uv = v_texCoord2; break;
	case 3: uv = v_texCoord3; break;
	default: uv = v_texCoord0; break; // case 0 and fallback
	}

	// glTF 2.0 spec defines UV coordinates with origin at upper-left corner (0,0).
	// KHR_texture_transform operates in this image-space coordinate system.
	// Since images are not Y-flipped during load (matching KHR Sample Viewer),
	// we flip UV.y here and negate rotation to maintain spec compliance.	
	// Non-traditional approach: No image Y-flip at load (per KHR Sample Viewer).
	// Coordinate system compensation applied in shader:
	//   - UV.y flip: glTF operates in image-space (top-left origin)
	//   - Rotation negation: glTF rotates around origin (0,0), not image center
	// This matches Assimp's documented KHR_texture_transform coordinate conversion.
	// Reference: assimp/code/AssetLib/glTF2/glTF2Importer.cpp
	//   "transform.mRotation = -prop.TextureTransformExt_t.rotation; // must be negated"
	uv = vec2(uv.x, 1.0 - uv.y); 
	float angle = -transform.rotation;

	// Step 2: Apply KHR_texture_transform
	// Order: scale -> rotation -> offset
	
	//Scale
	uv = uv * transform.scale;

	// Apply rotation matrix	
	float cosR = cos(angle);
	float sinR = sin(angle);
	mat2 rotMat = mat2(cosR, sinR, -sinR, cosR);
	uv = rotMat * uv;

	// Apply offset
	uv = uv + transform.offset;

	return uv;
}

// ========== CONVENIENCE FUNCTIONS FOR EACH TEXTURE TYPE ==========
vec2 getAlbedoUV()
{
	return getTransformedUV(albedoTexTransform.texCoordIndex, albedoTexTransform);
}

vec2 getMetallicUV()
{
	return getTransformedUV(metallicTexTransform.texCoordIndex, metallicTexTransform);
}

vec2 getRoughnessUV()
{
	return getTransformedUV(roughnessTexTransform.texCoordIndex, roughnessTexTransform);
}

vec2 getNormalUV()
{
	return getTransformedUV(normalTexTransform.texCoordIndex, normalTexTransform);
}

vec2 getHeightUV()
{
	return getTransformedUV(heightTexTransform.texCoordIndex, heightTexTransform);
}

vec2 getAOUV()
{
	return getTransformedUV(aoTexTransform.texCoordIndex, aoTexTransform);
}

vec2 getOpacityUV()
{
	return getTransformedUV(opacityTexTransform.texCoordIndex, opacityTexTransform);
}

vec2 getEmissiveUV()
{
	return getTransformedUV(emissiveTexTransform.texCoordIndex, emissiveTexTransform);
}

vec2 getTransmissionUV()
{
	return getTransformedUV(transmissionTexTransform.texCoordIndex, transmissionTexTransform);
}

vec2 getIORUV()
{
	return getTransformedUV(iorTexTransform.texCoordIndex, iorTexTransform);
}

vec2 getSheenColorUV()
{
	return getTransformedUV(sheenColorTexTransform.texCoordIndex, sheenColorTexTransform);
}

vec2 getSheenRoughnessUV()
{
	return getTransformedUV(sheenRoughnessTexTransform.texCoordIndex, sheenRoughnessTexTransform);
}

vec2 getClearcoatUV()
{
	return getTransformedUV(clearcoatTexTransform.texCoordIndex, clearcoatTexTransform);
}

vec2 getClearcoatRoughnessUV()
{
	return getTransformedUV(clearcoatRoughnessTexTransform.texCoordIndex, clearcoatRoughnessTexTransform);
}

vec2 getClearcoatNormalUV()
{
	return getTransformedUV(clearcoatNormalTexTransform.texCoordIndex, clearcoatNormalTexTransform);
}

vec2 getSpecularFactorUV()
{
	return getTransformedUV(specularFactorTexTransform.texCoordIndex, specularFactorTexTransform);
}

vec2 getSpecularColorUV()
{
	return getTransformedUV(specularColorTexTransform.texCoordIndex, specularColorTexTransform);
}

vec2 getAnisotropyUV()
{
	return getTransformedUV(anisotropyTexTransform.texCoordIndex, anisotropyTexTransform);
}

vec2 getIridescenceUV()
{
	return getTransformedUV(iridescenceTexTransform.texCoordIndex, iridescenceTexTransform);
}

vec2 getIridescenceThicknessUV()
{
	return getTransformedUV(iridescenceThicknessTexTransform.texCoordIndex, iridescenceThicknessTexTransform);
}

vec2 getThicknessUV()
{
	return getTransformedUV(thicknessTexTransform.texCoordIndex, thicknessTexTransform);
}

vec2 getDiffuseTransmissionUV()
{
	return getTransformedUV(diffuseTransmissionTexTransform.texCoordIndex, diffuseTransmissionTexTransform);
}

vec2 getDiffuseTransmissionColorUV()
{
	return getTransformedUV(diffuseTransmissionColorTexTransform.texCoordIndex, diffuseTransmissionColorTexTransform);
}

vec2 getDiffuseUV()
{
	return getTransformedUV(diffuseTexTransform.texCoordIndex, diffuseTexTransform);
}

vec2 getSpecularGlossinessUV()
{
	return getTransformedUV(specularGlossinessTexTransform.texCoordIndex, specularGlossinessTexTransform);
}



// Legacy ADS texture UV functions
vec2 getDiffuseTextureUV()
{
	return getTransformedUV(diffuseTextureTransform.texCoordIndex, diffuseTextureTransform);
}

vec2 getSpecularTextureUV()
{
	return getTransformedUV(specularTextureTransform.texCoordIndex, specularTextureTransform);
}

vec2 getEmissiveTextureUV()
{
	return getTransformedUV(emissiveTextureTransform.texCoordIndex, emissiveTextureTransform);
}

vec2 getNormalTextureUV()
{
	return getTransformedUV(normalTextureTransform.texCoordIndex, normalTextureTransform);
}

vec2 getHeightTextureUV()
{
	return getTransformedUV(heightTextureTransform.texCoordIndex, heightTextureTransform);
}

vec2 getOpacityTextureUV()
{
	return getTransformedUV(opacityTextureTransform.texCoordIndex, opacityTextureTransform);
}

float samplePackedChannelValue(sampler2D tex, bool hasTexture, vec2 uv,
	int channel, int invert, float scale, float bias,
	float fallback)
{
	if (!hasTexture) return fallback;

	vec4 texel = texture(tex, uv);
	float val;

	if (channel >= 0)
	{
		val = pickChannel(texel, channel, invert, scale, bias);
	}
	else
	{
		val = texel.r * scale + bias;
		if (invert != 0) val = 1.0 - val;
	}

	return clamp(val, 0.0, 1.0);
}


// ----------------------------------------------------------------------------
// Advanced PBR Functions

// IOR-based Fresnel calculation
vec3 fresnelSchlickIOR(float cosTheta, float ior)
{
	float f0 = pow((ior - 1.0) / (ior + 1.0), 2.0);
	return vec3(f0) + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

// Charlie distribution for sheen (fabric-like materials)
float distributionCharlie(vec3 N, vec3 H, float roughness)
{
	float alpha = roughness * roughness;
	float invAlpha = 1.0 / alpha;
	float cos2h = dot(N, H) * dot(N, H);
	float sin2h = max(1.0 - cos2h, 0.0078125); // 2^(-7), so sin2h is always > 0
	return (2.0 + invAlpha) * pow(sin2h, invAlpha * 0.5) / (2.0 * PI);
}

// Charlie geometry function for sheen
float geometryCharlie(float NdotV, float roughness)
{
	float alpha = roughness * roughness;
	float sinTheta = sqrt(1.0 - NdotV * NdotV);
	return NdotV / (NdotV + alpha * sinTheta);
}

// Charlie D function variant for sheen IBL
float D_Charlie(float roughness, float NoH)
{
	float invAlpha = 1.0 / max(roughness, 0.001);
	float sin2h = max(1.0 - NoH * NoH, 0.0078125);
	return (2.0 + invAlpha) * pow(sin2h, invAlpha * 0.5) / (2.0 * PI);
}

// ============================================================================
// KHR_materials_sheen: Visibility Function with lambdaSheen Approximation
// ============================================================================

float lambdaSheenNumericHelper(float x, float alphaG)
{
	float oneMinusAlphaSq = (1.0 - alphaG) * (1.0 - alphaG);
	float a = mix(21.5473, 25.3245, oneMinusAlphaSq);
	float b = mix(3.82987, 3.32435, oneMinusAlphaSq);
	float c = mix(0.19823, 0.16801, oneMinusAlphaSq);
	float d = mix(-1.97760, -1.27393, oneMinusAlphaSq);
	float e = mix(-4.32054, -4.85967, oneMinusAlphaSq);
	return a / (1.0 + b * pow(x, c)) + d * x + e;
}


float lambdaSheen(float cosTheta, float alphaG)
{
	if (abs(cosTheta) < 0.5)
	{
		return exp(lambdaSheenNumericHelper(cosTheta, alphaG));
	}
	else
	{
		return exp(2.0 * lambdaSheenNumericHelper(0.5, alphaG) -
			lambdaSheenNumericHelper(1.0 - cosTheta, alphaG));
	}
}

float V_Sheen(float NdotL, float NdotV, float sheenRoughness)
{
	sheenRoughness = max(sheenRoughness, 0.000001);
	float alphaG = sheenRoughness * sheenRoughness;

	return clamp(1.0 / ((1.0 + lambdaSheen(NdotV, alphaG) +
		lambdaSheen(NdotL, alphaG)) *
		(4.0 * NdotV * NdotL)), 0.0, 1.0);
}

// ============================================================================
// Utility Functions
// ============================================================================

float max3(vec3 v)
{
	return max(max(v.x, v.y), v.z);
}

// ============================================================================
// KHR-compliant direct sheen calculation
// Formula: f_sheen = D_charlie(alpha, h) * V_sheen(NdotL, NdotV, alpha) * sheenColor
// sheenColor is already the complete color contribution (no metallic interaction)
// ============================================================================
vec3 calculateSheen(vec3 N, vec3 V, vec3 L, vec3 sheenColor, float sheenRoughness)
{
	vec3 H = normalize(V + L);
	float NdotL = clamp(dot(N, L), 0.0, 1.0);
	float NdotV = clamp(dot(N, V), 0.0, 1.0);
	float NdotH = clamp(dot(N, H), 0.0, 1.0);

	// Early exit for backfacing
	if (NdotL <= 0.0 || NdotV <= 0.0)
		return vec3(0.0);

	// Clamp roughness to safe range
	float sheenRoughFinal = clamp(sheenRoughness, 0.000001, 1.0);

	// D_Charlie distribution
	float D = D_Charlie(sheenRoughFinal, NdotH);

	// V_Sheen visibility (KHR-compliant, handles grazing angles correctly)
	float V_sheen = V_Sheen(NdotL, NdotV, sheenRoughFinal);

	// Sheen BRDF = D * V * sheenColor
	// (No Fresnel term - sheen color is already the full diffuse contribution)
	vec3 sheenBRDF = sheenColor * D * V_sheen;

	// Return: includes NdotL weighting for proper irradiance
	return sheenBRDF * NdotL;
}


// ============================================================================
// KHR-compliant sheen IBL calculation
// Formula: f_sheen_ibl = (L_ibl * sheenColor) * E_sheen
// where E_sheen is the directional-albedo from BRDF LUT (pre-integrated D*V)
// NO separate Fresnel or Geometry - both are in E_sheen already
// ============================================================================
vec3 calculateSheenIBL(vec3 N, vec3 V, float sheenRoughness, vec3 sheenColor)
{
	// Clamp roughness to safe range
	float sheenRoughFinal = clamp(sheenRoughness, 0.000001, 1.0);

	// Correct V direction (negative for reflection calculation - KHR spec)
	vec3 V_norm = normalize(-V);

	// Reflection vector from view direction
	vec3 R = reflect(V_norm, N);
	R = normalize(R);

	// LOD calculation for prefiltered environment map
	float maxLevels = textureQueryLevels(prefilterMap);
	float MAX_LOD = max(maxLevels - 1.0, 0.0);
	float lod = sheenRoughFinal * MAX_LOD;
	lod = clamp(lod, 0.0, MAX_LOD);

	// Sample prefiltered environment map
	vec3 prefilteredLight = textureLod(prefilterMap, R, lod).rgb;
	prefilteredLight = max(prefilteredLight, vec3(0.0));

	// BRDF LUT sampling for sheen directional-albedo
	// Blue channel contains pre-integrated D_charlie * V_sheen
	float NdotV_val = clamp(dot(N, V_norm), 0.0, 1.0);
	vec2 brdfCoord = clamp(vec2(NdotV_val, sheenRoughFinal), vec2(0.0), vec2(1.0));
	float E_sheen = texture(brdfLUT, brdfCoord).b;

	// KHR formula: only multiply by sheenColor and E_sheen
	// BRDF LUT already includes all BRDF integration (D * V)
	vec3 sheenIBL = prefilteredLight * sheenColor * E_sheen;

	return sheenIBL;
}


// Calculate clearcoat contribution - CORRECTED per KHR specification
vec3 calculateClearcoat(vec3 N, vec3 V, vec3 L, float clearcoat,
	float clearcoatRoughness, vec3 clearcoatNormal)
{
	clearcoat = clamp(clearcoat, 0.0, 1.0);
	if (clearcoat <= 0.0) return vec3(0.0);

	// Use clearcoatRoughness directly - NO modulation by normal variation
	float alphaRoughness = clearcoatRoughness * clearcoatRoughness;
	alphaRoughness = clamp(alphaRoughness, 0.0, 1.0);

	vec3 V_norm = normalize(V);
	vec3 L_norm = normalize(L);
	vec3 N_norm = normalize(clearcoatNormal);

	vec3 H = normalize(V_norm + L_norm);
	float NdotL = max(dot(N_norm, L_norm), 0.0);
	float NdotV = max(dot(N_norm, V_norm), 0.0);
	float NdotH = max(dot(N_norm, H), 0.0);

	// Compute BRDF terms WITHOUT Fresnel (per KHR spec)    
	float D = distributionGGX(N_norm, H, alphaRoughness);
	float G = geometrySmith(N_norm, V_norm, L_norm, alphaRoughness);
	float V_smith = G;

	// BRDF = D * V (NO Fresnel, NO denominator - V_Smith handles normalization)
	vec3 clearcoatBRDF = vec3(D * V_smith);

	// Return: clearcoat mask * NdotL * BRDF
	// Fresnel is applied LATER during composition, not here
	return clearcoatBRDF * clearcoat * NdotL;
}


// Improved calculateClearcoatIBL - CORRECTED per KHR specification
vec3 calculateClearcoatIBL(vec3 N, vec3 V, vec3 clearcoatNormal,
	float clearcoatRoughness, float clearcoat)
{
	vec3 V_norm = normalize(V);
	vec3 N_coat = normalize(clearcoatNormal);

	// Use clearcoat normal for reflection (more physically correct)
	//vec3 R = reflect(-V_norm, N_coat);
	vec3 R = reflect(V_norm, N);
	R = normalize(R);

	// Map roughness to LOD (squared mapping consistent with perceptual roughness)
	float maxLevels = textureQueryLevels(prefilterMap);
	float MAX_LOD = max(maxLevels - 1.0, 0.0);

	float lod = (clearcoatRoughness * clearcoatRoughness) * MAX_LOD;
	lod = clamp(lod, 0.0, MAX_LOD);

	// Sample prefiltered environment
	vec3 prefilteredColor = textureLod(prefilterMap, R, lod).rgb;
	prefilteredColor = max(prefilteredColor, vec3(0.0));

	// Compute Fresnel separately (NOT via BRDF LUT)
	// Use NdotV (not VdotH) for energy conservation per KHR spec line 157
	float dotNV = clamp(dot(N_coat, V_norm), 0.0, 1.0);
	vec3 F0 = vec3(0.04);  // Dielectric clearcoat (IOR 1.5)
	vec3 F = F0 + (vec3(1.0) - F0) * pow(clamp(1.0 - dotNV, 0.0, 1.0), 5.0);

	// Simple composition: prefiltered * Fresnel * clearcoat mask
	vec3 specIBL = prefilteredColor * F;

	return specIBL * clearcoat;
}


// ==== NEW GLTF EXTENSION IMPLEMENTATIONS ====

// NEW: Anisotropic visibility/masking function (Khronos spec line 174-181)
float V_GGX_anisotropic(float NdotL, float NdotV, float BdotV, float TdotV,
	float TdotL, float BdotL, float at, float ab)
{
	float GGXV = NdotL * length(vec3(at * TdotV, ab * BdotV, NdotV));
	float GGXL = NdotV * length(vec3(at * TdotL, ab * BdotL, NdotL));
	float v = 0.5 / (GGXV + GGXL);
	return clamp(v, 0.0, 1.0);
}

// NEW: Anisotropic GGX normal distribution (Khronos spec line 166-172)
float D_GGX_anisotropic(float NdotH, float TdotH, float BdotH, float at, float ab)
{
	const float PI = 3.141592653589793;
	float a2 = at * ab;
	vec3 f = vec3(ab * TdotH, at * BdotH, a2 * NdotH);
	float w2 = a2 / dot(f, f);
	return a2 * w2 * w2 / PI;
}

AnisotropyData decodeAnisotropyTexture(
	vec3 texelRGB,                           // Raw texture data
	float uniformStrength,                   // Uniform strength parameter
	float uniformRotation,                   // Uniform rotation in radians
	bool hasTexture
)
{
	AnisotropyData result;

	if (!hasTexture)
	{
		// Default: direction (1,0) = +X axis, full strength
		result.direction = vec2(1.0, 0.0);
		result.strength = uniformStrength;
		result.rotation = uniformRotation;
		return result;
	}

	// ====== SPEC-COMPLIANT TEXTURE DECODING (Line 82) ======
	// Red [0,1] -> X [-1,1], Green [0,1] -> Y [-1,1], Blue = strength [0,1]

	result.direction = texelRGB.rg * 2.0 - 1.0;  // [0,1] -> [-1,1]
	float directionLength = length(result.direction);
	if (directionLength < 0.0001)
	{
		// Neutral texels should not generate unstable directions/angles.
		// Fall back to the canonical +X direction and let the uniform rotation drive it.
		result.direction = vec2(1.0, 0.0);
	}
	else
	{
		result.direction /= directionLength; // Unit vector in tangent space
	}

	// Blue channel is strength, multiply by uniform
	result.strength = texelRGB.b * uniformStrength;
	result.strength = clamp(result.strength, 0.0, 1.0);

	// Direction rotation: apply uniform rotation to texture direction
	// (Spec line 131-132: rotate the direction vector by the rotation matrix)
	float c = cos(uniformRotation);
	float s = sin(uniformRotation);
	vec2 rotated = vec2(
		c * result.direction.x - s * result.direction.y,
		s * result.direction.x + c * result.direction.y
	);

	// Convert 2D rotated direction to rotation angle for T/B rotation
	result.rotation = atan(rotated.y, rotated.x);

	return result;
}

vec3 calculateAnisotropy(
	vec3 N, vec3 V, vec3 L,
	vec3 T, vec3 B,
	float anisotropyStrength,  // In [0,1], strength of anisotropic effect
	float anisotropyRotation,   // In radians, rotation of anisotropy direction
	float roughness,            // Base roughness [0,1]
	vec3 F0                     // Fresnel base color
)
{
	const float PI = 3.141592653589793;

	// Clamp strength to valid range [0,1]
	anisotropyStrength = clamp(anisotropyStrength, 0.0, 1.0);

	// SPEC-COMPLIANT: Compute alpha roughness values
	// Formula (Spec line 88-94):
	//   materialAlphaRoughness = materialRoughness^2
	//   directionAlphaRoughness = mix(materialAlphaRoughness, 1.0, strength^2)
	//   ab = materialAlphaRoughness (perpendicular direction stays same)
	//   at = directionAlphaRoughness (parallel direction gets rougher)

	float a = roughness * roughness;  // materialAlphaRoughness
	float strength_sq = anisotropyStrength * anisotropyStrength;
	float at = mix(a, 1.0, strength_sq);  // Use mix formula
	float ab = a;                         // Bitangent uses base roughness

	// Prevent degenerate values
	at = max(at, 0.0001);
	ab = max(ab, 0.0001);

	// ========================================================================
	// Apply anisotropy rotation to tangent/bitangent
	// (Spec line 129-137: rotate the anisotropy direction, then transform to world)
	// ========================================================================
	float c = cos(anisotropyRotation);
	float s = sin(anisotropyRotation);
	vec3 T_aniso = c * T + s * B;  // Rotate tangent
	vec3 B_aniso = -s * T + c * B; // Rotate bitangent (perpendicular)

	// Ensure orthonormality after rotation
	T_aniso = normalize(T_aniso);
	B_aniso = normalize(cross(N, T_aniso));

	// ========================================================================
	// Compute required dot products
	// ========================================================================
	vec3 H = normalize(V + L);

	// Essential dot products
	float NdotH = clamp(dot(N, H), 0.0, 1.0);
	float NdotV = clamp(dot(N, V), 0.0, 1.0);
	float NdotL = clamp(dot(N, L), 0.0, 1.0);

	// Anisotropic-specific dot products (with rotated T and B)
	float TdotV = dot(T_aniso, V);
	float BdotV = dot(B_aniso, V);
	float TdotL = dot(T_aniso, L);
	float BdotL = dot(B_aniso, L);
	float TdotH = dot(T_aniso, H);
	float BdotH = dot(B_aniso, H);

	// ========================================================================
	// Compute anisotropic BRDF components
	// ========================================================================

	// Fresnel (same as isotropic)
	float VdotH = clamp(dot(V, H), 0.0, 1.0);
	vec3 F = fresnelSchlick(VdotH, F0);  // Default F90=1.0

	// Distribution: Anisotropic GGX
	float D = D_GGX_anisotropic(NdotH, TdotH, BdotH, at, ab);

	// Visibility: Anisotropic Smith
	float V_vis = V_GGX_anisotropic(NdotL, NdotV, BdotV, TdotV, TdotL, BdotL, at, ab);

	// ========================================================================
	// Cook-Torrance: (D * V * F) / (4 * NdotL * NdotV) but V already includes denominator
	// ========================================================================
	vec3 specular = D * V_vis * F;

	// Return with NdotL multiplier for energy conservation
	return specular * NdotL;
}


// ============================================================================
// KHRONOS IRIDESCENCE - HELPERS & CONVERSION FUNCTIONS
// ============================================================================

const float M_PI = 3.14159265359;
const mat3 XYZ_TO_REC709 = mat3(
	3.2404542, -0.9692660, 0.0556434,
	-1.5371385, 1.8760108, -0.2040259,
	-0.4985314, 0.0415560, 1.0572252
);

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

// Scalar version with F90
float F_Schlick_Iridescence(float f0, float cosTheta, float f90)
{
	return f0 + (f90 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Vector version with F90
vec3 F_Schlick_Iridescence(vec3 f0, float cosTheta, vec3 f90)
{
	return f0 + (f90 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Keep old versions for backward compatibility (optional)
float F_Schlick_Iridescence(float f0, float cosTheta)
{
	return F_Schlick_Iridescence(f0, cosTheta, 1.0);
}

vec3 F_Schlick_Iridescence(vec3 f0, float cosTheta)
{
	return F_Schlick_Iridescence(f0, cosTheta, vec3(1.0));
}

// CRITICAL: XYZ sensitivity curves -> RGB (makes colors vibrant!)
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

vec3 evalIridescence(float outsideIOR, float eta2, float cosTheta1,
	float thinFilmThickness, vec3 baseF0)
{
	return evalIridescence(outsideIOR, eta2, cosTheta1, thinFilmThickness, baseF0, vec3(1.0));
}

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

	// Higher order interference
	vec3 Cm = Rs - T121;
	for (int m = 1; m <= 2; ++m)
	{
		Cm *= r123;
		vec3 Sm = 2.0 * evalSensitivity(float(m) * OPD, float(m) * phi);
		I += Cm * Sm;
	}
	return max(I, vec3(0.0));
}

vec3 computeIridescentFresnel(float cosTheta,
	vec3 baseF0,
	vec3 baseF90,
	float iridescenceFactor,
	float iridescenceIor,
	float iridescenceThickness)
{
	float clampedCosTheta = clamp(cosTheta, 0.0, 1.0);
	vec3 baseFresnel = fresnelSchlick(clampedCosTheta, baseF0, baseF90);
	if (iridescenceFactor <= 0.001)
	{
		return baseFresnel;
	}

	vec3 iridescentFresnel = evalIridescence(
		1.0,
		iridescenceIor,
		clampedCosTheta,
		iridescenceThickness,
		clamp(baseF0, vec3(0.0), vec3(0.9999)),
		baseF90
	);
	return mix(baseFresnel, iridescentFresnel, iridescenceFactor);
}

// KHR_material_transmission
vec3 calculateTransmission(vec3 N, vec3 V, vec3 L, float transmission, float ior, vec3 albedo)
{
	if (transmission <= 0.0) return vec3(0.0);

	vec3 H = normalize(V + L);
	float VdotH = clamp(dot(V, H), 0.0, 1.0);
	float NdotL = clamp(dot(N, L), -1.0, 1.0);
	float NdotV = clamp(dot(N, V), 0.0, 1.0);

	// Calculate proper Fresnel for transmission
	float f0 = pow((ior - 1.0) / (ior + 1.0), 2.0);
	float fresnel = f0 + (1.0 - f0) * pow(1.0 - abs(VdotH), 5.0);
	float transmittance = (1.0 - fresnel) * transmission;

	// Improved transmission with both forward and back scattering
	float backScatter = max(0.0, -NdotL) * 0.8; // Light from behind
	float forwardScatter = max(0.0, NdotL) * 0.5; // Light from front (subsurface)

	// Add thickness approximation
	float thickness = max(0.01, pbrLighting.thicknessFactor);
	float attenuationFactor = exp(-thickness * (1.0 - transmission));

	vec3 transmissionColor = albedo * transmittance * attenuationFactor * (backScatter + forwardScatter);

	return transmissionColor;
}

// KHR_materials_volume
vec3 calculateVolumeAttenuation(vec3 transmittedLight, float distance, float thickness, vec3 attenuationColor, float attenuationDistance)
{
	if (attenuationDistance <= 0.0)
	{
		return transmittedLight;
	}

	float d = max(attenuationDistance, 1e-6);
	float t = max(thickness, 1e-6);

	// Beer-Lambert law
	vec3 transmittance = exp(-attenuationColor * (t / d));

	return transmittedLight * transmittance;
}


// ============================================================================
// Volume Fresnel Calculation (KHR_materials_volume)
// ============================================================================

// Calculate F0 from IOR (dielectric Fresnel at normal incidence)
float iorToF0(float ior)
{
	return sq((ior - 1.0) / (ior + 1.0));
}

// Case 1: Light enters medium with higher IOR (eta_o >= eta_i)
float fresnelVolumeEntering(float f0, float NdotH)
{
	return f0 + (1.0 - f0) * pow(clamp(1.0 - NdotH, 0.0, 1.0), 5.0);
}

// Case 2: Light exits medium (eta_o < eta_i) without total internal reflection
float fresnelVolumeExiting(float f0, float etaRatio, float NdotH)
{
	// eta_ratio = eta_i / eta_o
	float sinThetaO_sq = etaRatio * etaRatio * (1.0 - NdotH * NdotH);

	// Check for total internal reflection
	if (sinThetaO_sq >= 1.0)
		return 1.0;  // Total internal reflection

	float cosThetaO = sqrt(1.0 - sinThetaO_sq);
	return f0 + (1.0 - f0) * pow(clamp(1.0 - cosThetaO, 0.0, 1.0), 5.0);
}

// Determine which Fresnel case applies
float calculateVolumeFresnel(float ior, float iorIncident, float NdotV)
{
	float f0 = iorToF0(ior);
	float etaRatio = iorIncident / ior;

	if (etaRatio <= 1.0)
	{
		// Case 1: Entering denser medium (air -> glass)
		return fresnelVolumeEntering(f0, NdotV);
	}
	else
	{
		// Case 2: Exiting or between denser media
		// Case 3: Handled inside (returns 1.0 for TIR)
		return fresnelVolumeExiting(f0, etaRatio, NdotV);
	}
}

// ============================================================================
// Helper: Apply IOR to Roughness (for LOD calculation)
// ============================================================================
float applyIorToRoughness(float roughness, float ior)
{
	// Scale roughness with IOR so that an IOR of 1.0 results in no microfacet refraction and
	// an IOR of 1.5 results in the default amount of microfacet refraction.
	return roughness * clamp(ior * 2.0 - 2.0, 0.0, 1.0);
}

// ============================================================================
// Calculate 3D Transmission Ray Exit Point
// ============================================================================
vec3 getVolumeTransmissionRay(vec3 n, vec3 v, float thickness, float ior, mat4 modelMatrix)
{
	// Direction of refracted light (Snell's law)
	vec3 refractionVector = refract(-v, normalize(n), 1.0 / ior);

	// Compute rotation-independent scaling of the model matrix
	vec3 modelScale;
	modelScale.x = length(vec3(modelMatrix[0].xyz));
	modelScale.y = length(vec3(modelMatrix[1].xyz));
	modelScale.z = length(vec3(modelMatrix[2].xyz));

	// The thickness is specified in local space
	// Returns the world-space displacement vector
	return normalize(refractionVector) * thickness * modelScale;
}

// ============================================================================
// Sample Transmission with Roughness LOD
// ============================================================================
vec3 getTransmissionSample(vec2 fragCoord, float roughness, float ior)
{
	// Calculate LOD based on roughness and IOR
	// Higher roughness and/or higher IOR = access lower (blurred) mip levels
	float framebufferLod = log2(transmissionFramebufferSize.x) * applyIorToRoughness(roughness, ior);

	// Sample with automatic mipmap interpolation
	vec3 transmittedLight = textureLod(transmissionSceneTexture, fragCoord.xy, framebufferLod).rgb;

	return transmittedLight;
}

// ============================================================================
// Main Transmission Ray Tracing Function
// ============================================================================
vec3 getIBLVolumeRefraction(vec3 n, vec3 v, float perceptualRoughness, vec3 baseColor,
	vec3 position, mat4 modelMatrix, float ior, float thickness,
	vec3 attenuationColor, float attenuationDistance)
{
	// Calculate 3D transmission ray (refracted path through volume)
	vec3 transmissionRay = getVolumeTransmissionRay(n, v, thickness, ior, modelMatrix);

	// Calculate exit point in world space
	vec3 refractedRayExit = position + transmissionRay;

	// Project exit point to screen space
	vec4 ndcPos = projectionMatrix * viewMatrix * vec4(refractedRayExit, 1.0);
	vec2 refractionCoords = ndcPos.xy / ndcPos.w;
	refractionCoords += 1.0;
	refractionCoords /= 2.0;

	// Get transmission ray length for attenuation
	float transmissionRayLength = length(transmissionRay);

	// Sample framebuffer at projected coordinates with roughness LOD
	vec3 transmittedLight = getTransmissionSample(refractionCoords, perceptualRoughness, ior);

	// Apply volume attenuation (Beer's law)
	if (transmissionRayLength > 0.0 && attenuationDistance > 0.0)
	{
		vec3 transmittance = pow(attenuationColor, vec3(transmissionRayLength / attenuationDistance));
		transmittedLight *= transmittance;
	}

	// Apply base color tinting
	transmittedLight *= baseColor;

	return transmittedLight;
}

// ============================================================================
// Per-Channel Transmission for Dispersion
// ============================================================================
vec3 getIBLVolumeRefractionPerChannel(vec3 n, vec3 v, float perceptualRoughness, vec3 baseColor,
	vec3 position, mat4 modelMatrix, vec3 iors, float thickness,
	vec3 attenuationColor, float attenuationDistance)
{
	vec3 transmittedLight = vec3(0.0);

	// Process each channel (R, G, B) with different IOR
	for (int i = 0; i < 3; i++)
	{
		float ior = iors[i];

		// Get transmission ray for this channel
		vec3 transmissionRay = getVolumeTransmissionRay(n, v, thickness, ior, modelMatrix);
		vec3 refractedRayExit = position + transmissionRay;
		float transmissionRayLength = length(transmissionRay);

		// Project to screen space
		vec4 ndcPos = projectionMatrix * viewMatrix * vec4(refractedRayExit, 1.0);
		vec2 refractionCoords = ndcPos.xy / ndcPos.w;
		refractionCoords += 1.0;
		refractionCoords /= 2.0;

		// Sample with LOD for this channel's IOR
		vec3 sampledLight = getTransmissionSample(refractionCoords, perceptualRoughness, ior);

		// Apply attenuation
		if (transmissionRayLength > 0.0 && attenuationDistance > 0.0)
		{
			vec3 transmittance = pow(attenuationColor, vec3(transmissionRayLength / attenuationDistance));
			sampledLight *= transmittance;
		}

		// Extract channel component for chromatic aberration effect
		transmittedLight[i] = sampledLight[i] * baseColor[i];
	}

	return transmittedLight;
}

// KHR_materials_volume_scatter: Convert multi-scatter color to single scatter ratio
// Based on glTF 2.0 specification
vec3 multiToSingleScatter()
{
	vec3 s = 4.09712 + 4.20863 * multiScatterColor - sqrt(9.59217 + 41.6808 * multiScatterColor + 17.7126 * multiScatterColor * multiScatterColor);
	return 1.0 - s * s;
}


// ----------------------------------------------------------------------------
float calculateShadow(vec4 fragPosLightSpace)
{
	// perform perspective divide
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	// transform to [0,1] range
	projCoords = projCoords * 0.5 + 0.5;
	// get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
	float closestDepth = texture(shadowMap, projCoords.xy).r;
	// get depth of current fragment from light's perspective
	float currentDepth = projCoords.z;

	vec3 normal = normalize(fs_in_shadow.Normal);
	vec3 lightDir = normalize(lightSource.position);

	//float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
	float bias = clamp(0.005 * tan(acos(dot(normal, lightDir))), 0.005, 0.05);

	// PCF - Percentage Closer Filtering
	float shadow = 0.0;
	vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
			shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
		}
	}

	shadow /= shadowSamples;

	// keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
	if (projCoords.z > 1.0)
		shadow = 0.0;

	return shadow;
}

// Function to fetch shadow value with variable kernel size
float calculateShadowVariableKernel(vec4 fragPosLightSpace, vec3 fragPos, vec3 lightPos)
{
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;

	float distanceToLight = length(fragPos - lightPos);

	// Size-aware kernel scaling
	int kernelSize;
	float distanceThreshold1 = 5.0 / max(shadowSizeScale, 0.1);  // NEW uniform
	float distanceThreshold2 = 15.0 / max(shadowSizeScale, 0.1);

	if (distanceToLight < distanceThreshold1)
		kernelSize = max(2, shadowMaxKernelSize - 2);
	else if (distanceToLight < distanceThreshold2)
		kernelSize = max(3, shadowMaxKernelSize - 1);
	else
		kernelSize = shadowMaxKernelSize;

	// Size-aware adaptive softness
	float sizeAdjustedSoftness = shadowSoftness * shadowSizeScale;
	float adaptiveSoftness = sizeAdjustedSoftness * clamp(
		distanceToLight * shadowSoftnessScale,
		1.0,
		shadowMaxSoftnessClamp
	);

	// Rest of the function remains the same...
	float currentDepth = projCoords.z;
	float shadow = 0.0;
	float totalWeight = 0.0;

	for (int x = -kernelSize; x <= kernelSize; ++x)
	{
		for (int y = -kernelSize; y <= kernelSize; ++y)
		{
			float distance = sqrt(float(x * x + y * y));
			float weight = exp(-distance * distance / (2.0 * float(kernelSize * kernelSize) * 0.5));
			totalWeight += weight;

			vec2 offset = vec2(x, y) * adaptiveSoftness * 1.5 / lightFarPlane;
			float sampleDepth = texture(shadowMap, projCoords.xy + offset).r;

			float bias = mix(shadowBiasMin, shadowBiasMax,
				clamp(distanceToLight * 0.05, 0.0, 1.0));
			float depthDiff = currentDepth - sampleDepth - bias;

			float shadowContrib = smoothstep(-shadowTransitionRange, shadowTransitionRange, depthDiff);
			shadow += shadowContrib * weight;
		}
	}

	shadow /= totalWeight;
	shadow = pow(shadow, shadowGammaCorrection);

	return shadow;
}

// Parallax mapping function
vec2 applyParallaxMapping(vec2 baseUV, sampler2D heightMap, float heightScale, bool enabled)
{
	if (!enabled) return baseUV;

	// Build TBN matrix
	vec3 n = normalize(v_normal);
	vec3 t = normalize(v_tangent - dot(v_tangent, n) * n);
	vec3 b = normalize(cross(n, t));
	mat3 TBN = mat3(t, b, n);

	// Transform view direction to tangent space
	vec3 viewDirWorld = normalize(cameraPos - v_position);
	vec3 viewDirTangent = TBN * viewDirWorld;

	// Apply parallax mapping
	vec2 parallaxUV = parallaxOcclusionMapping(baseUV, viewDirTangent, heightMap, heightScale);

	// Clamp to UV bounds
	if (parallaxUV.x < 0.0 || parallaxUV.x > 1.0 ||
		parallaxUV.y < 0.0 || parallaxUV.y > 1.0)
		parallaxUV = baseUV;

	return parallaxUV;
}

// Function for parallax mapping to simulate depth displacement
vec2 parallaxOcclusionMapping(vec2 texCoords, vec3 viewDir, sampler2D heightMap, float heightScale)
{
	// Number of layers varies with angle
	const float minLayers = 8.0;
	const float maxLayers = 32.0;
	float numLayers = mix(maxLayers, minLayers,
		abs(dot(vec3(0.0, 0.0, 1.0), normalize(viewDir))));

	// Calculate layer depth and step size
	float layerDepth = 1.0 / numLayers;
	float currentLayerDepth = 0.0;

	// Shift per step (Z-up -> use xz)
	vec2 P = viewDir.xz * heightScale;
	vec2 deltaTexCoords = P / numLayers;

	// Start at original texcoords
	vec2 currentTexCoords = texCoords;
	float currentDepthMapValue = texture(heightMap, currentTexCoords).r;

	// Iteratively march
	while (currentLayerDepth < currentDepthMapValue)
	{
		currentTexCoords -= deltaTexCoords;
		currentDepthMapValue = texture(heightMap, currentTexCoords).r;
		currentLayerDepth += layerDepth;
	}

	// Interpolate between last two steps for smoothness
	vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

	float afterDepth = currentDepthMapValue - currentLayerDepth;
	float beforeDepth = texture(heightMap, prevTexCoords).r - (currentLayerDepth - layerDepth);

	float weight = afterDepth / (afterDepth - beforeDepth);

	return prevTexCoords * weight + currentTexCoords * (1.0 - weight);
}

// ----------------------------------------------------------------------------
// Easy trick to get tangent-normals to world-space to keep PBR code simplified.
vec3 getNormalFromMap(sampler2D map)
{
	vec3 tangentNormal = texture(map, getNormalUV()).xyz * 2.0 - 1.0;

	vec3 Q1 = dFdx(v_position);
	vec3 Q2 = dFdy(v_position);
	vec2 st1 = dFdx(getNormalUV());
	vec2 st2 = dFdy(getNormalUV());

	vec3 N = normalize(v_normal);
	vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

mat3 getTBNFromMap(sampler2D map)
{
	vec3 tangentNormal = texture(map, getNormalUV()).xyz * 2.0 - 1.0;

	vec3 Q1 = dFdx(v_position);
	vec3 Q2 = dFdy(v_position);
	vec2 st1 = dFdx(getNormalUV());
	vec2 st2 = dFdy(getNormalUV());

	vec3 N = normalize(v_normal);
	vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return TBN;
}

// ----------------------------------------------------------------------------
float distributionGGX(vec3 N, vec3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;

	float nom = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return nom / max(denom, 0.001); // prevent divide by zero for roughness=0.0 and NdotH=1.0
}
// ----------------------------------------------------------------------------
float geometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	float nom = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom / denom;
}
// ----------------------------------------------------------------------------
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = geometrySchlickGGX(NdotV, roughness);
	float ggx1 = geometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return fresnelSchlick(cosTheta, F0, vec3(1.0));
}

vec3 fresnelSchlick(float cosTheta, vec3 F0, vec3 F90)
{
	return F0 + (F90 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}


// http://ogldev.atspace.co.uk/www/tutorial26/tutorial26.html
// Robust calcBumpedNormal: uses provided mesh tangent & bitangent, preserves handedness
vec3 calcBumpedNormal(sampler2D map, vec2 texCoord)
{
    // base geometric normal (world space)
    vec3 N = normalize(v_normal);
    
    // Check if we have valid tangent data
    bool hasTangents = (length(v_tangent) > 0.01);
    
    vec3 T, B;
    
    if (hasTangents)
    {
        // Use mesh-provided tangent and bitangent
        // Make tangent orthogonal to normal
        T = normalize(v_tangent - dot(v_tangent, N) * N);
        
        // Prefer using the provided bitangent (v_bitangent) instead of computing cross(N, T)
        // but orthogonalize it too
        B = normalize(v_bitangent - dot(v_bitangent, N) * N);
        
        // Ensure T, B, N form a right-handed basis; if not, flip B
        float handedness = sign(dot(cross(T, B), N));
        if (handedness < 0.0)
        {
            B = -B;
        }
    }
    else
    {
        // FALLBACK: Compute tangent space from screen-space derivatives
        // This is the same technique as getNormalFromMap
        vec3 Q1 = dFdx(v_position);
        vec3 Q2 = dFdy(v_position);
        vec2 st1 = dFdx(texCoord);
        vec2 st2 = dFdy(texCoord);
        
        T = normalize(Q1 * st2.t - Q2 * st1.t);
        B = -normalize(cross(N, T));
    }
    
    mat3 TBN = mat3(T, B, N);
    
    vec3 bumpMapNormal = texture(map, texCoord).rgb;
    bumpMapNormal = bumpMapNormal * 2.0 - 1.0;
    
    vec3 Nw = normalize(TBN * bumpMapNormal);
    return Nw;
}


vec2 calculateBackgroundUV()
{
	vec2 ndc = (gl_FragCoord.xy / screenSize) * 2.0 - 1.0;
	return ndc * 0.5 + 0.5;
}

vec3 calculateBackgroundColor()
{
	vec2 v_uv = calculateBackgroundUV();

	vec4 frag_color;
	if (gradientStyle == 0)
	{
		frag_color = mix(botColor, topColor, v_uv.y);
	}
	else if (gradientStyle == 1)
	{
		frag_color = mix(topColor, botColor, v_uv.x);
	}
	else if (gradientStyle == 2)
	{
		float diagonal_factor = (v_uv.x + (1.0 - v_uv.y)) * 0.5;
		frag_color = mix(topColor, botColor, diagonal_factor);
	}
	else if (gradientStyle == 3)
	{
		float diagonal_factor = ((1.0 - v_uv.x) + (1.0 - v_uv.y)) * 0.5;
		frag_color = mix(topColor, botColor, diagonal_factor);
	}
	else
	{
		frag_color = mix(botColor, topColor, v_uv.y);
	}

	return frag_color.rgb;
}

// sRGB <-> Linear helpers (fast-enough approximations)
vec3 sRGBToLinear(vec3 c)
{
	return pow(c, vec3(2.2));
}
vec3 linearTosRGB(vec3 c)
{
	return pow(c, vec3(1.0 / 2.2));
}

float sRGBSaturation(vec3 c)
{
	float mx = max(max(c.r, c.g), c.b);
	float mn = min(min(c.r, c.g), c.b);
	return mx - mn; // cheap proxy; OK for gray detection
}


float readMaskChannel(vec4 texel, int channel)
{
	if (channel == 0) return texel.r;
	if (channel == 1) return texel.g;
	if (channel == 2) return texel.b;
	return texel.a;
}

vec3 computeBaseColor(vec2 uv,
	vec3 matBaseColor_sRGB,   // material.diffuse in sRGB
	sampler2D albedoTex,
	bool hasAlbedoTex,
	vec3 vertexColor_sRGB,    // pass v_color.rgb
	bool useVertexColor)
{
	vec3 base_sRGB = matBaseColor_sRGB;
	vec3 tex_sRGB = hasAlbedoTex ? texture(albedoTex, uv).rgb : vec3(1.0);

	// Optional vertex color (apply as a tint *in linear*; many pipelines want this)
	vec3 vtx_sRGB = useVertexColor ? vertexColor_sRGB : vec3(1.0);

	// Convert to linear for math
	vec3 base_L = sRGBToLinear(base_sRGB);
	vec3 tex_L = sRGBToLinear(tex_sRGB);
	vec3 vtx_L = sRGBToLinear(vtx_sRGB);

	vec3 out_L;

	if (!hasAlbedoTex)
	{
		out_L = base_L; // color only
	}
	else
	{
		out_L = tex_L * base_L;		
	}

	// Apply vertex color last (in linear)
	out_L *= vtx_L;

	return out_L; // keep in linear for the rest of PBR
}

// pickChannel helper: choose channel (0=r,1=g,2=b,3=a), optionally invert and remap
float pickChannel(vec4 v, int ch, int invertFlag, float scale, float bias)
{
	float c = 0.0;
	if (ch == 0) c = v.r;
	else if (ch == 1) c = v.g;
	else if (ch == 2) c = v.b;
	else if (ch == 3) c = v.a;
	else c = 0.0; // sentinel value (no channel)

	if (invertFlag != 0) c = 1.0 - c;
	c = c * scale + bias;
	return clamp(c, 0.0, 1.0);
}

// compute sampled opacity from a dedicated opacity map (with packing support)
float sampleOpacityMap(vec2 uv)
{
	if (!hasOpacityMap) return 1.0; // neutral when no map
	vec4 opTex = texture(opacityMap, uv);

	// channel-aware extraction: if channel < 0 fallback to .r (legacy)
	float val;
	if (opacityChannel >= 0)
	{
		val = pickChannel(opTex, opacityChannel, opacityInvert, opacityScale, opacityBias);
	}
	else
	{
		// legacy fallback: red channel + optional invert/scale/bias
		val = opTex.r * opacityScale + opacityBias;
		if (opacityInvert != 0) val = 1.0 - val;
		val = clamp(val, 0.0, 1.0);
	}
	return val;
}

// compute fallback opacity from albedo/diffuse or legacy opacity texture
float sampleFallbackOpacity(vec2 uv)
{
	float val = 1.0;

	if (renderingMode == 0)
	{ // ADS -> use diffuse alpha (if present) and/or texture_opacity
		if (hasDiffuseTexture)
		{
			vec4 diff = texture(texture_diffuse, uv);
			val *= diff.a; // alpha from diffuse texture
		}
		if (hasOpacityTexture)
		{
			// legacy single-channel opacity texture (no channel packing currently)
			float optex = texture(texture_opacity, uv).r;
			if (opacityTextureInverted) optex = 1.0 - optex;
			val *= optex;
		}
	}
	else
	{ // PBR mode -> albedo alpha may indicate opacity
		if (hasAlbedoMap)
		{
			vec4 alb = texture(albedoMap, uv);
			// Use albedo alpha as fallback but be conservative (do not force)
			val *= alb.a;
		}
	}

	return clamp(val, 0.0, 1.0);
}


// ============================================================================
// Tone Mapping
// ============================================================================

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
const mat3 ACESInputMat = mat3
(
	0.59719, 0.07600, 0.02840,
	0.35458, 0.90834, 0.13383,
	0.04823, 0.01566, 0.83777
);


// ODT_SAT => XYZ => D60_2_D65 => sRGB
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

// ACES filmic tone map approximation
// see https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
vec3 RRTAndODTFit(vec3 color)
{
	vec3 a = color * (color + 0.0245786) - 0.000090537;
	vec3 b = color * (0.983729 * color + 0.4329510) + 0.238081;
	return a / b;
}

vec3 toneMapACES_Hill(vec3 color)
{
	color = ACESInputMat * color;

	// Apply RRT and ODT
	color = RRTAndODTFit(color);

	color = ACESOutputMat * color;

	// Clamp to [0, 1]
	color = clamp(color, 0.0, 1.0);

	return color;
}

// Khronos PBR neutral tone mapping
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

vec3 applyToneMapping(vec3 color)
{
	if (!hdrToneMapping) return color;

	color *= iblExposure;

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
		// boost exposure as discussed in https://github.com/mrdoob/three.js/pull/19621
		// this factor is based on the exposure correction of Krzysztof Narkowicz in his
		// implementation of ACES tone mapping
		color /= 0.6;
		color = toneMapACES_Hill(color);
	}
	else if (toneMapMode == 4)
	{
		color = uncharted2ToneMapping(color);
	}
	else
	{
		// Reinhard
		color = color / (color + vec3(1.0));
	}

	return color;
}
