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

void GenerateLine(int index)
{
    gl_Position = projectionMatrix * vec4(gs_in[index].positionView, 1.0);
    g_clipDistX = clipDistX[index];
    g_clipDistY = clipDistY[index];
    g_clipDistZ = clipDistZ[index];
    g_clipDist = clipDist[index];

    gl_ClipDistance[0] = g_clipDistX;
    gl_ClipDistance[1] = g_clipDistY;
    gl_ClipDistance[2] = g_clipDistZ;
    gl_ClipDistance[3] = g_clipDist;
    EmitVertex();

    vec3 lineEndView = gs_in[index].positionView + gs_in[index].normalView * normalMagnitude;
    gl_Position = projectionMatrix * vec4(lineEndView, 1.0);
    g_clipDistX = clipDistX[index];
    g_clipDistY = clipDistY[index];
    g_clipDistZ = clipDistZ[index];
    g_clipDist = clipDist[index];

    gl_ClipDistance[0] = g_clipDistX;
    gl_ClipDistance[1] = g_clipDistY;
    gl_ClipDistance[2] = g_clipDistZ;
    gl_ClipDistance[3] = g_clipDist;
    EmitVertex();

    EndPrimitive();
}

void main()
{
    GenerateLine(0); // first vertex normal
    GenerateLine(1); // second vertex normal
    GenerateLine(2); // third vertex normal
}
