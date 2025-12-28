#version 450 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec4 vertexColor;
layout(location = 3) in vec2 texCoord0;
layout(location = 4) in vec2 texCoord1;
layout(location = 5) in vec2 texCoord2;
layout(location = 6) in vec2 texCoord3;
layout(location = 7) in vec3 vertexTangent;
layout(location = 8) in vec3 vertexBitangent;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelViewMatrix;
uniform mat3 normalMatrix;
uniform mat4 projectionMatrix;
uniform vec4 clipPlaneX;
uniform vec4 clipPlaneY;
uniform vec4 clipPlaneZ;
uniform mat4 lightSpaceMatrix;
uniform vec3 cameraPos;
uniform vec3 lightPos;

// user defined clip plane
uniform vec4 clipPlane;

out vec3 v_position;
out vec3 v_normal;
out vec4 v_color;
out vec2 v_texCoord0;
out vec2 v_texCoord1;
out vec2 v_texCoord2;
out vec2 v_texCoord3;
out vec3 v_tangent;
out vec3 v_bitangent;
out vec3 v_tangentLightPos;
out vec3 v_tangentViewPos;
out vec3 v_tangentFragPos;

out vec3 v_reflectionPosition;
out vec3 v_reflectionNormal;

out VS_OUT_SHADOW {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
    vec3 cameraPos;
    vec3 lightPos;
} vs_out_shadow;

void main()
{    
    vec4 worldPos = modelMatrix * vec4(vertexPosition, 1.0);
    v_position   = vec3(worldPos);              // vertex pos in eye coords
    vec3 transformedNormal = normalMatrix * vertexNormal;
    if (length(transformedNormal) < 0.01)
    {
        v_normal = vec3(0.0, 0.0, 0.0);  // Keep it zero
    }
    else
    {
        v_normal = normalize(transformedNormal);  // Only normalize if non-zero
    }
    v_color      = vertexColor;
    v_texCoord0 = texCoord0;
    v_texCoord1 = texCoord1;
    v_texCoord2 = texCoord2;
    v_texCoord3 = texCoord3;
    vec3 transformedTangent = normalMatrix * vertexTangent;
    if (length(transformedTangent) < 0.01)
        v_tangent = vec3(0.0);
    else
        v_tangent = normalize(transformedTangent);

    vec3 transformedBitangent = normalMatrix * vertexBitangent;
    if (length(transformedBitangent) < 0.01)
        v_bitangent = vec3(0.0);
    else
        v_bitangent = normalize(transformedBitangent);

    gl_Position = projectionMatrix * viewMatrix * worldPos;

    // Shadow mapping
    vs_out_shadow.FragPos = vec3(worldPos);
    vec3 shadowNormal = mat3(transpose(inverse(modelMatrix))) * vertexNormal;
    if (length(shadowNormal) < 0.01)
        vs_out_shadow.Normal = vec3(0.0);
    else
        vs_out_shadow.Normal = normalize(shadowNormal);
    vs_out_shadow.TexCoords = v_texCoord0;
    vs_out_shadow.FragPosLightSpace = lightSpaceMatrix * vec4(vs_out_shadow.FragPos, 1.0);
    vs_out_shadow.cameraPos = cameraPos;
    vs_out_shadow.lightPos = lightPos;

    // Cube environment mapping
    v_reflectionPosition = vec3(worldPos);
    vec3 reflNormal = mat3(transpose(inverse(modelMatrix))) * vertexNormal;
    if (length(reflNormal) < 0.01)
        v_reflectionNormal = vec3(0.0);
    else
        v_reflectionNormal = normalize(reflNormal);

    // Depth mapping
    vec3 T = mat3(modelViewMatrix) * vertexTangent;
    if (length(T) < 0.01)
        T = vec3(0.0);
    else
        T = normalize(T);

    vec3 N = mat3(modelViewMatrix) * vertexNormal;
    if (length(N) < 0.01)
        N = vec3(0.0);
    else
        N = normalize(N);

    vec3 B = normalize(cross(N, T));  // Safe now, N and T are valid
    if (length(N) > 0.01 && length(T) > 0.01)  // Only check if both valid
    {
        if (dot(cross(N, T), B) < 0.0)
            T = -T;
    }
    mat3 TBN = transpose(mat3(T, B, N));

    v_tangentLightPos = TBN * lightPos;
    v_tangentViewPos  = TBN * cameraPos;
    v_tangentFragPos  = TBN * v_position;

    // Assign clip distances for hardware clipping   
    gl_ClipDistance[0] = dot(clipPlaneX, modelViewMatrix* vec4(vertexPosition, 1));
    gl_ClipDistance[1] = dot(clipPlaneY, modelViewMatrix* vec4(vertexPosition, 1));
    gl_ClipDistance[2] = dot(clipPlaneZ, modelViewMatrix* vec4(vertexPosition, 1));
    gl_ClipDistance[3] = dot(clipPlane, modelViewMatrix* vec4(vertexPosition, 1));  

}