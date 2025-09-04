#version 450 core

layout(location=0) in vec3 vertexPosition;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

out vec3 vWorldPos;

void main()
{
    vec4 wp = modelMatrix * vec4(vertexPosition, 1.0);
    vWorldPos = wp.xyz;                               // World-space position
    gl_Position = projectionMatrix * viewMatrix * wp;
}
