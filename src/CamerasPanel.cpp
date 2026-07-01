#include "CamerasPanel.h"
#include "ViewportWidget.h"
#include "SceneGraph.h"
#include "GltfCameraData.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QMenu>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Icon helpers
// ---------------------------------------------------------------------------

namespace
{
QIcon makeCircleIcon(bool filled, const QColor& color)
{
    constexpr int size = 16;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 1.25));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRectF(2.0, 2.0, size - 4.0, size - 4.0));
    if (filled)
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(QRectF(5.1, 5.1, size - 10.2, size - 10.2));
    }
    return QIcon(pixmap);
}
} // namespace

// ---------------------------------------------------------------------------
// CamerasPanel
// ---------------------------------------------------------------------------

CamerasPanel::CamerasPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    _tree = new QTreeWidget(this);
    _tree->setHeaderHidden(true);
    _tree->setColumnCount(1);
    _tree->setRootIsDecorated(true);
    _tree->setIndentation(16);
    _tree->setAlternatingRowColors(true);
    _tree->setSelectionMode(QAbstractItemView::NoSelection);
    _tree->setFocusPolicy(Qt::NoFocus);
    _tree->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(_tree);

    connect(_tree, &QTreeWidget::itemClicked,
            this,  &CamerasPanel::onItemClicked);
    connect(_tree, &QWidget::customContextMenuRequested,
            this, &CamerasPanel::onTreeContextMenuRequested);
}

void CamerasPanel::setSceneGraph(SceneGraph* sg)
{
    _sceneGraph = sg;
}

void CamerasPanel::setGLWidget(ViewportWidget* viewportWidget)
{
    _viewportWidget = viewportWidget;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void CamerasPanel::refresh()
{
    _tree->clear();

    if (!_sceneGraph)
        return;

    // Determine the currently active camera so we can pre-mark it.
    const bool systemCamActive = !_viewportWidget || !_viewportWidget->isGltfCameraActive();
    const QString activeFile   = _viewportWidget ? _viewportWidget->activeGltfCameraFile()  : QString();
    const int     activeIndex  = _viewportWidget ? _viewportWidget->activeGltfCameraIndex() : -1;

    // --- System Camera item (always present) ---
    QTreeWidgetItem* sysItem = makeSystemCameraItem(systemCamActive);
    _tree->addTopLevelItem(sysItem);

    // --- Per-file glTF camera groups ---
    const QStringList files = _sceneGraph->filesWithGltfCameras();
    for (const QString& sourceFile : files)
    {
        const GltfCameraData cd = _sceneGraph->gltfCameraDataForFile(sourceFile);
        if (cd.isEmpty())
            continue;

        const QString displayName = QFileInfo(sourceFile).fileName();
        QTreeWidgetItem* fileItem = makeFileItem(sourceFile, displayName);
        _tree->addTopLevelItem(fileItem);

        for (int i = 0; i < cd.cameras.size(); ++i)
        {
            const bool active = !systemCamActive
                                && sourceFile == activeFile
                                && i == activeIndex;
            fileItem->addChild(makeCameraItem(cd.cameras[i].name, sourceFile, i, active));
        }

        fileItem->setExpanded(true);
    }
}

void CamerasPanel::setDetachedOverlayMode(bool enabled)
{
    if (_overlayMode == enabled)
        return;

    if (enabled)
    {
        _savedStyleSheet        = _tree->styleSheet();
        _savedPalette           = _tree->palette();
        _savedViewportPalette   = _tree->viewport()->palette();
        _savedAutoFill          = _tree->autoFillBackground();
        _savedViewportAutoFill  = _tree->viewport()->autoFillBackground();

        QPalette p = _savedPalette;
        QColor base      = p.color(QPalette::Base);
        QColor alternate = p.color(QPalette::AlternateBase);
        base.setAlpha(0);
        alternate.setAlpha(0);
        p.setColor(QPalette::Base, base);
        p.setColor(QPalette::AlternateBase, alternate);

        _tree->setPalette(p);
        _tree->viewport()->setPalette(p);
        _tree->setAutoFillBackground(false);
        _tree->viewport()->setAutoFillBackground(false);
        _tree->setAttribute(Qt::WA_NoSystemBackground, true);
        _tree->viewport()->setAttribute(Qt::WA_NoSystemBackground, true);
        _tree->viewport()->setAttribute(Qt::WA_StyledBackground, false);
        setProperty("detachedOverlayMode", true);
        _tree->setProperty("detachedOverlayMode", true);
        _tree->viewport()->setProperty("detachedOverlayMode", true);
        _tree->setStyleSheet(QString());

        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
    }
    else
    {
        _tree->setStyleSheet(_savedStyleSheet);
        _tree->setPalette(_savedPalette);
        _tree->viewport()->setPalette(_savedViewportPalette);
        _tree->setAutoFillBackground(_savedAutoFill);
        _tree->viewport()->setAutoFillBackground(_savedViewportAutoFill);
        _tree->setAttribute(Qt::WA_NoSystemBackground, false);
        _tree->viewport()->setAttribute(Qt::WA_NoSystemBackground, false);
        _tree->viewport()->setAttribute(Qt::WA_StyledBackground, false);
        setProperty("detachedOverlayMode", false);
        _tree->setProperty("detachedOverlayMode", false);
        _tree->viewport()->setProperty("detachedOverlayMode", false);

        setAttribute(Qt::WA_NoSystemBackground, false);
        setAutoFillBackground(true);
    }

    _overlayMode = enabled;
    refreshDetachedOverlayTheme();
    _tree->viewport()->update();
    _tree->update();
    update();
}

void CamerasPanel::refreshDetachedOverlayTheme()
{
    if (!_overlayMode || !_tree)
        return;

    const bool lightText = property("overlayViewerLightText").toBool();
    const QColor textColor = lightText ? QColor(255, 255, 255) : QColor(0, 0, 0);
    _detachedOverlayFillColor = lightText ? QColor(255, 255, 255, 65) : QColor(0, 0, 0, 45);

    QPalette treePalette = _tree->palette();
    treePalette.setColor(QPalette::Text, textColor);
    treePalette.setColor(QPalette::WindowText, textColor);
    treePalette.setColor(QPalette::ButtonText, textColor);
    treePalette.setColor(QPalette::HighlightedText, textColor);
    _tree->setPalette(treePalette);

    QPalette viewportPalette = _tree->viewport()->palette();
    viewportPalette.setColor(QPalette::Text, textColor);
    viewportPalette.setColor(QPalette::WindowText, textColor);
    viewportPalette.setColor(QPalette::ButtonText, textColor);
    viewportPalette.setColor(QPalette::HighlightedText, textColor);
    _tree->viewport()->setPalette(viewportPalette);

    if (_sceneGraph)
        refresh();
}

void CamerasPanel::paintEvent(QPaintEvent* event)
{
    if (_overlayMode)
    {
        QPainter painter(this);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(event->rect(), _detachedOverlayFillColor);
    }

    QWidget::paintEvent(event);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void CamerasPanel::onItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item)
        return;

    // File-group items are not clickable
    if (item->data(0, IsFileItemRole).toBool())
        return;

    const bool isSystemCam = item->data(0, IsSystemCamRole).toBool();

    if (isSystemCam)
    {
        markActive(QString(), -1, /*isSystemCam=*/true);
        emit systemCameraRequested();
        return;
    }

    // glTF camera item: SourceFileRole is stored directly on camera items too
    const QString sourceFile  = item->data(0, SourceFileRole).toString();
    const int     cameraIndex = item->data(0, CameraIndexRole).toInt();

    if (sourceFile.isEmpty() || cameraIndex < 0)
        return;

    markActive(sourceFile, cameraIndex, /*isSystemCam=*/false);
    emit gltfCameraActivated(sourceFile, cameraIndex);
}

void CamerasPanel::onTreeContextMenuRequested(const QPoint& pos)
{
    if (!_tree)
        return;

    QTreeWidgetItem* item = _tree->itemAt(pos);
    if (!item)
        return;

    if (item->data(0, IsSystemCamRole).toBool())
        return;

    const bool isFileItem = item->data(0, IsFileItemRole).toBool();
    const QString sourceFile = isFileItem
        ? item->data(0, SourceFileRole).toString()
        : (item->parent() ? item->parent()->data(0, SourceFileRole).toString() : QString());
    const int cameraIndex = isFileItem ? -1 : item->data(0, CameraIndexRole).toInt();

    if (sourceFile.isEmpty())
        return;

    QMenu menu(this);
    QAction* deleteAction = menu.addAction(isFileItem ? tr("Delete All") : tr("Delete"));
    QAction* chosen = menu.exec(_tree->viewport()->mapToGlobal(pos));
    if (chosen == deleteAction)
        emit gltfCameraDeleteRequested(sourceFile, cameraIndex);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

QTreeWidgetItem* CamerasPanel::makeSystemCameraItem(bool active) const
{
    auto* item = new QTreeWidgetItem();
    item->setText(0, tr("System Camera"));
    item->setData(0, IsSystemCamRole, true);
    item->setData(0, IsFileItemRole,  false);
    item->setData(0, CameraIndexRole, -1);
    item->setData(0, SourceFileRole,  QString());
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    item->setIcon(0, active ? activeIcon() : inactiveIcon());
    QFont f = item->font(0);
    f.setItalic(true);
    item->setFont(0, f);
    return item;
}

QTreeWidgetItem* CamerasPanel::makeFileItem(const QString& sourceFile,
                                             const QString& displayName) const
{
    auto* item = new QTreeWidgetItem();
    item->setText(0, displayName);
    item->setToolTip(0, sourceFile);
    item->setData(0, SourceFileRole,  sourceFile);
    item->setData(0, IsFileItemRole,  true);
    item->setData(0, IsSystemCamRole, false);
    item->setData(0, CameraIndexRole, QVariant());
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    QFont f = item->font(0);
    f.setBold(true);
    item->setFont(0, f);
    return item;
}

QTreeWidgetItem* CamerasPanel::makeCameraItem(const QString& label,
                                               const QString& sourceFile,
                                               int cameraIndex,
                                               bool active) const
{
    auto* item = new QTreeWidgetItem();
    item->setText(0, label);
    item->setData(0, SourceFileRole,  sourceFile);
    item->setData(0, CameraIndexRole, cameraIndex);
    item->setData(0, IsFileItemRole,  false);
    item->setData(0, IsSystemCamRole, false);
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    item->setIcon(0, active ? activeIcon() : inactiveIcon());
    return item;
}

void CamerasPanel::markActive(const QString& sourceFile, int cameraIndex, bool isSystemCam)
{
    for (int ti = 0; ti < _tree->topLevelItemCount(); ++ti)
    {
        QTreeWidgetItem* topItem = _tree->topLevelItem(ti);

        // System Camera top-level item
        if (topItem->data(0, IsSystemCamRole).toBool())
        {
            topItem->setIcon(0, isSystemCam ? activeIcon() : inactiveIcon());
            continue;
        }

        // File-group item — update its children
        if (topItem->data(0, IsFileItemRole).toBool())
        {
            const QString fileKey = topItem->data(0, SourceFileRole).toString();
            for (int ci = 0; ci < topItem->childCount(); ++ci)
            {
                QTreeWidgetItem* child = topItem->child(ci);
                const bool active = !isSystemCam
                                    && fileKey == sourceFile
                                    && child->data(0, CameraIndexRole).toInt() == cameraIndex;
                child->setIcon(0, active ? activeIcon() : inactiveIcon());
            }
        }
    }
}

QIcon CamerasPanel::activeIcon() const
{
    const QColor c = _tree ? _tree->palette().color(QPalette::Text)
                           : palette().color(QPalette::Text);
    return makeCircleIcon(true, c);
}

QIcon CamerasPanel::inactiveIcon() const
{
    QColor c = _tree ? _tree->palette().color(QPalette::Text)
                     : palette().color(QPalette::Text);
    c.setAlpha(160);
    return makeCircleIcon(false, c);
}
