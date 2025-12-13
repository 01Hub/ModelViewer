#version 450 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;

out VS_OUT {
    vec3 fragPos;
    vec3 normal;
} vs_out;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

void main()
{
    vs_out.fragPos = vec3(modelMatrix * vec4(vertexPosition, 1.0));
    vs_out.normal = normalize(mat3(modelMatrix) * vertexNormal);
    gl_Position = projectionMatrix * viewMatrix * vec4(vs_out.fragPos, 1.0);
}