# Advanced PBR Materials Usage Guide

## Overview
This guide covers the advanced PBR materials that showcase sophisticated rendering effects like clearcoat, sheen, transmission, and complex material layering. These materials demonstrate the full capabilities of modern physically-based rendering.

## Clearcoat Materials

### What is Clearcoat?
Clearcoat represents a thin transparent layer on top of the base material, commonly found in automotive paints, piano finishes, and coated surfaces.

**Key Properties:**
- `setClearcoat(float)` - Strength of the clearcoat layer (0.0-1.0)
- `setClearcoatRoughness(float)` - Roughness of the clearcoat surface (typically very low)

### Available Clearcoat Materials:

#### `CAR_PAINT_RED()`
- **Use Case**: Automotive paint, toys, glossy surfaces
- **Properties**: Bright red base with smooth clearcoat
- **Clearcoat**: 1.0 (full strength)
- **Clearcoat Roughness**: 0.05 (very smooth)

#### `CAR_PAINT_METALLIC_BLUE()`
- **Use Case**: Metallic automotive finishes
- **Properties**: Slightly metallic base with clearcoat
- **Special**: Combines metallic base (0.3 metalness) with clearcoat

#### `PIANO_BLACK()`
- **Use Case**: Piano finishes, high-gloss furniture
- **Properties**: Very dark base with mirror-like clearcoat
- **Clearcoat Roughness**: 0.01 (extremely smooth)

## Sheen Materials

### What is Sheen?
Sheen provides a soft, fabric-like reflection that's different from traditional specular highlights. It's essential for rendering fabrics, velvet, and organic materials.

**Key Properties:**
- `setSheenColor(QVector3D)` - Color of the sheen highlight
- `setSheenRoughness(float)` - Controls sheen highlight distribution

### Available Sheen Materials:

#### `VELVET_RED()`
- **Use Case**: Velvet fabrics, plush materials
- **Properties**: High roughness base with bright sheen
- **Sheen Color**: Bright with red tint (1.0, 0.8, 0.8)

#### `SATIN_FABRIC()`
- **Use Case**: Silk, satin, luxury fabrics
- **Properties**: Smooth sheen with sharp highlight
- **Sheen Roughness**: 0.1 (sharp reflection)

#### `MICROFIBER_CLOTH()`
- **Use Case**: Cleaning cloths, synthetic fabrics
- **Properties**: Diffuse sheen, very rough base
- **Sheen Roughness**: 0.8 (very diffuse)

## Transmission Materials

### What is Transmission?
Transmission allows light to pass through materials, creating realistic glass, crystal, and transparent effects.

**Key Properties:**
- `setTransmission(float)` - Amount of light transmission (0.0-1.0)
- `setIOR(float)` - Index of refraction for realistic light bending
- `setBlendMode(BlendMode::Alpha)` - Required for transparency

### Available Transmission Materials:

#### `FROSTED_GLASS()`
- **Use Case**: Bathroom windows, privacy glass
- **Properties**: High transmission with moderate roughness
- **Transmission**: 0.8, **Roughness**: 0.3 (creates frosting effect)

#### `COLORED_GLASS_GREEN()`
- **Use Case**: Colored windows, bottles, art glass
- **Properties**: Color filtering with high transmission
- **Transmission**: 0.85, **Opacity**: 0.15

#### `CRYSTAL_QUARTZ()`
- **Use Case**: Crystals, gems, clear minerals
- **Properties**: High clarity with realistic IOR
- **IOR**: 1.544 (actual quartz value)

## Emissive Materials

### Enhanced Emissive Properties
Modern PBR supports HDR emissive materials that can actually contribute light to the scene.

**Key Properties:**
- `setEmissiveStrength(float)` - Multiplier for emissive color (can be > 1.0)
- Values > 1.0 create HDR lighting effects

### Available Emissive Materials:

#### `NEON_BLUE()`
- **Use Case**: Neon signs, LED strips, sci-fi elements
- **Emissive Strength**: 3.0 (bright HDR emission)

#### `LED_WHITE()`
- **Use Case**: LED panels, light sources
- **Emissive Strength**: 5.0 (very bright)

## Complex Materials

### Materials with Multiple Advanced Properties:

#### `IRIDESCENT_SOAP_BUBBLE()`
- **Properties**: Transmission + Sheen
- **Use Case**: Soap bubbles, oil slicks, iridescent surfaces
- **Features**:
  - Nearly full transmission (0.95)
  - Iridescent sheen color
  - Extremely low roughness (0.005)

#### `CARBON_FIBER()`
- **Properties**: Clearcoat + Slight Metalness
- **Use Case**: Composite materials, high-tech surfaces
- **Features**:
  - Resin clearcoat layer (0.6 strength)
  - Slightly metallic base (0.2 metalness)

#### `WET_ASPHALT()`
- **Properties**: Clearcoat representing water layer
- **Use Case**: Wet roads, rain effects
- **Features**:
  - Water clearcoat with water IOR (1.33)
  - Rough base material (0.8 roughness)

## Advanced Helper Functions

### `createAnimatedEmissive()`
Creates materials with time-varying emissive strength for animated effects:

```cpp
// Example usage:
float currentTime = getCurrentTime();
GLMaterial pulsing = GLMaterial::createAnimatedEmissive(
    QVector3D(0.1f, 0.1f, 0.8f),  // base color
    QVector3D(0.2f, 0.2f, 1.0f),  // emissive color
    2.0f,                         // max emissive strength
    currentTime                   // time for animation
);
```

### `blendMaterials()`
Blends two materials for layered effects:

```cpp
// Example: Blend metal with rust
GLMaterial rustMetal = GLMaterial::blendMaterials(
    GLMaterial::METAL_IRON_RAW(),
    GLMaterial::RED_RUBBER(),  // represents rust
    0.3f  // 30% rust, 70% metal
);
```

## Shader Requirements

To fully utilize these advanced materials, your shaders should support:

1. **Clearcoat**: Additional specular layer calculations
2. **Sheen**: Fabric-style reflection model
3. **Transmission**: Ray refraction and color filtering
4. **HDR Emissive**: Values > 1.0 for bloom effects
5. **IOR**: Fresnel calculations for realistic reflections

## Usage Tips

### Performance Considerations:
- **Clearcoat**: Doubles specular calculations
- **Transmission**: Requires additional ray tracing or approximation
- **Sheen**: Additional reflection calculations
- **Complex Materials**: Use sparingly for hero objects

### Visual Guidelines:
- **Clearcoat**: Best for hard surfaces (cars, pianos, phones)
- **Sheen**: Perfect for fabrics, velvet, organic materials
- **Transmission**: Use for glass, liquids, crystals
- **Emissive**: Light sources, screens, glowing objects

### Common Parameter Ranges:

#### Clearcoat:
- **Strength**: 0.0-1.0 (1.0 for full automotive clear coat)
- **Roughness**: 0.01-0.1 (very smooth for realistic results)

#### Sheen:
- **Color**: Usually bright (0.8-1.0) for visible effect
- **Roughness**: 0.1-0.8 (0.1 for sharp, 0.8 for diffuse)

#### Transmission:
- **Amount**: 0.0-1.0 (0.9+ for clear glass)
- **IOR**: 1.0-3.0 (1.33 water, 1.5 glass, 2.4 diamond)

#### Emissive:
- **Strength**: 1.0-10.0+ (>3.0 for visible lighting contribution)

## Material Combination Examples

### Layered Materials
Combine properties for realistic complex surfaces:

```cpp
// Wet metal (clearcoat water layer on metal)
GLMaterial wetMetal = GLMaterial::METAL_STEEL();
wetMetal.setClearcoat(0.4f);           // Water layer
wetMetal.setClearcoatRoughness(0.1f);  // Slightly rough water
wetMetal.setIOR(1.33f);                // Water IOR

// Dusty glass (reduced transmission, increased roughness)
GLMaterial dustyGlass = GLMaterial::GLASS();
dustyGlass.setTransmission(0.6f);      // Reduced transmission
dustyGlass.setRoughness(0.2f);         // Surface roughness from dust
dustyGlass.setOpacity(0.3f);           // More opaque
```

### Fabric Variations
Different sheen settings for various fabric types:

```cpp
// Silk (sharp sheen)
GLMaterial silk = GLMaterial::SATIN_FABRIC();
silk.setSheenRoughness(0.05f);         // Very sharp highlight

// Cotton (diffuse sheen)
GLMaterial cotton = GLMaterial::FABRIC();
cotton.setSheenColor(QVector3D(0.3f, 0.3f, 0.3f));  // Subtle sheen
cotton.setSheenRoughness(0.9f);        // Very diffuse

// Denim (directional sheen)
GLMaterial denim = GLMaterial::BLUE_PLASTIC();
denim.setRoughness(0.8f);              // Rough base
denim.setSheenColor(QVector3D(0.4f, 0.5f, 0.8f));   // Blue-tinted sheen
denim.setSheenRoughness(0.6f);         // Moderate sheen spread
```

## Shader Integration Guide

### Vertex Shader Requirements
No additional vertex shader changes needed - all calculations happen in fragment shader.

### Fragment Shader Additions

#### Clearcoat Implementation:
```glsl
// Additional uniforms needed
uniform float u_clearcoat;
uniform float u_clearcoatRoughness;

// In fragment shader BRDF calculation
vec3 clearcoatSpecular = computeSpecular(
    clearcoatNormal,           // May be different from base normal
    viewDir, 
    lightDir, 
    u_clearcoatRoughness,
    vec3(0.04)                 // F0 for clearcoat (typically 0.04)
);

// Combine with base material
vec3 finalColor = mix(baseColor, clearcoatSpecular, u_clearcoat);
```

#### Sheen Implementation:
```glsl
// Additional uniforms
uniform vec3 u_sheenColor;
uniform float u_sheenRoughness;

// Fabric sheen calculation (simplified)
float sheenTerm = pow(1.0 - dot(normal, viewDir), u_sheenRoughness);
vec3 sheenContribution = u_sheenColor * sheenTerm;

// Add to final color
vec3 finalColor = baseColor + sheenContribution;
```

#### Transmission Implementation:
```glsl
// Additional uniforms
uniform float u_transmission;
uniform float u_ior;

// Refraction calculation
vec3 refractionDir = refract(viewDir, normal, 1.0 / u_ior);
vec3 transmittedLight = sampleEnvironment(refractionDir) * u_transmission;

// Blend with reflection based on Fresnel
float fresnel = computeFresnel(dot(normal, viewDir), u_ior);
vec3 finalColor = mix(transmittedLight, reflectedLight, fresnel);
```

## Real-World Material Examples

### Automotive:
- **Car Paint**: Use clearcoat materials with appropriate base colors
- **Chrome Bumpers**: High metalness + low roughness + high IOR
- **Tires**: Black rubber with very high roughness
- **Headlight Lenses**: Transmission materials with slight yellow tint

### Architecture:
- **Marble Floors**: Stone materials with moderate clearcoat
- **Glass Windows**: Transmission materials with realistic IOR
- **Fabric Furniture**: Sheen materials with appropriate colors
- **Metal Railings**: Metallic materials with weather-appropriate roughness

### Electronics:
- **Phone Screens**: High clearcoat + emissive for content display
- **Plastic Casings**: Plastic materials with slight clearcoat
- **LED Indicators**: Emissive materials with HDR strength
- **Circuit Boards**: Complex materials blending metal and plastic

### Organic Materials:
- **Skin**: Subsurface approximation with transmission
- **Hair**: Anisotropic sheen (requires shader modification)
- **Eyes**: Complex layered material with clearcoat cornea
- **Fabric Clothing**: Sheen materials with fabric-appropriate parameters

## Troubleshooting Common Issues

### Clearcoat Problems:
- **Too reflective**: Reduce clearcoat strength or increase roughness
- **Unrealistic**: Ensure clearcoat roughness is much lower than base roughness
- **Performance**: Consider using clearcoat only on hero objects

### Sheen Problems:
- **Too subtle**: Increase sheen color brightness
- **Wrong look**: Adjust sheen roughness (lower = sharper, higher = diffuse)
- **Color issues**: Ensure sheen color complements base material

### Transmission Problems:
- **Not transparent enough**: Increase transmission value, decrease opacity
- **Wrong refraction**: Check IOR values against real materials
- **Render order**: Ensure transparent materials render back-to-front

### Emissive Problems:
- **Not glowing**: Increase emissive strength above 1.0
- **Too bright**: Use HDR tone mapping in post-processing
- **No lighting contribution**: Ensure renderer supports emissive lighting

## Performance Optimization

### LOD (Level of Detail) Strategy:
1. **Distance-based**: Use simpler materials for distant objects
2. **Importance-based**: Full complexity for hero objects only
3. **Effect-based**: Disable expensive effects when not visible

### Shader Variants:
Create multiple shader versions:
- **Full PBR**: All advanced features enabled
- **Standard PBR**: Basic PBR without advanced effects
- **Legacy**: Fallback to traditional lighting for low-end devices

This comprehensive guide should help you effectively implement and use advanced PBR materials in your rendering pipeline. The key is to balance visual quality with performance requirements based on your specific use case.