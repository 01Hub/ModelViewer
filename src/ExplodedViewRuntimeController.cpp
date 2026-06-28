#include "ExplodedViewRuntimeController.h"

void ExplodedViewRuntimeController::setHintsCache(
    const QSet<QUuid>& assemblyUuids,
    const QUuid& anchorUuid,
    const AssemblyRelationGraph::AutoPlacementHints& hints)
{
    _cachedAutoHints          = hints;
    _cachedHintsAssemblyUuids = assemblyUuids;
    _cachedHintsAnchorUuid    = anchorUuid;
    _cachedHintsValid         = true;
}

void ExplodedViewRuntimeController::resetSessionTransforms()
{
    _explodedViewManualPlacementSessionUuids.clear();
    _explodedViewManualSessionStartStates.clear();
    _explodedViewManualSessionStartMatrices.clear();
    _explodedViewManualSessionStartPivot         = QVector3D(0.0f, 0.0f, 0.0f);
    _explodedViewManualSessionTranslationDelta   = QVector3D(0.0f, 0.0f, 0.0f);
    _explodedViewManualSessionRotationQuat       = QQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
    _explodedViewManualSessionRotationEuler      = QVector3D(0.0f, 0.0f, 0.0f);
    _explodedViewManualDragStartTranslationDelta = QVector3D(0.0f, 0.0f, 0.0f);
    _explodedViewManualDragStartRotationQuat     = QQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
    _explodedViewManualDragStartRotationEuler    = QVector3D(0.0f, 0.0f, 0.0f);
}

void ExplodedViewRuntimeController::clearManualPlacement()
{
    _explodedViewManualPlacementActive = false;
    resetSessionTransforms();
    _explodedViewManualHiddenStates.clear();
    _explodedViewManualPlacementSuppressed = false;
    _explodedViewManualOriginalStates.clear();
}
