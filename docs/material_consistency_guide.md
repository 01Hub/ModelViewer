# Material Consistency Guide: ADS and PBR Properties

## Overview
This guide explains the uniform approach I've implemented for all GLMaterial functions to ensure both traditional ADS (Ambient, Diffuse, Specular) lighting and modern PBR (Physically Based Rendering) properties are properly set.

## Uniform Pattern for All Materials

### 1. Constructor Pattern
Every material now follows this constructor pattern:
```cpp
GLMaterial mat({ ambient_r, ambient_g, ambient_b },     // ambient
               { diffuse_r, diffuse_g, diffuse_b },      // diffuse  
               { specular_r, specular_g, specular_b },   // specular
               { emissive_r, emissive_g, emissive_b },   // emissive
               shininess_value,                          // shininess
               is_metallic_bool,                         // metallic flag
               opacity_value);                           // opacity
```

### 2. PBR Properties Setup
After the constructor, each material sets:
```cpp
mat.setAlbedoColor(QVector3D(...));
mat.setMetalness(...);
mat.setRoughness(...);
mat.setIOR(...);
mat.setShadingModel(ShadingModel::PBR);
mat.updateConsistency();  // Important: synchronizes ADS and PBR
```

## Material-Specific Guidelines

### Stone Materials
- **Ambient**: 20% of diffuse color (stones reflect environment)
- **Diffuse**: Primary material color
- **Specular**: Very low (0.01-0.05) - stones are generally matte
- **Shininess**: Low (4-12) for rough surfaces
- **Roughness**: High (0.8-0.95) - natural stones are rough
- **IOR**: 1.45 (typical for mineral materials)

### Metal Materials
- **Ambient**: 20% of diffuse color
- **Diffuse**: Base metal color (lower than albedo for metals)
- **Specular**: High (0.6-1.0) - metals are reflective
- **Shininess**: High (32-128) depending on polish
- **Metalness**: 1.0 (fully metallic)
- **Roughness**: Low to medium (0.1-0.6) depending on finish
- **IOR**: Material-specific (1.4-3.0)

### Dielectric Materials (Glass, Plastic, etc.)
- **Ambient**: Low (0.02-0.2) 
- **Diffuse**: Primary color
- **Specular**: Based on IOR using Fresnel equations
- **Shininess**: High for smooth materials
- **Metalness**: 0.0 (non-metallic)
- **Roughness**: Based on surface finish
- **IOR**: Material-specific

## Key Relationships

### Ambient Calculation
```cpp
ambient = diffuse * 0.2f;  // Standard 20% ambient factor
```

### Shininess to Roughness
```cpp
roughness = 1.0f - (shininess / 128.0f);
shininess = (1.0f - roughness) * 128.0f;
```

### Metallic Materials Specular
```cpp
if (metallic) {
    specular = albedo_color;  // Metals use albedo as specular
} else {
    // Dielectrics use Fresnel F0
    float f0 = (ior - 1.0f) / (ior + 1.0f);
    f0 = f0 * f0;
    specular = QVector3D(f0, f0, f0);
}
```

## Updated Materials List

### Stone Materials (Complete ADS + PBR)
- `STONE_GRANITE()` - Speckled gray stone
- `STONE_LIMESTONE()` - Light beige stone
- `STONE_MARBLE()` - White polished stone
- `STONE_SLATE()` - Dark gray stone
- `STONE_SANDSTONE()` - Tan/brown stone
- `STONE_BASALT()` - Very dark volcanic stone
- `STONE_TRAVERTINE()` - Light cream stone
- `STONE_QUARTZITE()` - Light reflective stone
- `STONE_SOAPSTONE()` - Dark green-gray stone

### Metal Materials (Complete ADS + PBR)
- `METAL_TITANIUM()` - Blue-gray metal
- `METAL_PLATINUM()` - Bright white metal
- `METAL_MAGNESIUM()` - Very bright white metal
- `METAL_ZINC()` - Blue-gray metal
- `METAL_NICKEL()` - Silver-gray metal
- `METAL_ALUMINUM()` - Bright silver metal
- `METAL_IRON_RAW()` - Dark unpolished iron
- `METAL_COBALT()` - Blue-tinted metal
- `METAL_PEWTER()` - Matte gray metal
- `METAL_TUNGSTEN()` - Dark highly reflective metal

### Other Materials (Complete ADS + PBR)
- `GLASS()` - Clear transparent glass
- `WATER()` - Blue-tinted water
- `DIAMOND()` - Extremely reflective crystal
- `CERAMIC()` - White ceramic
- `FABRIC()` - Brown textile with sheen
- `SKIN()` - Human skin tone
- `PAPER()` - Off-white paper
- `WOOD()` - Brown wood
- `METAL()` - Generic metal
- `PLASTIC()` - Generic plastic
- `STONE()` - Generic gray stone

## Best Practices

1. **Always call `updateConsistency()`** after setting material properties
2. **Set both ADS and PBR properties** for maximum compatibility
3. **Use realistic IOR values** based on material physics
4. **Clamp all values** to valid ranges
5. **Consider the material's real-world properties** when setting roughness and metalness
6. **Use appropriate blend modes** for transparent materials
7. **Test materials** in both legacy and PBR rendering pipelines

## Validation Checklist

For each material, verify:
- [ ] All ADS values are set and clamped (0.0-1.0)
- [ ] All PBR values are set and clamped appropriately  
- [ ] Shininess matches roughness relationship
- [ ] Metalness and metallic flag are consistent
- [ ] IOR is realistic for the material type
- [ ] `updateConsistency()` is called
- [ ] Material works in both rendering modes

This uniform approach ensures that all materials work consistently across different rendering pipelines and provides a solid foundation for both traditional and modern lighting calculations.