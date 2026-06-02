#version 450 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 3) in vec2 texCoord0;
layout(location = 4) in vec2 texCoord1;
layout(location = 5) in vec2 texCoord2;
layout(location = 6) in vec2 texCoord3;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

out vec3 v_position;

void main()
{
	vec4 worldPos = modelMatrix * vec4(vertexPosition, 1.0);
	v_position = worldPos.xyz;
	gl_Position = projectionMatrix * viewMatrix * worldPos;
}
