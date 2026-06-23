#version 450 core

// Lightweight wireframe fragment shader.
// Samples base colour (factor + optional albedo texture + vertex colour), applies a
// simple key-light diffuse term for 3D depth cues, then outputs one of two results:
//   isWireframePass = false  ->  WIREFRAME:    lit colour at 75% alpha
//   isWireframePass = true   ->  WIRESHADED wire pass: brightness-contrast overlay

in vec2 v_texCoord;
in vec3 v_worldNormal;
in vec4 v_vertexColor;

uniform vec3      baseColor;      // material albedo colour factor
uniform bool      hasAlbedoMap;   // true when an albedo texture is bound
uniform sampler2D albedoMap;      // unit 0 — only sampled when hasAlbedoMap
uniform bool      isWireframePass;

out vec4 fragColor;

void main()
{
    // Compose colour exactly as main_scene does for the base layer:
    //   albedo factor * vertex colour * albedo texture
    vec3 color = baseColor * v_vertexColor.rgb;
    if (hasAlbedoMap)
        color *= texture(albedoMap, v_texCoord).rgb;

    // Key-light diffuse — fixed world-space direction gives 3D depth cues
    // without depending on scene light positions.
    const vec3 keyLight = normalize(vec3(1.0, 1.5, 2.0));
    float diffuse  = max(dot(normalize(v_worldNormal), keyLight), 0.0);
    float lighting = 0.35 + 0.65 * diffuse;   // 35% ambient + 65% diffuse range
    color *= lighting;

    if (isWireframePass)
    {
        // WIRESHADED wire overlay — brightness-contrast logic applied after lighting.
        float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
        vec3 overlay;
        if (brightness < 0.2)
            overlay = color + vec3(0.6);
        else if (brightness > 0.8)
            overlay = color * 0.3;
        else
            overlay = brightness > 0.5 ? color * 0.5 : color + vec3(0.4);
        fragColor = vec4(clamp(overlay, 0.0, 1.0), 1.0);
    }
    else
    {
        // WIREFRAME — semi-transparent lit colour
        fragColor = vec4(color, 0.75);
    }
}
