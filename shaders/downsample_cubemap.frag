#version 450 core

out vec4 FragColor;
in vec3 WorldPos;

uniform samplerCube u_sourceMap;
uniform int u_currentMipLevel;

void main()
{
    // Simply sample from current mip level
    // GPU's texture filtering handles the downsampling automatically
    vec3 N = normalize(WorldPos);
    vec3 color = textureLod(u_sourceMap, N, float(u_currentMipLevel)).rgb;
    
    // Clamp negative values to prevent artifacts
    color = max(color, vec3(0.0));
    
    FragColor = vec4(color, 1.0);
}
