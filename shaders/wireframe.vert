#version 450 core

// Lightweight wireframe vertex shader.
// Attribute names and locations match main_scene.vert so existing VAOs bind correctly.

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec4 vertexColor;
layout(location = 3) in vec2 texCoord0;
layout(location = 9) in vec4 jointIndices;
layout(location = 10) in vec4 jointWeights;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform bool hasVertexColors;
uniform bool hasSkinning;
uniform int  jointCount;
uniform mat4 jointMatrices[128];

out vec2 v_texCoord;
out vec3 v_worldNormal;
out vec4 v_vertexColor;

mat4 computeSkinMatrix()
{
    if (!hasSkinning || jointCount <= 0)
        return mat4(1.0);

    mat4 skin = mat4(0.0);
    float totalWeight = 0.0;
    for (int i = 0; i < 4; ++i)
    {
        float weight = jointWeights[i];
        if (weight <= 0.0) continue;
        int idx = int(jointIndices[i]);
        if (idx < 0 || idx >= jointCount || idx >= 128) continue;
        skin += jointMatrices[idx] * weight;
        totalWeight += weight;
    }
    return totalWeight <= 0.0 ? mat4(1.0) : skin;
}

void main()
{
    mat4 skinMatrix = computeSkinMatrix();
    vec4 skinnedPos  = skinMatrix * vec4(vertexPosition, 1.0);
    vec3 skinnedNorm = mat3(skinMatrix) * vertexNormal;

    v_texCoord    = texCoord0;
    // mat3(modelMatrix) skips inverse-transpose; acceptable for wireframe depth cues.
    v_worldNormal = normalize(mat3(modelMatrix) * skinnedNorm);
    v_vertexColor = hasVertexColors ? vertexColor : vec4(1.0);
    gl_Position   = projectionMatrix * viewMatrix * modelMatrix * skinnedPos;
}
