#include "AssImpMeshBuilder.h"
#include "AssImpModelLoader.h"
#include "GLWidget.h"
#include "SceneMesh.h"

#include <QString>

SceneMesh* AssImpMeshBuilder::build(const AssImpMeshData& meshData,
                                    QOpenGLShaderProgram* shader,
                                    GLWidget* gl)
{
    std::vector<GLMaterial::Texture> textures = meshData.textures;
    for (GLMaterial::Texture& texture : textures)
    {
        const QString originalPath = QString::fromStdString(texture.path);

        TextureSamplerSettings samplers{ texture.wrapS, texture.wrapT, texture.minFilter, texture.magFilter };
        if (!texture.imageData.isNull())
        {
            const QString cacheKey = originalPath.isEmpty()
                ? QStringLiteral("embedded://%1/%2/%3")
                    .arg(meshData.name)
                    .arg(QString::fromStdString(texture.type))
                    .arg(textures.size())
                : originalPath;
            texture.id = gl->getOrCreateTextureCached(cacheKey, texture.imageData, samplers);
        }
        else if (!texture.path.empty())
        {
            const QString texturePath = QString::fromStdString(texture.path);
            if (texturePath.endsWith(".ktx2", Qt::CaseInsensitive))
            {
                texture.id = gl->getOrLoadKtx2TextureCached(texturePath, texture.type, samplers);
            }
            else
            {
                texture.id = gl->getOrLoadTextureCached(texturePath, samplers);
            }
        }
        else if (texture.id != 0)
        {
            // Keep externally prepared IDs only when there is no path/image payload to re-resolve.
        }
    }

    GLMaterial resolvedMaterial = meshData.material;
    for (const GLMaterial::Texture& texture : textures)
    {
        const QString texturePath = QString::fromStdString(texture.path);
        if (texture.type == "albedoMap" || texture.type == "texture_diffuse")
        {
            resolvedMaterial.setAlbedoTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setAlbedoMap(texturePath);
        }
        else if (texture.type == "metallicMap" || texture.type == "texture_specular")
        {
            resolvedMaterial.setMetallicTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setMetallicMap(texturePath);
        }
        else if (texture.type == "roughnessMap")
        {
            resolvedMaterial.setRoughnessTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setRoughnessMap(texturePath);
        }
        else if (texture.type == "normalMap" || texture.type == "texture_normal")
        {
            resolvedMaterial.setNormalTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setNormalMap(texturePath);
        }
        else if (texture.type == "aoMap" || texture.type == "occlusionMap" || texture.type == "occlusion")
        {
            resolvedMaterial.setOcclusionTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setAOMap(texturePath);
        }
        else if (texture.type == "opacityMap" || texture.type == "texture_opacity")
        {
            resolvedMaterial.setOpacityTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setOpacityMap(texturePath);
        }
        else if (texture.type == "heightMap" || texture.type == "texture_height")
        {
            resolvedMaterial.setHeightTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setHeightMap(texturePath);
        }
        else if (texture.type == "emissiveMap" || texture.type == "texture_emissive")
        {
            resolvedMaterial.setEmissiveTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setEmissiveMap(texturePath);
        }
        else if (texture.type == "transmissionMap")
        {
            resolvedMaterial.setTransmissionTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setTransmissionMap(texturePath);
        }
        else if (texture.type == "iorMap")
        {
            resolvedMaterial.setIORTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setIORMap(texturePath);
        }
        else if (texture.type == "sheenColorMap")
        {
            resolvedMaterial.setSheenColorTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setSheenColorMap(texturePath);
        }
        else if (texture.type == "sheenRoughnessMap")
        {
            resolvedMaterial.setSheenRoughnessTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setSheenRoughnessMap(texturePath);
        }
        else if (texture.type == "clearcoatColorMap")
        {
            resolvedMaterial.setClearcoatColorTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setClearcoatColorMap(texturePath);
        }
        else if (texture.type == "clearcoatRoughnessMap")
        {
            resolvedMaterial.setClearcoatRoughnessTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setClearcoatRoughnessMap(texturePath);
        }
        else if (texture.type == "clearcoatNormalMap")
        {
            resolvedMaterial.setClearcoatNormalTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setClearcoatNormalMap(texturePath);
        }
        else if (texture.type == "iridescenceMap")
        {
            resolvedMaterial.setIridescenceTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setIridescenceMap(texturePath);
        }
        else if (texture.type == "iridescenceThicknessMap")
        {
            resolvedMaterial.setIridescenceThicknessTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setIridescenceThicknessMap(texturePath);
        }
        else if (texture.type == "specularFactorMap")
        {
            resolvedMaterial.setSpecularFactorTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setSpecularFactorMap(texturePath);
        }
        else if (texture.type == "specularColorMap")
        {
            resolvedMaterial.setSpecularColorTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setSpecularColorMap(texturePath);
        }
        else if (texture.type == "anisotropyMap")
        {
            resolvedMaterial.setAnisotropyTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setAnisotropyMap(texturePath);
        }
        else if (texture.type == "thicknessMap")
        {
            resolvedMaterial.setThicknessTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setThicknessMap(texturePath);
        }
        else if (texture.type == "diffuseMap")
        {
            resolvedMaterial.setDiffuseTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setDiffuseMap(texturePath);
        }
        else if (texture.type == "diffuseTransmissionMap")
        {
            resolvedMaterial.setDiffuseTransmissionTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setDiffuseTransmissionMap(texturePath);
        }
        else if (texture.type == "diffuseTransmissionColorMap")
        {
            resolvedMaterial.setDiffuseTransmissionColorTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setDiffuseTransmissionColorMap(texturePath);
        }
        else if (texture.type == "specularGlossinessMap")
        {
            resolvedMaterial.setSpecularGlossinessTextureId(texture.id);
            if (!texturePath.isEmpty()) resolvedMaterial.setSpecularGlossinessMap(texturePath);
        }
    }

    // Ensure ADS values are recalculated after copy and texture assignment
    // (copy assignment operator doesn't call updateConsistency)
    resolvedMaterial.updateConsistency();

    auto* mesh = new SceneMesh(shader,
        meshData.name,
        meshData.vertices,
        meshData.indices,
        textures,
        resolvedMaterial,
        !meshData.morphTargets.isEmpty(),
        meshData.primitiveMode);
    mesh->setHasNegativeScale(meshData.hasNegativeScale);
    mesh->setSceneIndex(meshData.sceneIndex);
    mesh->setOriginalMaterialIndex(meshData.originalMaterialIndex);
    mesh->setSourceFile(meshData.sourceFile);
    mesh->setSourceNodeName(meshData.sourceNodeName);
    mesh->setSkinJoints(meshData.skinJoints);
    mesh->setMorphTargets(meshData.morphTargets, meshData.defaultMorphWeights);
    if (meshData.preserveNodeTransform)
        mesh->setSceneRenderTransformFast(meshData.nodeWorldTransform);
    if (!meshData.precomputedOccEdges.empty())
        mesh->setPrecomputedOccEdges(meshData.precomputedOccEdges,
                                     meshData.precomputedOccEdgeBoundaries);
    if (!meshData.variantMappings.isEmpty())
    {
        mesh->setVariantMappings(meshData.variantMappings);
        mesh->setAllVariantMaterials(meshData.allVariantMaterials);
    }

    GLMaterial runtimeResolved = GLWidget::resolveMaterialTextures(gl, mesh->getMaterial());
    mesh->setMaterial(runtimeResolved);
    mesh->setTextureMaps(runtimeResolved);
    mesh->invertOpacityADSMap(runtimeResolved.isOpacityMapInverted());
    mesh->invertOpacityPBRMap(runtimeResolved.isOpacityMapInverted());

    return mesh;
}
