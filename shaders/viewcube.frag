#version 450 core

in VS_OUT {
    vec3 normalView;
} fs_in;

out vec4 fragColor;

uniform vec3 baseColor;
uniform vec3 lightDirView;
uniform float ambientStrength;
uniform float diffuseStrength;

void main()
{
    vec3 n = normalize(fs_in.normalView);
    vec3 l = normalize(lightDirView);
    float ndotl = max(dot(n, l), 0.0);
    vec3 shaded = baseColor * (ambientStrength + diffuseStrength * ndotl);
    fragColor = vec4(shaded, 1.0);
}
