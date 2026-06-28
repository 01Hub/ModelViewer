#pragma once

#include "GltfAnimationData.h"
#include "GLLights.h"
#include "GLMaterial.h"

#include <QElapsedTimer>
#include <QMultiHash>
#include <QSet>
#include <QString>
#include <QQuaternion>
#include <QTimer>
#include <QUuid>
#include <QVector>
#include <QVector3D>
#include <vector>

// ---------------------------------------------------------------------------
// AnimationRuntimeController
//
// Owns all animation playback state previously scattered through GLWidget.
// GLWidget embeds one instance and accesses state through this API.
//
// Introduced in Phase 8 of the mesh/render/runtime separation refactor.
// De-aliased in the controller ownership cleanup pass.
//
// Note: _animationTimer (QTimer*) is created by GLWidget with itself as
// parent (required for Qt's object-tree ownership) and registered here via
// setAnimationTimer().
// ---------------------------------------------------------------------------
class AnimationRuntimeController
{
public:
    // ---- Stable per-node runtime transform ---------------------------------
    struct RuntimeNodeTransform
    {
        QVector3D   translation = QVector3D(0.0f, 0.0f, 0.0f);
        QQuaternion rotation;
        QVector3D   scale       = QVector3D(1.0f, 1.0f, 1.0f);
    };

    // ---- Per-file animation runtime state ----------------------------------
    struct RuntimeAnimationFileState
    {
        GltfAnimationData data;

        QHash<QUuid, RuntimeNodeTransform> defaultNodeTransformsByUuid;
        QHash<QUuid, QVector<float>>       defaultNodeMorphWeightsByUuid;
        QHash<QUuid, GLMaterial>           defaultMeshMaterials;
        QMultiHash<int, QUuid>             meshUuidsByMaterialIndex;

        QHash<QString, QUuid> nodeUuidByName;
        QHash<int,    QUuid>  nodeUuidByIndex;
        QHash<QUuid,  int>    nodeIndexByUuid;
    };

    // ---- Playback state ----------------------------------------------------
    QString activeAnimationFile() const              { return _activeAnimationFile; }
    void    setActiveAnimationFile(const QString& f) { _activeAnimationFile = f; }

    int  activeAnimationClip() const          { return _activeAnimationClip; }
    void setActiveAnimationClip(int clip)     { _activeAnimationClip = clip; }

    double animationCurrentTimeSeconds() const      { return _animationCurrentTimeSeconds; }
    void   setAnimationCurrentTimeSeconds(double t) { _animationCurrentTimeSeconds = t; }

    bool isPlaying() const            { return _animationPlaying; }
    void setPlaying(bool playing)     { _animationPlaying = playing; }

    bool isLooping() const            { return _animationLooping; }
    void setLooping(bool looping)     { _animationLooping = looping; }

    double playbackSpeed() const          { return _animationPlaybackSpeed; }
    void   setPlaybackSpeed(double speed) { _animationPlaybackSpeed = speed; }

    // Timer is created by GLWidget (needs GLWidget as parent for Qt ownership)
    // and registered here for storage only.
    QTimer* animationTimer() const       { return _animationTimer; }
    void    setAnimationTimer(QTimer* t) { _animationTimer = t; }

    QElapsedTimer& animationElapsed()    { return _animationElapsed; }

    void resetPlayback();

    // ---- Per-file animation data -------------------------------------------
    QHash<QString, RuntimeAnimationFileState>&       runtimeAnimationsByFile()       { return _runtimeAnimationsByFile; }
    const QHash<QString, RuntimeAnimationFileState>& runtimeAnimationsByFile() const { return _runtimeAnimationsByFile; }

    void removeAnimationFile(const QString& file) { _runtimeAnimationsByFile.remove(file); }
    void clearAllAnimationFiles()                 { _runtimeAnimationsByFile.clear(); }

    // ---- glTF camera activation --------------------------------------------
    QString activeGltfCameraFile() const  { return _activeGltfCameraFile; }
    int     activeGltfCameraIndex() const { return _activeGltfCameraIndex; }
    void    setActiveGltfCamera(const QString& file, int index);

    // ---- Light transform animation -----------------------------------------
    QString                      animatedLightTransformSourceFile() const { return _animatedLightTransformSourceFile; }
    const std::vector<GPULight>& animatedParsedLights() const             { return _animatedParsedLights; }

    void setAnimatedLightTransform(const QString& sourceFile, const std::vector<GPULight>& lights);
    void clearAnimatedLightTransform(const QString& sourceFile);

    // ---- Light visibility animation ----------------------------------------
    QString              animatedLightVisibilitySourceFile() const { return _animatedLightVisibilitySourceFile; }
    const QVector<bool>& animatedLightVisibilityMask() const       { return _animatedLightVisibilityMask; }

    void setAnimatedLightVisibility(const QString& sourceFile, const QVector<bool>& mask);
    void clearAnimatedLightVisibility(const QString& sourceFile);

    // ---- Mesh visibility animation -----------------------------------------
    QString            animatedMeshVisibilitySourceFile() const { return _animatedMeshVisibilitySourceFile; }
    const QSet<QUuid>& animatedHiddenMeshUuids() const          { return _animatedHiddenMeshUuids; }

    void setAnimatedMeshVisibility(const QString& sourceFile, const QSet<QUuid>& hiddenUuids);
    void clearAnimatedMeshVisibility(const QString& sourceFile);

    // Clears all animation-driven light and mesh visibility state (e.g. on scene reset).
    void clearAllAnimatedState();

private:
    // Playback
    QHash<QString, RuntimeAnimationFileState> _runtimeAnimationsByFile;
    QString       _activeAnimationFile;
    int           _activeAnimationClip         = -1;
    double        _animationCurrentTimeSeconds = 0.0;
    bool          _animationPlaying            = false;
    bool          _animationLooping            = true;
    double        _animationPlaybackSpeed      = 1.0;
    QTimer*       _animationTimer              = nullptr;
    QElapsedTimer _animationElapsed;

    // glTF camera
    QString _activeGltfCameraFile;
    int     _activeGltfCameraIndex = -1;

    // Light animation
    QString               _animatedLightTransformSourceFile;
    std::vector<GPULight> _animatedParsedLights;
    QString               _animatedLightVisibilitySourceFile;
    QVector<bool>         _animatedLightVisibilityMask;

    // Mesh visibility animation
    QString     _animatedMeshVisibilitySourceFile;
    QSet<QUuid> _animatedHiddenMeshUuids;
};
