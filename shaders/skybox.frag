#version 450 core

out vec4 fragColor;

in vec3 texCoords;

uniform samplerCube skybox;
uniform float skyboxLod = 0.0;
uniform bool useSkyboxLod = false;
uniform float iblExposure = 1.0;
uniform bool hdrToneMapping = false;
uniform bool gammaCorrection = false;
uniform float screenGamma = 2.2;
// 0=KhronosPbrNeutral, 1=ACES_Narkowicz, 2=ACES_Hill,
// 3=AECS_Hill_Exposure_Boost, 4=Uncharted2, 5=Reinhard(Linear)
uniform int toneMapMode = 0; 

vec3 applyToneMapping(vec3 color);
vec3 acesToneMapping(vec3 color);
vec3 uncharted2ToneMapping(vec3 color);

void main()
{
    fragColor = useSkyboxLod
        ? textureLod(skybox, texCoords, skyboxLod)
        : texture(skybox, texCoords);
        
    // HDR tonemapping with IBL exposure
    if(hdrToneMapping)
        fragColor = vec4(applyToneMapping(fragColor.rgb), fragColor.a);//fragColor / (fragColor + vec4(1.0));
    // gamma correct
    if(gammaCorrection)
        fragColor = pow(fragColor, vec4(1.0/screenGamma));
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
    -0.53108,  1.10813, -0.07276,
    -0.07367, -0.00605,  1.07602
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
vec3 toneMap_KhronosPbrNeutral( vec3 color )
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
		// implemetation of ACES tone mapping
		color /= 0.6;
		color = toneMapACES_Hill(color);
	}
	else if (toneMapMode == 4)
	{
		color = uncharted2ToneMapping(color);		
	}
	else
	{
		// Default Reinhard
		color = color / (color + vec3(1.0));
	}

	return color;
}
