#version 450 core
#extension GL_OES_standard_derivatives : enable

// Adpated from https://learnopengl.com/

in vec3 g_position;
in vec3 g_normal;
in vec2 g_texCoord2d;
in vec3 g_tangent;
in vec3 g_bitangent;
noperspective in vec3 g_edgeDistance;
in vec3 g_reflectionPosition;
in vec3 g_reflectionNormal;
in vec3 g_tangentLightPos;
in vec3 g_tangentViewPos;
in vec3 g_tangentFragPos;

in GS_OUT_SHADOW {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
    vec3 cameraPos;
    vec3 lightPos;
} fs_in_shadow;

uniform float opacity;
uniform bool texEnabled;
uniform sampler2D texUnit;

// ADS light maps
uniform sampler2D texture_diffuse;
uniform sampler2D texture_specular;
uniform sampler2D texture_emissive;
uniform sampler2D texture_normal;
uniform sampler2D texture_height;
uniform sampler2D texture_opacity;
uniform bool hasDiffuseTexture = false;
uniform bool hasSpecularTexture = false;
uniform bool hasEmissiveTexture = false;
uniform bool hasNormalTexture = false;
uniform bool hasHeightTexture = false;
uniform bool hasOpacityTexture = false;
uniform bool opacityTextureInverted = false;

uniform samplerCube envMap;
uniform sampler2D shadowMap;
uniform float shadowSoftness; // Adjustable softness factor
uniform float lightFarPlane; // Far plane of the light's perspective
// Gaussian weights for a 9x9 kernel
float gaussianKernel[9] = float[](
    0.05, 0.09, 0.12, 0.15, 0.18, 0.15, 0.12, 0.09, 0.05
);

// IBL
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;

// material parameters
uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D heightMap;
uniform sampler2D aoMap;
uniform sampler2D opacityMap;
uniform bool hasAlbedoMap;
uniform bool hasMetallicMap;
uniform bool hasRoughnessMap;
uniform bool hasNormalMap;
uniform bool hasAOMap;
uniform bool hasHeightMap;
uniform float heightScale = 0.02;
uniform bool hasOpacityMap;
uniform bool opacityMapInverted = false;

// Advanced PBR Material Properties
uniform sampler2D transmissionMap;
uniform sampler2D iorMap;
uniform sampler2D sheenColorMap;
uniform sampler2D sheenRoughnessMap;
uniform sampler2D clearcoatMap;
uniform sampler2D clearcoatRoughnessMap;
uniform sampler2D clearcoatNormalMap;

uniform bool hasTransmissionMap = false;
uniform bool hasIORMap = false;
uniform bool hasSheenColorMap = false;
uniform bool hasSheenRoughnessMap = false;
uniform bool hasClearcoatMap = false;
uniform bool hasClearcoatRoughnessMap = false;
uniform bool hasClearcoatNormalMap = false;

uniform bool envMapEnabled;
uniform mat3 envMapRotationMatrix;
uniform bool shadowsEnabled;
uniform bool selfShadowsEnabled;
uniform float shadowSamples;
uniform vec3 cameraPos;
uniform mat4 viewMatrix;
uniform bool sectionActive;
uniform int displayMode;
uniform int renderingMode;
uniform bool selected;
uniform vec4 reflectColor;
uniform bool floorRendering;
uniform bool lockLightAndCamera = true;
uniform bool hdrToneMapping = false;
uniform bool gammaCorrection = false;
uniform float screenGamma = 2.2;

uniform bool skyBoxEnabled;
uniform sampler2D skyboxColorTexture;

uniform vec4 u_topColor;    
uniform vec4 u_botColor;
uniform int u_gradientStyle;
uniform vec2 u_screenSize;
uniform vec3 u_screenCenter;
uniform float u_floorSize;
uniform bool isReflectedPass;

struct LineInfo
{
    float Width;
    vec4 Color;
};

uniform LineInfo Line;

struct LightSource
{
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    vec3 position;
};
uniform LightSource lightSource;

struct LightModel
{
    vec3 ambient;
};
uniform LightModel lightModel;

struct Material {
    vec3  emission;
    vec3  ambient;
    vec3  diffuse;
    vec3  specular;
    float shininess;
    bool  metallic;
};
uniform Material material;

struct PBRLighting {
    vec3 albedo;
    float metallic;
    float roughness;
    float ambientOcclusion;
    // Advanced PBR Properties
    float transmission;
    float ior;
    vec3 sheenColor;
    float sheenRoughness;
    float clearcoat;
    float clearcoatRoughness;
};
uniform PBRLighting pbrLighting;

const float PI = 3.14159265359;

layout( location = 0 ) out vec4 fragColor;

vec4    shadeBlinnPhong(LightSource source, LightModel model, Material mat, vec3 position, vec3 normal);
vec4    calculatePBRLighting(int renderMode, float side);

void    applyEnvironmentMapping(float alpha);
vec3    getNormalFromMap();
mat3    getTBNFromMap();
float   distributionGGX(vec3 N, vec3 H, float roughness);
float   geometrySchlickGGX(float NdotV, float roughness);
float   geometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3    fresnelSchlick(float cosTheta, vec3 F0);
vec3    fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);
vec2    parallaxMapping(vec2 texCoords, vec3 viewDir, sampler2D heightMap, float heightScale);
vec3    calcBumpedNormal(sampler2D map, vec2 texCoord);

// Advanced PBR Functions
vec3    fresnelSchlickIOR(float cosTheta, float ior);
float   distributionCharlie(vec3 N, vec3 H, float roughness);
float   geometryCharlie(float NdotV, float roughness);
vec3    calculateTransmission(vec3 N, vec3 V, vec3 L, float transmission, float ior, vec3 albedo);
vec3    calculateSheen(vec3 N, vec3 V, vec3 L, vec3 sheenColor, float sheenRoughness);
vec3    calculateClearcoat(vec3 N, vec3 V, vec3 L, float clearcoat, float clearcoatRoughness, vec3 clearcoatNormal);

float   calculateShadow(vec4 fragPosLightSpace);
// Function to fetch shadow value with variable kernel size
float   calculateShadowVariableKernel(vec4 fragPosLightSpace, vec3 fragPos, vec3 lightPos);

vec2    calculateBackgroundUV();
vec3    calculateBackgroundColor();

float floorRadius = u_floorSize * 0.5; // Adjust radius based on floor size
float fadeStart = floorRadius * 0.65;   // Start fading 
float fadeEnd = floorRadius * 1.05;     // Fully faded

void main()
{
    vec4 v_color_front;
    vec4 v_color_back;
    vec4 v_color;

    if (isReflectedPass) 
    {
        float distance = length(g_position - u_screenCenter);        
        if (distance > fadeStart)
            discard;        
    }

    if(renderingMode == 0)
    {
        v_color_front = shadeBlinnPhong(lightSource, lightModel, material, g_position, g_normal);
        v_color_back  = shadeBlinnPhong(lightSource, lightModel, material, g_position, -g_normal);
    }
    else
    {
        v_color_front = calculatePBRLighting(renderingMode, 1.0f);
        v_color_back  = calculatePBRLighting(renderingMode, -1.0f);
    }

    if( gl_FrontFacing )
    {
        v_color = v_color_front;
    }
    else
    {
        if(sectionActive)
            v_color = v_color_back + 0.15f;
        else
            v_color = v_color_back;
    }

    float mixVal; // overlay line
    if(displayMode == 0 || displayMode == 3) // shaded
    {
        if(texEnabled == true)
            fragColor = v_color * texture2D(texUnit, g_texCoord2d);
        else
            fragColor = v_color;
    }
    else if(displayMode == 1) // wireframe
    {
        fragColor = vec4(v_color.rgb, 0.75f);
    }
    else // wireshaded
    {
        // Find the smallest distance
        float d = min(g_edgeDistance.x, g_edgeDistance.y);
        d = min(d, g_edgeDistance.z);

        if (d < Line.Width - 1.0f) {
            mixVal = 1.0f;
        } else if (d > Line.Width + 1.0f) {
            mixVal = 0.0f;
        } else {
            float x = d - (Line.Width - 1.0f);
            mixVal = exp2(-2.0f * (x * x));
        }

        if (texEnabled == true)
            v_color *= texture2D(texUnit, g_texCoord2d);

        // Adaptive overlay color based on base diffuse
        vec3 baseColor = material.diffuse;
        float brightness = dot(baseColor, vec3(0.2126, 0.7152, 0.0722));

        vec3 overlayColor;
        if (brightness < 0.2) {
            overlayColor = baseColor + vec3(0.6); // brighten dark
        } else if (brightness > 0.8) {
            overlayColor = baseColor * 0.3; // darken bright
        } else {
            overlayColor = brightness > 0.5 ? baseColor * 0.5 : baseColor + vec3(0.4);
        }
        overlayColor = clamp(overlayColor, 0.0, 1.0);

        fragColor = mix(v_color, vec4(overlayColor, 1.0), mixVal);
    }

    // Get alpha from maps if available
    float alpha = opacity;
    if(renderingMode == 0 && hasOpacityTexture)
    {
        if(opacityTextureInverted)
            alpha = 1.0f - texture(texture_opacity, g_texCoord2d).r;
        else
            alpha = texture(texture_opacity, g_texCoord2d).r;
    }

    if(envMapEnabled && displayMode == 3)
    {
        applyEnvironmentMapping(alpha);
    }

    if (selected) // with glow
    {
        // Compute lighting
        vec3 norm = normalize(gl_FrontFacing ? g_normal : -g_normal);
        vec3 lightDir = normalize(lightSource.position);
        float diff = max(dot(norm, lightDir), 0.0);

        vec3 viewDir = normalize(cameraPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

        // Base color from fragColor
        vec3 baseColor = fragColor.rgb;

        // Lighten the color with lighting + subtle spec
        vec3 lightened = baseColor + vec3(0.3) * diff + vec3(0.2) * spec + vec3(0.1);
        lightened = clamp(lightened, 0.0, 1.0);

        // Apply a subtle transparency
        float alpha = fragColor.a * 0.99;

        // Add glow effect
        vec3 glowColor = lightened * 1.2; // Make the glow a bit brighter
        glowColor = clamp(glowColor, 0.0, 1.0);

        // Mix base color with the glow
        vec3 finalColor = mix(lightened, glowColor, 0.5); // blend base and glow color

        fragColor = vec4(finalColor, alpha);

        if (displayMode == 2)
            fragColor = mix(fragColor, Line.Color, mixVal);
    }
     
    if (floorRendering) 
    {
        // Compute distance-based blending factor
        float distance = length(g_position - u_screenCenter);
    
        // Set fade parameters first, before any calculations
        if (skyBoxEnabled) 
        {
            fadeStart = floorRadius * 0.64;
            fadeEnd = floorRadius * 0.67;  
        }
    
        // Early discard for pixels beyond fade range
        if (distance > fadeEnd)
            discard;
    
        // Calculate fade factor
        float fadeFactor = smoothstep(fadeStart, fadeEnd, distance);
        if (fadeFactor >= 1.0)
            discard;
    
        // Get background color
        vec3 backgroundColor;
        if (skyBoxEnabled)
        {
            vec3 viewDir = normalize(g_position - cameraPos);
            backgroundColor = texture(envMap, viewDir).rgb;
        
            // Apply power curve for smoother transition
            fadeFactor = pow(fadeFactor, 1.4);
        
            // Optional: Handle gamma correction if needed
            float gamma = 2.2;
            vec3 linearFrag = pow(fragColor.rgb, vec3(gamma));
            vec3 linearSky = pow(backgroundColor, vec3(gamma));
            vec3 linearMix = mix(linearFrag, linearSky, fadeFactor);
            fragColor.rgb = pow(linearMix, vec3(1.0 / gamma));
        
            // Handle alpha similar to gradient case
            fragColor.a *= (1.0 - fadeFactor);
        }   
        else
        {
            // Interpolate background gradient color
            backgroundColor = calculateBackgroundColor();
        
            // Blend floor color with the background gradient
            fragColor.rgb = mix(fragColor.rgb, backgroundColor, fadeFactor);
            fragColor.a *= (1.0 - fadeFactor);
        }
    } 
}

// ----------------------------------------------------------------------------
vec4 shadeBlinnPhong(LightSource source, LightModel model, Material mat, vec3 position, vec3 normal)
{    
    vec2 clippedTexCoord = g_texCoord2d;
    vec3 lightDir;
    vec3 viewDir;
    if(lockLightAndCamera)
    {
        lightDir = source.position;
        viewDir = vec3(0);
    }
    else
    {
        lightDir = source.position + cameraPos;
        viewDir = cameraPos;
    }

    if(hasNormalTexture)
    {
        // obtain normal from normal map in range [0,1]
        normal = calcBumpedNormal(texture_normal, g_texCoord2d);
    }    

    if (hasHeightTexture)
    {
        // Build TBN matrix
        vec3 n = normalize(g_normal);
        vec3 t = normalize(g_tangent - dot(g_tangent, n) * n);
        vec3 b = normalize(cross(n, t));
        mat3 TBN = mat3(t, b, n);

        // Compute view direction in world space
        vec3 viewDirWorld = normalize(cameraPos - g_position);

        // Transform to tangent space
        vec3 viewDirTangent = TBN * viewDirWorld;

        // Parallax mapping
        clippedTexCoord = parallaxMapping(g_texCoord2d, viewDirTangent, texture_height, heightScale);

        if(clippedTexCoord.x > 1.0 || clippedTexCoord.y > 1.0 ||
           clippedTexCoord.x < 0.0 || clippedTexCoord.y < 0.0)
            discard;

        normal = calcBumpedNormal(texture_normal, clippedTexCoord);
    }

    vec3 halfVector = normalize(lightDir + viewDir); // light half vector
    float nDotVP    = dot(normal, normalize(lightDir + viewDir));                 // normal . light direction
    float nDotHV    = max(0.f, dot(normal,  halfVector));                      // normal . light half vector
    float pf        = mix(0.f, pow(nDotHV, mat.shininess), step(0.f, nDotVP)); // power factor

    vec3 ambient    = source.ambient;
    vec3 diffuse    = source.diffuse * nDotVP;
    vec3 specular   = source.specular * pf;

    vec3 matAmbient = mat.ambient;
    if(hasDiffuseTexture)
    {
        matAmbient = texture2D(texture_diffuse, clippedTexCoord).rgb;
    }
    vec3 matDiffuse = mat.diffuse;
    if(hasDiffuseTexture)
    {
        matDiffuse = nDotVP * texture2D(texture_diffuse, clippedTexCoord).rgb;
    }
    vec3 matSpecular = mat.specular;
    if(hasSpecularTexture)
    {
        matSpecular = pf * texture2D(texture_specular, clippedTexCoord).rgb;
    }
    vec3 matEmissive = mat.emission;
    if(hasEmissiveTexture)
    {
        matEmissive = texture2D(texture_emissive, clippedTexCoord).rgb;
    }
    float alpha = opacity;
    if(hasOpacityTexture)
    {
        if(opacityTextureInverted)
            alpha = 1.0f - texture(texture_opacity, clippedTexCoord).r;
        else
            alpha = texture(texture_opacity, clippedTexCoord).r;
    }

    vec3 sceneColor = matEmissive + matAmbient * model.ambient;
    vec4 colorLinear;

    if(shadowsEnabled && displayMode == 3 && (selfShadowsEnabled || floorRendering)) // Shadow Mapping
    {        
        // Pass both fragment position and light position for distance calculation
        float shadowFactor = calculateShadowVariableKernel(
            fs_in_shadow.FragPosLightSpace, 
            fs_in_shadow.FragPos, 
            fs_in_shadow.lightPos
        );
            
        vec3 ambientContrib = ambient * matAmbient * 0.6;
        shadowFactor = clamp(shadowFactor, 0.0, 0.7);
        float lightFactor = 1.0 - shadowFactor;
        vec3 directContrib = lightFactor * (diffuse * matDiffuse + specular * matSpecular);
        colorLinear = vec4(clamp(sceneColor + ambientContrib + directContrib, 0.0, 1.0), alpha);
    }
    else
    {
        colorLinear =  vec4(clamp(sceneColor +
                             ambient  * matAmbient +
                             diffuse  * matDiffuse +
                             specular * matSpecular, 0.f, 1.f ), alpha);
    }

    // HDR tonemapping
    if(hdrToneMapping)
        colorLinear.rgb = colorLinear.rgb / (colorLinear.rgb + vec3(1.0));
    // gamma correct
    if(gammaCorrection)
        colorLinear = pow(colorLinear, vec4(1.0/screenGamma));

    return colorLinear;
}

// ----------------------------------------------------------------------------
// Calculate PBR lighting based on the render mode
vec4 calculatePBRLighting(int renderMode, float side) // side 1 = front, -1 = back
{
    vec3 normal = g_normal * side;
    vec3 albedo;
    float metallic;
    float roughness;
    float ambientOcclusion;
    // Advanced PBR properties
    float transmission;
    float ior;
    vec3 sheenColor;
    float sheenRoughness;
    float clearcoat;
    float clearcoatRoughness;
    vec3 clearcoatNormal;
    
    vec3 N; vec3 V; vec3 L;

    if(lockLightAndCamera)
    {
        V = normalize(lightSource.position);
        L = normalize(lightSource.position);
    }
    else
    {
        V = normalize(lightSource.position + cameraPos);
        L = normalize(lightSource.position + cameraPos);
    }

    vec2 texCoords = g_texCoord2d;
    vec2 clippedTexCoord = texCoords;

    if(renderMode == 1)
    {
        N = normalize(normal);
        albedo = pbrLighting.albedo;
        metallic = pbrLighting.metallic;
        roughness = pbrLighting.roughness;
        ambientOcclusion = pbrLighting.ambientOcclusion;
        // Advanced properties from uniforms
        transmission = pbrLighting.transmission;
        ior = pbrLighting.ior;
        sheenColor = pbrLighting.sheenColor;
        sheenRoughness = pbrLighting.sheenRoughness;
        clearcoat = pbrLighting.clearcoat;
        clearcoatRoughness = pbrLighting.clearcoatRoughness;
        clearcoatNormal = normalize(normal);
    }
    else
    {
        if(hasNormalMap)
            N = calcBumpedNormal(normalMap, g_texCoord2d) * side;
        else
            N = normalize(normal);

        if(hasHeightMap)
        {
            vec3 viewDir;
            if(lockLightAndCamera)
                viewDir = normalize(g_tangentViewPos - g_tangentFragPos);
            else
                viewDir = normalize(g_tangentLightPos + g_tangentFragPos);

            // Calculate TBN matrix
            vec3 normal = normalize(g_normal);
            vec3 tangent = normalize(g_tangent - dot(g_tangent, normal) * normal);
            vec3 bitangent = normalize(cross(normal, tangent));
            mat3 TBN = mat3(tangent, bitangent, normal);

            // Compute view direction in world space
            vec3 viewDirWorld = normalize(cameraPos - g_position);

            // Transform to tangent space
            vec3 viewDirTangent = TBN * viewDirWorld;

            // Apply parallax mapping and get the new texture coordinates
            clippedTexCoord = parallaxMapping(g_texCoord2d, viewDirTangent, heightMap, heightScale);

            // Ensure the new coordinates are within the [0, 1] range
            if(clippedTexCoord.x < 0.0 || clippedTexCoord.x > 1.0 ||
               clippedTexCoord.y < 0.0 || clippedTexCoord.y > 1.0)
                discard;

            // Recalculate the normal based on the bumped normal map and new texture coordinates
            N = calcBumpedNormal(normalMap, clippedTexCoord) * side;
        }

        // Material properties
        // === ALBEDO (Base Color) ===
        if(hasAlbedoMap)
        {
            vec3 textureColor = pow(texture(albedoMap, clippedTexCoord).rgb, vec3(2.2));
    
            // Check if albedo color is near white (indicating texture-only intent)
            float colorDeviation = length(pbrLighting.albedo - vec3(1.0));
    
            if(colorDeviation < 0.1) // Threshold for "white enough"
            {
                albedo = textureColor; // Pure texture
            }
            else
            {
                albedo = pbrLighting.albedo * textureColor; // Multiplicative
            }
        }
        else
        {
            albedo = pbrLighting.albedo;
        }

        // === METALLIC ===
        if(hasMetallicMap)
        {
            float metallicTexture = texture(metallicMap, clippedTexCoord).r;
            metallic = pbrLighting.metallic * metallicTexture; // Multiplicative
        }
        else
        {
            metallic = pbrLighting.metallic;
        }

        // === ROUGHNESS ===
        if(hasRoughnessMap)
        {
            float roughnessTexture = texture(roughnessMap, clippedTexCoord).r;
            roughness = pbrLighting.roughness * roughnessTexture; // Multiplicative
            roughness = clamp(roughness, 0.02, 1.0);
        }
        else
        {
            roughness = pbrLighting.roughness;
            roughness = clamp(roughness, 0.02, 1.0);
        }

        // === AMBIENT OCCLUSION ===
        if(hasAOMap)
        {
            float aoTexture = texture(aoMap, clippedTexCoord).r;
            ambientOcclusion = pbrLighting.ambientOcclusion * aoTexture; // Multiplicative
        }
        else
        {
            ambientOcclusion = pbrLighting.ambientOcclusion;
        }

        // === ADVANCED PBR PROPERTIES ===

        // TRANSMISSION
        if(hasTransmissionMap)
        {
            float transmissionTexture = texture(transmissionMap, clippedTexCoord).r;
            transmission = pbrLighting.transmission * transmissionTexture; // Multiplicative
        }
        else
        {
            transmission = pbrLighting.transmission;
        }

        // INDEX OF REFRACTION (IOR) - Usually additive/blend approach
        if(hasIORMap)
        {
            float iorTexture = texture(iorMap, clippedTexCoord).r;
            // IOR is often blended rather than multiplied since it's a physical property
            ior = mix(1.0, pbrLighting.ior, iorTexture); // Blend from air (1.0) to material IOR
        }
        else
        {
            ior = pbrLighting.ior;
        }

        // SHEEN COLOR
        if(hasSheenColorMap)
        {
            vec3 sheenColorTexture = texture(sheenColorMap, clippedTexCoord).rgb;
            float sheenColorDeviation = length(pbrLighting.sheenColor - vec3(1.0));
    
            if(sheenColorDeviation < 0.1)
            {
                sheenColor = sheenColorTexture; // Pure texture
            }
            else
            {
                sheenColor = pbrLighting.sheenColor * sheenColorTexture; // Multiplicative
            }
        }
        else
        {
            sheenColor = pbrLighting.sheenColor;
        }

        // SHEEN ROUGHNESS
        if(hasSheenRoughnessMap)
        {
            float sheenRoughnessTexture = texture(sheenRoughnessMap, clippedTexCoord).r;
            sheenRoughness = pbrLighting.sheenRoughness * sheenRoughnessTexture; // Multiplicative
        }
        else
        {
            sheenRoughness = pbrLighting.sheenRoughness;
        }

        // CLEARCOAT
        if(hasClearcoatMap)
        {
            float clearcoatTexture = texture(clearcoatMap, clippedTexCoord).r;
            clearcoat = pbrLighting.clearcoat * clearcoatTexture; // Multiplicative
        }
        else
        {
            clearcoat = pbrLighting.clearcoat;
        }

        // CLEARCOAT ROUGHNESS
        if(hasClearcoatRoughnessMap)
        {
            float clearcoatRoughnessTexture = texture(clearcoatRoughnessMap, clippedTexCoord).r;
            clearcoatRoughness = pbrLighting.clearcoatRoughness * clearcoatRoughnessTexture; // Multiplicative
        }
        else
        {
            clearcoatRoughness = pbrLighting.clearcoatRoughness;
        }

        // CLEARCOAT NORMAL (Special case - not multiplicative)
        if(hasClearcoatNormalMap)
        {
            clearcoatNormal = calcBumpedNormal(clearcoatNormalMap, clippedTexCoord) * side;
        }
        else
        {
            clearcoatNormal = N; // Use base normal
        }
    }

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // For transmission, we need to adjust F0 based on IOR
    if(transmission > 0.0) {
        float f0_from_ior = pow((ior - 1.0) / (ior + 1.0), 2.0);
        F0 = mix(F0, vec3(f0_from_ior), transmission);
    }

    if(metallic < 0.1) {
        F0 = max(F0, vec3(0.04)); // Ensure minimum reflectance
    }

    // reflectance equation
    vec3 Lo = vec3(0.0);

    // calculate light radiance
    vec3 H = normalize(V + L);
    float distance = length(lightSource.position - vec3(0));
    float attenuation = 1.0 / (distance * distance);
    
    // ENHANCEMENT 1: Increase light intensity for better visibility
    float lightIntensity = 3000.0f; // Moderate increase from 1000.0f
    vec3 lightColor = vec3(3.0f, 3.0f, 3.0f) * lightIntensity;
    
    vec3 radiance;
    if(shadowsEnabled && displayMode == 3 && (selfShadowsEnabled || floorRendering))
    {
        float shadowFactor = calculateShadowVariableKernel(
            fs_in_shadow.FragPosLightSpace,
            fs_in_shadow.FragPos,
            fs_in_shadow.lightPos
        );
        // ENHANCEMENT 2: Reduce shadow impact
        radiance = (lightSource.ambient + (1.0 - shadowFactor * 0.85)) * (lightSource.diffuse + lightSource.specular);
    }
    else
    {
        radiance = (lightSource.ambient + lightSource.diffuse + lightSource.specular);
    }

    // Base PBR BRDF calculation
    float NDF = distributionGGX(N, H, roughness);
    float G   = geometrySmith(N, V, L, roughness);
    vec3 F    = fresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

    vec3 nominator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    vec3 specular = nominator / max(denominator, 0.001);

    specular *= 1.5; // Make direct specular more visible

    // Energy conservation
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    // Base diffuse and specular contribution
    float NdotL = max(dot(N, L), 0.0);
    vec3 baseBRDF = (kD * albedo / PI + specular) * radiance * NdotL;

    // Add transmission contribution
    vec3 transmissionContrib = vec3(0.0);
    if(transmission > 0.0) {
        transmissionContrib = calculateTransmission(N, V, L, transmission, ior, albedo);
    }

    // Add sheen contribution
    vec3 sheenContrib = vec3(0.0);
    if(length(sheenColor) > 0.0) {
        sheenContrib = calculateSheen(N, V, L, sheenColor, sheenRoughness);
    }

    // Add clearcoat contribution
    vec3 clearcoatContrib = vec3(0.0);
    if(clearcoat > 0.0) {
        clearcoatContrib = calculateClearcoat(clearcoatNormal, V, L, clearcoat, clearcoatRoughness, clearcoatNormal);
    }

    // Combine all contributions
    Lo = baseBRDF + transmissionContrib + sheenContrib;
    
    // Apply clearcoat on top (it affects the entire BRDF)
    Lo = mix(Lo, Lo + clearcoatContrib, clearcoat);

    // ambient lighting (note that the IBL will replace
    // this ambient lighting with environment lighting).
    vec3 ambient;
    // ambient lighting (we now use IBL as the ambient term)
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuse = irradiance * albedo;

    if(displayMode == 3)
    {
        if(envMapEnabled)
        {
            vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

            kS = F;
            kD = 1.0 - kS;
            kD *= 1.0 - metallic;
                        
            vec3 I = normalize(cameraPos - g_reflectionPosition);
            vec3 R = refract(-I, normalize(-g_reflectionNormal), 1.0f);

            // Sample both the pre-filter map and the BRDF lut
            const float MAX_REFLECTION_LOD = textureQueryLevels(prefilterMap) - 1.0;
            vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
            vec2 brdf = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
        
            // ENHANCEMENT: Boost specular contribution significantly
            vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
            specular *= 2.5; // Increase specular visibility
        
            // Extra boost for metallic materials
            if(metallic > 0.1) {
                specular *= (1.0 + metallic * 1.5); // More metallic = more specular
            }
        
            // Reduce AO impact on specular (AO shouldn't affect reflections as much)
            float diffuseAO = mix(1.0, ambientOcclusion, 0.8);
            float specularAO = mix(1.0, ambientOcclusion, 0.3); // Much less AO on specular
        
            vec3 irradiance = texture(irradianceMap, N).rgb;
            vec3 diffuse = irradiance * albedo;
        
            ambient = (kD * diffuse * diffuseAO) + (specular * specularAO);
        }
        else
        {
            kS = fresnelSchlick(max(dot(N, V), 0.0), F0);
            kD = 1.0 - kS;
            kD *= 1.0 - metallic;
            float boostedAO = mix(1.0, ambientOcclusion, 0.8);
            ambient = (kD * diffuse) * boostedAO;
        }
    }
    else
    {
        float boostedAO = mix(1.0, ambientOcclusion, 0.8);
        ambient = ((lightSource.ambient * diffuse) + specular) * boostedAO;
    }

    // Emission map
    vec3 matEmissive = material.emission;
    if(hasEmissiveTexture)
    {
        matEmissive = texture2D(texture_emissive, clippedTexCoord).rgb;
    }

    vec3 color = matEmissive + ambient + Lo;

    // HDR tonemapping
    if(hdrToneMapping)
        color = color / (color + vec3(1.0));
    // gamma correct
    if(gammaCorrection)
        color = pow(color, vec3(1.0/screenGamma));

    float alpha = opacity;
    if(hasOpacityMap)
    {
        if(opacityMapInverted)
            alpha = 1.0f - texture(opacityMap, clippedTexCoord).r;
        else
            alpha = texture(opacityMap, clippedTexCoord).r;
    }

    // Handle transmission alpha
    float finalAlpha = opacity;
    if(transmission > 0.0) {
        // More subtle transmission alpha effect
        finalAlpha = mix(opacity, opacity * 0.8, transmission * 0.5);

        // Ensure minimum visibility for very thin materials
        finalAlpha = max(finalAlpha, 0.1);
    }

    if(hasOpacityMap) {
        float mapAlpha;
        if(opacityMapInverted)
        mapAlpha = 1.0f - texture(opacityMap, clippedTexCoord).r;
        else
        mapAlpha = texture(opacityMap, clippedTexCoord).r;

        finalAlpha *= mapAlpha;
    }

    return vec4(color, finalAlpha);
}

// ----------------------------------------------------------------------------
// Advanced PBR Functions

// IOR-based Fresnel calculation
vec3 fresnelSchlickIOR(float cosTheta, float ior)
{
    float f0 = pow((ior - 1.0) / (ior + 1.0), 2.0);
    return vec3(f0) + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

// Charlie distribution for sheen (fabric-like materials)
float distributionCharlie(vec3 N, vec3 H, float roughness)
{
    float alpha = roughness * roughness;
    float invAlpha = 1.0 / alpha;
    float cos2h = dot(N, H) * dot(N, H);
    float sin2h = max(1.0 - cos2h, 0.0078125); // 2^(-7), so sin2h is always > 0
    return (2.0 + invAlpha) * pow(sin2h, invAlpha * 0.5) / (2.0 * PI);
}

// Charlie geometry function for sheen
float geometryCharlie(float NdotV, float roughness)
{
    float alpha = roughness * roughness;
    float sinTheta = sqrt(1.0 - NdotV * NdotV);
    return NdotV / (NdotV + alpha * sinTheta);
}

// Calculate transmission contribution
vec3 calculateTransmission(vec3 N, vec3 V, vec3 L, float transmission, float ior, vec3 albedo)
{
    if(transmission <= 0.0) return vec3(0.0);

    vec3 H = normalize(V + L);
    float VdotH = clamp(dot(V, H), 0.0, 1.0);
    float NdotL = clamp(dot(N, L), -1.0, 1.0);
    float NdotV = clamp(dot(N, V), 0.0, 1.0);

    // Calculate proper Fresnel for transmission
    float f0 = pow((ior - 1.0) / (ior + 1.0), 2.0);
    float fresnel = f0 + (1.0 - f0) * pow(1.0 - abs(VdotH), 5.0);
    float transmittance = (1.0 - fresnel) * transmission;

    // Improved transmission with both forward and back scattering
    float backScatter = max(0.0, -NdotL) * 0.8; // Light from behind
    float forwardScatter = max(0.0, NdotL) * 0.5; // Light from front (subsurface)

    // Add thickness approximation (you can make this a uniform)
    float thickness = 0.1; // Adjust based on your model
    float attenuationFactor = exp(-thickness * (1.0 - transmission));

    vec3 transmissionColor = albedo * transmittance * attenuationFactor * (backScatter + forwardScatter);

    return transmissionColor;
}

// Calculate sheen contribution (for fabric-like materials)
vec3 calculateSheen(vec3 N, vec3 V, vec3 L, vec3 sheenColor, float sheenRoughness)
{
    vec3 H = normalize(V + L);
    float NdotL = clamp(dot(N, L), 0.0, 1.0);
    float NdotV = clamp(dot(N, V), 0.0, 1.0);
    float NdotH = clamp(dot(N, H), 0.0, 1.0);
    float VdotH = clamp(dot(V, H), 0.0, 1.0);

    // Charlie distribution and geometry
    float D = distributionCharlie(N, H, sheenRoughness);
    float G = geometryCharlie(NdotV, sheenRoughness) * geometryCharlie(NdotL, sheenRoughness);
    
    // Sheen BRDF
    vec3 sheenBRDF = sheenColor * D * G / (4.0 * NdotV * NdotL + 0.001);
    
    return sheenBRDF * NdotL;
}

// Calculate clearcoat contribution
vec3 calculateClearcoat(vec3 N, vec3 V, vec3 L, float clearcoat, float clearcoatRoughness, vec3 clearcoatNormal)
{
    vec3 H = normalize(V + L);
    float NdotL = clamp(dot(clearcoatNormal, L), 0.0, 1.0);
    float NdotV = clamp(dot(clearcoatNormal, V), 0.0, 1.0);
    float NdotH = clamp(dot(clearcoatNormal, H), 0.0, 1.0);
    float VdotH = clamp(dot(V, H), 0.0, 1.0);

    // Clearcoat uses a fixed IOR of 1.5 (typical for automotive clearcoat)
    vec3 F0_clearcoat = vec3(0.04); // F0 for IOR 1.5
    vec3 F = fresnelSchlick(VdotH, F0_clearcoat);
    
    float D = distributionGGX(clearcoatNormal, H, clearcoatRoughness);
    float G = geometrySmith(clearcoatNormal, V, L, clearcoatRoughness);
    
    vec3 clearcoatBRDF = (D * G * F) / (4.0 * NdotV * NdotL + 0.001);
    
    return clearcoatBRDF * clearcoat * NdotL;
}

// ----------------------------------------------------------------------------
float calculateShadow(vec4 fragPosLightSpace)
{
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;

    vec3 normal = normalize(fs_in_shadow.Normal);
    vec3 lightDir;
    if(lockLightAndCamera)
        lightDir = normalize(lightSource.position);
    else
        lightDir = normalize(lightSource.position + fs_in_shadow.cameraPos);
    //float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    float bias = clamp(0.005 * tan(acos(dot(normal, lightDir))), 0.005, 0.05);

    // PCF - Percentage Closer Filtering
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;
        }
    }

    shadow /= shadowSamples;

    // keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    if(projCoords.z > 1.0)
        shadow = 0.0;

    return shadow;
}

// Function to fetch shadow value with variable kernel size
float calculateShadowVariableKernel(vec4 fragPosLightSpace, vec3 fragPos, vec3 lightPos) 
{
    // Transform to [0,1] range for sampling
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    // Calculate distance for both kernel size and softness
    float distanceToLight = length(fragPos - lightPos);
    
    // Stepped kernel sizes for GPU coherency
    int kernelSize;
    if (distanceToLight < 5.0) kernelSize = 3; 
    else if (distanceToLight < 15.0) kernelSize = 4;
    else kernelSize = 5;
    
    // More aggressive adaptive shadow softness - increased multipliers
    float adaptiveSoftness = shadowSoftness * clamp(distanceToLight * 0.15, 1.0, 3.5);
    
    // Current depth
    float currentDepth = projCoords.z;
    
    // Shadow factor calculation
    float shadow = 0.0;
    float totalWeight = 0.0;
    
    // Variable kernel size loop with adaptive softness
    for (int x = -kernelSize; x <= kernelSize; ++x) 
    {
        for (int y = -kernelSize; y <= kernelSize; ++y) 
        {
            // Enhanced Gaussian weight calculation for smoother falloff
            float distance = sqrt(float(x * x + y * y));
            float weight = exp(-distance * distance / (2.0 * float(kernelSize * kernelSize) * 0.5));
            totalWeight += weight;
            
            // Increased adaptive softness multiplier for softer shadows
            vec2 offset = vec2(x, y) * adaptiveSoftness * 1.5 / lightFarPlane;
            
            // Sample shadow map with adaptive offset
            float sampleDepth = texture(shadowMap, projCoords.xy + offset).r;
            
            // Softer depth comparison with reduced bias and smooth transition
            float bias = mix(0.002, 0.008, clamp(distanceToLight * 0.05, 0.0, 1.0));
            float depthDiff = currentDepth - sampleDepth - bias;
            
            // Smooth shadow transition instead of hard cutoff
            float shadowContrib = smoothstep(-0.003, 0.003, depthDiff);
            shadow += shadowContrib * weight;
        }
    }
    
    // Normalize shadow factor
    shadow /= totalWeight;
    
    // Apply gentle gamma correction for softer appearance
    shadow = pow(shadow, 0.75);
    
    return shadow;
}

// Function for parallax mapping to simulate depth displacement
vec2 parallaxMapping(vec2 texCoords, vec3 viewDir, sampler2D heightMap, float heightScale)
{
    // Sample height from the height map (assuming grayscale)
    float height = texture(heightMap, texCoords).r;
    // Apply parallax scaling and bias
    float parallaxAmount = height * heightScale;
    // Offset texture coordinates based on the view direction (xy components)
    return texCoords - parallaxAmount * viewDir.xy;
}

void applyEnvironmentMapping(float alpha)
{
    vec3 I = normalize(g_reflectionPosition - cameraPos);
    vec3 N = normalize(g_reflectionNormal); 

    if(pbrLighting.transmission > 0.0) // Handle transmission materials specifically
    {
        // Use proper IOR for transmission, but keep the refract hack for Z-up
        float iorRatio = 1.0 / pbrLighting.ior;
        vec3 R = refract(I, N, iorRatio);
        R = envMapRotationMatrix * R;

        // Sample environment with slight roughness for transmission
        float transmissionRoughness = max(pbrLighting.roughness, 0.1);
        vec3 envColor = textureLod(envMap, R, transmissionRoughness * 3.0).rgb;

        // Apply transmission color filtering
        vec3 filteredEnvColor = envColor * pbrLighting.albedo;

        // Blend based on transmission strength
        float transmissionStrength = pbrLighting.transmission * 0.7;
        fragColor = mix(fragColor, vec4(filteredEnvColor, fragColor.a), transmissionStrength);
    }
    else if(alpha < 1.0f && !floorRendering) // Regular transparency - keep your existing logic
    {
        vec3 R = refract(I, N, 1.0f - alpha); // Keep your original approach
        R = envMapRotationMatrix * R;

        if(texEnabled == true)
        fragColor = mix(texture2D(texUnit, g_texCoord2d), vec4(texture(envMap, R).rgb, 1.0f - alpha), 1.0f - alpha);
        else
        fragColor = vec4(texture(envMap, R).rgb, 1.0f - alpha);

        // Your original color mixing
        vec4 colour = fragColor;
        fragColor = mix(fragColor, colour, alpha/1.0f);
    }
    else if(renderingMode == 0) // Reflection - keep existing
    {
        vec3 R = refract(-I, N, 1.0f); // Keep Z-up hack
        R = envMapRotationMatrix * -R;

        float specularIntensity = dot(min(material.specular, vec3(1.0)), vec3(0.2126, 0.7152, 0.0722));
        float fresnelPower = 1.0 + (1.0 - specularIntensity) * 4.0;
        float fresnel = pow(1.0 - max(dot(-I, N), 0.0), fresnelPower);

        float factor = material.metallic ? length(material.specular) : length(material.diffuse);
        float roughness = 1.0 - (material.shininess / 128.0);
        float roughnessReduction = 1.0 - (roughness * 0.8);

        float reflectionStrength = (material.shininess / 128.0) * factor * fresnel * roughnessReduction * 0.3;
        fragColor = mix(fragColor, vec4(texture(envMap, R).rgb, 1.0f), reflectionStrength);
    }
}

// ----------------------------------------------------------------------------
// Easy trick to get tangent-normals to world-space to keep PBR code simplified.
// Don't worry if you don't get what's going on; you generally want to do normal
// mapping the usual way for performance anways; I do plan make a note of this
// technique somewhere later in the normal mapping tutorial.
vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(normalMap, g_texCoord2d).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(g_position);
    vec3 Q2  = dFdy(g_position);
    vec2 st1 = dFdx(g_texCoord2d);
    vec2 st2 = dFdy(g_texCoord2d);

    vec3 N   = normalize(g_normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

mat3 getTBNFromMap()
{
    vec3 tangentNormal = texture(normalMap, g_texCoord2d).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(g_position);
    vec3 Q2  = dFdy(g_position);
    vec2 st1 = dFdx(g_texCoord2d);
    vec2 st2 = dFdy(g_texCoord2d);

    vec3 N   = normalize(g_normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return TBN;
}

// ----------------------------------------------------------------------------
float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.001); // prevent divide by zero for roughness=0.0 and NdotH=1.0
}
// ----------------------------------------------------------------------------
float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// http://ogldev.atspace.co.uk/www/tutorial26/tutorial26.html
vec3 calcBumpedNormal(sampler2D map, vec2 texCoord)
{
    vec3 normal = normalize(g_normal);
    vec3 tangent = normalize(g_tangent - dot(g_tangent, normal) * normal);
    vec3 bitangent = normalize(cross(normal, tangent));
    mat3 TBN = mat3(tangent, bitangent, normal);

    vec3 bumpMapNormal = texture(map, texCoord).rgb;
    bumpMapNormal = 2.0 * bumpMapNormal - 1.0;
    // Uncomment the next line if your normal maps need Y flipped
    // bumpMapNormal.y = -bumpMapNormal.y;
    return normalize(TBN * bumpMapNormal);
}

vec2 calculateBackgroundUV() {
    vec2 ndc = (gl_FragCoord.xy / u_screenSize) * 2.0 - 1.0;
    return ndc * 0.5 + 0.5;
}

vec3 calculateBackgroundColor() {
    vec2 v_uv = calculateBackgroundUV();
    
    vec4 frag_color;
    if (u_gradientStyle == 0) {
        frag_color = mix(u_botColor, u_topColor, v_uv.y);
    }
    else if (u_gradientStyle == 1) {
        frag_color = mix(u_topColor, u_botColor, v_uv.x);
    }
    else if (u_gradientStyle == 2) {
        float diagonal_factor = (v_uv.x + (1.0 - v_uv.y)) * 0.5;
        frag_color = mix(u_topColor, u_botColor, diagonal_factor);
    }
    else if (u_gradientStyle == 3) {
        float diagonal_factor = ((1.0 - v_uv.x) + (1.0 - v_uv.y)) * 0.5;
        frag_color = mix(u_topColor, u_botColor, diagonal_factor);
    }
    else {
        frag_color = mix(u_botColor, u_topColor, v_uv.y);
    }
    
    return frag_color.rgb;
}