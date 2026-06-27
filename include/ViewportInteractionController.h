#pragma once

#include "BoundingBox.h"
#include "BoundingSphere.h"
#include "GLCamera.h"
#include "RenderEnums.h"
#include "TransformCommand.h"

#include <QMap>
#include <QMatrix4x4>
#include <QPoint>
#include <QQuaternion>
#include <QRect>
#include <QSet>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

// ---------------------------------------------------------------------------
// ViewportInteractionController
//
// Groups all viewport navigation, camera, gizmo drag, and rubber-band
// interaction state that was previously scattered through GLWidget's private
// section.  GLWidget embeds one instance and aliases every field via reference
// members so all existing call sites in GLWidget.cpp compile unchanged.
//
// Introduced in Phase 11 of the mesh/render/runtime separation refactor.
//
// Qt-owned pointers (_primaryCamera, _orthoViewsCamera, navigation timers,
// _rubberBand, _selectRect, _inertiaTimer) are NOT stored here — they remain
// as direct GLWidget members because Qt object-tree ownership requires the
// parent to be a QObject.
// ---------------------------------------------------------------------------
class ViewportInteractionController
{
public:
    // ---- Saved system-camera state -----------------------------------------
    // Captured when a glTF camera is first activated; restored on deactivation.
    bool                     _systemCameraStateSaved  = false;
    QVector3D                _savedCameraPos;
    QVector3D                _savedCameraDir;
    QVector3D                _savedCameraUp;
    QVector3D                _savedCameraRight;
    GLCamera::ProjectionType _savedProjectionType     = GLCamera::ProjectionType::PERSPECTIVE;
    float                    _savedCameraFOV          = 45.0f;
    float                    _savedCameraViewRange    = 200.0f;

    // ---- View / navigation state -------------------------------------------
    QVector3D   _currentTranslation;
    QQuaternion _currentRotation;
    QQuaternion _customViewTargetRotation;
    float       _slerpStep                   = 0.0f;
    float       _slerpFrac                   = 0.0f;
    float       _currentViewRange            = 0.0f;
    float       _scaleFrac                   = 0.0f;
    float       _viewRange                   = 0.0f;
    float       _viewBoundingSphereDia       = 0.0f;
    float       _FOV                         = 45.0f;
    bool        _autoFitViewOnUpdate         = false;
    float       _zoomInLimit                 = 1.0f;

    // ---- View mode ---------------------------------------------------------
    ViewMode     _viewMode                   = ViewMode::NONE;
    ViewProjection _projection               = ViewProjection::PERSPECTIVE;
    GLCamera::ProjectionType _previousProjection = GLCamera::ProjectionType::PERSPECTIVE;
    bool         _multiViewActive            = false;
    int          _viewCubeHoveredRegionId    = -1;
    bool         _customViewAnimationActive  = false;
    bool         _cameraUpAxisZUp            = true;
    bool         _showViewCubeOverride       = true;
    CornerAxisPosition _cornerAxisPosition   = CornerAxisPosition::TOP_RIGHT;
    bool         _showAxis                   = true;
    bool         _userShowAxisOverride       = false;
    bool         _userShowCornerAxisOverride = false;

    // ---- Matrices ----------------------------------------------------------
    QMatrix4x4 _projectionMatrix;
    QMatrix4x4 _viewMatrix;
    QMatrix4x4 _modelMatrix;
    QMatrix4x4 _modelViewMatrix;
    QMatrix4x4 _viewportMatrix;
    QVector4D  _frustumPlanes[6];

    // ---- Mouse interaction state -------------------------------------------
    bool     _windowZoomActive           = false;
    bool     _viewZooming                = false;
    bool     _viewPanning                = false;
    bool     _viewRotating               = false;
    QPoint   _leftButtonPoint;
    QPoint   _rightButtonPoint;
    QPoint   _middleButtonPoint;
    bool     _navigationViewportLocked   = false;
    QRect    _navigationLockedViewport;
    QRect    _navigationLockedClientRect;
    QPoint   _lastPanPoint;
    int      _lastZoomDirection          = 0;
    float    _lastZoomStep               = 1.05f;
    QVector3D _lastZoomPanVector;
    QVector3D _inertiaZoomPanVelocity;
    QVector2D _inertiaPanVelocity;
    float    _inertiaZoomVelocity        = 0.0f;
    QVector2D _inertiaRotateVelocity;
    float    _inertiaDamping             = 0.8f;
    bool     _mouseMovedSincePress       = false;
    qint64   _lastMouseMoveTime          = 0;
    QPoint   _lastMousePos;
    qint64   _lastMouseTime              = 0;

    // ---- Rubber-band selection state ---------------------------------------
    QVector3D _rubberBandPan;
    float     _rubberBandZoomRatio       = 0.0f;
    float     _rubberBandRadius          = 0.0f;
    QVector3D _rubberBandCenter;
    bool      _shiftDragActive           = false;
    QPoint    _sweepStartPoint;

    // ---- Transform-gizmo drag state ----------------------------------------
    bool      _transformGizmoRequested            = false;
    bool      _transformGizmoTranslating          = false;
    bool      _transformGizmoScaling              = false;
    bool      _transformGizmoUniformScaling       = false;
    bool      _transformGizmoRotating             = false;
    QPoint    _transformGizmoDragStartPixel;
    QVector3D _transformGizmoDragAxis;
    QVector3D _transformGizmoStartPivot;
    float     _transformGizmoDragScale            = 1.0f;
    QMap<int, TransformState>  _transformGizmoStartStates;
    QMap<int, QVector3D>       _transformGizmoStartCenters;
    QMap<int, QMatrix4x4>      _transformGizmoStartMatrices;
    QVector3D _transformGizmoCurrentTranslationDelta;
    QVector3D _transformGizmoCurrentScaleDelta    = QVector3D(1.0f, 1.0f, 1.0f);
    QVector3D _transformGizmoRotationPlaneNormal  = QVector3D(0.0f, 0.0f, 1.0f);
    QVector3D _transformGizmoRotationStartVector  = QVector3D(1.0f, 0.0f, 0.0f);
    QVector3D _transformGizmoCurrentRotationDelta;
    bool      _transformGizmoLoggedTranslationUpdate = false;

    // ---- Scene bounding sphere / box --------------------------------------
    BoundingSphere _boundingSphere;
    BoundingSphere _selectionBoundingSphere;
    BoundingBox    _boundingBox;
    float          _visibleHighestZ = 0.0f;
    float          _visibleLowestZ  = 0.0f;
};
