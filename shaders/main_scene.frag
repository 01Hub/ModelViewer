#version 450 core
#extension GL_OES_standard_derivatives : enable

// Adpated from https://learnopengl.com/

in vec3 g_position;
in vec3 g_normal;
in vec2 g_texCoord0;
in vec2 g_texCoord1;
in vec2 g_texCoord2;
in vec2 g_texCoord3;
in vec3 g_tangent;
in vec3 g_bitangent;
noperspective in vec3 g_edgeDistance;
in vec3 g_reflectionPosition;
in vec3 g_reflectionNormal;
in vec3 g_tangentLightPos;
in vec3 g_tangentViewPos;
in vec3 g_tangentFragPos;

in GS_OUT_SHADOW{
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
	vec4 FragPosLightSpace;
	vec3 cameraPos;
	vec3 lightPos;
} fs_in_shadow;

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

uniform float u_floorAlpha = 0.95;
uniform float u_floorSpecularScale = 0.6;  // scale specular on floor [0..1]
uniform float u_floorFresnelDampen = 0.5;  // how much to dampen spec at normal incidence [0..1]


// IBL
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;

// material parameters
uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D heightMap;
uniform sampler2D aoMap;
uniform sampler2D opacityMap;
uniform bool hasAlbedoMap;
uniform bool hasMetallicMap;
uniform bool hasRoughnessMap;
uniform bool hasNormalMap;
uniform bool hasAOMap;
uniform bool hasHeightMap;
uniform float heightScale = 0.08;
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

// Advanced PBR Material Properties
uniform sampler2D transmissionMap;
uniform sampler2D iorMap;
uniform sampler2D sheenColorMap;
uniform sampler2D sheenRoughnessMap;
uniform sampler2D clearcoatMap;
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

// KHR_materials_anisotropy
uniform sampler2D anisotropyMap;
uniform bool hasAnisotropyMap = false;

// KHR_materials_iridescence
uniform sampler2D iridescenceMap;
uniform sampler2D iridescenceThicknessMap;
uniform bool hasIridescenceMap = false;
uniform bool hasIridescenceThicknessMap = false;

// KHR_materials_volume
uniform sampler2D thicknessMap;
uniform bool hasThicknessMap = false;
uniform bool hasThicknessAlpha = false;

// KHR_materials_emissive_strength (uses existing texture_emissive)
uniform float emissiveStrength = 1.0;

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
uniform TextureTransform thicknessTexTransform;

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
uniform bool sectionActive;
uniform int displayMode;
uniform int renderingMode;
uniform bool selected;
uniform vec4 reflectColor;
uniform bool floorRendering;
uniform bool lockLightAndCamera = true;
uniform bool hdrToneMapping = false;
uniform bool gammaCorrection = false;
uniform float screenGamma = 2.2;

uniform float envMapExposure = 1.0;
uniform float iblExposure = 1.0;
uniform int toneMapMode = 1; // 0=Reinhard, 1=ACES, 2=Uncharted2

uniform bool skyBoxEnabled;
uniform sampler2D skyboxColorTexture;

uniform vec4 u_topColor;
uniform vec4 u_botColor;
uniform int u_gradientStyle;
uniform vec2 u_screenSize;
uniform vec3 u_screenCenter;
uniform float u_floorSize;
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
	float ambientOcclusion;
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

	// KHR_materials_unlit
	bool unlit;
};
uniform PBRLighting pbrLighting;

uniform int   tintMode = 1;     // 0=Off, 1=AutoGray, 2=ForceGray, 3=LerpMask
uniform float tintStrength = 1.0;
uniform float grayEpsilon = 0.02; // grayscale detection threshold in sRGB
uniform bool  useVertexColor = false; // include vtx color
uniform int   tintMaskChannel = 0; // 0=R, 1=G, 2=B, 3=A

const float PI = 3.14159265359;

layout(location = 0) out vec4 fragColor;

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

// Legacy ADS
vec2    getDiffuseTextureUV();
vec2    getSpecularTextureUV();
vec2    getEmissiveTextureUV();
vec2    getNormalTextureUV();
vec2    getHeightTextureUV();
vec2    getOpacityTextureUV();

vec4    shadeBlinnPhong(LightSource source, LightModel model, Material mat, vec3 position, vec3 normal);
vec4    calculatePBRLighting(int renderMode, float side);

float	samplePackedChannelValue(sampler2D tex, bool hasTexture, vec2 uv,
	int channel, int invert, float scale, float bias,
	float fallback);

vec3    getNormalFromMap(sampler2D map);
mat3    getTBNFromMap(sampler2D map);
float   distributionGGX(vec3 N, vec3 H, float roughness);
float   geometrySchlickGGX(float NdotV, float roughness);
float   geometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3    fresnelSchlick(float cosTheta, vec3 F0);
vec3	fresnelSchlick(float cosTheta, vec3 F0, vec3 F90);
vec3    fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);
vec3    fresnelSchlickRoughness(float cosTheta, vec3 F0, vec3 F90, float roughness);
vec2    parallaxOcclusionMapping(vec2 texCoords, vec3 viewDir, sampler2D heightMap, float heightScale);
vec2	applyParallaxMapping(vec2 baseUV, sampler2D heightMap, float heightScale, bool enabled);
vec3    calcBumpedNormal(sampler2D map, vec2 texCoord);

// Advanced PBR Functions
vec3    fresnelSchlickIOR(float cosTheta, float ior);
float   distributionCharlie(vec3 N, vec3 H, float roughness);
float   geometryCharlie(float NdotV, float roughness);
float	D_Charlie(float roughness, float NoH);
vec3    calculateTransmission(vec3 N, vec3 V, vec3 L, float transmission, float ior, vec3 albedo);
vec3    calculateSheen(vec3 N, vec3 V, vec3 L, vec3 sheenColor, float sheenRoughness);
vec3    calculateSheenIBL(vec3 N, vec3 V, float sheenRoughness, vec3 sheenColor);
vec3    calculateClearcoat(vec3 N, vec3 V, vec3 L, float clearcoat, float clearcoatRoughness, vec3 clearcoatNormal);
vec3    calculateClearcoatIBL(vec3 N, vec3 V, vec3 clearcoatNormal, float clearcoatRoughness, float clearcoat);

// ==== NEW glTF EXTENSION FUNCTIONS ====
vec3	calculateAnisotropy(vec3 N, vec3 V, vec3 L, vec3 T, vec3 B, float anisotropyStrength, float anisotropyRotation, float roughness, vec3 F0);
vec3	calculateIridescence(vec3 N, vec3 V, float iridescenceFactor, float iridescenceIor, float thickness);
vec3	calculateVolumeAttenuation(vec3 transmittedLight, float distance, float thickness, vec3 attenuationColor, float attenuationDistance);

vec3	evalIridescence(float outsideIOR, float eta2, float cosTheta1, float thinFilmThickness, vec3 baseF0);
vec3	evalIridescence(float outsideIOR, float eta2, float cosTheta1, float thinFilmThickness, vec3 baseF0, vec3 baseF90);

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

vec3 srgbToLinear(vec3 c);
vec3 linearToSrgb(vec3 c);
float saturationSRGB(vec3 c);
float readMaskChannel(vec4 texel, int channel);
vec3 computeBaseColor(vec2 uv,
	vec3 matBaseColor_sRGB,   // material.diffuse in sRGB
	sampler2D albedoTex,
	bool hasAlbedoTex,
	vec3 vertexColor_sRGB,    // pass v_color.rgb
	bool useVertexColor);

float floorRadius = u_floorSize * 0.5; // Adjust radius based on floor size
float fadeStart = floorRadius * 0.65;   // Start fading 
float fadeEnd = floorRadius * 1.025;     // Fully faded


void main()
{
	vec4 v_color_front;
	vec4 v_color_back;
	vec4 v_color;

	if (isReflectedPass)
	{
		float distance = length(g_position - u_screenCenter);
		if (distance > fadeStart)
			discard;
	}

	if (renderingMode == 0)
	{
		v_color_front = shadeBlinnPhong(lightSource, lightModel, material, g_position, g_normal);
		v_color_back = shadeBlinnPhong(lightSource, lightModel, material, g_position, -g_normal);
	}
	else
	{
		v_color_front = calculatePBRLighting(renderingMode, 1.0f);
		v_color_back = calculatePBRLighting(renderingMode, -1.0f);
	}

	if (gl_FrontFacing)
	{
		v_color = v_color_front;
	}
	else
	{
		if (sectionActive)
			v_color = v_color_back + 0.15f;
		else
			v_color = v_color_back;
	}

	float mixVal; // overlay line
	if (displayMode == 0 || displayMode == 3) // shaded
	{
		if (texEnabled == true)
			fragColor = v_color * texture2D(texUnit, g_texCoord0);
		else
			fragColor = v_color;
	}
	else if (displayMode == 1) // wireframe
	{
		fragColor = vec4(v_color.rgb, 0.75f);
	}
	else // wireshaded
	{
		// Find the smallest distance
		float d = min(g_edgeDistance.x, g_edgeDistance.y);
		d = min(d, g_edgeDistance.z);

		if (d < Line.Width - 1.0f)
		{
			mixVal = 1.0f;
		}
		else if (d > Line.Width + 1.0f)
		{
			mixVal = 0.0f;
		}
		else
		{
			float x = d - (Line.Width - 1.0f);
			mixVal = exp2(-2.0f * (x * x));
		}

		if (texEnabled == true)
			v_color *= texture2D(texUnit, g_texCoord0);

		// Adaptive overlay color based on base diffuse
		vec3 baseColor = material.diffuse;
		float brightness = dot(baseColor, vec3(0.2126, 0.7152, 0.0722));

		vec3 overlayColor;
		if (brightness < 0.2)
		{
			overlayColor = baseColor + vec3(0.6); // brighten dark
		}
		else if (brightness > 0.8)
		{
			overlayColor = baseColor * 0.3; // darken bright
		}
		else
		{
			overlayColor = brightness > 0.5 ? baseColor * 0.5 : baseColor + vec3(0.4);
		}
		overlayColor = clamp(overlayColor, 0.0, 1.0);

		fragColor = mix(v_color, vec4(overlayColor, 1.0), mixVal);
	}

	// UNIFIED BLEND MODE AWARE OPACITY CALCULATION
	// Works for both ADS and PBR pipelines with dynamic texture availability
	// Skip for floor rendering - it handles its own alpha
	float finalAlpha = fragColor.a; // Start with whatever alpha the rendering functions set

	if (!floorRendering)
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

	// Premultiply for blending (non-floor; floor path already premultiplies)
	if (!floorRendering)
	{
		fragColor.rgb *= finalAlpha;
	}

	if (selected) // with glow
	{
		// Compute lighting
		vec3 norm = normalize(gl_FrontFacing ? g_normal : -g_normal);
		vec3 lightDir = normalize(lightSource.position);
		float diff = max(dot(norm, lightDir), 0.0);

		vec3 viewDir = normalize(cameraPos);
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

		if (displayMode == 2)
			fragColor = mix(fragColor, Line.Color, mixVal);
	}

	if (floorRendering)
	{
		// Compute distance-based blending factor
		float distance = length(g_position - u_screenCenter);

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
		vec3 N_main = normalize(gl_FrontFacing ? g_normal : -g_normal);
		vec3 V_main = normalize(cameraPos - g_position);
		float NdotV_main = clamp(dot(N_main, V_main), 0.0, 1.0);

		// Reduce background contribution when NdotV is high (top view).
		// When looking straight down (NdotV~1), bgMix gets smaller -> floor doesn't wash out.
		// Reduce background mixing when looking straight down (NdotV ~ 1)
		float angleMod = mix(1.0, 0.25, NdotV_main);
		// at grazing -> 1.0, at top-down -> 0.25
		float bgMix = fadeFactor * angleMod;

		// Get background color
		vec3 backgroundColor;
		if (skyBoxEnabled)
		{
			vec3 N = normalize(g_reflectionNormal);
			vec3 V = normalize(cameraDir);

			// Refract ray into environment
			float ior = 1.5; // IOR of glass
			vec3 R = refract(V, N, 1.0 / ior);

			// Sample environment
			vec3 backgroundColor = texture(envMap, R).rgb;
		}
		else
		{
			// Interpolate background gradient color
			//backgroundColor = calculateBackgroundColor();
			backgroundColor = texture2D(texUnit, g_texCoord0).rgb;
		}

		// Blend floor color with background gradient
		fragColor.rgb = mix(fragColor.rgb, backgroundColor, clamp(bgMix, 0.0, 1.0));
		fragColor.a *= (1.0 - fadeFactor) * opacity;
	}
}

// ========== LEGACY BLINN-PHONG SHADING FUNCTION ==========
vec4 shadeBlinnPhong(LightSource source, LightModel model, Material mat, vec3 position, vec3 normal)
{
	vec2 clippedTexCoord = g_texCoord0;

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
	if (lockLightAndCamera)
	{
		lightDir = normalize(source.position - g_position);
		viewDir = normalize(vec3(0, 0, 1));
	}
	else
	{
		lightDir = normalize(source.position + cameraPos - g_position);
		viewDir = normalize(cameraPos);
	}

	vec3 halfVector = normalize(lightDir + viewDir);
	float nDotVP = max(dot(normal, normalize(lightDir + viewDir)), 0.0);
	float nDotHV = max(0.0, dot(normal, halfVector));
	float pf = pow(nDotHV, mat.shininess);

	// --- Material terms ---
	vec3 matAmbient = mat.ambient;
	vec3 matDiffuse = mat.diffuse * nDotVP;
	vec3 matSpecular = mat.specular * pf;
	vec3 matEmissive = mat.emission;

	if (hasDiffuseTexture)
	{
		vec4 d = texture(texture_diffuse, getDiffuseTextureUV());
		matAmbient = d.rgb;
		matDiffuse = d.rgb * nDotVP;
	}
	if (hasSpecularTexture)
		matSpecular = texture(texture_specular, getSpecularTextureUV()).rgb * pf;
	if (hasEmissiveTexture)
		matEmissive = texture(texture_emissive, getEmissiveTextureUV()).rgb;

	// --- Build lighting buckets ---
	vec3 ambient = source.ambient * matAmbient * model.ambient;
	vec3 diffuse = source.diffuse * matDiffuse;
	vec3 specular = source.specular * matSpecular;

	vec3 baseNoSpec, specOnly;
	vec3 sceneColor = matEmissive + ambient;

	if (shadowsEnabled && displayMode == 3 && (selfShadowsEnabled || floorRendering))
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
		float fa = clamp(u_floorAlpha, 0.0, 1.0);

		// View-angle term to avoid "whiteout" when looking straight down
		vec3 Nf = normalize(gl_FrontFacing ? g_normal : -g_normal);
		vec3 Vf = normalize(cameraPos - g_position);
		float NdotVf = clamp(dot(Nf, Vf), 0.0, 1.0);
		// Fresnel-like dampening of spec when NdotV is high (looking straight down)
		float fresDampen = mix(1.0 - u_floorFresnelDampen, 1.0, pow(1.0 - NdotVf, 5.0));

		// Scale specular; keep non-spec premultiplied
		vec3 floorRGB = baseNoSpec * fa + specOnly * (u_floorSpecularScale * fresDampen);

		if (hdrToneMapping) floorRGB = applyToneMapping(floorRGB * iblExposure);
		if (gammaCorrection) floorRGB = pow(floorRGB, vec3(1.0 / screenGamma));
		return vec4(floorRGB, fa);
	}

	vec3 composed;
	composed = baseNoSpec + specOnly;

	// ADS reflection with exposure
	if (envMapEnabled)
	{
		vec3 I = normalize(cameraDir);
		vec3 N = normalize(g_reflectionNormal);
		vec3 offset = normalize(cameraPos - g_reflectionPosition);
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
		composed = applyToneMapping(composed * iblExposure);
	}
	if (gammaCorrection)
		composed = pow(composed, vec3(1.0 / screenGamma));

	return vec4(composed, 1.0);
}


// ----------------------------------------------------------------------------
// Calculate PBR lighting based on the render mode
vec4 calculatePBRLighting(int renderMode, float side) // side 1 = front, -1 = back
{
	vec3	normal = g_normal * side;

	vec3	albedo;
	float	metallic;
	float	roughness;
	float	ambientOcclusion;
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
	float	attenuationDistance;
	vec3	attenuationColor;
	bool	unlit;

	vec3	N;
	vec3	V_direct;
	vec3	L;

	// Pre-compute reflection view (used by IBL, clearcoat, transmission)
	vec3 V_reflect_base = normalize(cameraDir);
	vec3 V_reflect_offset = normalize(cameraPos - g_reflectionPosition);
	vec3 V_reflect = normalize(V_reflect_base - V_reflect_offset * 0.3);

	if (lockLightAndCamera)
	{
		V_direct = normalize(lightSource.position - g_position);
		L = normalize(lightSource.position);
	}
	else
	{
		V_direct = normalize(lightSource.position + cameraPos - g_position);
		L = normalize(lightSource.position + cameraPos);
	}

	// Optional shadows affecting direct terms
	float lightShadowFactor = 0.0;
	if (shadowsEnabled && displayMode == 3 && (selfShadowsEnabled || floorRendering))
	{
		float s = calculateShadowVariableKernel(
			fs_in_shadow.FragPosLightSpace,
			fs_in_shadow.FragPos,
			fs_in_shadow.lightPos
		);
		lightShadowFactor = clamp(s, 0.0, 0.85); // a bit stronger on direct light
	}
	float lightFactor = 1.0 - lightShadowFactor;

	vec2 clippedTexCoord = g_texCoord0;
	vec4 textureColor = vec4(1.0);

	float blendFactor = float(isGLTFMaterial);

	// --- Material source: uniforms-only (renderMode==1) vs texture-driven (renderMode!=1)
	if (renderMode == 1)
	{
		N = normalize(normal);
		albedo = pbrLighting.albedo;
		metallic = pbrLighting.metallic;
		roughness = clamp(pbrLighting.roughness, 0.001, 1.0);
		ambientOcclusion = pbrLighting.ambientOcclusion;
		transmission = pbrLighting.transmission;
		ior = pbrLighting.ior;
		sheenColor = pbrLighting.sheenColor;
		sheenRoughness = pbrLighting.sheenRoughness;
		clearcoat = pbrLighting.clearcoat;
		clearcoatRoughness = pbrLighting.clearcoatRoughness;
		clearcoatNormal = normalize(g_reflectionNormal);

		specularFactor = pbrLighting.specularFactor;
		specularColorFactor = pbrLighting.specularColorFactor;
		anisotropyStrength = pbrLighting.anisotropyStrength;
		anisotropyRotation = pbrLighting.anisotropyRotation;
		iridescenceFactor = pbrLighting.iridescenceFactor;
		iridescenceIor = pbrLighting.iridescenceIor;
		iridescenceThickness = (pbrLighting.iridescenceThicknessMin + pbrLighting.iridescenceThicknessMax) * 0.5;
		thicknessFactor = pbrLighting.thicknessFactor;
		attenuationDistance = pbrLighting.attenuationDistance;
		attenuationColor = pbrLighting.attenuationColor;
		unlit = pbrLighting.unlit;

	}
	else
	{
		// Normal map / Parallax
		if (hasNormalMap)  N = calcBumpedNormal(normalMap, getNormalUV()) * side;
		else               N = normalize(normal);

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

		// --- packed-channel aware PBR sampling ---
		// Note: pickChannel(vec4 v, int ch, int invertFlag, float scale, float bias)
		// is assumed to return a value in [0,1] for valid channel indices 0..3.
		// For a different fallback for ch < 0, modify accordingly.		
		// Metallic
		float texMetallic = samplePackedChannelValue(metallicMap, hasMetallicMap, getMetallicUV(),
			metallicChannel, metallicInvert,
			metallicScale, metallicBias,
			isGLTFMaterial ? 1.0 : pbrLighting.metallic // fallback if no texture
		);
		metallic = mix(texMetallic, pbrLighting.metallic * texMetallic, blendFactor);
		metallic = clamp(metallic, 0.0, 1.0);

		// Roughness
		float texRoughness = samplePackedChannelValue(roughnessMap, hasRoughnessMap, getRoughnessUV(),
			roughnessChannel, roughnessInvert,
			roughnessScale, roughnessBias,
			isGLTFMaterial ? 1.0 : pbrLighting.roughness // fallback if no texture
		);
		roughness = mix(texRoughness, pbrLighting.roughness * texRoughness, blendFactor);
		roughness = clamp(roughness, 0.001, 1.0);

		// Ambient Occlusion
		float texAO = samplePackedChannelValue(aoMap, hasAOMap, getAOUV(),
			aoChannel, aoInvert, aoScale, aoBias,
			isGLTFMaterial ? 1.0 : pbrLighting.ambientOcclusion);
		ambientOcclusion = mix(texAO, pbrLighting.ambientOcclusion * texAO, blendFactor);
		ambientOcclusion = clamp(ambientOcclusion, 0.05, 1.0); // prevent total blackout

		// Specular (KHR_materials_specular)
		float texSpecularFactor = hasSpecularFactorMap ? texture(specularFactorMap, getSpecularFactorUV()).a : 1.0;
		specularFactor = mix(texSpecularFactor, pbrLighting.specularFactor * texSpecularFactor, blendFactor);

		vec3 texSpecularColor = hasSpecularColorMap ? texture(specularColorMap, getSpecularColorUV()).rgb : vec3(1.0);
		specularColorFactor = mix(texSpecularColor, pbrLighting.specularColorFactor * texSpecularColor, blendFactor);
		
		// Anisotropy (KHR_materials_anisotropy)
		if (hasAnisotropyMap)
		{
			vec3 anisoData = texture(anisotropyMap, getAnisotropyUV()).rgb;
			anisotropyStrength = length(anisoData.rg);
			anisotropyRotation = atan(anisoData.g, anisoData.r);
		}
		else
		{
			anisotropyStrength = pbrLighting.anisotropyStrength;
			anisotropyRotation = pbrLighting.anisotropyRotation;
		}
	}

	// Early out for unlit materials
	if (unlit)
	{
		vec3 unlitColor = albedo;

		// Apply emissive
		vec3 emissive_L = material.emission;
		if (hasEmissiveTexture) emissive_L = texture(texture_emissive, getEmissiveUV()).rgb * material.emission;

		// Apply emissive strength
		emissive_L *= emissiveStrength;

		unlitColor += emissive_L;

		if (hdrToneMapping) unlitColor = applyToneMapping(unlitColor * iblExposure);
		if (gammaCorrection) unlitColor = pow(unlitColor, vec3(1.0 / screenGamma));

		return vec4(unlitColor, 1.0);
	}

	// ============================================================================
	// BASE LAYER - F0 and Material Foundation (WITH F90)
	// ============================================================================
	vec3 F0 = vec3(0.04);
	vec3 F90 = vec3(1.0);  // Standard for all material types

	// Apply KHR_materials_specular
	if (specularFactor > 0.0)
	{
		F0 = F0 * 2.0 * specularColorFactor * specularFactor;
		F90 = vec3(1.0);
	}

	// Mix with albedo for metals
	F0 = mix(F0, albedo, metallic);
	// F90 remains 1.0 for all materials

	// Transmission IOR override
	if (transmission > 0.0)
	{
		float f0_from_ior = pow((ior - 1.0) / (ior + 1.0), 2.0);
		F0 = mix(F0, vec3(f0_from_ior), transmission);
		F90 = vec3(1.0);
	}
	if (metallic < 0.1) F0 = max(F0, vec3(0.04));

	// Setup tangent space for anisotropy
	vec3 T = normalize(g_tangent - dot(g_tangent, N) * N);
	vec3 B = normalize(cross(N, T));

	// ============================================================================
	// DIFFUSE & SPECULAR BASE LAYER (WITH F90)
	// ============================================================================
	vec3 H = normalize(V_direct + L);

	vec3 specBRDF;
	if (anisotropyStrength > 0.0)
	{
		// Use anisotropic BRDF
		specBRDF = calculateAnisotropy(normalize(g_reflectionNormal), normalize(cameraDir), L, T, B, anisotropyStrength, anisotropyRotation, roughness, F0);
	}
	else
	{
		// Standard isotropic GGX
		float NDF = distributionGGX(N, H, roughness);
		float G = geometrySmith(N, V_direct, L, roughness);
		// UPDATED: Added F90 parameter
		vec3 F = fresnelSchlick(clamp(dot(H, V_direct), 0.0, 1.0), F0, F90);
		specBRDF = (NDF * G * F) / max(4.0 * max(dot(N, V_direct), 0.0) * max(dot(N, L), 0.0), 0.001);
	}

	specBRDF *= 1.5;

	// UPDATED: Added F90 parameter
	vec3 kS = fresnelSchlick(clamp(dot(H, V_direct), 0.0, 1.0), F0, F90);
	vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

	float NdotL = max(dot(N, L), 0.0);

	vec3 directDiffuse_L = kD * albedo / PI * (lightSource.ambient + lightSource.diffuse + lightSource.specular) * NdotL * lightFactor;
	vec3 directSpecular_L = specBRDF * (lightSource.ambient + lightSource.diffuse + lightSource.specular) * NdotL * lightFactor;

	// ============================================================================
	// TRANSMISSION LAYER
	// ============================================================================
	transmission = hasTransmissionMap ? texture(transmissionMap, getTransmissionUV()).r : pbrLighting.transmission;
	ior = hasIORMap ? texture(iorMap, getIORUV()).r : pbrLighting.ior;

	vec3 transmission_L = vec3(0.0);
	if (transmission > 0.0)
	{
		transmission_L = calculateTransmission(N, V_direct, L, transmission, ior, albedo);
	}

	// ============================================================================
	// SHEEN LAYER
	// ============================================================================
	if (hasSheenColorMap)
	{
		vec3 sc = texture(sheenColorMap, getSheenColorUV()).rgb;
		sheenColor = sc * pbrLighting.sheenColor;
	}
	else
	{
		sheenColor = pbrLighting.sheenColor;
	}
	sheenColor = clamp(sheenColor, vec3(0.0), vec3(1.0));
	sheenRoughness = hasSheenRoughnessMap ? texture(sheenRoughnessMap, getSheenRoughnessUV()).r * pbrLighting.sheenRoughness : pbrLighting.sheenRoughness;
	sheenRoughness = clamp(sheenRoughness, 0.0001, 1.0);

	vec3 sheen_L = vec3(0.0);
	vec3 sheenIBL_L = vec3(0.0);

	if (length(sheenColor) > 0.0)
	{
		sheen_L = calculateSheen(N, V_direct, L, sheenColor, sheenRoughness);
		sheenIBL_L = calculateSheenIBL(g_reflectionNormal, V_reflect, sheenRoughness, sheenColor);

		// Additional sheen BRDF contribution
		float NoH = clamp(dot(N, H), 0.0, 1.0);
		float sheenD = D_Charlie(sheenRoughness, NoH);
		vec3 sheenBRDF = sheenD * sheenColor;
		sheen_L += sheenBRDF * NdotL;

		vec3 R = reflect(V_reflect, g_reflectionNormal);
		vec3 prefilteredEnv = textureLod(prefilterMap, R, sheenRoughness * 8.0).rgb;
		vec3 dfg = texture(brdfLUT, vec2(dot(N, V_direct), sheenRoughness)).rgb;
		dfg = max(dfg, vec3(0.0));
		sheenIBL_L += dfg.b * prefilteredEnv * sheenColor;
	}

	// ============================================================================
	// CLEARCOAT LAYER (WITH F90)
	// ============================================================================
	clearcoat = hasClearcoatMap ? texture(clearcoatMap, getClearcoatUV()).r * pbrLighting.clearcoat : pbrLighting.clearcoat;
	clearcoat = clamp(clearcoat, 0.0, 1.0);
	clearcoatRoughness = hasClearcoatRoughnessMap ? texture(clearcoatRoughnessMap, getClearcoatRoughnessUV()).g * pbrLighting.clearcoatRoughness : pbrLighting.clearcoatRoughness;
	clearcoatRoughness = clamp(clearcoatRoughness, 0.089, 1.0);
	clearcoatNormal = hasClearcoatNormalMap ? calcBumpedNormal(clearcoatNormalMap, getClearcoatNormalUV()) * side : N;

	vec3 clearcoat_L = vec3(0.0);
	vec3 clearcoatIBL_L = vec3(0.0);
	float clearcoatAttenuation = 1.0;

	if (clearcoat > 0.0)
	{
		clearcoat_L = calculateClearcoat(N, V_direct, L, clearcoat, clearcoatRoughness, clearcoatNormal);
		clearcoatIBL_L = calculateClearcoatIBL(g_reflectionNormal, V_reflect, clearcoatNormal, clearcoatRoughness, clearcoat);

		// Calculate clearcoat Fresnel to attenuate base material
		vec3 F0_clearcoat = vec3(0.04);
		vec3 F90_clearcoat = vec3(1.0);
		// UPDATED: Added F90 parameter
		float clearcoatFresnel = fresnelSchlick(max(dot(clearcoatNormal, cameraDir), 0.0), F0_clearcoat, F90_clearcoat).r;
		clearcoatAttenuation = mix(1.0, 1.0 - clearcoat * clearcoatFresnel, 0.5);
	}

	// Treat clearcoat as specular-like
	directSpecular_L += clearcoat_L * lightSource.specular;

	// ============================================================================
	// IRIDESCENCE LAYER (WITH F90)
	// ============================================================================
	vec3 iridescenceF0 = vec3(0.0);

	// Load iridescence properties
	iridescenceFactor = hasIridescenceMap ? texture(iridescenceMap, getIridescenceUV()).r : pbrLighting.iridescenceFactor;
	iridescenceIor = pbrLighting.iridescenceIor;

	if (hasIridescenceThicknessMap)
	{
		float thicknessNorm = texture(iridescenceThicknessMap, getIridescenceThicknessUV()).g;
		iridescenceThickness = mix(pbrLighting.iridescenceThicknessMin, pbrLighting.iridescenceThicknessMax, thicknessNorm);
	}
	else
	{
		iridescenceThickness = (pbrLighting.iridescenceThicknessMin + pbrLighting.iridescenceThicknessMax) * 0.5;
	}

	if (iridescenceFactor > 0.0)
	{
		// Use viewing direction, not light direction!
		vec3 V_view = normalize(V_direct);
		float NdotV_view = clamp(dot(N, V_view), 0.0, 1.0);

		// Compute iridescence for dielectric path with F90
		vec3 iridFresnel_dielectric = evalIridescence(
			1.0,                           // outsideIOR (air)
			iridescenceIor,
			NdotV_view,
			iridescenceThickness,
			vec3(0.04),                    // dielectric F0
			vec3(1.0)                      // F90
		);

		// Compute iridescence for metallic path with F90
		vec3 metallicF0 = mix(albedo, vec3(0.5), 0.2);
		vec3 iridFresnel_metallic = evalIridescence(
			1.0,                           // outsideIOR (air)
			iridescenceIor,
			NdotV_view,
			iridescenceThickness,
			metallicF0,                    // metallic F0
			vec3(1.0)                      // F90
		);

		// Blend both paths based on metallicness, then blend in to F0
		vec3 iridescenceF0 = mix(iridFresnel_dielectric, iridFresnel_metallic, metallic);
		F0 = mix(F0, iridescenceF0, iridescenceFactor);
	}

	// ============================================================================
	// IBL - IMAGE BASED LIGHTING (WITH F90)
	// ============================================================================
	vec3 irradiance = texture(irradianceMap, N).rgb;
	vec3 diffuseIBL_L = irradiance * albedo;
	vec3 specIBL_L = vec3(0.0);
	vec3 ambient_L = vec3(0.0);

	if (envMapEnabled)
	{
		float dotNV = max(dot(N, V_direct), 0.0);
		// UPDATED: Added F90 parameter
		vec3 Fibl = fresnelSchlickRoughness(dotNV, F0, F90, roughness);
		vec3 kSibl = Fibl;
		vec3 kDibl = (vec3(1.0) - kSibl) * (1.0 - metallic);

		// Use cached reflection view
		vec3 R = reflect(V_reflect, g_reflectionNormal);

		const float MAX_REFLECTION_LOD = textureQueryLevels(prefilterMap) - 1.0;

		float lod = roughness * MAX_REFLECTION_LOD;
		lod = clamp(lod, 0.0, MAX_REFLECTION_LOD);

		vec3 prefilteredColor = textureLod(prefilterMap, R, lod).rgb;
		prefilteredColor = max(prefilteredColor, vec3(0.0)); // clamp negatives

		// Apply envMapExposure to specular IBL
		prefilteredColor *= envMapExposure;

		vec2 brdf = texture(brdfLUT, vec2(dotNV, roughness)).rg;
		brdf = max(brdf, vec2(0.0));
		specIBL_L = prefilteredColor * (Fibl * brdf.x + brdf.y);

		// Apply exposure to diffuse IBL too
		diffuseIBL_L = irradiance * albedo;

		float diffuseAO = mix(1.0, ambientOcclusion, 0.6);
		float specularAO = mix(1.0, ambientOcclusion, 0.2);

		ambient_L = (kDibl * diffuseIBL_L) * diffuseAO + specIBL_L * specularAO;
	}
	else
	{
		// UPDATED: Added F90 parameter
		vec3 kS0 = fresnelSchlick(max(dot(N, V_direct), 0.0), F0, F90);
		vec3 kD0 = (vec3(1.0) - kS0) * (1.0 - metallic);
		float boostedAO = mix(1.0, ambientOcclusion, 0.8);
		ambient_L = (kD0 * diffuseIBL_L) * boostedAO;
	}

	// Apply clearcoat IBL attenuation
	ambient_L *= clearcoatAttenuation;
	ambient_L += clearcoatIBL_L;
	ambient_L += sheenIBL_L;

	// ============================================================================
	// EMISSION
	// ============================================================================
	vec3 emissive_L = material.emission;
	if (hasEmissiveTexture) emissive_L = texture(texture_emissive, getEmissiveUV()).rgb * material.emission;

	// Apply emissive strength
	emissive_L *= emissiveStrength;

	// ============================================================================
	// LAYER COMPOSITION
	// ============================================================================
	// Split into "non-specular" bucket vs "specular-only"
	vec3 baseNoSpec_L = emissive_L + ambient_L + directDiffuse_L + transmission_L + sheen_L;
	vec3 specOnly_L = directSpecular_L; // (spec IBL already inside 'ambient_L')

	// --- Floor override (non-reflected) ---
	if (floorRendering && !isReflectedPass)
	{
		float fa = clamp(u_floorAlpha, 0.0, 1.0);
		vec3 Nf = normalize(gl_FrontFacing ? g_normal : -g_normal);
		vec3 Vf = normalize(cameraPos - g_position);
		float NdotVf = clamp(dot(Nf, Vf), 0.0, 1.0);
		float fresDampen = mix(1.0 - u_floorFresnelDampen, 1.0, pow(1.0 - NdotVf, 5.0));

		vec3 floorRGB_L = baseNoSpec_L * fa + specOnly_L * (u_floorSpecularScale * fresDampen);

		if (hdrToneMapping) floorRGB_L = applyToneMapping(floorRGB_L * iblExposure);
		if (gammaCorrection) floorRGB_L = pow(floorRGB_L, vec3(1.0 / screenGamma));
		return vec4(floorRGB_L, fa);
	}

	vec3 outRGB = baseNoSpec_L + specOnly_L;

	// ============================================================================
	// TRANSMISSION VOLUME & REFRACTION
	// ============================================================================
	float transmissionFactor = pbrLighting.transmission;
	if (hasTransmissionMap)
	{
		float mapVal = texture(transmissionMap, getTransmissionUV()).r;
		transmissionFactor *= mapVal;
	}

	if (transmissionFactor > 0.0)
	{
		vec3 N_trans = normalize(g_reflectionNormal);
		float ior_trans = max(1e-3, pbrLighting.ior);

		vec3 refractionVector = refract(-V_reflect, N_trans, 1.0 / ior_trans);

		float thickness = thicknessFactor;
		if (hasThicknessMap)
		{
			vec4 thicknessTexel = texture(thicknessMap, getThicknessUV());
			float thicknessSample = hasThicknessAlpha ? thicknessTexel.a : thicknessTexel.r;
			thickness *= thicknessSample;
		}

		vec3 modelScale;
		modelScale.x = length(vec3(modelMatrix[0].xyz));
		modelScale.y = length(vec3(modelMatrix[1].xyz));
		modelScale.z = length(vec3(modelMatrix[2].xyz));

		vec3 transmissionRay = normalize(refractionVector) * thickness * modelScale;
		float transmissionRayLength = length(transmissionRay);

		float tRough = max(roughness, 0.1);
		vec3 envColor = textureLod(prefilterMap, normalize(refractionVector), tRough * 3.0).rgb;

		if (transmissionRayLength > 0.0 && attenuationDistance > 0.0)
		{
			vec3 transmittance = pow(attenuationColor,
				vec3(transmissionRayLength / attenuationDistance));
			envColor *= transmittance;
		}

		vec3 transmissionColor = envColor * albedo;

		// For transmission materials, blend between normal shading and transmission
		outRGB = mix(outRGB, transmissionColor, transmissionFactor);
	}

	// ============================================================================
	// TONE MAPPING & GAMMA CORRECTION
	// ============================================================================
	if (hdrToneMapping)
	{
		outRGB *= envMapExposure;
		outRGB = applyToneMapping(outRGB * iblExposure);
	}
	if (gammaCorrection)
	{
		outRGB = pow(outRGB, vec3(1.0 / screenGamma));
	}

	return vec4(outRGB, 1.0);
}


// ========== CORE TEXTURE TRANSFORM FUNCTION ==========
vec2 getTransformedUV(int texCoordIndex, TextureTransform transform)
{
	// Step 1: Select the appropriate base UV coordinate set
	vec2 uv;
	switch (texCoordIndex)
	{
	case 1: uv = g_texCoord1; break;
	case 2: uv = g_texCoord2; break;
	case 3: uv = g_texCoord3; break;
	default: uv = g_texCoord0; break; // case 0 and fallback
	}

	// Step 2: Apply KHR_texture_transform
	// Order: rotation (around 0.5, 0.5) -> scale -> offset

	// Rotation is applied around the center point (0.5, 0.5)
	const vec2 pivot = vec2(0.5, 0.5);
	uv -= pivot;

	// Apply rotation matrix
	float cosR = cos(transform.rotation);
	float sinR = sin(transform.rotation);
	mat2 rotMat = mat2(cosR, sinR, -sinR, cosR);
	uv = rotMat * uv;

	// Translate back from pivot
	uv += pivot;

	// Apply scale and offset
	uv = uv * transform.scale + transform.offset;

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

// Calculate transmission contribution
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

// Calculate sheen contribution (for fabric-like materials)
// ------------------------
// Improved calculateSheen
// Returns a radiance contribution (already multiplied by NdotL), so the caller
// can add it directly to outgoing radiance: Lo += calculateSheen(...);
vec3 calculateSheen(vec3 N, vec3 V, vec3 L, vec3 sheenColor, float sheenRoughness)
{
    // Half vector and common dot products
    vec3 H = normalize(V + L);
    float NdotL = clamp(dot(N, L), 0.0, 1.0);
    float NdotV = clamp(dot(N, V), 0.0, 1.0);
    float VdotH  = clamp(dot(V, H), 0.0, 1.0);

    if (NdotL <= 0.0 || NdotV <= 0.0)
        return vec3(0.0);

    // Remap roughness — many Charlie implementations expect squared mapping
    float r = clamp(sheenRoughness, 0.0, 1.0);
    float mappedRough = max(0.002, r * r);

    // Charlie distribution & geometry (use your existing functions)
    float D = distributionCharlie(N, H, mappedRough);
    float G = geometryCharlie(NdotV, mappedRough) * geometryCharlie(NdotL, mappedRough);

    // Fresnel — colored sheen: Schlick with V·H (F0 = sheenColor)
    vec3 F = fresnelSchlick(VdotH, sheenColor);

    // Safe denominator to avoid spikes at grazing
    float denom = max(4.0 * NdotV * NdotL, 1e-5);

    // Microfacet-like sheen BRDF (reflectance)
    vec3 sheenBRDF = (D * G / denom) * F;

    // Sheen is primarily a dielectric effect — attenuate on metals
    sheenBRDF *= (1.0 - pbrLighting.metallic);

    // Return radiance contribution for this light (multiply by NdotL here to match current shader style)
    return sheenBRDF * NdotL;
}


// ------------------------
// Improved calculateSheenIBL
// Uses your prefiltered env map (prefilterMap) and brdfLUT (sampler2D brdfLUT).
// Returns a radiance contribution that can be added directly to Lo.
vec3 calculateSheenIBL(vec3 N, vec3 V, float sheenRoughness, vec3 sheenColor)
{
    // Compute stable reflection direction consistent with your shader (you used reflect(V, N))
    vec3 R = reflect(V, N);
    R = normalize(R);

    // Prefilter sampling LOD
    float MAX_LOD = max(textureQueryLevels(prefilterMap) - 1.0, 0.0);
    float lod = clamp(sheenRoughness, 0.0, 1.0) * MAX_LOD;

    // Sample environment (prefiltered reflection)
    vec3 prefilteredColor = textureLod(prefilterMap, R, lod).rgb;
	prefilteredColor = max(prefilteredColor, vec3(0.0)); // clamp negatives

    // N·V used for LUT indexing/split-sum mapping
    float dotNV = clamp(dot(N, V), 0.0, 1.0);

    // Roughness remap to match Charlie direct term
    float r = clamp(sheenRoughness, 0.0, 1.0);
    float mappedRough = max(0.002, r * r);

    // Fresnel for sheen on IBL path — use roughness-aware Schlick
    vec3 F_sheen = fresnelSchlickRoughness(dotNV, sheenColor, sheenRoughness);

    // Geometry factor (Charlie) — single-term using NdotV is OK for IBL
    float G = geometryCharlie(dotNV, mappedRough);

    // Directional-albedo / BRDF integration factor from the split-sum LUT (blue channel)
    // brdfLUT is declared as: uniform sampler2D brdfLUT;
    float E = texture(brdfLUT, vec2(dotNV, mappedRough)).b;
	
    // Combine: prefiltered radiance * sheen color & BRDF terms * directional albedo
    vec3 sheenIBL = prefilteredColor * sheenColor * G * F_sheen * E;

    // Attenuate for metallics
    sheenIBL *= (1.0 - pbrLighting.metallic);

    // Return IBL radiance contribution (already includes env sample)
    return sheenIBL;
}


// Calculate clearcoat contribution
vec3 calculateClearcoat(vec3 N, vec3 V, vec3 L, float clearcoat,
    float clearcoatRoughness, vec3 clearcoatNormal)
{
    // --- Early out if coat disabled
    clearcoat = clamp(clearcoat, 0.0, 1.0);
    if (clearcoat <= 0.0) return vec3(0.0);

    // clearcoatRoughness should already be (factor * texture.r) on the caller side
    float rough = clamp(clearcoatRoughness, 0.02, 1.0);

    vec3 V_norm = normalize(V);
    vec3 L_norm = normalize(L);

    // IMPORTANT: clearcoatNormal must be tangent->world (same space as 'N')
    vec3 N_norm = normalize(clearcoatNormal);

    vec3 H = normalize(V_norm + L_norm);
    float NdotL = max(dot(N_norm, L_norm), 0.0);
    float NdotV = max(dot(N_norm, V_norm), 0.0);
    float VdotH = max(dot(V_norm, H), 0.0);

    // Modulate roughness by normal detail (optional artistic tweak)
    vec3 normalVariation = normalize(clearcoatNormal) - normalize(N);
    float bumpiness = length(normalVariation) * 0.5;
    float modulatedRoughness = clamp(mix(rough, rough * 1.5, bumpiness), 0.02, 1.0);

    // Dielectric coat F0 (achromatic)
    vec3 F0_clearcoat = vec3(0.04);
    vec3 F = fresnelSchlick(VdotH, F0_clearcoat);

    float D = distributionGGX(N_norm, H, modulatedRoughness);
    float G = geometrySmith(N_norm, V_norm, L_norm, modulatedRoughness);

    float denominator = max(4.0 * NdotV * NdotL, 1e-4);
    vec3 clearcoatBRDF = (D * G * F) / denominator;

    // The function returns energy scaled by clearcoat mask and NdotL
    return clearcoatBRDF * clearcoat * NdotL;
}


// Improved calculateClearcoatIBL
vec3 calculateClearcoatIBL(vec3 N, vec3 V, vec3 clearcoatNormal,
    float clearcoatRoughness, float clearcoat)
{
    // --- Prepare / normalize inputs
    vec3 V_norm = normalize(V);
    // Use the coat normal for coat reflection & dot products (preferred)
    vec3 N_coat = normalize(clearcoatNormal);

    // If you intentionally want "stable" reflection using base normal
    // (to reduce popping when clearcoat normal differs a lot), uncomment below:
    vec3 R = reflect(V_norm, normalize(N)); // stable reflection choice
    // Otherwise use coat normal (more correct per clearcoat semantics):
    //vec3 R = reflect(V_norm, N_coat);
    R = normalize(R);

    // Safely compute MAX_LOD (ensure non-negative)
    float maxLevels = textureQueryLevels(prefilterMap);
    float MAX_LOD = max(maxLevels - 1.0, 0.0);

    // Modulate roughness by normal detail (optional artistic tweak)
    vec3 normalVariation = N_coat - normalize(N); // N should be in same space as N_coat
    float bumpiness = length(normalVariation) * 0.5;
    float modulatedRoughness = mix(clearcoatRoughness, clearcoatRoughness * 1.5, bumpiness);
    modulatedRoughness = clamp(modulatedRoughness, 0.02, 1.0);

    // Map roughness to LOD. Many engines use linear; some use squared mapping.
    // Choose mapping consistent with your prefilter generation:
    //float lod = modulatedRoughness * MAX_LOD;         // linear
    float lod = (modulatedRoughness * modulatedRoughness) * MAX_LOD; // perceptual-ish		
    lod = clamp(lod, 0.0, MAX_LOD);

    // Sample prefiltered environment (cubemap) using reflection vector + lod
    vec3 prefilteredColor = textureLod(prefilterMap, R, lod).rgb;
	prefilteredColor = max(prefilteredColor, vec3(0.0)); // clamp negatives

    // Fresnel inputs: use N·V with coat normal (common IBL approximation)
    float dotNV = clamp(dot(N, V_norm), 0.0, 1.0);

    // Sample BRDF LUT (expects NdotV, roughness)
    vec2 brdf = texture(brdfLUT, vec2(dotNV, modulatedRoughness)).rg;
	brdf = max(brdf, vec2(0.0));

    // Fresnel for coat (dielectric F0)
    vec3 F0_clearcoat = vec3(0.04);
    vec3 F = fresnelSchlickRoughness(dotNV, F0_clearcoat, modulatedRoughness);

    // Compose: prefilteredColor * (F * scale + bias) per standard split-sum approx.
    vec3 specIBL = prefilteredColor * (F * brdf.x + brdf.y);

    // Optional attenuation / artistic scaling:
    float iblAttenuation = mix(0.4, 0.25, modulatedRoughness); // reduces IBL at high roughness
    // You may calibrate or remove this; it's an artistic tweak.
    float globalScale = 0.2; // calibrate to your environment exposure

    // Multiply by clearcoat mask
    return specIBL * clearcoat * iblAttenuation * globalScale;
}


// ==== NEW GLTF EXTENSION IMPLEMENTATIONS ====
// KHR_materials_anisotropy
vec3 calculateAnisotropy(vec3 N, vec3 V, vec3 L, vec3 T, vec3 B, float anisotropyStrength, float anisotropyRotation, float roughness, vec3 F0)
{
	// Rotate tangent by anisotropy rotation
	float cosRot = cos(anisotropyRotation);
	float sinRot = sin(anisotropyRotation);
	vec3 T_rot = cosRot * T + sinRot * B;
	vec3 B_rot = -sinRot * T + cosRot * B;

	vec3 H = normalize(V + L);
	float NdotH = max(dot(N, H), 0.0);
	float TdotH = dot(T_rot, H);
	float BdotH = dot(B_rot, H);
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);

	// Anisotropic roughness
	float at = max(roughness * (1.0 + anisotropyStrength), 0.001);
	float ab = max(roughness * (1.0 - anisotropyStrength), 0.001);

	// Anisotropic GGX distribution
	float a2 = at * ab;
	float denom = TdotH * TdotH / (at * at) + BdotH * BdotH / (ab * ab) + NdotH * NdotH;
	float D = 1.0 / (PI * a2 * denom * denom);

	// Use standard Smith geometry
	float G = geometrySmith(N, V, L, roughness);

	// Fresnel
	vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

	vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
	return specular * NdotL;
}

vec3 calculateIridescence(vec3 N, vec3 V, float iridescenceFactor, float iridescenceIor, float thickness)
{
	float outsideIor = 1.0;
	float baseIor = pbrLighting.ior;

	float cosTheta1 = clamp(dot(N, V), 0.0, 1.0);
	float sinTheta1 = sqrt(1.0 - cosTheta1 * cosTheta1);
	float sinTheta2 = sinTheta1 * outsideIor / iridescenceIor;

	if (sinTheta2 > 1.0) return vec3(1.0);

	float cosTheta2 = sqrt(1.0 - sinTheta2 * sinTheta2);

	float R01 = pow((outsideIor - iridescenceIor) / (outsideIor + iridescenceIor), 2.0);
	float R12 = pow((iridescenceIor - baseIor) / (iridescenceIor + baseIor), 2.0);

	float opticalPath = 2.0 * iridescenceIor * thickness * cosTheta2;
	vec3 phase = (2.0 * 3.14159 / vec3(650.0, 510.0, 475.0)) * opticalPath;

	vec3 r = sqrt(vec3(R01) * vec3(R12));
	vec3 cos_phase = cos(phase);

	vec3 numerator = vec3(R01) + vec3(R12) - 2.0 * r * cos_phase;
	vec3 denominator = 1.0 + vec3(R01 * R12) - 2.0 * r * cos_phase;
	vec3 iridescenceColor = numerator / (denominator + 0.0001);
	iridescenceColor = clamp(iridescenceColor, 0.0, 1.0);

	// ===== Much higher boost =====
	iridescenceColor = iridescenceColor * 5.0;

	return clamp(iridescenceColor, 0.0, 1.0);
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

// Helper: square a value (you may already have this)
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
	vec3 I;

	float iridescenceIor = mix(outsideIOR, eta2, smoothstep(0.0, 0.03, thinFilmThickness));
	float sinTheta2Sq = sq(outsideIOR / iridescenceIor) * (1.0 - sq(cosTheta1));

	float cosTheta2Sq = 1.0 - sinTheta2Sq;
	if (cosTheta2Sq < 0.0)
	{
		return vec3(1.0);
	}

	float cosTheta2 = sqrt(cosTheta2Sq);

	// First interface
	float R0 = IorToFresnel0(iridescenceIor, outsideIOR);
	float R12 = F_Schlick_Iridescence(R0, cosTheta1);
	float R21 = R12;
	float T121 = 1.0 - R12;
	float phi12 = 0.0;
	if (iridescenceIor < outsideIOR) phi12 = M_PI;
	float phi21 = M_PI - phi12;

	// Second interface
	vec3 baseIOR = Fresnel0ToIor(clamp(baseF0, 0.0, 0.9999));
	vec3 R1 = IorToFresnel0(baseIOR, iridescenceIor);
	vec3 R23 = F_Schlick_Iridescence(R1, cosTheta2);
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
	vec3 lightDir;
	if (lockLightAndCamera)
		lightDir = normalize(lightSource.position);
	else
		lightDir = normalize(lightSource.position + fs_in_shadow.cameraPos);
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
	vec3 n = normalize(g_normal);
	vec3 t = normalize(g_tangent - dot(g_tangent, n) * n);
	vec3 b = normalize(cross(n, t));
	mat3 TBN = mat3(t, b, n);

	// Transform view direction to tangent space
	vec3 viewDirWorld = normalize(cameraPos - g_position);
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

	vec3 Q1 = dFdx(g_position);
	vec3 Q2 = dFdy(g_position);
	vec2 st1 = dFdx(getNormalUV());
	vec2 st2 = dFdy(getNormalUV());

	vec3 N = normalize(g_normal);
	vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

mat3 getTBNFromMap(sampler2D map)
{
	vec3 tangentNormal = texture(map, getNormalUV()).xyz * 2.0 - 1.0;

	vec3 Q1 = dFdx(g_position);
	vec3 Q2 = dFdy(g_position);
	vec2 st1 = dFdx(getNormalUV());
	vec2 st2 = dFdy(getNormalUV());

	vec3 N = normalize(g_normal);
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
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0, vec3 F90)
{
    return F0 + (F90 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ----------------------------------------------------------------------------
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, vec3 F90, float roughness)
{
    return F0 + (F90 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// http://ogldev.atspace.co.uk/www/tutorial26/tutorial26.html
// Robust calcBumpedNormal: uses provided mesh tangent & bitangent, preserves handedness
vec3 calcBumpedNormal(sampler2D map, vec2 texCoord)
{
    // base geometric normal (world space)
    vec3 N = normalize(g_normal);

    // Use mesh-provided tangent and bitangent if available
    // Make tangent orthogonal to normal
    vec3 T = normalize(g_tangent - dot(g_tangent, N) * N);

    // Prefer using the provided bitangent (g_bitangent) instead of computing cross(N, T)
    // but orthogonalize it too
    vec3 B = normalize(g_bitangent - dot(g_bitangent, N) * N);

    // Ensure T, B, N form a right-handed basis; if not, flip B
    float handedness = sign(dot(cross(T, B), N));
    if (handedness < 0.0) {
        B = -B;
    }

    mat3 TBN = mat3(T, B, N);

    vec3 bumpMapNormal = texture(map, texCoord).rgb;
    bumpMapNormal = bumpMapNormal * 2.0 - 1.0;

    vec3 Nw = normalize(TBN * bumpMapNormal);
    return Nw;
}


vec2 calculateBackgroundUV()
{
	vec2 ndc = (gl_FragCoord.xy / u_screenSize) * 2.0 - 1.0;
	return ndc * 0.5 + 0.5;
}

vec3 calculateBackgroundColor()
{
	vec2 v_uv = calculateBackgroundUV();

	vec4 frag_color;
	if (u_gradientStyle == 0)
	{
		frag_color = mix(u_botColor, u_topColor, v_uv.y);
	}
	else if (u_gradientStyle == 1)
	{
		frag_color = mix(u_topColor, u_botColor, v_uv.x);
	}
	else if (u_gradientStyle == 2)
	{
		float diagonal_factor = (v_uv.x + (1.0 - v_uv.y)) * 0.5;
		frag_color = mix(u_topColor, u_botColor, diagonal_factor);
	}
	else if (u_gradientStyle == 3)
	{
		float diagonal_factor = ((1.0 - v_uv.x) + (1.0 - v_uv.y)) * 0.5;
		frag_color = mix(u_topColor, u_botColor, diagonal_factor);
	}
	else
	{
		frag_color = mix(u_botColor, u_topColor, v_uv.y);
	}

	return frag_color.rgb;
}

// sRGB <-> Linear helpers (fast-enough approximations)
vec3 srgbToLinear(vec3 c)
{
	return pow(c, vec3(2.2));
}
vec3 linearToSrgb(vec3 c)
{
	return pow(c, vec3(1.0 / 2.2));
}

float saturationSRGB(vec3 c)
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
	vec3 base_L = srgbToLinear(base_sRGB);
	vec3 tex_L = srgbToLinear(tex_sRGB);
	vec3 vtx_L = srgbToLinear(vtx_sRGB);

	vec3 out_L;

	if (!hasAlbedoTex)
	{
		out_L = base_L; // color only
	}
	else
	{
		if (tintMode == 0)
		{
			// Texture only
			out_L = tex_L;
		}
		else if (tintMode == 2)
		{
			// Force grayscale treatment (skip detection)
			float lum = dot(tex_L, vec3(0.2126, 0.7152, 0.0722));
			out_L = mix(tex_L, base_L * lum, tintStrength);
		}
		else if (tintMode == 3)
		{
			// Masked lerp (use one channel as a mask-often A or R; customize as needed)
			vec4 texel = texture(albedoTex, uv);
			float mask = readMaskChannel(texel, tintMaskChannel);
			out_L = mix(tex_L, base_L, clamp(tintStrength * mask, 0.0, 1.0));
		}
		else
		{
			// AutoGray (default): only tint grayscale texels
			float sat = saturationSRGB(tex_sRGB);        // in sRGB is fine for detection
			float grayMask = 1.0 - smoothstep(grayEpsilon, grayEpsilon * 4.0, sat);
			// Use linear luminance for intensity
			float lum = dot(tex_L, vec3(0.2126, 0.7152, 0.0722));
			vec3 grayTint_L = base_L * lum;
			out_L = mix(tex_L, grayTint_L, clamp(tintStrength * grayMask, 0.0, 1.0));
		}
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

vec3 acesToneMapping(vec3 color)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
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

	if (toneMapMode == 1)
	{
		return acesToneMapping(color);
	}
	else if (toneMapMode == 2)
	{
		return uncharted2ToneMapping(color);
	}
	else
	{
		// Default Reinhard
		return color / (color + vec3(1.0));
	}
}
