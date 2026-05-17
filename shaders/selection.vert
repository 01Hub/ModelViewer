#version 450 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 9) in vec4 jointIndices;
layout(location = 10) in vec4 jointWeights;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform bool hasSkinning;
uniform int jointCount;
uniform mat4 jointMatrices[128];

mat4 computeSkinMatrix()
{
    if (!hasSkinning || jointCount <= 0)
        return mat4(1.0);

    mat4 skin = mat4(0.0);
    float totalWeight = 0.0;
    for (int i = 0; i < 4; ++i)
    {
        float weight = jointWeights[i];
        if (weight <= 0.0)
            continue;

        int jointIndex = int(jointIndices[i]);
        if (jointIndex < 0 || jointIndex >= jointCount || jointIndex >= 128)
            continue;

        skin += jointMatrices[jointIndex] * weight;
        totalWeight += weight;
    }

    return totalWeight > 0.0 ? skin : mat4(1.0);
}

void main()
{
    vec4 posedPosition = computeSkinMatrix() * vec4(vertexPosition, 1.0);
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * posedPosition;
}
