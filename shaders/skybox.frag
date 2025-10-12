#version 450 core

out vec4 fragColor;

in vec3 texCoords;

uniform samplerCube skybox;
uniform float iblExposure = 1.0;
uniform bool hdrToneMapping = false;
uniform bool gammaCorrection = false;
uniform float screenGamma = 2.2;
uniform int toneMapMode = 1; // 0=Reinhard, 1=ACES, 2=Uncharted2

vec3 applyToneMapping(vec3 color);
vec3 acesToneMapping(vec3 color);
vec3 uncharted2ToneMapping(vec3 color);

void main()
{
    fragColor = texture(skybox, texCoords);
        
    // HDR tonemapping with IBL exposure
    if(hdrToneMapping)
        fragColor = vec4(applyToneMapping(fragColor.rgb * iblExposure), fragColor.a);//fragColor / (fragColor + vec4(1.0));
    // gamma correct
    if(gammaCorrection)
        fragColor = pow(fragColor, vec4(1.0/screenGamma));
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
