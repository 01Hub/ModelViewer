#include "AnimationRuntimeController.h"
#include "SceneGraph.h"

#include <assimp/matrix4x4.h>
#include <assimp/quaternion.h>
#include <assimp/vector3.h>

namespace
{
float uniformScaleOf(const QMatrix4x4& m)
{
    const float sx = QVector3D(m(0, 0), m(1, 0), m(2, 0)).length();
    const float sy = QVector3D(m(0, 1), m(1, 1), m(2, 1)).length();
    const float sz = QVector3D(m(0, 2), m(1, 2), m(2, 2)).length();
    return (sx + sy + sz) / 3.0f;
}
}

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

void AnimationRuntimeController::rebuildCurrentRepositionedLights(const QHash<QString, QMatrix4x4>& userTransforms)
{
    if (_originalParsedLights.empty())
    {
        _currentRepositionedLights.clear();
        return;
    }

    const std::vector<GPULight>& sourceLights =
        (!_animatedLightTransformSourceFile.isEmpty() &&
         _animatedParsedLights.size() == _originalParsedLights.size())
        ? _animatedParsedLights
        : _originalParsedLights;
    _currentRepositionedLights = sourceLights;

    if (_lightFileIndexMap.size() == static_cast<int>(_currentRepositionedLights.size()))
    {
        for (int i = 0; i < static_cast<int>(_currentRepositionedLights.size()); ++i)
        {
            const QString& file = _lightFileIndexMap[i].file;
            const QMatrix4x4 transform = userTransforms.value(file, QMatrix4x4());
            if (transform.isIdentity())
                continue;

            auto& light = _currentRepositionedLights[i];

            const QVector3D pos = transform.map(
                QVector3D(light.position.x, light.position.y, light.position.z));
            light.position = glm::vec3(pos.x(), pos.y(), pos.z());

            const QVector3D dir = transform.mapVector(
                QVector3D(light.direction.x, light.direction.y, light.direction.z)).normalized();
            light.direction = glm::vec3(dir.x(), dir.y(), dir.z());

            const float s = uniformScaleOf(transform);
            if (light.range > 0.0f)
                light.range *= s;
            if (light.type != static_cast<int>(LightType::Directional))
                light.intensity *= (s * s);
        }
    }

    if (!_animatedLightVisibilitySourceFile.isEmpty() &&
        _animatedLightVisibilityMask.size() == static_cast<qsizetype>(_currentRepositionedLights.size()))
    {
        std::vector<GPULight> visibleLights;
        visibleLights.reserve(_currentRepositionedLights.size());
        for (int lightIndex = 0; lightIndex < static_cast<int>(_currentRepositionedLights.size()); ++lightIndex)
        {
            if (_animatedLightVisibilityMask[lightIndex])
                visibleLights.push_back(_currentRepositionedLights[lightIndex]);
        }
        _currentRepositionedLights = std::move(visibleLights);
    }
}

std::vector<GPULight> AnimationRuntimeController::rebuildAndBuildUploadLights(
    const std::function<QMatrix4x4(const QString&)>& userTransformResolver,
    const std::function<bool(const LightOrigin&)>& isLightEnabled)
{
    QHash<QString, QMatrix4x4> transformCache;
    if (userTransformResolver)
    {
        for (const LightOrigin& origin : _lightFileIndexMap)
        {
            if (!transformCache.contains(origin.file))
                transformCache.insert(origin.file, userTransformResolver(origin.file));
        }
    }

    rebuildCurrentRepositionedLights(transformCache);
    return isLightEnabled ? buildUploadLights(isLightEnabled) : buildUploadLights();
}

std::vector<GPULight> AnimationRuntimeController::buildUploadLightsWithSceneGraph(
    const std::function<QMatrix4x4(const QString&)>& userTransformResolver,
    const SceneGraph* sg)
{
    if (!sg)
        return rebuildAndBuildUploadLights(userTransformResolver);

    return rebuildAndBuildUploadLights(
        userTransformResolver,
        [sg](const LightOrigin& origin) {
            const GltfLightData& ld = sg->lightDataForFile(origin.file);
            return origin.index < static_cast<int>(ld.lights.size()) &&
                   ld.lights[origin.index].enabled;
        });
}

std::vector<GPULight> AnimationRuntimeController::buildUploadLights() const
{
    return _currentRepositionedLights;
}

std::vector<GPULight> AnimationRuntimeController::buildUploadLights(
    const std::function<bool(const LightOrigin&)>& isLightEnabled) const
{
    if (!isLightEnabled ||
        _lightFileIndexMap.isEmpty() ||
        _lightFileIndexMap.size() != static_cast<int>(_currentRepositionedLights.size()))
    {
        return _currentRepositionedLights;
    }

    std::vector<GPULight> uploadLights;
    uploadLights.reserve(_currentRepositionedLights.size());
    for (int i = 0; i < static_cast<int>(_currentRepositionedLights.size()); ++i)
    {
        if (isLightEnabled(_lightFileIndexMap[i]))
            uploadLights.push_back(_currentRepositionedLights[i]);
    }
    return uploadLights;
}

// ---------------------------------------------------------------------------
// Light population
// ---------------------------------------------------------------------------

void AnimationRuntimeController::setParsedLightsFromSingleFile(const GltfLightData& lightData)
{
    _originalParsedLights.clear();
    _lightFileIndexMap.clear();
    _currentRepositionedLights.clear();
    for (const GltfLightEntry& entry : lightData.lights)
        _originalParsedLights.push_back(entry.gpuLight);
    // lightFileIndexMap intentionally not built here — onSceneLightDataChanged
    // (→ rebuildParsedLightsFromSceneGraph) is the authoritative rebuilder once
    // the file is registered in SceneGraph.
}

void AnimationRuntimeController::rebuildParsedLightsFromSceneGraph(const SceneGraph* sg)
{
    _originalParsedLights.clear();
    _lightFileIndexMap.clear();
    if (!sg)
        return;
    const QStringList files = sg->filesWithLights();
    for (const QString& file : files)
    {
        const GltfLightData& ld = sg->lightDataForFile(file);
        for (int i = 0; i < static_cast<int>(ld.lights.size()); ++i)
        {
            _originalParsedLights.push_back(ld.lights[i].gpuLight);
            _lightFileIndexMap.append({file, i});
        }
    }
}

void AnimationRuntimeController::clearParsedLights()
{
    _originalParsedLights.clear();
    _lightFileIndexMap.clear();
    _currentRepositionedLights.clear();
}

std::vector<GPULight> AnimationRuntimeController::buildGizmoLights(const SceneGraph* sg) const
{
    if (_currentRepositionedLights.empty())
        return {};

    const bool hasEnabledMap = sg
        && !_lightFileIndexMap.isEmpty()
        && static_cast<int>(_currentRepositionedLights.size()) == _lightFileIndexMap.size();

    if (!hasEnabledMap)
        return _currentRepositionedLights;

    std::vector<GPULight> result;
    result.reserve(_currentRepositionedLights.size());
    for (int i = 0; i < static_cast<int>(_currentRepositionedLights.size()); ++i)
    {
        const LightOrigin& origin = _lightFileIndexMap[i];
        const GltfLightData& ld = sg->lightDataForFile(origin.file);
        if (origin.index < static_cast<int>(ld.lights.size()) && ld.lights[origin.index].enabled)
            result.push_back(_currentRepositionedLights[i]);
    }
    return result;
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
