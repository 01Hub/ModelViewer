#version 450 core
#define MAX_LIGHTS 8

in vec3 vNormal;
in vec3 vPos;
in vec2 vTexCoord;
in vec4 vTangentW;   // xyz = world-space tangent, w = handedness
out vec4 FragColor;

uniform vec3 uCamPos;

struct Light { vec3 position; vec3 color; };
uniform Light uLights[MAX_LIGHTS];
uniform int   uNumLights;

// === Base material uniforms ===
uniform vec3  uAlbedo;
uniform float uMetalness;
uniform float uRoughness;
uniform float uOpacity;
uniform float uClearcoat;
uniform float uClearcoatRoughness;
uniform vec3  uSheenColor;
uniform float uSheenRoughness;
uniform float uTransmission;
uniform float uIOR;
uniform float uSpecular;

// === Texture samplers ===
uniform sampler2D uAlbedoMap;
uniform sampler2D uMetalnessMap;
uniform sampler2D uRoughnessMap;
uniform sampler2D uNormalMap;
uniform sampler2D uOpacityMap;
uniform sampler2D uAOMap;
uniform sampler2D uEmissiveMap;

// === Enable flags ===
uniform bool uUseAlbedoMap;
uniform bool uUseMetalnessMap;
uniform bool uUseRoughnessMap;
uniform bool uUseNormalMap;
uniform bool uUseOpacityMap;
uniform bool uUseAOMap;
uniform bool uUseEmissiveMap;

// === Extra controls ===
uniform float uAOIntensity;
uniform vec3  uEmissiveColor;
uniform float uEmissiveIntensity;
uniform vec2  uUVScale;
uniform float uNormalIntensity;

// --- Normal mapping via TBN from vertex tangent ---
vec3 getNormalFromMap(vec2 texCoords)
{
    if (!uUseNormalMap) return normalize(vNormal);

    // Sample tangent-space normal [-1,1]
    vec3 nTS = texture(uNormalMap, texCoords).xyz * 2.0 - 1.0;
    nTS.xy *= uNormalIntensity;

    // Build TBN in world space
    vec3 N = normalize(vNormal);
    vec3 T = normalize(vTangentW.xyz);
    vec3 B = normalize(cross(N, T)) * vTangentW.w; // handedness in .w

    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * nTS);
}

void main()
{
    vec2 texCoords = vTexCoord * uUVScale;

    // --- Sample or fallback ---
    vec3  albedo    = uUseAlbedoMap    ? texture(uAlbedoMap,    texCoords).rgb : uAlbedo;
    albedo = clamp(albedo, vec3(0.01), vec3(1.0));

    float metalness = uUseMetalnessMap ? texture(uMetalnessMap, texCoords).r   : uMetalness;
    metalness = clamp(metalness, 0.0, 1.0);

    float roughness = uUseRoughnessMap ? texture(uRoughnessMap, texCoords).r   : uRoughness;
    roughness = clamp(roughness, 0.01, 1.0);

    float opacity   = uUseOpacityMap   ? texture(uOpacityMap,   texCoords).r   : uOpacity;
    opacity = clamp(opacity, 0.0, 1.0);

    float ao        = uUseAOMap        ? texture(uAOMap,        texCoords).r   : 1.0;
    ao = mix(1.0, ao, uAOIntensity);

    vec3 emissive = vec3(0.0);
    if (uUseEmissiveMap) {
        emissive = texture(uEmissiveMap, texCoords).rgb * uEmissiveColor * uEmissiveIntensity;
    }

    // --- Normals & view ---
    vec3 N = getNormalFromMap(texCoords);
    vec3 V = normalize(uCamPos - vPos);
    if (length(N) < 0.1) N = vec3(0.0, 1.0, 0.0);
    if (length(V) < 0.1) V = vec3(0.0, 0.0, 1.0);

    // --- Metal vs dielectric ---
    vec3 dielectricDiffuse  = albedo;
    vec3 dielectricSpecular = vec3(0.04);
    vec3 metallicDiffuse    = vec3(0.0);
    vec3 metallicSpecular   = albedo;

    vec3 diffuseColor  = mix(dielectricDiffuse,  metallicDiffuse,  metalness);
    vec3 specularColor = mix(dielectricSpecular, metallicSpecular, metalness);

    // --- Lighting ---
    vec3 color = vec3(0.075) * albedo; // small base light

    for (int i = 0; i < uNumLights; i++)
    {
        // You’re using these as direction-like lights already:
        vec3 L = normalize(uLights[i].position);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL <= 0.0) continue;

        vec3 H = normalize(V + L);
        float NdotH = max(dot(N, H), 0.0);

        float specPow    = clamp(mix(8.0, 128.0, 1.0 - roughness), 1.0, 256.0);
        float specFactor = pow(NdotH, specPow);

        vec3 diffuse = diffuseColor * NdotL;
        vec3 spec    = specularColor * specFactor;

        color += (diffuse + spec) * uLights[i].color * NdotL;
    }

    // --- Ambient (with AO) ---
    vec3 dielectricAmbient = albedo * 0.2 * ao;
    vec3 metallicAmbient   = albedo * 0.1 * ao;
    vec3 ambient           = mix(dielectricAmbient, metallicAmbient, metalness);
    color += ambient;

    // --- Clearcoat (simple lobe over light 0 direction-like) ---
    if (uClearcoat > 0.001 && uNumLights > 0)
    {
        vec3 Lc = normalize(uLights[0].position);
        vec3 Hc = normalize(V + Lc);
        float ccNdotH   = max(dot(N, Hc), 0.0);
        float ccSpecPow = clamp(mix(64.0, 512.0, 1.0 - uClearcoatRoughness), 1.0, 1024.0);
        float ccSpec    = pow(ccNdotH, ccSpecPow);
        color += vec3(0.25) * uClearcoat * ccSpec;
    }

    // --- Sheen ---
    if (length(uSheenColor) > 0.001)
    {
        float NdotV = max(dot(N, V), 0.0);
        float sheen = pow(clamp(1.0 - NdotV, 0.0, 1.0), 2.0);
        color += uSheenColor * sheen * 0.5;
    }

    // --- Transmission (very simple tint mix) ---
    if (uTransmission > 0.001)
    {
        vec3 glassTint = mix(albedo, vec3(1.0), clamp((uIOR - 1.0) * 0.3, 0.0, 1.0));
        color = mix(color, glassTint, uTransmission);
    }

    // --- Rim ---
    float rim = pow(clamp(1.0 - max(dot(N, V), 0.0), 0.0, 1.0), 2.0);
    color += albedo * rim * 0.2;

    // --- Emissive ---
    color += emissive;

    // --- Brightness tweak ---
    color *= mix(1.1, 2.5, metalness);

    // --- Final clamp ---
    color = clamp(color, vec3(0.0), vec3(20.0));

    FragColor = vec4(color, opacity);
}
