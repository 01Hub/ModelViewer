#version 450 core

in VS_OUT {
    vec3 fragPos;
    vec3 normal;
} fs_in;

out vec4 fragColor;

uniform vec3 lightColor;
uniform float intensity;
uniform float intensityScale;

void main()
{
    // Apply intensity as per KHR spec
    vec3 scaledColor = lightColor * intensity * intensityScale;
    vec3 finalColor = scaledColor;
    fragColor = vec4(finalColor, 1.0);
}