#version 450 core

out vec4 fragColor;

uniform samplerCube environmentMap;
uniform mat3 faceBasis;             // Columns: U (right), V (up), W (forward)
uniform vec2 resolution;            // Face size
uniform float roughness;
uniform float environmentMapResolution;

const float PI = 3.14159265359;

float D_Charlie(float roughness, float NoH)
{
    // KHR_materials_sheen spec: alpha = sheenRoughness directly (no squaring).
    // The charlieLUT.b and sheenELUT.r PNGs were generated with this convention.
    float alphaG = max(roughness, 0.000001);
    float invAlpha = 1.0 / alphaG;
    float sin2h = max(1.0 - NoH * NoH, 0.0078125);
    return (2.0 + invAlpha) * pow(sin2h, invAlpha * 0.5) / (2.0 * PI);
}

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

void buildOrthonormalBasis(vec3 N, out vec3 T, out vec3 B)
{
    if (N.z < -0.9999999f) {
        T = vec3(0.0f, -1.0f, 0.0f);
        B = vec3(-1.0f, 0.0f, 0.0f);
    } else {
        float a = 1.0f / (1.0f + N.z);
        float b = -N.x * N.y * a;
        T = vec3(1.0f - N.x * N.x * a, b, -N.x);
        B = vec3(b, 1.0f - N.y * N.y * a, -N.y);
    }
}

vec3 ImportanceSampleCharlie(vec2 Xi, vec3 N, float sheenRoughness)
{
    // alpha = sheenRoughness directly, consistent with D_Charlie and the LUT PNGs
    float alpha = max(sheenRoughness, 0.001);
    float phi = 2.0 * PI * Xi.x;
    float sinTheta = pow(Xi.y, alpha / (2.0 * alpha + 1.0));
    float cosTheta = sqrt(max(1.0 - sinTheta * sinTheta, 0.0));

    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    vec3 tangent, bitangent;
    buildOrthonormalBasis(N, tangent, bitangent);
    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

void main()
{
    vec2 uv = (gl_FragCoord.xy / resolution) * 2.0 - 1.0;
    vec3 N = normalize(faceBasis * vec3(uv, 1.0));
    N.y = -N.y;

    vec3 R = N;
    vec3 V = R;

    const uint SAMPLE_COUNT = 2048u;
    vec3 prefilteredColor = vec3(0.0);
    float totalWeight = 0.0;
    float envMapMipLevels = float(textureQueryLevels(environmentMap));

    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleCharlie(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            float D = D_Charlie(roughness, max(dot(N, H), 0.0));
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            float pdf = D * NdotH / max(4.0 * HdotV, 0.0001) + 0.0001;

            float srcResolution = environmentMapResolution;
            float saTexel = 4.0 * PI / (6.0 * srcResolution * srcResolution);
            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

            float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);
            mipLevel = clamp(mipLevel, 0.0, envMapMipLevels - 1.0);

            vec3 envSample = textureLod(environmentMap, L, mipLevel).rgb;
            envSample = max(envSample, vec3(0.0));

            prefilteredColor += envSample * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor = prefilteredColor / max(totalWeight, 0.0001);
    fragColor = vec4(prefilteredColor, 1.0);
}
