#include "AnimationRuntimeController.h"

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
