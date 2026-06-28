#include "AnimationRuntimeController.h"

#include <assimp/matrix4x4.h>
#include <assimp/quaternion.h>
#include <assimp/vector3.h>

void AnimationRuntimeController::resetPlayback()
{
    _activeAnimationFile.clear();
    _activeAnimationClip = -1;
    _animationCurrentTimeSeconds = 0.0;
    _animationPlaying = false;
}

void AnimationRuntimeController::setActiveGltfCamera(const QString& file, int index)
{
    _activeGltfCameraFile  = file;
    _activeGltfCameraIndex = index;
}

void AnimationRuntimeController::setAnimatedLightTransform(const QString& sourceFile,
                                                            const std::vector<GPULight>& lights)
{
    _animatedLightTransformSourceFile = sourceFile;
    _animatedParsedLights = lights;
}

void AnimationRuntimeController::clearAnimatedLightTransform(const QString& sourceFile)
{
    if (_animatedLightTransformSourceFile != sourceFile)
        return;
    _animatedLightTransformSourceFile.clear();
    _animatedParsedLights.clear();
}

void AnimationRuntimeController::setAnimatedLightVisibility(const QString& sourceFile,
                                                             const QVector<bool>& mask)
{
    _animatedLightVisibilitySourceFile = sourceFile;
    _animatedLightVisibilityMask = mask;
}

void AnimationRuntimeController::clearAnimatedLightVisibility(const QString& sourceFile)
{
    if (_animatedLightVisibilitySourceFile != sourceFile)
        return;
    _animatedLightVisibilitySourceFile.clear();
    _animatedLightVisibilityMask.clear();
}

void AnimationRuntimeController::setAnimatedMeshVisibility(const QString& sourceFile,
                                                            const QSet<QUuid>& hiddenUuids)
{
    _animatedMeshVisibilitySourceFile = sourceFile;
    _animatedHiddenMeshUuids = hiddenUuids;
}

void AnimationRuntimeController::clearAnimatedMeshVisibility(const QString& sourceFile)
{
    if (_animatedMeshVisibilitySourceFile != sourceFile)
        return;
    _animatedMeshVisibilitySourceFile.clear();
    _animatedHiddenMeshUuids.clear();
}

void AnimationRuntimeController::clearAllAnimatedState()
{
    _animatedLightTransformSourceFile.clear();
    _animatedParsedLights.clear();
    _animatedLightVisibilitySourceFile.clear();
    _animatedLightVisibilityMask.clear();
    _animatedMeshVisibilitySourceFile.clear();
    _animatedHiddenMeshUuids.clear();
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

QMatrix4x4 AnimationRuntimeController::aiToQMatrix(const aiMatrix4x4& m)
{
    QMatrix4x4 out;
    out.setRow(0, QVector4D(m.a1, m.a2, m.a3, m.a4));
    out.setRow(1, QVector4D(m.b1, m.b2, m.b3, m.b4));
    out.setRow(2, QVector4D(m.c1, m.c2, m.c3, m.c4));
    out.setRow(3, QVector4D(m.d1, m.d2, m.d3, m.d4));
    return out;
}

AnimationRuntimeController::RuntimeNodeTransform
AnimationRuntimeController::decomposeNodeTransform(const aiMatrix4x4& matrix)
{
    aiVector3D   scaling;
    aiQuaternion rotation;
    aiVector3D   position;
    matrix.Decompose(scaling, rotation, position);

    RuntimeNodeTransform result;
    result.translation = QVector3D(position.x, position.y, position.z);
    result.rotation    = QQuaternion(rotation.w, rotation.x, rotation.y, rotation.z).normalized();
    result.scale       = QVector3D(scaling.x, scaling.y, scaling.z);
    return result;
}

QMatrix4x4 AnimationRuntimeController::composeNodeTransform(const RuntimeNodeTransform& tr)
{
    QMatrix4x4 m;
    m.translate(tr.translation);
    m.rotate(tr.rotation);
    m.scale(tr.scale);
    return m;
}

QUuid AnimationRuntimeController::resolveRuntimeNodeUuid(const RuntimeAnimationFileState& runtime,
                                                           int targetNodeIndex,
                                                           const QString& fallbackNodeName)
{
    if (targetNodeIndex >= 0 && runtime.nodeUuidByIndex.contains(targetNodeIndex))
        return runtime.nodeUuidByIndex.value(targetNodeIndex);

    if (!fallbackNodeName.isEmpty())
        return runtime.nodeUuidByName.value(fallbackNodeName, QUuid());

    return QUuid();
}

void AnimationRuntimeController::applyTexturePointerValue(GLMaterial& material,
    GltfAnimationTextureTarget textureTarget,
    GltfAnimationPointerProperty property,
    const QVector2D& vec2Value,
    float scalarValue)
{
    auto applyVec2 = [&](auto setOffset, auto setScale) {
        switch (property)
        {
        case GltfAnimationPointerProperty::Offset: (material.*setOffset)(vec2Value); break;
        case GltfAnimationPointerProperty::Scale:  (material.*setScale)(vec2Value);  break;
        default: break;
        }
    };
    auto applyRotation = [&](auto setRotation) {
        if (property == GltfAnimationPointerProperty::Rotation)
            (material.*setRotation)(scalarValue);
    };

    switch (textureTarget)
    {
    case GltfAnimationTextureTarget::Albedo:
        applyVec2(&GLMaterial::setAlbedoTexOffset, &GLMaterial::setAlbedoTexScale);
        applyRotation(&GLMaterial::setAlbedoTexRotation); break;
    case GltfAnimationTextureTarget::Metallic:
        applyVec2(&GLMaterial::setMetallicTexOffset, &GLMaterial::setMetallicTexScale);
        applyRotation(&GLMaterial::setMetallicTexRotation); break;
    case GltfAnimationTextureTarget::Roughness:
        applyVec2(&GLMaterial::setRoughnessTexOffset, &GLMaterial::setRoughnessTexScale);
        applyRotation(&GLMaterial::setRoughnessTexRotation); break;
    case GltfAnimationTextureTarget::MetallicRoughness:
        applyVec2(&GLMaterial::setMetallicTexOffset, &GLMaterial::setMetallicTexScale);
        applyRotation(&GLMaterial::setMetallicTexRotation);
        applyVec2(&GLMaterial::setRoughnessTexOffset, &GLMaterial::setRoughnessTexScale);
        applyRotation(&GLMaterial::setRoughnessTexRotation); break;
    case GltfAnimationTextureTarget::Normal:
        applyVec2(&GLMaterial::setNormalTexOffset, &GLMaterial::setNormalTexScale);
        applyRotation(&GLMaterial::setNormalTexRotation); break;
    case GltfAnimationTextureTarget::Occlusion:
        applyVec2(&GLMaterial::setOcclusionTexOffset, &GLMaterial::setOcclusionTexScale);
        applyRotation(&GLMaterial::setOcclusionTexRotation); break;
    case GltfAnimationTextureTarget::Emissive:
        applyVec2(&GLMaterial::setEmissiveTexOffset, &GLMaterial::setEmissiveTexScale);
        applyRotation(&GLMaterial::setEmissiveTexRotation); break;
    case GltfAnimationTextureTarget::Transmission:
        applyVec2(&GLMaterial::setTransmissionTexOffset, &GLMaterial::setTransmissionTexScale);
        applyRotation(&GLMaterial::setTransmissionTexRotation); break;
    case GltfAnimationTextureTarget::Thickness:
        applyVec2(&GLMaterial::setThicknessTexOffset, &GLMaterial::setThicknessTexScale);
        applyRotation(&GLMaterial::setThicknessTexRotation); break;
    case GltfAnimationTextureTarget::IOR:
        applyVec2(&GLMaterial::setIORTexOffset, &GLMaterial::setIORTexScale);
        applyRotation(&GLMaterial::setIORTexRotation); break;
    case GltfAnimationTextureTarget::SheenColor:
        applyVec2(&GLMaterial::setSheenColorTexOffset, &GLMaterial::setSheenColorTexScale);
        applyRotation(&GLMaterial::setSheenColorTexRotation); break;
    case GltfAnimationTextureTarget::SheenRoughness:
        applyVec2(&GLMaterial::setSheenRoughnessTexOffset, &GLMaterial::setSheenRoughnessTexScale);
        applyRotation(&GLMaterial::setSheenRoughnessTexRotation); break;
    case GltfAnimationTextureTarget::Clearcoat:
        applyVec2(&GLMaterial::setClearcoatColorTexOffset, &GLMaterial::setClearcoatColorTexScale);
        applyRotation(&GLMaterial::setClearcoatColorTexRotation); break;
    case GltfAnimationTextureTarget::ClearcoatRoughness:
        applyVec2(&GLMaterial::setClearcoatRoughnessTexOffset, &GLMaterial::setClearcoatRoughnessTexScale);
        applyRotation(&GLMaterial::setClearcoatRoughnessTexRotation); break;
    case GltfAnimationTextureTarget::ClearcoatNormal:
        applyVec2(&GLMaterial::setClearcoatNormalTexOffset, &GLMaterial::setClearcoatNormalTexScale);
        applyRotation(&GLMaterial::setClearcoatNormalTexRotation); break;
    case GltfAnimationTextureTarget::Iridescence:
        applyVec2(&GLMaterial::setIridescenceTexOffset, &GLMaterial::setIridescenceTexScale);
        applyRotation(&GLMaterial::setIridescenceTexRotation); break;
    case GltfAnimationTextureTarget::IridescenceThickness:
        applyVec2(&GLMaterial::setIridescenceThicknessTexOffset, &GLMaterial::setIridescenceThicknessTexScale);
        applyRotation(&GLMaterial::setIridescenceThicknessTexRotation); break;
    case GltfAnimationTextureTarget::SpecularFactor:
        applyVec2(&GLMaterial::setSpecularFactorTexOffset, &GLMaterial::setSpecularFactorTexScale);
        applyRotation(&GLMaterial::setSpecularFactorTexRotation); break;
    case GltfAnimationTextureTarget::SpecularColor:
        applyVec2(&GLMaterial::setSpecularColorTexOffset, &GLMaterial::setSpecularColorTexScale);
        applyRotation(&GLMaterial::setSpecularColorTexRotation); break;
    case GltfAnimationTextureTarget::Anisotropy:
        applyVec2(&GLMaterial::setAnisotropyTexOffset, &GLMaterial::setAnisotropyTexScale);
        applyRotation(&GLMaterial::setAnisotropyTexRotation); break;
    case GltfAnimationTextureTarget::DiffuseTransmission:
        applyVec2(&GLMaterial::setDiffuseTransmissionTexOffset, &GLMaterial::setDiffuseTransmissionTexScale);
        applyRotation(&GLMaterial::setDiffuseTransmissionTexRotation); break;
    case GltfAnimationTextureTarget::DiffuseTransmissionColor:
        applyVec2(&GLMaterial::setDiffuseTransmissionColorTexOffset, &GLMaterial::setDiffuseTransmissionColorTexScale);
        applyRotation(&GLMaterial::setDiffuseTransmissionColorTexRotation); break;
    case GltfAnimationTextureTarget::Diffuse:
        applyVec2(&GLMaterial::setDiffuseTexOffset, &GLMaterial::setDiffuseTexScale);
        applyRotation(&GLMaterial::setDiffuseTexRotation); break;
    case GltfAnimationTextureTarget::SpecularGlossiness:
        applyVec2(&GLMaterial::setSpecularGlossinessTexOffset, &GLMaterial::setSpecularGlossinessTexScale);
        applyRotation(&GLMaterial::setSpecularGlossinessTexRotation); break;
    default: break;
    }
}

void AnimationRuntimeController::applyMaterialFactorPointerValue(GLMaterial& material,
    GltfAnimationPointerProperty property,
    const QVector4D& vec4Value)
{
    if (property != GltfAnimationPointerProperty::BaseColorFactor)
        return;
    const QVector3D color(vec4Value.x(), vec4Value.y(), vec4Value.z());
    material.setAlbedoColor(color);
    material.setDiffuse(color);
    material.setOpacity(vec4Value.w());
}
