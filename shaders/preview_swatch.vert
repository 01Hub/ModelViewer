#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord; // Add texture coordinates

uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNormalMatrix;

out vec3 vNormal;
out vec3 vPos;
out vec2 vTexCoord; // Pass texture coordinates

out vec3 vWorldPos;

void main() 
{
    vPos = vec3(uModel * vec4(aPos, 1.0));
    vWorldPos = (uModel * vec4(aPos,1)).xyz;
    vNormal = normalize(uNormalMatrix * aNormal);
    vTexCoord = aTexCoord; // Pass through texture coordinates
    gl_Position = uMVP * vec4(aPos, 1.0);
}