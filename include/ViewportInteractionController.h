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
// section. A small public API now owns externally visible viewport settings
// and matrix access, while the bulk interaction state remains an internal
// aggregate for GLWidget during the incremental de-aliasing refactor.
//
// Introduced in Phase 11 of the mesh/render/runtime separation refactor.
// De-aliased in the controller ownership cleanup pass.
//
// Qt-owned pointers (_primaryCamera, _orthoViewsCamera, navigation timers,
// _rubberBand, _selectRect, _inertiaTimer) are NOT stored here — they remain
// as direct GLWidget members because Qt object-tree ownership requires the
// parent to be a QObject.
// ---------------------------------------------------------------------------

class ViewportInteractionController
{
public:
    // ---- Public-facing config / state accessors ----------------------------
    bool cameraUpAxisZUp() const                         { return _cameraUpAxisZUp; }
    void setCameraUpAxisZUp(bool zUp)                    { _cameraUpAxisZUp = zUp; }

    bool multiViewActive() const                         { return _multiViewActive; }
    void setMultiViewActive(bool active)                 { _multiViewActive = active; }

    bool userShowAxisOverride() const                    { return _userShowAxisOverride; }
    void setUserShowAxisOverride(bool show)              { _userShowAxisOverride = show; }

    bool userShowCornerAxisOverride() const              { return _userShowCornerAxisOverride; }
    void setUserShowCornerAxisOverride(bool show)        { _userShowCornerAxisOverride = show; }

    bool showViewCubeOverride() const                    { return _showViewCubeOverride; }
    void setShowViewCubeOverride(bool show)
    {
        _showViewCubeOverride = show;
        if (!show)
            _viewCubeHoveredRegionId = -1;
    }

    bool showAxis() const                                { return _showAxis; }
    void setShowAxis(bool show)                          { _showAxis = show; }

    float FOV() const                                    { return _FOV; }
    void setFOV(float fov)                               { _FOV = fov; }

    float viewRange() const                              { return _viewRange; }
    void setViewRange(float range)                       { _viewRange = range; }

    float currentViewRange() const                       { return _currentViewRange; }
    void setCurrentViewRange(float range)                { _currentViewRange = range; }

    float viewBoundingSphereDia() const                  { return _viewBoundingSphereDia; }
    void setViewBoundingSphereDia(float diameter)        { _viewBoundingSphereDia = diameter; }

    ViewProjection projection() const                    { return _projection; }
    void setProjection(ViewProjection projection)        { _projection = projection; }

    GLCamera::ProjectionType previousProjection() const  { return _previousProjection; }
    void setPreviousProjection(GLCamera::ProjectionType projection)
    {
        _previousProjection = projection;
    }

    ViewMode viewMode() const                            { return _viewMode; }
    void setViewMode(ViewMode mode)                      { _viewMode = mode; }

    const QVector3D& currentTranslation() const          { return _currentTranslation; }
    void setCurrentTranslation(const QVector3D& translation)
    {
        _currentTranslation = translation;
    }

    const QQuaternion& currentRotation() const           { return _currentRotation; }
    void setCurrentRotation(const QQuaternion& rotation) { _currentRotation = rotation; }

    const QQuaternion& customViewTargetRotation() const  { return _customViewTargetRotation; }
    void setCustomViewTargetRotation(const QQuaternion& rotation)
    {
        _customViewTargetRotation = rotation;
    }

    bool customViewAnimationActive() const               { return _customViewAnimationActive; }
    void setCustomViewAnimationActive(bool active)       { _customViewAnimationActive = active; }

    CornerAxisPosition cornerAxisPosition() const        { return _cornerAxisPosition; }
    void setCornerAxisPosition(CornerAxisPosition pos)
    {
        _cornerAxisPosition = normalizeCornerAxisPosition(pos);
    }

    const QMatrix4x4& modelMatrix() const                { return _modelMatrix; }
    void setModelMatrix(const QMatrix4x4& matrix)
    {
        _modelMatrix = matrix;
        recomputeModelViewMatrix();
    }

    const QMatrix4x4& viewMatrix() const                 { return _viewMatrix; }
    void setViewMatrix(const QMatrix4x4& matrix)
    {
        _viewMatrix = matrix;
        recomputeModelViewMatrix();
    }

    const QMatrix4x4& projectionMatrix() const           { return _projectionMatrix; }
    void setProjectionMatrix(const QMatrix4x4& matrix)   { _projectionMatrix = matrix; }

    const QMatrix4x4& modelViewMatrix() const            { return _modelViewMatrix; }
    const QMatrix4x4& viewportMatrix() const             { return _viewportMatrix; }

    void setViewportMatrix(float width, float height)
    {
        _viewportMatrix = QMatrix4x4(width / 2.0f, 0.0f, 0.0f, 0.0f,
                                     0.0f, height / 2.0f, 0.0f, 0.0f,
                                     0.0f, 0.0f, 1.0f, 0.0f,
                                     width / 2.0f, height / 2.0f, 0.0f, 1.0f);
    }

    void recomputeModelViewMatrix()
    {
        _modelViewMatrix = _viewMatrix * _modelMatrix;
    }

    void syncMatricesFromCamera(const GLCamera& camera)
    {
        _viewMatrix = camera.getViewMatrix();
        _projectionMatrix = camera.getProjectionMatrix();
        recomputeModelViewMatrix();
    }

    void syncPoseFromCamera(const GLCamera& camera)
    {
        _currentRotation = QQuaternion::fromRotationMatrix(
            camera.getViewMatrix().toGenericMatrix<3, 3>());
        _currentTranslation = camera.getPosition();
    }

    void syncRotationFromCamera(const GLCamera& camera)
    {
        _currentRotation = QQuaternion::fromRotationMatrix(
            camera.getViewMatrix().toGenericMatrix<3, 3>());
    }

    void syncTranslationFromCamera(const GLCamera& camera)
    {
        _currentTranslation = camera.getPosition();
    }

    void syncCurrentViewRange()
    {
        _currentViewRange = _viewRange;
    }

    void syncPoseAndRangeFromCamera(const GLCamera& camera)
    {
        syncPoseFromCamera(camera);
        syncCurrentViewRange();
    }

    void updateFrustumPlanes()
    {
        const QMatrix4x4 vp = _projectionMatrix * _viewMatrix;
        const QVector4D r0 = vp.row(0);
        const QVector4D r1 = vp.row(1);
        const QVector4D r2 = vp.row(2);
        const QVector4D r3 = vp.row(3);

        _frustumPlanes[0] = r3 + r0;
        _frustumPlanes[1] = r3 - r0;
        _frustumPlanes[2] = r3 + r1;
        _frustumPlanes[3] = r3 - r1;
        _frustumPlanes[4] = r3 + r2;
        _frustumPlanes[5] = r3 - r2;

        for (int i = 0; i < 6; ++i)
        {
            const float len = QVector3D(_frustumPlanes[i].x(),
                                        _frustumPlanes[i].y(),
                                        _frustumPlanes[i].z()).length();
            if (len > 1e-6f)
                _frustumPlanes[i] /= len;
        }
    }

    const QVector4D& frustumPlane(int index) const       { return _frustumPlanes[index]; }

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

    // ---- Static picking / viewport helpers ---------------------------------
    // Returns the CornerAxisPosition clamped to one of the four valid corners.
    static CornerAxisPosition normalizeCornerAxisPosition(CornerAxisPosition position);


    // Converts a pixel coordinate to a world-space ray (orig, dir).
    // Returns false if the viewport is degenerate or the ray is zero.
    static bool convertPixelToRay(const QPoint& pixel, const QRect& viewport,
                                   int widgetHeight,
                                   const QMatrix4x4& view,
                                   const QMatrix4x4& projection,
                                   QVector3D& orig, QVector3D& dir);

    // Intersects a ray with a plane; populates outPoint and returns true on hit.
    static bool intersectRayPlane(const QVector3D& rayOrigin, const QVector3D& rayDir,
                                   const QVector3D& planePoint,
                                   const QVector3D& planeNormal,
                                   QVector3D& outPoint);

    // Rotates a point around an arbitrary axis through pivot by angleDegrees.
    static QVector3D rotatePointAroundAxis(const QVector3D& point, const QVector3D& pivot,
                                            const QVector3D& axis, float angleDegrees);
};
