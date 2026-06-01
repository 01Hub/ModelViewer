#version 330 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in vec3 v_position[];
in vec3 v_normal[];
in vec4 v_color[];
in vec4 v_rawVertexColor[];
in vec2 v_texCoord0[];
in vec2 v_texCoord1[];
in vec2 v_texCoord2[];
in vec2 v_texCoord3[];
in vec3 v_tangent[];
in vec3 v_bitangent[];
in vec3 v_worldTangent[];
in vec3 v_worldBitangent[];
in vec3 v_reflectionPosition[];
in vec3 v_reflectionNormal[];
in vec3 v_tangentLightPos[];
in vec3 v_tangentViewPos[];
in vec3 v_tangentFragPos[];

in VS_OUT_SHADOW {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
    vec3 cameraPos;
    vec3 lightPos;
} gs_in_shadow[];

out vec3 g_position;
out vec3 g_normal;
out vec4 g_color;
out vec4 g_rawVertexColor;
out vec2 g_texCoord0;
out vec2 g_texCoord1;
out vec2 g_texCoord2;
out vec2 g_texCoord3;
out vec3 g_tangent;
out vec3 g_bitangent;
out vec3 g_worldTangent;
out vec3 g_worldBitangent;
out vec3 g_reflectionPosition;
out vec3 g_reflectionNormal;
out vec3 g_tangentLightPos;
out vec3 g_tangentViewPos;
out vec3 g_tangentFragPos;
flat out vec3 g_flatNormal;
flat out vec3 g_flatReflectionNormal;

out GS_OUT_SHADOW {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
    vec3 cameraPos;
    vec3 lightPos;
} gs_out_shadow;

uniform mat4 viewMatrix;

vec3 safeNormalize(vec3 value, vec3 fallback)
{
    float len = length(value);
    if (len > 1e-8)
        return value / len;
    return fallback;
}

void main()
{
    vec3 edge0 = v_position[1] - v_position[0];
    vec3 edge1 = v_position[2] - v_position[0];
    vec3 faceNormalWorld = cross(edge0, edge1);

    vec3 smoothWorldFallback = safeNormalize(
        v_reflectionNormal[0] + v_reflectionNormal[1] + v_reflectionNormal[2],
        vec3(0.0, 0.0, 1.0)
    );
    faceNormalWorld = safeNormalize(faceNormalWorld, smoothWorldFallback);

    vec3 smoothViewFallback = safeNormalize(
        v_normal[0] + v_normal[1] + v_normal[2],
        vec3(0.0, 0.0, 1.0)
    );
    vec3 faceNormalView = safeNormalize(mat3(viewMatrix) * faceNormalWorld, smoothViewFallback);

    for (int i = 0; i < gl_in.length(); ++i)
    {
        g_position = v_position[i];
        g_normal = v_normal[i];
        g_color = v_color[i];
        g_rawVertexColor = v_rawVertexColor[i];
        g_texCoord0 = v_texCoord0[i];
        g_texCoord1 = v_texCoord1[i];
        g_texCoord2 = v_texCoord2[i];
        g_texCoord3 = v_texCoord3[i];
        g_tangent = v_tangent[i];
        g_bitangent = v_bitangent[i];
        g_worldTangent = v_worldTangent[i];
        g_worldBitangent = v_worldBitangent[i];
        g_reflectionPosition = v_reflectionPosition[i];
        g_reflectionNormal = v_reflectionNormal[i];
        g_tangentLightPos = v_tangentLightPos[i];
        g_tangentViewPos = v_tangentViewPos[i];
        g_tangentFragPos = v_tangentFragPos[i];
        g_flatNormal = faceNormalView;
        g_flatReflectionNormal = faceNormalWorld;

        gs_out_shadow.FragPos = gs_in_shadow[i].FragPos;
        gs_out_shadow.Normal = gs_in_shadow[i].Normal;
        gs_out_shadow.TexCoords = gs_in_shadow[i].TexCoords;
        gs_out_shadow.FragPosLightSpace = gs_in_shadow[i].FragPosLightSpace;
        gs_out_shadow.cameraPos = gs_in_shadow[i].cameraPos;
        gs_out_shadow.lightPos = gs_in_shadow[i].lightPos;

        gl_Position = gl_in[i].gl_Position;
        gl_ClipDistance[0] = gl_in[i].gl_ClipDistance[0];
        gl_ClipDistance[1] = gl_in[i].gl_ClipDistance[1];
        gl_ClipDistance[2] = gl_in[i].gl_ClipDistance[2];
        gl_ClipDistance[3] = gl_in[i].gl_ClipDistance[3];
        EmitVertex();
    }

    EndPrimitive();
}
