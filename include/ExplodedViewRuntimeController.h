#pragma once

#include "AssemblyRelationGraph.h"
#include "TransformCommand.h"

#include <QMap>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QSet>
#include <QUuid>
#include <QVector3D>

class ExplodedViewManager;

// ---------------------------------------------------------------------------
// ExplodedViewRuntimeController
//
// Groups all exploded-view runtime state that was previously scattered
// through GLWidget's private section.  GLWidget embeds one instance and
// aliases every field via reference members so all existing call sites in
// GLWidget.cpp compile unchanged.
//
// Introduced in Phase 9 of the mesh/render/runtime separation refactor.
//
// Note: _explodedViewManager is created by GLWidget with itself as Qt parent
// (required for ownership).  ExplodedViewRuntimeController stores the pointer
// only — ownership stays with GLWidget.
// ---------------------------------------------------------------------------
class ExplodedViewRuntimeController
{
public:
    // ---- Manager -----------------------------------------------------------
    // Raw pointer; Qt-owned by GLWidget.
    ExplodedViewManager* _explodedViewManager = nullptr;

    // ---- Assembly-aware auto-placement hints cache -------------------------
    // Invalidated whenever assemblyUuids or anchorUuid change so the O(n²)
    // graph build does not run on every slider tick.
    QSet<QUuid>                               _cachedHintsAssemblyUuids;
    QUuid                                     _cachedHintsAnchorUuid;
    AssemblyRelationGraph::AutoPlacementHints _cachedAutoHints;
    bool                                      _cachedHintsValid = false;

    // ---- Manual placement session state ------------------------------------
    bool                        _explodedViewManualPlacementActive      = false;
    QMap<QUuid, TransformState> _explodedViewManualOriginalStates;
    QMap<QUuid, TransformState> _explodedViewManualHiddenStates;
    bool                        _explodedViewManualPlacementSuppressed   = false;
    QSet<QUuid>                 _explodedViewManualPlacementSessionUuids;
    QMap<QUuid, TransformState> _explodedViewManualSessionStartStates;
    QMap<QUuid, QMatrix4x4>    _explodedViewManualSessionStartMatrices;
    QVector3D                   _explodedViewManualSessionStartPivot         = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D                   _explodedViewManualSessionTranslationDelta   = QVector3D(0.0f, 0.0f, 0.0f);
    QQuaternion                 _explodedViewManualSessionRotationQuat       = QQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
    QVector3D                   _explodedViewManualSessionRotationEuler      = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D                   _explodedViewManualDragStartTranslationDelta = QVector3D(0.0f, 0.0f, 0.0f);
    QQuaternion                 _explodedViewManualDragStartRotationQuat     = QQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
    QVector3D                   _explodedViewManualDragStartRotationEuler    = QVector3D(0.0f, 0.0f, 0.0f);
};
