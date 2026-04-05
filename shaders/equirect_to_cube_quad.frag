#version 450 core

out vec4 FragColor;
in vec2 FragCoord;

uniform sampler2D equirectangularMap;
uniform int faceIndex;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

vec3 ComputeWorldPos(int face, vec2 uv)
{
    // uv is in [0,1], compute the 3D position the cube would have
    vec2 coord = uv * 2.0 - 1.0;  // Convert to [-1,1]    

    if (face == 0)  // +X
        return vec3(1.0, coord.y, -coord.x);
    else if (face == 1)  // -X
        return vec3(-1.0, coord.y, coord.x);
    else if (face == 2)  // +Y
        return vec3(coord.x, -1.0, coord.y);
    else if (face == 3)  // -Y
        return vec3(coord.x, 1.0, -coord.y);
    else if (face == 4)  // +Z
        return vec3(coord.x, coord.y, 1.0);
    else  //
        return vec3(-coord.x, coord.y, -1.0);
}

void main()
{

    // Compute WorldPos exactly as the cube mesh would
    vec3 worldPos = ComputeWorldPos(faceIndex, FragCoord);
    
    // Apply the same spherical map sampling as the original
    vec2 equirectUV = SampleSphericalMap(normalize(worldPos));
    
    // Sample equirectangular texture
    vec3 color = texture(equirectangularMap, equirectUV).rgb;
    color = max(color, vec3(0.0));
    
    FragColor = vec4(color, 1.0);
}
