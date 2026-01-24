#version 450 core

out vec4 fragColor;

uniform samplerCube environmentMap;
uniform mat3 uFaceBasis;     // Columns: U (right), V (up), W (forward)
uniform vec2 uResolution;    // Render target size (e.g., 64x64)

const float PI = 3.14159265359;
const float TWO_PI = 2.0 * PI;
const float HALF_PI = 0.5 * PI;

void main()
{
    // Compute normalized clip-space coordinates [-1, 1]
    vec2 uv = (gl_FragCoord.xy / uResolution) * 2.0 - 1.0;
    
    // Per-fragment direction derived from basis + clip coordinates
    vec3 N = normalize(uFaceBasis * vec3(uv, 1.0));

    // Invert Y to match cubemap convention
    N.y = -N.y;

    // Build orthonormal tangent frame using Frisvad's method
    vec3 T, B;
    if (N.z < -0.9999999f) 
    {
        // N pointing in -Z direction
        T = vec3(0.0f, -1.0f, 0.0f);
        B = vec3(-1.0f, 0.0f, 0.0f);
    }
    else 
    {
        float a = 1.0f / (1.0f + N.z);
        float b = -N.x * N.y * a;
        T = vec3(1.0f - N.x * N.x * a, b, -N.x);
        B = vec3(b, 1.0f - N.y * N.y * a, -N.y);
    }
    
    /*
    ================================================================================
    SAMPLE DELTA REFERENCE TABLE
    ================================================================================
    This table shows the relationship between sampleDelta values and the resulting
    number of samples per pixel during irradiance convolution.

    Calculation:
        numPhiSteps = ceil(2*PI / sampleDelta) = ceil(6.283 / sampleDelta)
        numThetaSteps = ceil(PI/2 / sampleDelta) = ceil(1.5708 / sampleDelta)
        totalSamples = numPhiSteps * numThetaSteps

    sampleDelta | numPhiSteps | numThetaSteps | totalSamples | Quality/Speed
    ------------|-------------|---------------|--------------|----------------
    0.050       | 126         | 32            | 4,032        | Fast (low)
    0.0333      | 189         | 48            | 9,072        | Good/Fast
    0.025       | 251         | 63            | 15,813       | Good (original)
    0.0167      | 376         | 94            | 35,344       | Very Good
    0.0125      | 503         | 126           | 63,378       | Excellent (4x)
    0.01        | 628         | 158           | 99,224       | Very High (6x)
    0.00833     | 754         | 189           | 142,506      | Overkill (9x)

    PRACTICAL RECOMMENDATIONS:
    - 0.025 (15,813 samples):  Original working value - clean results
    - 0.0125 (63,378 samples): 4x more samples - noticeable quality improvement
    - 0.01 (99,224 samples):   6x more samples - diminishing returns

    NOTES:
    - All values are per-pixel, per-cubemap-face during convolution
    - Irradiance map is 64x64, so total mip generation time scales with sampleDelta
    - Smaller sampleDelta = finer grid = smoother gradients but slower computation
    - Value 0.025 produces arc artifacts in extremely bright regions
    - Value 0.0125 eliminates artifacts for most use cases
    - This is a ONE-TIME offline computation during map load
    ================================================================================
    */

    float sampleDelta = 0.0125;
    
    int numPhiSteps = int(ceil(TWO_PI / sampleDelta));
    int numThetaSteps = int(ceil(HALF_PI / sampleDelta));
    float totalSamples = float(numPhiSteps * numThetaSteps);
    
    // Hemisphere sampling with cosine weighting
    vec3 irradiance = vec3(0.0);
    for(int iPhi = 0; iPhi < numPhiSteps; ++iPhi)
    {
        float phi = float(iPhi) * sampleDelta;
        for(int iTheta = 0; iTheta < numThetaSteps; ++iTheta)
        {
            float theta = float(iTheta) * sampleDelta;
            
            // Spherical to Cartesian in tangent space
            vec3 tangentSample = vec3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta)
            );
            
            // Transform from tangent space to world space
            vec3 sampleVec = tangentSample.x * T + 
                            tangentSample.y * B + 
                            tangentSample.z * N;
            
            // Sample environment and accumulate with cosine weighting
            irradiance += texture(environmentMap, sampleVec).rgb * cos(theta) * sin(theta);
        }
    }
    
    // Normalize by integral (PI) and sample count
    irradiance = PI * irradiance / totalSamples;
    fragColor = vec4(irradiance, 1.0);
}