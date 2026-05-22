#version 450 core

out float fragColor;
in vec2 texCoords;

const float PI = 3.14159265359;

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

float D_Charlie(float roughness, float NoH)
{
    float invAlpha = 1.0 / max(roughness, 0.001);
    float sin2h = max(1.0 - NoH * NoH, 0.0078125);
    return (2.0 + invAlpha) * pow(sin2h, invAlpha * 0.5) / (2.0 * PI);
}

float lambdaSheenNumericHelper(float x, float alphaG)
{
    float oneMinusAlphaSq = (1.0 - alphaG) * (1.0 - alphaG);
    float a = mix(21.5473, 25.3245, oneMinusAlphaSq);
    float b = mix(3.82987, 3.32435, oneMinusAlphaSq);
    float c = mix(0.19823, 0.16801, oneMinusAlphaSq);
    float d = mix(-1.97760, -1.27393, oneMinusAlphaSq);
    float e = mix(-4.32054, -4.85967, oneMinusAlphaSq);
    return a / (1.0 + b * pow(x, c)) + d * x + e;
}

float lambdaSheen(float cosTheta, float alphaG)
{
    if (abs(cosTheta) < 0.5)
    {
        return exp(lambdaSheenNumericHelper(cosTheta, alphaG));
    }
    else
    {
        return exp(2.0 * lambdaSheenNumericHelper(0.5, alphaG) -
            lambdaSheenNumericHelper(1.0 - cosTheta, alphaG));
    }
}

float V_Sheen(float NdotL, float NdotV, float sheenRoughness)
{
    sheenRoughness = max(sheenRoughness, 0.000001);
    float alphaG = sheenRoughness * sheenRoughness;

    return clamp(1.0 / ((1.0 + lambdaSheen(NdotV, alphaG) +
        lambdaSheen(NdotL, alphaG)) *
        (4.0 * NdotV * NdotL)), 0.0, 1.0);
}

void buildOrthonormalBasis(vec3 N, out vec3 T, out vec3 B)
{
    if (N.z < -0.9999999)
    {
        T = vec3(0.0, -1.0, 0.0);
        B = vec3(-1.0, 0.0, 0.0);
    }
    else
    {
        float a = 1.0 / (1.0 + N.z);
        float b = -N.x * N.y * a;
        T = vec3(1.0 - N.x * N.x * a, b, -N.x);
        B = vec3(b, 1.0 - N.y * N.y * a, -N.y);
    }
}

vec3 ImportanceSampleCharlie(vec2 Xi, vec3 N, float sheenRoughness)
{
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

float IntegrateCharlieLUT(float NdotV, float sheenRoughness)
{
    vec3 V;
    V.x = sqrt(max(1.0 - NdotV * NdotV, 0.0));
    V.y = 0.0;
    V.z = NdotV;

    vec3 N = vec3(0.0, 0.0, 1.0);
    float sheenAccum = 0.0;

    const uint SAMPLE_COUNT = 2048u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleCharlie(Xi, N, sheenRoughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        if (NdotL > 0.0 && NdotH > 0.0)
        {
            float sheenD = D_Charlie(sheenRoughness, NdotH);
            float sheenV = V_Sheen(NdotL, NdotV, sheenRoughness);
            sheenAccum += sheenD * sheenV * NdotL;
        }
    }

    return sheenAccum / float(SAMPLE_COUNT);
}

void main()
{
    float NdotV = clamp(texCoords.x, 0.0, 1.0);
    float sheenRoughness = clamp(texCoords.y, 0.0, 1.0);
    fragColor = IntegrateCharlieLUT(NdotV, sheenRoughness);
}
