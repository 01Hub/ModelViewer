#version 450 core
#extension GL_EXT_geometry_shader : enable
#extension GL_OES_geometry_shader : enable

layout (triangles) in;
layout (line_strip, max_vertices = 6) out;

in VS_OUT {
    vec3 positionView;
    vec3 normalView;
} gs_in[];

uniform mat4 projectionMatrix;
uniform float normalMagnitude;

out vec3 g_normal;
out vec3 g_position;
out vec2 g_texCoord2d;

in float clipDistX[];
in float clipDistY[];
in float clipDistZ[];
in float clipDist[];

out float g_clipDistX;
out float g_clipDistY;
out float g_clipDistZ;
out float g_clipDist;

void main()
{
    vec3 P0 = gs_in[0].positionView;
    vec3 P1 = gs_in[1].positionView;
    vec3 P2 = gs_in[2].positionView;

    vec3 V0 = P0 - P1;
    vec3 V1 = P2 - P1;

    vec3 N = cross(V1, V0);
    N = normalize(N);

    // Center of the triangle
    vec3 P = (P0+P1+P2) / 3.0;

    gl_Position = projectionMatrix * vec4(P, 1.0);
    g_clipDistX = clipDistX[0];
    g_clipDistY = clipDistY[0];
    g_clipDistZ = clipDistZ[0];
    g_clipDist = clipDist[0];

    gl_ClipDistance[0] = g_clipDistX;
    gl_ClipDistance[1] = g_clipDistY;
    gl_ClipDistance[2] = g_clipDistZ;
    gl_ClipDistance[3] = g_clipDist;
    EmitVertex();

    vec3 lineEndView = P + N * normalMagnitude;
    gl_Position = projectionMatrix * vec4(lineEndView, 1.0);
    g_clipDistX = clipDistX[1];
    g_clipDistY = clipDistY[1];
    g_clipDistZ = clipDistZ[1];
    g_clipDist = clipDist[1];

    gl_ClipDistance[0] = g_clipDistX;
    gl_ClipDistance[1] = g_clipDistY;
    gl_ClipDistance[2] = g_clipDistZ;
    gl_ClipDistance[3] = g_clipDist;
    EmitVertex();
    EndPrimitive();
}
