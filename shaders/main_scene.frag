#version 450 core
#extension GL_OES_standard_derivatives : enable

// Adpated from https://learnopengl.com/

in vec3 g_position;
in vec3 g_normal;
in vec2 g_texCoord2d;
in vec3 g_tangent;
in vec3 g_bitangent;
noperspective in vec3 g_edgeDistance;
in vec3 g_reflectionPosition;
in vec3 g_reflectionNormal;
in vec3 g_tangentLightPos;
in vec3 g_tangentViewPos;
in vec3 g_tangentFragPos;

in GS_OUT_SHADOW {
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
uniform float u_floorSpecularScale  = 0.6;  // scale specular on floor [0..1]
uniform float u_floorFresnelDampen  = 0.5;  // how much to dampen spec at normal incidence [0..1]


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

// KHR_materials_emissive_strength (uses existing texture_emissive)
uniform float emissiveStrength = 1.0;

uniform bool envMapEnabled;
uniform mat3 envMapRotationMatrix;
uniform bool shadowsEnabled;
uniform bool selfShadowsEnabled;
uniform float shadowSamples;
uniform vec3 cameraPos;
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

struct Material {
	vec3  emission;
	vec3  ambient;
	vec3  diffuse;
	vec3  specular;
	float shininess;
	bool  metallic;
};
uniform Material material;

struct PBRLighting {
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

layout( location = 0 ) out vec4 fragColor;

vec4    shadeBlinnPhong(LightSource source, LightModel model, Material mat, vec3 position, vec3 normal);
vec4    calculatePBRLighting(int renderMode, float side);

void    applyEnvironmentMapping(float alpha);
vec3    getNormalFromMap();
mat3    getTBNFromMap();
float   distributionGGX(vec3 N, vec3 H, float roughness);
float   geometrySchlickGGX(float NdotV, float roughness);
float   geometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3    fresnelSchlick(float cosTheta, vec3 F0);
vec3    fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);
vec2    parallaxOcclusionMapping(vec2 texCoords, vec3 viewDir, sampler2D heightMap, float heightScale);
vec3    calcBumpedNormal(sampler2D map, vec2 texCoord);

// Advanced PBR Functions
vec3    fresnelSchlickIOR(float cosTheta, float ior);
float   distributionCharlie(vec3 N, vec3 H, float roughness);
float   geometryCharlie(float NdotV, float roughness);
vec3    calculateTransmission(vec3 N, vec3 V, vec3 L, float transmission, float ior, vec3 albedo);
vec3    calculateSheen(vec3 N, vec3 V, vec3 L, vec3 sheenColor, float sheenRoughness);
vec3    calculateClearcoat(vec3 N, vec3 V, vec3 L, float clearcoat, float clearcoatRoughness, vec3 clearcoatNormal);

// ==== NEW glTF EXTENSION FUNCTIONS ====
vec3	calculateAnisotropy(vec3 N, vec3 V, vec3 L, vec3 T, vec3 B, float anisotropyStrength, float anisotropyRotation, float roughness, vec3 F0);
vec3	calculateIridescence(vec3 N, vec3 V, float iridescenceFactor, float iridescenceIor, float thickness);
vec3	calculateVolumeAttenuation(vec3 transmittedLight, float distance, vec3 attenuationColor, float attenuationDistance);

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

	if(renderingMode == 0)
	{
		v_color_front = shadeBlinnPhong(lightSource, lightModel, material, g_position, g_normal);
		v_color_back  = shadeBlinnPhong(lightSource, lightModel, material, g_position, -g_normal);
	}
	else
	{
		v_color_front = calculatePBRLighting(renderingMode, 1.0f);
		v_color_back  = calculatePBRLighting(renderingMode, -1.0f);
	}

	if( gl_FrontFacing )
	{
		v_color = v_color_front;
	}
	else
	{
		if(sectionActive)
			v_color = v_color_back + 0.15f;
		else
			v_color = v_color_back;
	}

	float mixVal; // overlay line
	if(displayMode == 0 || displayMode == 3) // shaded
	{
		if(texEnabled == true)
			fragColor = v_color * texture2D(texUnit, g_texCoord2d);
		else
			fragColor = v_color;
	}
	else if(displayMode == 1) // wireframe
	{
		fragColor = vec4(v_color.rgb, 0.75f);
	}
	else // wireshaded
	{
		// Find the smallest distance
		float d = min(g_edgeDistance.x, g_edgeDistance.y);
		d = min(d, g_edgeDistance.z);

		if (d < Line.Width - 1.0f) {
			mixVal = 1.0f;
		} else if (d > Line.Width + 1.0f) {
			mixVal = 0.0f;
		} else {
			float x = d - (Line.Width - 1.0f);
			mixVal = exp2(-2.0f * (x * x));
		}

		if (texEnabled == true)
			v_color *= texture2D(texUnit, g_texCoord2d);

		// Adaptive overlay color based on base diffuse
		vec3 baseColor = material.diffuse;
		float brightness = dot(baseColor, vec3(0.2126, 0.7152, 0.0722));

		vec3 overlayColor;
		if (brightness < 0.2) {
			overlayColor = baseColor + vec3(0.6); // brighten dark
		} else if (brightness > 0.8) {
			overlayColor = baseColor * 0.3; // darken bright
		} else {
			overlayColor = brightness > 0.5 ? baseColor * 0.5 : baseColor + vec3(0.4);
		}
		overlayColor = clamp(overlayColor, 0.0, 1.0);

		fragColor = mix(v_color, vec4(overlayColor, 1.0), mixVal);
	}

	// UNIFIED BLEND MODE AWARE OPACITY CALCULATION
	// Works for both ADS and PBR pipelines with dynamic texture availability
	// Skip for floor rendering - it handles its own alpha
	float finalAlpha = fragColor.a; // Start with whatever alpha the rendering functions set

	if (!floorRendering) {
		if (blendMode == 0) {
			// OPAQUE: ignore alpha maps, always fully opaque
			finalAlpha = 1.0;
		} else if (blendMode == 1) {
			// MASK: cutout alpha test. Compute testAlpha from material scalar and textures.
			float testAlpha = opacity; // material scalar

			// Priority: dedicated opacity map > fallbacks
			if (hasOpacityMap) {
				float opVal = sampleOpacityMap(g_texCoord2d);
				testAlpha *= opVal;
			} else {
				// fallback to albedo/diffuse alpha or legacy opacity texture
				float fallback = sampleFallbackOpacity(g_texCoord2d);
				testAlpha *= fallback;
			}

			// Alpha test
			if (testAlpha < alphaThreshold) discard;
			finalAlpha = 1.0; // cutout either opaque or discarded
		} else { // blendMode == 2 (BLEND) - standard transparency
			// Compute finalAlpha as material scalar * dedicated opacity map * fallback alpha
			float alphaVal = opacity;

			if (hasOpacityMap) {
				alphaVal *= sampleOpacityMap(g_texCoord2d);
			} else {
				alphaVal *= sampleFallbackOpacity(g_texCoord2d);
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
	if (!floorRendering) {
		fragColor.rgb *= finalAlpha;
	}

	// --- TRANSMISSION HANDLING (PBR only) ---
	// Transmission affects color, NOT alpha.
	// Keep alpha from opacity logic above.
	if (renderingMode == 1) { // PBR mode
		float transmissionFactor = pbrLighting.transmission;

		if (hasTransmissionMap) {
			float mapVal = texture(transmissionMap, g_texCoord2d).r;
			transmissionFactor *= mapVal;
		}

		if (transmissionFactor > 0.0) {
			vec3 N = normalize(g_normal);
			vec3 V = normalize(cameraPos - g_position);

			// Refract ray into environment
			float ior = (pbrLighting.ior > 0.0) ? pbrLighting.ior : 1.5;
			vec3 R = refract(-V, N, 1.0 / ior);

			// Sample environment
			vec3 envColor = texture(envMap, R).rgb;

			// Apply volume attenuation if available
			if (pbrLighting.attenuationDistance > 0.0) {
				float pathLength = length(g_position - cameraPos);
				envColor = calculateVolumeAttenuation(envColor, pathLength, 
					pbrLighting.attenuationColor, pbrLighting.attenuationDistance);
			}

			// Blend transmission into RGB
			fragColor.rgb = mix(fragColor.rgb, envColor, transmissionFactor);
		}
	}

	// Apply environment mapping (outside floorRendering block)
	if(envMapEnabled && displayMode == 3) {
		applyEnvironmentMapping(finalAlpha); // Pass the correct alpha
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
			vec3 N = normalize(g_normal);
			vec3 V = normalize(cameraPos - g_position);

			// Refract ray into environment
			float ior = 1.5; // IOR of glass
			vec3 R = refract(-V, N, 1.0 / ior);

			// Sample environment
			vec3 backgroundColor = texture(envMap, R).rgb;
		}
		else
		{
			// Interpolate background gradient color
			//backgroundColor = calculateBackgroundColor();
			backgroundColor = texture2D(texUnit, g_texCoord2d).rgb;	
		}
		
		// Blend floor color with background gradient
		fragColor.rgb = mix(fragColor.rgb, backgroundColor, clamp(bgMix, 0.0, 1.0));		
		fragColor.a   *= (1.0 - fadeFactor) * opacity;
	} 
}

vec4 shadeBlinnPhong(LightSource source, LightModel model, Material mat, vec3 position, vec3 normal)
{
	vec2 clippedTexCoord = g_texCoord2d;

	// --- Normal / Parallax (same as before) ---
	if (hasNormalTexture)
		normal = calcBumpedNormal(texture_normal, g_texCoord2d);

	if (hasHeightTexture) {		
		vec3 n = normalize(g_normal);
		vec3 t = normalize(g_tangent - dot(g_tangent, n) * n);
		vec3 b = normalize(cross(n, t));
		mat3 TBN = mat3(t, b, n);

		vec3 viewDirWorld = normalize(cameraPos - g_position);
		vec3 viewDirTangent = TBN * viewDirWorld;
		clippedTexCoord = parallaxOcclusionMapping(g_texCoord2d, viewDirTangent, texture_height, heightScale);

		if (clippedTexCoord.x < 0.0 || clippedTexCoord.x > 1.0 ||
				clippedTexCoord.y < 0.0 || clippedTexCoord.y > 1.0)
			clippedTexCoord = g_texCoord2d; // discard;

		normal = calcBumpedNormal(texture_normal, clippedTexCoord);
	}

	// --- Lighting vectors ---
	vec3 lightDir, viewDir;
	if (lockLightAndCamera) {
		lightDir = normalize(source.position -g_position);		
		viewDir  = normalize(vec3(0,0,1));
	} else {
		lightDir = normalize(source.position + cameraPos - g_position);
		viewDir  = normalize(cameraPos);
	}

	vec3 halfVector = normalize(lightDir + viewDir);
	float nDotVP    = max(dot(normal, normalize(lightDir + viewDir)), 0.0);
	float nDotHV    = max(0.0, dot(normal, halfVector));
	float pf        = pow(nDotHV, mat.shininess);

	// --- Material terms ---
	vec3 matAmbient  = mat.ambient;
	vec3 matDiffuse  = mat.diffuse * nDotVP;
	vec3 matSpecular = mat.specular * pf;
	vec3 matEmissive = mat.emission;

	if (hasDiffuseTexture) {
		vec4 d = texture(texture_diffuse, clippedTexCoord);
		matAmbient = d.rgb;
		matDiffuse = d.rgb * nDotVP;
	}
	if (hasSpecularTexture)
		matSpecular = texture(texture_specular, clippedTexCoord).rgb * pf;
	if (hasEmissiveTexture)
		matEmissive = texture(texture_emissive, clippedTexCoord).rgb;

	// --- Build lighting buckets ---
	vec3 ambient = source.ambient * matAmbient * model.ambient;
	vec3 diffuse = source.diffuse * matDiffuse;
	vec3 specular = source.specular * matSpecular;

	vec3 baseNoSpec, specOnly;
	vec3 sceneColor = matEmissive + ambient;

	if (shadowsEnabled && displayMode == 3 && (selfShadowsEnabled || floorRendering)) {
		float shadowFactor = calculateShadowVariableKernel(
				fs_in_shadow.FragPosLightSpace,
				fs_in_shadow.FragPos,
				fs_in_shadow.lightPos
				);
		shadowFactor = clamp(shadowFactor, 0.0, 0.7);
		float lightFactor = 1.0 - shadowFactor;

		vec3 ambientContrib = ambient * 0.6;
		vec3 directDiffuse  = lightFactor * diffuse;
		vec3 directSpecular = lightFactor * specular;

		baseNoSpec = sceneColor + ambientContrib + directDiffuse;
		specOnly   = directSpecular;
	} else {
		baseNoSpec = sceneColor + diffuse;
		specOnly   = specular;
	}

	// --- Floor override (non-reflected) ---
	// Ensure the floor is actually translucent even if its material is OPAQUE
	if (floorRendering && !isReflectedPass) {
		float fa = clamp(u_floorAlpha, 0.0, 1.0);

		// View-angle term to avoid “whiteout” when looking straight down
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
	composed   = baseNoSpec + specOnly;

	// --- Tone/gamma ---
	if (hdrToneMapping)
		composed = applyToneMapping(composed * iblExposure);
	if (gammaCorrection)
		composed = pow(composed, vec3(1.0 / screenGamma));

	return vec4(composed, 1.0);
}


// ----------------------------------------------------------------------------
// Calculate PBR lighting based on the render mode
vec4 calculatePBRLighting(int renderMode, float side) // side 1 = front, -1 = back
{
	vec3 normal = g_normal * side;

	vec3  albedo;
	float metallic;
	float roughness;
	float ambientOcclusion;
	float transmission;
	float ior;
	vec3  sheenColor;
	float sheenRoughness;
	float clearcoat;
	float clearcoatRoughness;
	vec3  clearcoatNormal;

	// New glTF extension parameters
	float specularFactor;
	vec3 specularColorFactor;
	float anisotropyStrength;
	float anisotropyRotation;
	float iridescenceFactor;
	float iridescenceIor;
	float iridescenceThickness;
	float thicknessFactor;
	float attenuationDistance;
	vec3 attenuationColor;
	bool unlit;

	vec3 N; vec3 V; vec3 L;

	if (lockLightAndCamera) {
		V = normalize(lightSource.position - g_position);
		L = normalize(lightSource.position);
	} else {
		V = normalize(lightSource.position + cameraPos - g_position);
		L = normalize(lightSource.position + cameraPos);
	}

	vec2 clippedTexCoord = g_texCoord2d;
	vec4 textureColor = vec4(1.0);

	// --- Material source: uniforms-only (renderMode==1) vs texture-driven (renderMode!=1)
	if (renderMode == 1) {
		N                 = normalize(normal);
		albedo            = pbrLighting.albedo;
		metallic          = pbrLighting.metallic;
		roughness         = clamp(pbrLighting.roughness, 0.02, 1.0);
		ambientOcclusion  = pbrLighting.ambientOcclusion;
		transmission      = pbrLighting.transmission;
		ior               = pbrLighting.ior;
		sheenColor        = pbrLighting.sheenColor;
		sheenRoughness    = pbrLighting.sheenRoughness;
		clearcoat         = pbrLighting.clearcoat;
		clearcoatRoughness= pbrLighting.clearcoatRoughness;
		clearcoatNormal   = normalize(normal);

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

	} else {
		// Normal map / Parallax
		if (hasNormalMap)  N = calcBumpedNormal(normalMap, g_texCoord2d) * side;
		else               N = normalize(normal);

		if (hasHeightMap) {
			// TBN for parallax
			vec3 n = normalize(g_normal);
			vec3 t = normalize(g_tangent - dot(g_tangent, n) * n);
			vec3 b = normalize(cross(n, t));
			mat3 TBN = mat3(t, b, n);

			vec3 viewDirWorld   = normalize(cameraPos - g_position);
			vec3 viewDirTangent = TBN * viewDirWorld;

			clippedTexCoord = parallaxOcclusionMapping(g_texCoord2d, viewDirTangent, heightMap, heightScale);
			if (clippedTexCoord.x < 0.0 || clippedTexCoord.x > 1.0 ||
					clippedTexCoord.y < 0.0 || clippedTexCoord.y > 1.0)
				clippedTexCoord = g_texCoord2d; // discard;

			N = calcBumpedNormal(normalMap, clippedTexCoord) * side;
		}

		// Albedo (grayscale-tint logic via computeBaseColor)
		if (hasAlbedoMap) {
			textureColor = texture(albedoMap, clippedTexCoord);
			vec3 texRGB_L = pow(textureColor.rgb, vec3(2.2));
			float colorDeviation = length(pbrLighting.albedo - vec3(1.0));
			if (colorDeviation < 0.1) {
				albedo = texRGB_L;
			} else {
				albedo = computeBaseColor(clippedTexCoord,
						pbrLighting.albedo, // sRGB in function; it converts internally
						albedoMap,
						hasAlbedoMap,
						vec3(1.0), false);
			}
		} else {
			albedo = pbrLighting.albedo;
		}

		// --- packed-channel aware PBR sampling ---
		// Note: pickChannel(vec4 v, int ch, int invertFlag, float scale, float bias)
		// is assumed to return a value in [0,1] for valid channel indices 0..3.
		// For a different fallback for ch < 0, modify accordingly.
		// Metallic
		float sampledMetal = 0.0;
		if (hasMetallicMap) {
			vec4 metalTex = texture(metallicMap, clippedTexCoord);
			// if metallicChannel < 0, fall back to using red channel (common default)
			if (metallicChannel >= 0) {
				sampledMetal = pickChannel(metalTex, metallicChannel, metallicInvert, metallicScale, metallicBias);
			} else {
				sampledMetal = metalTex.r * metallicScale + metallicBias;
				if (metallicInvert != 0) sampledMetal = 1.0 - sampledMetal;
				sampledMetal = clamp(sampledMetal, 0.0, 1.0);
			}
			metallic = sampledMetal;
		} else {
			metallic = pbrLighting.metallic;
		}

		// Roughness
		float sampledRough = 0.0;
		if (hasRoughnessMap) {
			vec4 roughTex = texture(roughnessMap, clippedTexCoord);
			if (roughnessChannel >= 0) {
				sampledRough = pickChannel(roughTex, roughnessChannel, roughnessInvert, roughnessScale, roughnessBias);
			} else {
				sampledRough = roughTex.r * roughnessScale + roughnessBias;
				if (roughnessInvert != 0) sampledRough = 1.0 - sampledRough;
				sampledRough = clamp(sampledRough, 0.0, 1.0);
			}
			roughness = sampledRough;
		} else {
			roughness = pbrLighting.roughness;
		}
		roughness = clamp(roughness, 0.02, 1.0);

		// Ambient Occlusion
		float sampledAO = 1.0;
		if (hasAOMap) {
			vec4 aoTex = texture(aoMap, clippedTexCoord);
			if (aoChannel >= 0) {
				sampledAO = pickChannel(aoTex, aoChannel, aoInvert, aoScale, aoBias);
			} else {
				sampledAO = aoTex.r * aoScale + aoBias;
				if (aoInvert != 0) sampledAO = 1.0 - sampledAO;
				sampledAO = clamp(sampledAO, 0.0, 1.0);
			}
			ambientOcclusion = sampledAO;
		} else {
			ambientOcclusion = pbrLighting.ambientOcclusion;
		}

		transmission     = hasTransmissionMap ? texture(transmissionMap, clippedTexCoord).r : pbrLighting.transmission;

		ior = hasIORMap ? texture(iorMap, clippedTexCoord).r : pbrLighting.ior;

		if (hasSheenColorMap) {
			vec3 sc = texture(sheenColorMap, clippedTexCoord).rgb;
			sheenColor =  sc;
		} else {
			sheenColor = pbrLighting.sheenColor;
		}
		sheenRoughness = hasSheenRoughnessMap ? texture(sheenRoughnessMap, clippedTexCoord).r : pbrLighting.sheenRoughness;

		clearcoat          = hasClearcoatMap ? texture(clearcoatMap, clippedTexCoord).r : pbrLighting.clearcoat;
		clearcoatRoughness = hasClearcoatRoughnessMap ? texture(clearcoatRoughnessMap, clippedTexCoord).r : pbrLighting.clearcoatRoughness;
		clearcoatNormal    = hasClearcoatNormalMap ? calcBumpedNormal(clearcoatNormalMap, clippedTexCoord) * side : N;

		// Specular (KHR_materials_specular)
		specularFactor = hasSpecularFactorMap ? texture(specularFactorMap, clippedTexCoord).a : pbrLighting.specularFactor;
		specularColorFactor = hasSpecularColorMap ? texture(specularColorMap, clippedTexCoord).rgb : pbrLighting.specularColorFactor;
		
		// Anisotropy (KHR_materials_anisotropy)
		if (hasAnisotropyMap) {
			vec3 anisoData = texture(anisotropyMap, clippedTexCoord).rgb;
			anisotropyStrength = length(anisoData.rg);
			anisotropyRotation = atan(anisoData.g, anisoData.r);
		} else {
			anisotropyStrength = pbrLighting.anisotropyStrength;
			anisotropyRotation = pbrLighting.anisotropyRotation;
		}
		
		// Iridescence (KHR_materials_iridescence)
		iridescenceFactor = hasIridescenceMap ? texture(iridescenceMap, clippedTexCoord).r : pbrLighting.iridescenceFactor;
		iridescenceIor = pbrLighting.iridescenceIor;
		if (hasIridescenceThicknessMap) {
			float thicknessNorm = texture(iridescenceThicknessMap, clippedTexCoord).g;
			iridescenceThickness = mix(pbrLighting.iridescenceThicknessMin, pbrLighting.iridescenceThicknessMax, thicknessNorm);
		} else {
			iridescenceThickness = (pbrLighting.iridescenceThicknessMin + pbrLighting.iridescenceThicknessMax) * 0.5;
		}
		
		// Volume (KHR_materials_volume)
		thicknessFactor = hasThicknessMap ? texture(thicknessMap, clippedTexCoord).g : pbrLighting.thicknessFactor;
		attenuationDistance = pbrLighting.attenuationDistance;
		attenuationColor = pbrLighting.attenuationColor;
		
		// Unlit
		unlit = pbrLighting.unlit;
	}

	// Early out for unlit materials
	if (unlit) {
		vec3 unlitColor = albedo;
		
		// Apply emissive
		vec3 emissive_L = material.emission * emissiveStrength;
		if (hasEmissiveTexture) emissive_L = texture(texture_emissive, clippedTexCoord).rgb * emissiveStrength;
		
		unlitColor += emissive_L;
		
		if (hdrToneMapping) unlitColor = applyToneMapping(unlitColor * iblExposure);
		if (gammaCorrection) unlitColor = pow(unlitColor, vec3(1.0 / screenGamma));
		
		return vec4(unlitColor, 1.0);
	}

	// F0 calculation with specular extension support
	vec3 F0 = vec3(0.04);
	
	// Apply KHR_materials_specular
	if (specularFactor > 0.0) {
		F0 = mix(vec3(0.0), vec3(0.08) * specularColorFactor, specularFactor);
	}
	
	// Mix with albedo for metals
	F0 = mix(F0, albedo, metallic);
	
	// Transmission IOR override
	if (transmission > 0.0) {
		float f0_from_ior = pow((ior - 1.0) / (ior + 1.0), 2.0);
		F0 = mix(F0, vec3(f0_from_ior), transmission);
	}
	if (metallic < 0.1) F0 = max(F0, vec3(0.04));

	// Apply iridescence to F0
	if (iridescenceFactor > 0.0) {
		vec3 iridescenceF0 = calculateIridescence(N, V, iridescenceFactor, iridescenceIor, iridescenceThickness);
		F0 = mix(F0, iridescenceF0, iridescenceFactor);
	}

	// Setup tangent space for anisotropy
	vec3 T = normalize(g_tangent - dot(g_tangent, N) * N);
	vec3 B = normalize(cross(N, T));

	// --- Direct BRDF (GGX/Smith/Schlick)
	vec3 H = normalize(V + L);
	vec3 specBRDF;
	if (anisotropyStrength > 0.0) {
		// Use anisotropic BRDF
		specBRDF = calculateAnisotropy(N, V, L, T, B, anisotropyStrength, anisotropyRotation, roughness, F0);
	} else {
		// Standard isotropic GGX
		float NDF = distributionGGX(N, H, roughness);
		float G = geometrySmith(N, V, L, roughness);
		vec3 F = fresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);
		specBRDF = (NDF * G * F) / max(4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0), 0.001);
	}
	
	specBRDF *= 1.5;

	vec3 kS = fresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);
	vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

	float NdotL = max(dot(N, L), 0.0);

	// Optional shadows affecting direct terms
	float lightShadowFactor = 0.0;
	if (shadowsEnabled && displayMode == 3 && (selfShadowsEnabled || floorRendering)) {
		float s = calculateShadowVariableKernel(
				fs_in_shadow.FragPosLightSpace,
				fs_in_shadow.FragPos,
				fs_in_shadow.lightPos
				);
		lightShadowFactor = clamp(s, 0.0, 0.85); // a bit stronger on direct light
	}
	float lightFactor = 1.0 - lightShadowFactor;

	vec3 directDiffuse_L  = kD * albedo / PI * (lightSource.ambient + lightSource.diffuse + lightSource.specular) * NdotL * lightFactor;
	vec3 directSpecular_L = specBRDF * (lightSource.ambient + lightSource.diffuse + lightSource.specular) * NdotL * lightFactor;

	// --- Extra lobes
	vec3 transmission_L = (transmission > 0.0) ? calculateTransmission(N, V, L, transmission, ior, albedo) : vec3(0.0);
	vec3 sheen_L        = (length(sheenColor) > 0.0) ? calculateSheen(N, V, L, sheenColor, sheenRoughness) : vec3(0.0);
	vec3 clearcoat_L    = (clearcoat > 0.0) ? calculateClearcoat(clearcoatNormal, V, L, clearcoat, clearcoatRoughness, clearcoatNormal) : vec3(0.0);
	// Treat clearcoat as specular-like
	directSpecular_L += clearcoat_L;

	// --- IBL (diffuse + specular)        
	vec3 irradiance = texture(irradianceMap, N).rgb;
	vec3 diffuseIBL_L = irradiance * albedo;
	vec3 ambient_L;

	if (envMapEnabled) {
		float dotNV = max(dot(N, V), 0.0);
		vec3 Fibl = fresnelSchlickRoughness(dotNV, F0, roughness);
		vec3 kSibl = Fibl;
		vec3 kDibl = (vec3(1.0) - kSibl) * (1.0 - metallic);

		//vec3 I = normalize(cameraPos - g_reflectionPosition);
		//vec3 R = refract(-I, normalize(-g_reflectionNormal), 1.0f);
		//R = normalize(R);

		vec3 R = reflect(-V, N);
		R = envMapRotationMatrix * R;
		R = normalize(R);

		const float MAX_REFLECTION_LOD = textureQueryLevels(prefilterMap) - 1.0;

		// --- Grazing control params (tweak these) ---
		// How strongly to soften/attenuate reflections at grazing angles
		float grazingPower = mix(1.8, 2.5, roughness); // Vary with roughness

		// KEY FIX: Only apply grazing blur for rough dielectrics
		// Smooth metals and mirrors (low roughness, high metallic) should maintain sharpness
		float grazingInfluence = roughness * (1.0 - metallic * 0.8); // Reduced for metals
		float extraLodFactor = 0.6 * grazingInfluence; // Scale the effect

		float grazingSpecReduce = mix(0.8, 0.5, metallic); // Metals handle grazing better
		// ------------------------------------------------

		// Compute a grazing factor: 0 at face-on, 1 at grazing (dotNV -> 0)
		float grazing = pow(1.0 - dotNV, grazingPower); // smooth curve

		// Base LOD from roughness
		float baseLod = roughness * MAX_REFLECTION_LOD;

		// Add grazing blur ONLY when appropriate (rough dielectrics)
		float grazingLod = grazing * extraLodFactor * (1.0 - roughness) * MAX_REFLECTION_LOD;

		// Combine: mirrors/smooth metals mostly ignore grazingLod
		float lod = baseLod + grazingLod;
		lod = clamp(lod, 0.0, MAX_REFLECTION_LOD);

		vec3 prefilteredColor = textureLod(prefilterMap, R, lod).rgb;

		// Apply IBL exposure
		//prefilteredColor *= iblExposure;
		// Apply envMapExposure to specular IBL
		prefilteredColor *= envMapExposure;

		vec2 brdf = texture(brdfLUT, vec2(dotNV, roughness)).rg;
		vec3 specIBL_L = prefilteredColor * (Fibl * brdf.x + brdf.y);

		// Attenuate specular IBL at grazing angles to counter overly strong Fresnel at glancing view
		// This reduces the specular intensity smoothly as grazing -> 1.
		float grazingAtten = mix(1.0, 1.0 - grazingSpecReduce, grazing);
		specIBL_L *= grazingAtten;

		// Apply exposure to diffuse IBL too
		vec3 irradiance = texture(irradianceMap, N).rgb;// * iblExposure;
		vec3 diffuseIBL_L = irradiance * albedo;

		float diffuseAO = mix(1.0, ambientOcclusion, 0.6);
		float specularAO = mix(1.0, ambientOcclusion, 0.2);

		ambient_L = (kDibl * diffuseIBL_L) * diffuseAO + specIBL_L * specularAO;
	} else {
		vec3 kS0 = fresnelSchlick(max(dot(N, V), 0.0), F0);
		vec3 kD0 = (vec3(1.0) - kS0) * (1.0 - metallic);
		float boostedAO = mix(1.0, ambientOcclusion, 0.8);
		ambient_L = (kD0 * diffuseIBL_L) * boostedAO;
	}

	// --- Emission
	vec3 emissive_L = material.emission;
	if (hasEmissiveTexture) emissive_L = texture(texture_emissive, clippedTexCoord).rgb;

	// --- Split into "non-specular" bucket vs "specular-only"
	vec3 baseNoSpec_L = emissive_L + ambient_L + directDiffuse_L + transmission_L + sheen_L;
	vec3 specOnly_L   = directSpecular_L; // (spec IBL already inside 'ambient_L')

	// --- Floor override (non-reflected) ---
	if (floorRendering && !isReflectedPass) {
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

	vec3 outRGB = baseNoSpec_L + specOnly_L; // override opacity

	// --- Tone map & gamma once, at the end
	//vec3 outRGB = composed_L;
	if (hdrToneMapping) outRGB = applyToneMapping(outRGB * iblExposure);
	if (gammaCorrection) outRGB = pow(outRGB, vec3(1.0 / screenGamma));

	return vec4(outRGB, 1.0);
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

// Calculate transmission contribution
vec3 calculateTransmission(vec3 N, vec3 V, vec3 L, float transmission, float ior, vec3 albedo)
{
	if(transmission <= 0.0) return vec3(0.0);

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

	// Add thickness approximation (can make this a uniform)
	float thickness = 0.1; // Adjust based on model
	float attenuationFactor = exp(-thickness * (1.0 - transmission));

	vec3 transmissionColor = albedo * transmittance * attenuationFactor * (backScatter + forwardScatter);

	return transmissionColor;
}

// Calculate sheen contribution (for fabric-like materials)
vec3 calculateSheen(vec3 N, vec3 V, vec3 L, vec3 sheenColor, float sheenRoughness)
{
	vec3 H = normalize(V + L);
	float NdotL = clamp(dot(N, L), 0.0, 1.0);
	float NdotV = clamp(dot(N, V), 0.0, 1.0);
	float NdotH = clamp(dot(N, H), 0.0, 1.0);
	float VdotH = clamp(dot(V, H), 0.0, 1.0);

	// Charlie distribution and geometry
	float D = distributionCharlie(N, H, sheenRoughness);
	float G = geometryCharlie(NdotV, sheenRoughness) * geometryCharlie(NdotL, sheenRoughness);

	// Sheen BRDF
	vec3 sheenBRDF = sheenColor * D * G / (4.0 * NdotV * NdotL + 0.001);

	return sheenBRDF * NdotL;
}

// Calculate clearcoat contribution
vec3 calculateClearcoat(vec3 N, vec3 V, vec3 L, float clearcoat, float clearcoatRoughness, vec3 clearcoatNormal)
{
	vec3 H = normalize(V + L);
	float NdotL = clamp(dot(clearcoatNormal, L), 0.0, 1.0);
	float NdotV = clamp(dot(clearcoatNormal, V), 0.0, 1.0);
	float NdotH = clamp(dot(clearcoatNormal, H), 0.0, 1.0);
	float VdotH = clamp(dot(V, H), 0.0, 1.0);

	// Clearcoat uses a fixed IOR of 1.5 (typical for automotive clearcoat)
	vec3 F0_clearcoat = vec3(0.04); // F0 for IOR 1.5
	vec3 F = fresnelSchlick(VdotH, F0_clearcoat);

	float D = distributionGGX(clearcoatNormal, H, clearcoatRoughness);
	float G = geometrySmith(clearcoatNormal, V, L, clearcoatRoughness);

	vec3 clearcoatBRDF = (D * G * F) / (4.0 * NdotV * NdotL + 0.001);

	return clearcoatBRDF * clearcoat * NdotL;
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

// KHR_materials_iridescence
vec3 calculateIridescence(vec3 N, vec3 V, float iridescenceFactor, float iridescenceIor, float thickness)
{
	float NdotV = max(dot(N, V), 0.0);
	
	// Simplified thin-film interference
	// Wavelengths for RGB (approximation)
	vec3 wavelengths = vec3(650.0, 510.0, 475.0); // nm (R, G, B)
	
	// Optical path difference
	float opticalPath = 2.0 * iridescenceIor * thickness;
	
	// Phase shift for each wavelength
	vec3 phase = 2.0 * PI * opticalPath / wavelengths;
	
	// Interference pattern
	vec3 interference = 0.5 + 0.5 * cos(phase);
	
	// Apply Fresnel-like modulation
	float fresnel = pow(1.0 - NdotV, 5.0);
	
	// Base iridescent color
	vec3 iridescenceColor = interference * mix(1.0, fresnel, 0.5);
	
	return iridescenceColor * iridescenceFactor;
}

// KHR_materials_volume
vec3 calculateVolumeAttenuation(vec3 transmittedLight, float distance, vec3 attenuationColor, float attenuationDistance)
{
	if (attenuationDistance <= 0.0) {
		return transmittedLight;
	}
	
	// Beer-Lambert law
	vec3 attenuation = -log(max(attenuationColor, vec3(0.001))) / attenuationDistance;
	vec3 transmittance = exp(-attenuation * distance);
	
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
	if(lockLightAndCamera)
		lightDir = normalize(lightSource.position);
	else
		lightDir = normalize(lightSource.position + fs_in_shadow.cameraPos);
	//float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
	float bias = clamp(0.005 * tan(acos(dot(normal, lightDir))), 0.005, 0.05);

	// PCF - Percentage Closer Filtering
	float shadow = 0.0;
	vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
	for(int x = -1; x <= 1; ++x)
	{
		for(int y = -1; y <= 1; ++y)
		{
			float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
			shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;
		}
	}

	shadow /= shadowSamples;

	// keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
	if(projCoords.z > 1.0)
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

    for (int x = -kernelSize; x <= kernelSize; ++x) {
        for (int y = -kernelSize; y <= kernelSize; ++y) {
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

    float afterDepth  = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture(heightMap, prevTexCoords).r - (currentLayerDepth - layerDepth);

    float weight = afterDepth / (afterDepth - beforeDepth);

    return prevTexCoords * weight + currentTexCoords * (1.0 - weight);
}


void applyEnvironmentMapping(float alphaIn)
{
    if (renderingMode != 0 && pbrLighting.transmission <= 0.0) {
        return;
    }

    float a = clamp(alphaIn, 0.0, 1.0);
    vec3 I = normalize(g_reflectionPosition - cameraPos);
    vec3 N = normalize(g_reflectionNormal);

    // 1) Transmission with exposure control
    if (pbrLighting.transmission > 0.0) {
        float eta = max(1e-3, pbrLighting.ior);
        vec3 R = refract(normalize(g_reflectionPosition - cameraPos), normalize(g_reflectionNormal), 1.0 / eta);
        R = envMapRotationMatrix * R;
        R = normalize(R);

        float tRough = max(pbrLighting.roughness, 0.1);
        vec3 envColor = textureLod(envMap, R, tRough * 3.0).rgb;
        
        // Apply exposure
        envColor *= envMapExposure;
        
        // Apply tone mapping
        envColor = applyToneMapping(envColor * iblExposure);

        float NdotV = clamp(dot(normalize(g_reflectionNormal), normalize(cameraPos - g_reflectionPosition)), 0.0, 1.0);
        float f0 = pow((pbrLighting.ior - 1.0) / (pbrLighting.ior + 1.0), 2.0);
        float F = f0 + (1.0 - f0) * pow(1.0 - NdotV, 5.0);

        float w = clamp(pbrLighting.transmission * F * (1.0 - pbrLighting.roughness), 0.0, 0.8);
        vec3 filtered = envColor * pbrLighting.albedo;
        fragColor.rgb = mix(fragColor.rgb, filtered, w);
        return;
    }

    // 2) Regular transparency with exposure
    if (a < 1.0 && !floorRendering) {
        float refrAmt = max(0.05, 1.0 - a);
        vec3 R = refract(I, N, refrAmt);
        R = envMapRotationMatrix * R;
        R = normalize(R);

        vec3 envColor = texture(envMap, R).rgb;
        
        // Apply exposure
        envColor *= envMapExposure;
        
        // Apply tone mapping  
        envColor = applyToneMapping(envColor * iblExposure);
        
        float envStrength = (1.0 - a) * 0.7;
        fragColor.rgb = mix(fragColor.rgb, envColor, envStrength);
        return;
    }

    // 3) ADS reflection with exposure
    if (renderingMode == 0) {
		vec3 R = reflect(-I, N);
		R = envMapRotationMatrix * R;
		float specLum = dot(material.specular, vec3(0.299, 0.587, 0.114));
		float NdotV = max(dot(-I, N), 0.0);
	
		// Gentler fresnel powers
		float nonMetallicFresnelPower = mix(2.0, 3.5, 1.0 - specLum); // Reduced
		float metallicFresnelPower = 1.2; // Reduced
		float fresnelPower = mix(nonMetallicFresnelPower, metallicFresnelPower, pbrLighting.metallic);
		float fresnel = pow(1.0 - NdotV, fresnelPower);
	
		// Limit grazing angle effect
		float grazingLimit = mix(0.6, 0.9, pbrLighting.metallic); // Metals can handle more
		fresnel = clamp(fresnel, 0.0, grazingLimit);
	
		float surfaceRoughness = 1.0 - (material.shininess / 128.0);
		float roughnessReduction = pow(1.0 - surfaceRoughness, 2.0);
	
		// Reduced base strengths
		float metallicStrength = mix(0.2, 0.4, specLum);
		float glossyStrength = mix(0.25, 0.5, specLum);
		float diffuseStrength = mix(0.02, 0.12, specLum);
	
		bool isHighSpecular = specLum > 0.5;
		bool isDiffuseDominant = dot(material.diffuse, vec3(0.299,0.587,0.114)) > specLum*2.0;
		float nonMetallicStrength = isHighSpecular && !isDiffuseDominant ? glossyStrength : diffuseStrength;
		float baseReflectionStrength = mix(nonMetallicStrength, metallicStrength, pbrLighting.metallic);
	
		// Additional roughness damping for very rough surfaces
		float roughnessDamping = mix(0.3, 1.0, 1.0 - surfaceRoughness);
		float reflectionStrength = clamp(baseReflectionStrength * fresnel * roughnessReduction * roughnessDamping, 0.0, 0.5);
	
		// === IMPROVED GRAZING LOD ===
		// Material-aware grazing influence: smooth metals/mirrors avoid extra blur
		float grazingInfluence = surfaceRoughness * (1.0 - pbrLighting.metallic * 0.8);
	
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
		envColor *= envMapExposure;
		envColor = applyToneMapping(envColor * iblExposure);
		fragColor.rgb += envColor * reflectionStrength;
		return;
	}
}

// ----------------------------------------------------------------------------
// Easy trick to get tangent-normals to world-space to keep PBR code simplified.
vec3 getNormalFromMap()
{
	vec3 tangentNormal = texture(normalMap, g_texCoord2d).xyz * 2.0 - 1.0;

	vec3 Q1  = dFdx(g_position);
	vec3 Q2  = dFdy(g_position);
	vec2 st1 = dFdx(g_texCoord2d);
	vec2 st2 = dFdy(g_texCoord2d);

	vec3 N   = normalize(g_normal);
	vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
	vec3 B  = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

mat3 getTBNFromMap()
{
	vec3 tangentNormal = texture(normalMap, g_texCoord2d).xyz * 2.0 - 1.0;

	vec3 Q1  = dFdx(g_position);
	vec3 Q2  = dFdy(g_position);
	vec2 st1 = dFdx(g_texCoord2d);
	vec2 st2 = dFdy(g_texCoord2d);

	vec3 N   = normalize(g_normal);
	vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
	vec3 B  = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return TBN;
}

// ----------------------------------------------------------------------------
float distributionGGX(vec3 N, vec3 H, float roughness)
{
	float a = roughness*roughness;
	float a2 = a*a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH*NdotH;

	float nom   = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return nom / max(denom, 0.001); // prevent divide by zero for roughness=0.0 and NdotH=1.0
}
// ----------------------------------------------------------------------------
float geometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;

	float nom   = NdotV;
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
// ----------------------------------------------------------------------------
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// http://ogldev.atspace.co.uk/www/tutorial26/tutorial26.html
vec3 calcBumpedNormal(sampler2D map, vec2 texCoord)
{
	vec3 normal = normalize(g_normal);
	vec3 tangent = normalize(g_tangent - dot(g_tangent, normal) * normal);
	vec3 bitangent = normalize(cross(normal, tangent));
	mat3 TBN = mat3(tangent, bitangent, normal);

	vec3 bumpMapNormal = texture(map, texCoord).rgb;
	bumpMapNormal = 2.0 * bumpMapNormal - 1.0;
	// Uncomment the next line if normal maps need Y flipped
	// bumpMapNormal.y = -bumpMapNormal.y;
	return normalize(TBN * bumpMapNormal);
}

vec2 calculateBackgroundUV() {
	vec2 ndc = (gl_FragCoord.xy / u_screenSize) * 2.0 - 1.0;
	return ndc * 0.5 + 0.5;
}

vec3 calculateBackgroundColor() {
	vec2 v_uv = calculateBackgroundUV();

	vec4 frag_color;
	if (u_gradientStyle == 0) {
		frag_color = mix(u_botColor, u_topColor, v_uv.y);
	}
	else if (u_gradientStyle == 1) {
		frag_color = mix(u_topColor, u_botColor, v_uv.x);
	}
	else if (u_gradientStyle == 2) {
		float diagonal_factor = (v_uv.x + (1.0 - v_uv.y)) * 0.5;
		frag_color = mix(u_topColor, u_botColor, diagonal_factor);
	}
	else if (u_gradientStyle == 3) {
		float diagonal_factor = ((1.0 - v_uv.x) + (1.0 - v_uv.y)) * 0.5;
		frag_color = mix(u_topColor, u_botColor, diagonal_factor);
	}
	else {
		frag_color = mix(u_botColor, u_topColor, v_uv.y);
	}

	return frag_color.rgb;
}

// sRGB <-> Linear helpers (fast-enough approximations)
vec3 srgbToLinear(vec3 c) {
	return pow(c, vec3(2.2));
}
vec3 linearToSrgb(vec3 c) {
	return pow(c, vec3(1.0/2.2));
}

float saturationSRGB(vec3 c) {
	float mx = max(max(c.r, c.g), c.b);
	float mn = min(min(c.r, c.g), c.b);
	return mx - mn; // cheap proxy; OK for gray detection
}


float readMaskChannel(vec4 texel, int channel) {
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
	vec3 tex_sRGB  = hasAlbedoTex ? texture(albedoTex, uv).rgb : vec3(1.0);

	// Optional vertex color (apply as a tint *in linear*; many pipelines want this)
	vec3 vtx_sRGB = useVertexColor ? vertexColor_sRGB : vec3(1.0);

	// Convert to linear for math
	vec3 base_L = srgbToLinear(base_sRGB);
	vec3 tex_L  = srgbToLinear(tex_sRGB);
	vec3 vtx_L  = srgbToLinear(vtx_sRGB);

	vec3 out_L;

	if (!hasAlbedoTex) {
		out_L = base_L; // color only
	} else {
		if (tintMode == 0) {
			// Texture only
			out_L = tex_L;
		} else if (tintMode == 2) {
			// Force grayscale treatment (skip detection)
			float lum = dot(tex_L, vec3(0.2126, 0.7152, 0.0722));
			out_L = mix(tex_L, base_L * lum, tintStrength);
		} else if (tintMode == 3) {
			// Masked lerp (use one channel as a mask-often A or R; customize as needed)
			vec4 texel = texture(albedoTex, uv);
			float mask = readMaskChannel(texel, tintMaskChannel);
			out_L = mix(tex_L, base_L, clamp(tintStrength * mask, 0.0, 1.0));
		} else {
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
float sampleOpacityMap(vec2 uv) {
    if (!hasOpacityMap) return 1.0; // neutral when no map
    vec4 opTex = texture(opacityMap, uv);

    // channel-aware extraction: if channel < 0 fallback to .r (legacy)
    float val;
    if (opacityChannel >= 0) {
        val = pickChannel(opTex, opacityChannel, opacityInvert, opacityScale, opacityBias);
    } else {
        // legacy fallback: red channel + optional invert/scale/bias
        val = opTex.r * opacityScale + opacityBias;
        if (opacityInvert != 0) val = 1.0 - val;
        val = clamp(val, 0.0, 1.0);
    }
    return val;
}

// compute fallback opacity from albedo/diffuse or legacy opacity texture
float sampleFallbackOpacity(vec2 uv) {
    float val = 1.0;

    if (renderingMode == 0) { // ADS -> use diffuse alpha (if present) and/or texture_opacity
        if (hasDiffuseTexture) {
            vec4 diff = texture(texture_diffuse, uv);
            val *= diff.a; // alpha from diffuse texture
        }
        if (hasOpacityTexture) {
            // legacy single-channel opacity texture (no channel packing currently)
            float optex = texture(texture_opacity, uv).r;
            if (opacityTextureInverted) optex = 1.0 - optex;
            val *= optex;
        }
    } else { // PBR mode -> albedo alpha may indicate opacity
        if (hasAlbedoMap) {
            vec4 alb = texture(albedoMap, uv);
            // Use albedo alpha as fallback but be conservative (do not force)
            val *= alb.a;
        }
    }

    return clamp(val, 0.0, 1.0);
}

vec3 acesToneMapping(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 uncharted2ToneMapping(vec3 color) {
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

vec3 applyToneMapping(vec3 color) {
    if (!hdrToneMapping) return color;
    
    if (toneMapMode == 1) {
        return acesToneMapping(color);
    } else if (toneMapMode == 2) {
        return uncharted2ToneMapping(color);
    } else {
        // Default Reinhard
        return color / (color + vec3(1.0));
    }
}
