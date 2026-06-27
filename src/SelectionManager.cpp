#include "SelectionManager.h"
#include "GLWidget.h"
#include "GLCamera.h"
#include "RenderableMesh.h"
#include <QColor>
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLContext>
#include <QApplication>
#include <QRect>
#include <QMatrix4x4>
#include <QVector4D>
#include <algorithm>
#include <iostream>
#include <map>
#include <vector>

SelectionManager::SelectionManager(
    GLWidget* glWidget,
    GLCamera* primaryCamera,
    std::vector<SceneMeshRecord>& meshStore,
    const std::vector<int>& displayedObjectsIds,
    const std::vector<int>& hiddenObjectsIds,
    bool& visibleSwapped,
    QObject* parent)
    : QObject(parent),
      _glWidget(glWidget),
      _primaryCamera(primaryCamera),
      _meshStore(meshStore),
      _displayedObjectsIds(displayedObjectsIds),
      _hiddenObjectsIds(hiddenObjectsIds),
      _visibleSwapped(visibleSwapped)
{
    // Constructor body - initialization handled in member initializer list
}

SelectionManager::~SelectionManager()
{
    cleanupFBOResources();
}

// ============================================================================
// Public Methods - Selection Operations
// ============================================================================

int SelectionManager::clickSelect(const QPoint& pixel)
{
    int id = -1;
    _selectedMeshIds.clear();  // Click select clears and selects ONE mesh

    const auto& ids = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
    if (ids.empty()) {
        return -1;
    }

    QVector3D rayPos, rayDir, intersectionPoint;
    QRect viewport = getViewportFromPoint(pixel);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    convertClickToRay(pixel, viewport, _glWidget->getCameraForPoint(pixel), rayPos, rayDir);
    if (rayDir.isNull()) {
        QApplication::restoreOverrideCursor();
        return -1;
    }
    rayDir.normalize();

    // === Ray-based intersection test ===
    QMap<int, float> selectedIdsDist;
    for (int i : ids) {
        TriangleMesh* mesh = _meshStore.at(i);
        if (mesh->getBoundingSphere().intersectsWithRay(rayPos, rayDir)) {
            if (mesh->intersectsWithRay(rayPos, rayDir, intersectionPoint)) {
                selectedIdsDist[i] = intersectionPoint.distanceToPoint(rayPos);
            }
        }
    }

    if (!selectedIdsDist.isEmpty()) {
        auto it = std::min_element(
            selectedIdsDist.constBegin(), selectedIdsDist.constEnd(),
            [](auto a, auto b) { return a < b; });
        id = it.key();
    }

    // === GPU color-picking ===
    // This is the authoritative path for animated meshes because it uses the
    // current render-time transforms / skinning state rather than cached CPU
    // triangle data.
    const int colId = _glWidget ? _glWidget->processSelection(pixel) : -1;

    QApplication::restoreOverrideCursor();

    int selectedId = -1;
    switch (_selectionMode) {
    case SelectionMode::RayOnly:
        selectedId = id;
        break;
    case SelectionMode::ColorOnly:
        selectedId = colId;
        break;
    case SelectionMode::Hybrid:
        selectedId = (colId != -1) ? colId : id;
        break;
    }

    if (selectedId >= 0)
        _selectedMeshIds.push_back(selectedId);

    // Always emit — an empty list notifies connected panels/views that
    // nothing is selected (e.g. the user clicked empty space in the viewport).
    emit selectionChanged(_selectedMeshIds);

    return selectedId;
}

int SelectionManager::hoverSelect(const QPoint& pixel)
{
    int hoveredId = -1;

    const auto& ids = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
    if (ids.empty())
        return -1;

    const bool animatedPoseActive = _glWidget
        && !_glWidget->activeAnimationFile().isEmpty()
        && _glWidget->activeAnimationClip() >= 0;

    if (_hoverHighlightMode == HoverHighlightMode::Accurate || animatedPoseActive)
    {
        hoveredId = _glWidget ? _glWidget->processSelection(pixel) : -1;
    }
    else
    {
        QVector3D rayPos, rayDir, intersectionPoint;
        QRect viewport = getViewportFromPoint(pixel);

        convertClickToRay(pixel, viewport, _glWidget->getCameraForPoint(pixel), rayPos, rayDir);
        if (rayDir.isNull())
            return -1;
        rayDir.normalize();

        // === Ray-based intersection test (performance-optimized) ===
        QMap<int, float> hitDistances;
        for (int i : ids) {
            TriangleMesh* mesh = _meshStore.at(i);
            if (mesh->getBoundingSphere().intersectsWithRay(rayPos, rayDir)) {
                if (mesh->intersectsWithRay(rayPos, rayDir, intersectionPoint)) {
                    hitDistances[i] = intersectionPoint.distanceToPoint(rayPos);
                }
            }
        }

        // Return the closest hit
        if (!hitDistances.isEmpty()) {
            auto it = std::min_element(
                hitDistances.constBegin(), hitDistances.constEnd(),
                [](auto a, auto b) { return a < b; });
            hoveredId = it.key();
        }
    }

    // Update hover state and emit signal if changed
    if (hoveredId != _hoveredMeshId) {
        _hoveredMeshId = hoveredId;
        emit hoverChanged(hoveredId);
    }

    return hoveredId;
}

QList<int> SelectionManager::sweepSelect(const QPoint& p1, const QPoint& p2)
{
    // Sweep selection not yet implemented in SelectionManager
    // This will be moved when needed
    return QList<int>();
}

// ============================================================================
// Settings Slots
// ============================================================================

void SelectionManager::setHoverHighlightMode(HoverHighlightMode mode)
{
    if (_hoverHighlightMode != mode)
    {
        _hoverHighlightMode = mode;
        if (mode == HoverHighlightMode::Disabled && _hoveredMeshId != -1)
        {
            _hoveredMeshId = -1;
            emit hoverChanged(-1);
        }
        emit hoverModeChanged(mode);
    }
}

void SelectionManager::setSelectionMode(SelectionMode mode)
{
    if (_selectionMode != mode)
    {
        _selectionMode = mode;
        emit selectionModeChanged(mode);
    }
}

// ============================================================================
// FBO Management
// ============================================================================

void SelectionManager::initializeFBOResources()
{
    // FBO resources are created on-demand in processSelection
    // This is called during initialization if needed in future
}

void SelectionManager::cleanupFBOResources()
{
    // Note: Actual GL cleanup happens in GLWidget::resizeGL() and destructor
    // SelectionManager just tracks the resource IDs
    _selectionFBO = 0;
    _selectionRBO = 0;
    _selectionDBO = 0;
}

void SelectionManager::resizeFBOResources(int width, int height)
{
    // Note: Actual GL cleanup happens in GLWidget::resizeGL()
    // SelectionManager just tracks dimensions
    _fboWidth = width;
    _fboHeight = height;
    // FBO resources will be recreated on demand in processSelection()
    _selectionFBO = 0;
    _selectionRBO = 0;
    _selectionDBO = 0;
}

// ============================================================================
// Helper Methods - Ray Conversion
// ============================================================================

void SelectionManager::getRayFromPixelCoords(const QPoint& pixel, QVector3D& rayPos, QVector3D& rayDir)
{
    // This is a placeholder - actual implementation uses convertClickToRay
    // Kept for API consistency
}

void SelectionManager::convertClickToRay(const QPoint& pixel, const QRect& viewport,
                                        GLCamera* camera, QVector3D& orig, QVector3D& dir)
{
    if (viewport.width() <= 0 || viewport.height() <= 0) {
        orig = QVector3D(0, 0, 0);
        dir  = QVector3D(0, 0, 0);
        return;
    }

    int yInverted = _glWidget->height() - pixel.y() - 1;

    QMatrix4x4 view = camera->getViewMatrix();
    QMatrix4x4 projection = camera->getProjectionMatrix();

    // Convert to Normalized Device Coordinates [-1, 1]
    float ndcX = (2.0f * (pixel.x() - viewport.x())) / viewport.width() - 1.0f;
    float ndcY = (2.0f * (yInverted - viewport.y())) / viewport.height() - 1.0f;

    QVector4D nearNDC(ndcX, ndcY, -1.0f, 1.0f); // Near plane
    QVector4D farNDC(ndcX, ndcY, 1.0f, 1.0f);   // Far plane

    QMatrix4x4 inv = (projection * view).inverted();

    QVector4D nearWorld = inv * nearNDC;
    QVector4D farWorld = inv * farNDC;

    // Homogeneous divide
    nearWorld /= nearWorld.w();
    farWorld /= farWorld.w();

    orig = nearWorld.toVector3D();
    QVector3D rawDir = farWorld.toVector3D() - orig;
    dir = rawDir.isNull() ? QVector3D(0, 0, 0) : rawDir.normalized();
}

QRect SelectionManager::getViewportFromPoint(const QPoint& point)
{
    QRect viewport;
    int w = _glWidget->width();
    int h = _glWidget->height();

    // Check if multi-view is active
    bool multiViewActive = _glWidget->isMultiViewActive();

    if (multiViewActive)
    {
        // top view
        if (point.x() < w / 2 && point.y() > h / 2)
            viewport = QRect(0, 0, w / 2, h / 2);
        // front view
        else if (point.x() < w / 2 && point.y() <= h / 2)
            viewport = QRect(0, h / 2, w / 2, h / 2);
        // left view
        else if (point.x() >= w / 2 && point.y() < h / 2)
            viewport = QRect(w / 2, h / 2, w / 2, h / 2);
        // isometric (also catches pixels exactly on the dividing lines)
        else
            viewport = QRect(w / 2, 0, w / 2, h / 2);
    }
    else
    {
        // single viewport
        viewport = QRect(0, 0, w, h);
    }

    return viewport;
}

// ============================================================================
// Helper Methods - Color Picking
// ============================================================================

unsigned int SelectionManager::processSelection(const QPoint& pixel)
{
    // NOTE: FBO color picking is handled by GLWidget::processSelection()
    // SelectionManager focuses on ray-casting selection logic only
    // This method is kept for potential future extension
    return 0;
}

unsigned int SelectionManager::colorToIndex(const QColor& color)
{
    int alpha = color.alpha();
    int red = color.red();
    int green = color.green();
    int blue = color.blue();
    unsigned int index = ((alpha << 24) | (red << 16) | (green << 8) | (blue));
    return index;
}

QColor SelectionManager::indexToColor(const unsigned int& index)
{
    int red = ((index >> 16) & 0xFF);
    int green = ((index >> 8) & 0xFF);
    int blue = (index & 0xFF);
    int alpha = ((index >> 24) & 0xFF);
    return QColor(red, green, blue, alpha);
}
