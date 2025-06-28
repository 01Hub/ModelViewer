#version 450 core
out vec4 fragColor;
in vec3 worldPos;
uniform samplerCube environmentMap;
uniform mat3 envMapRotationMatrix;

const float PI = 3.14159265359;
const float TWO_PI = 2.0 * PI;
const float HALF_PI = 0.5 * PI;

void main()
{
    // The world vector acts as the normal of a tangent surface
    // from the origin, aligned to worldPos. Given this normal, calculate all
    // incoming radiance of the environment. The result of this radiance
    // is the radiance of light coming from -Normal direction, which is what
    // we use in the PBR shader to sample irradiance.
    vec3 N = normalize(worldPos);
    vec3 irradiance = vec3(0.0);
    
    // More robust tangent space calculation - handles edge cases when N is parallel to up vector
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));
    
    float sampleDelta = 0.025;
    
    // Pre-calculate total sample count for better performance
    float phiSteps = ceil(TWO_PI / sampleDelta);
    float thetaSteps = ceil(HALF_PI / sampleDelta);
    float totalSamples = phiSteps * thetaSteps;
    
    for(float phi = 0.0; phi < TWO_PI; phi += sampleDelta)
    {
        for(float theta = 0.0; theta < HALF_PI; theta += sampleDelta)
        {
            // spherical to cartesian (in tangent space)
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
            // rotate sample vector to match environment map orientation
            vec3 rotatedSampleVec = envMapRotationMatrix * sampleVec;
            irradiance += texture(environmentMap, rotatedSampleVec).rgb * cos(theta) * sin(theta);
        }
    }
    
    irradiance = PI * irradiance / totalSamples;
    fragColor = vec4(irradiance, 1.0);
}
