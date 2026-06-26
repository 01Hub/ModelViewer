#version 450 core

layout (location = 0) in vec3 vertexPosition;
layout (location = 1) in vec3 vertexNormal;

out VS_OUT {
    vec3 positionView;
    vec3 normalView;
} vs_out;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform vec4 clipPlaneX;
uniform vec4 clipPlaneY;
uniform vec4 clipPlaneZ;
// user defined clip plane
uniform vec4 clipPlane;

out float clipDistX;
out float clipDistY;
out float clipDistZ;
out float clipDist;

void main()
{
    mat3 normalMatrix = mat3(transpose(inverse(modelViewMatrix)));
    vec4 positionView = modelViewMatrix * vec4(vertexPosition, 1.0);
    vs_out.positionView = positionView.xyz;
    vs_out.normalView = normalize(normalMatrix * vertexNormal);
    gl_Position = projectionMatrix * positionView;

    clipDistX = dot(clipPlaneX, positionView);
    clipDistY = dot(clipPlaneY, positionView);
    clipDistZ = dot(clipPlaneZ, positionView);
    clipDist = dot(clipPlane, positionView);
}
