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
// Groups all animation playback state that was previously scattered through
// GLWidget's private section.  GLWidget embeds one instance and aliases every
// field via reference members so all existing call sites compile unchanged.
//
// Introduced in Phase 8 of the mesh/render/runtime separation refactor.
//
// Note: _animationTimer (QTimer*) is created by GLWidget with itself as
// parent (required for Qt's object-tree ownership) but is stored here for
// field grouping.  GLWidget owns the object; this class merely holds the
// pointer.
// ---------------------------------------------------------------------------
class AnimationRuntimeController
{
public:
    // ---- Stable per-node runtime transform ---------------------------------
    // Replaces raw aiMatrix4x4 node names with UUID-keyed runtime state.
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

        // Stable runtime state keyed by SceneNode identity rather than imported
        // node names.  This is the authoritative glTF runtime transform contract.
        QHash<QUuid, RuntimeNodeTransform> defaultNodeTransformsByUuid;
        QHash<QUuid, QVector<float>>       defaultNodeMorphWeightsByUuid;
        QHash<QUuid, GLMaterial>           defaultMeshMaterials;
        QMultiHash<int, QUuid>             meshUuidsByMaterialIndex;

        // Transitional lookup tables from imported glTF animation/light/camera
        // references into stable SceneNode UUIDs for the currently loaded file.
        QHash<QString, QUuid> nodeUuidByName;
        QHash<int,    QUuid>  nodeUuidByIndex;
        QHash<QUuid,  int>    nodeIndexByUuid;
    };

    // ---- Playback state ----------------------------------------------------
    QHash<QString, RuntimeAnimationFileState> _runtimeAnimationsByFile;
    QString       _activeAnimationFile;
    int           _activeAnimationClip         = -1;
    double        _animationCurrentTimeSeconds = 0.0;
    bool          _animationPlaying            = false;
    bool          _animationLooping            = true;
    double        _animationPlaybackSpeed      = 1.0;
    QTimer*       _animationTimer              = nullptr;   // owned by GLWidget
    QElapsedTimer _animationElapsed;

    // ---- glTF camera activation --------------------------------------------
    QString _activeGltfCameraFile;
    int     _activeGltfCameraIndex = -1;

    // ---- Light animation ---------------------------------------------------
    QString               _animatedLightTransformSourceFile;
    std::vector<GPULight> _animatedParsedLights;
    QString               _animatedLightVisibilitySourceFile;
    QVector<bool>         _animatedLightVisibilityMask;

    // ---- Mesh visibility animation -----------------------------------------
    QString     _animatedMeshVisibilitySourceFile;
    QSet<QUuid> _animatedHiddenMeshUuids;
};
