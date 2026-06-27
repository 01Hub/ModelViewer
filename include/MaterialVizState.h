#pragma once

#include "GLMaterial.h"
#include "GltfVariantData.h"

#include <QMap>
#include <QVector>
#include <limits>
#include <vector>

// Shader-facing material state for a mesh.
//
// Owns the GLMaterial (PBR + ADS properties + texture IDs), the texture-alpha
// flag, and the KHR_materials_variants tables.  Volume thickness/attenuation
// base values are cached here so applyScaledVolumeProperties() can rescale them
// when the mesh is scaled without permanently corrupting the authored values.
//
// Intentionally free of GL resources, transform state, and geometry data.
class MaterialVizState
{
public:
    MaterialVizState();

    // ---- Core material --------------------------------------------------
    GLMaterial& material() { return _material; }
    const GLMaterial& material() const { return _material; }
    void setMaterial(const GLMaterial& m) { _material = m; }

    // ---- Texture-alpha flag ---------------------------------------------
    bool hasTextureAlpha() const { return _hasTextureAlpha; }
    void setHasTextureAlpha(bool v) { _hasTextureAlpha = v; }

    // ---- Volume properties cache ----------------------------------------
    // cacheBaseVolumeProperties() snapshots the current thickness/attenuation
    // values so they can be rescaled without losing the authored originals.
    void cacheBaseVolumeProperties();
    // applyScaledVolumeProperties() rescales cached values by the given uniform
    // scale factor and writes them back into _material.
    void applyScaledVolumeProperties();

    float baseThicknessFactor()     const { return _baseThicknessFactor; }
    float baseAttenuationDistance() const { return _baseAttenuationDistance; }

    // ---- KHR_materials_variants -----------------------------------------
    void setVariantMappings(const QVector<GltfVariantMapping>& m)   { _variantMappings = m; }
    const QVector<GltfVariantMapping>& variantMappings() const       { return _variantMappings; }

    void setAllVariantMaterials(const QMap<int, GLMaterial>& mats)   { _allVariantMaterials = mats; }
    const QMap<int, GLMaterial>& allVariantMaterials() const          { return _allVariantMaterials; }

    bool hasVariants() const { return !_variantMappings.isEmpty(); }

    // Returns a pointer to the pre-built GLMaterial for the given variant index,
    // or nullptr when this mesh has no mapping for that variant (keep default).
    // Pass variantIndex = -1 to retrieve the original/default material.
    const GLMaterial* materialForVariant(int variantIndex, int originalMaterialIndex) const;

    // ---- Texture list ---------------------------------------------------
    // Raw textures loaded from the source asset (by AssImpMesh). Used to build
    // the optimised PrecomputedTexture binding cache during render setup.
    // AssImpMesh keeps a reference alias _textures → this vector for zero
    // call-site churn (same pattern as GLMaterial& _material in TriangleMesh).
    const std::vector<GLMaterial::Texture>& textures() const { return _textures; }
    std::vector<GLMaterial::Texture>&       textures()       { return _textures; }
    void setTextures(std::vector<GLMaterial::Texture> t)     { _textures = std::move(t); }

private:
    GLMaterial _material;
    bool       _hasTextureAlpha = false;
    float      _baseThicknessFactor     = 0.f;
    float      _baseAttenuationDistance = std::numeric_limits<float>::infinity();

    QVector<GltfVariantMapping>      _variantMappings;
    QMap<int, GLMaterial>            _allVariantMaterials;
    std::vector<GLMaterial::Texture> _textures;
};
