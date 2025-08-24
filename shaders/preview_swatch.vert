#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec4 aTangent;   // xyz + handedness (w)

uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNormalMatrix;               // from model matrix

out vec3 vNormal;      // world-space normal
out vec3 vPos;         // world-space position (kept for your use)
out vec2 vTexCoord;    // uv
out vec3 vWorldPos;    // world-space position (duplicate of vPos; kept for compatibility)
out vec4 vTangentW;    // world-space tangent (xyz) + handedness (w)

void main()
{
    // World-space position
    vec4 Pw = uModel * vec4(aPos, 1.0);
    vPos      = Pw.xyz;
    vWorldPos = Pw.xyz;

    // Transform normal and tangent with the normal matrix
    vec3 Nw = normalize(uNormalMatrix * aNormal);
    vec3 Tw = normalize(uNormalMatrix * aTangent.xyz);

    vNormal   = Nw;
    vTangentW = vec4(Tw, aTangent.w);   // pass handedness through .w

    // UV passthrough
    vTexCoord = aTexCoord;

    // Clip-space position
    gl_Position = uMVP * vec4(aPos, 1.0);
}
