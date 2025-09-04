#version 450 core

in vec3 vWorldPos;

uniform bool selected;
uniform vec3 planeColor;
uniform sampler2D hatchMap;

// World-space mapping controls
uniform vec3  hatchOrigin;        // a point on the plane (world)
uniform vec3  uDir;               // unit tangent along U (world)
uniform vec3  vDir;               // unit tangent along V (world)
uniform float worldUnitsPerTile;  // world units per texture repeat

out vec4 fragColor;

void main()
{
    // Project world position to plane basis
    vec3 rel = vWorldPos - hatchOrigin;
    vec2 uv = vec2(dot(rel, uDir), dot(rel, vDir)) / worldUnitsPerTile;

    vec3 hatch = texture(hatchMap, uv).rgb;

    vec4 base = vec4(planeColor, 1.0);
    fragColor = mix(base, vec4(hatch, 1.0), 0.25);

    if (selected) {
        fragColor = mix(fragColor, vec4(1.0, 0.65, 0.0, 1.0), 0.5);
    }
}
