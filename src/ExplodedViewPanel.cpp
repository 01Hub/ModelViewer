#include "ExplodedViewPanel.h"
#include "GLWidget.h"
#include "SceneGraph.h"
#include "SceneNode.h"
#include "SelectionManager.h"
#include <QMenu>

// ---------------------------------------------------------------------------
// Parameter accessors
// ---------------------------------------------------------------------------
ExplodedViewManager::Mode ExplodedViewPanel::mode() const
{
    switch (comboBoxMode->currentIndex())
    {
    case 1: return ExplodedViewManager::Mode::AxisX;
    case 2: return ExplodedViewManager::Mode::AxisY;
    case 3: return ExplodedViewManager::Mode::AxisZ;
    case 4: return ExplodedViewManager::Mode::Vector;
    default: return ExplodedViewManager::Mode::Auto;
    }
}

QVector3D ExplodedViewPanel::userVector() const
{
    return QVector3D(
        static_cast<float>(doubleSpinBoxVectorX->value()),
        static_cast<float>(doubleSpinBoxVectorY->value()),
        static_cast<float>(doubleSpinBoxVectorZ->value()));
}

float ExplodedViewPanel::factor() const
{
    return sliderExplosion->value() / 100.0f;
}

ExplodedViewPanel::ExplodedViewPanel(GLWidget* parent)
    : QWidget(parent)
    , _glWidget(parent)
{
    setupUi(this);
    frameVector->setVisible(false);

    // Emit explosionParametersChanged when custom vector spinboxes change.
    auto emitParamChanged = [this]() { emit explosionParametersChanged(); };
    connect(doubleSpinBoxVectorX, &QDoubleSpinBox::valueChanged, this, emitParamChanged);
    connect(doubleSpinBoxVectorY, &QDoubleSpinBox::valueChanged, this, emitParamChanged);
    connect(doubleSpinBoxVectorZ, &QDoubleSpinBox::valueChanged, this, emitParamChanged);

    // Right-click context menu: clear assembly selection
    lineEditAssembly->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(lineEditAssembly, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (lineEditAssembly->text().isEmpty())
            return;
        QMenu menu(this);
        connect(menu.addAction(tr("Clear Selection")), &QAction::triggered, this, [this]() {
            lineEditAssembly->clear();
            lineEditAnchor->clear();
            _assemblyUuids.clear();
            _anchorUuid = QUuid();
            pushButtonSelectAssembly->setChecked(false);
            pushButtonSelectAnchor->setChecked(false);
            updateCaptureButton();
        });
        menu.exec(lineEditAssembly->mapToGlobal(pos));
    });

    // Right-click context menu: clear anchor only
    lineEditAnchor->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(lineEditAnchor, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (lineEditAnchor->text().isEmpty())
            return;
        QMenu menu(this);
        connect(menu.addAction(tr("Clear Anchor")), &QAction::triggered, this, [this]() {
            lineEditAnchor->clear();
            _anchorUuid = QUuid();
            pushButtonSelectAnchor->setChecked(false);
        });
        menu.exec(lineEditAnchor->mapToGlobal(pos));
    });
}

void ExplodedViewPanel::setSceneGraph(SceneGraph* sg)
{
    _sceneGraph = sg;
}

void ExplodedViewPanel::applyContrastTheme(const QColor& textColor)
{
    setStyleSheet(QString("color: %1;").arg(textColor.name()));
}

// ---------------------------------------------------------------------------
// Auto-seed assembly field from whatever the viewport/tree has selected.
// Called by GLWidget::showExplodedViewPanel(true).
// ---------------------------------------------------------------------------
void ExplodedViewPanel::captureCurrentSelection()
{
    if (!_glWidget)
        return;
    const QList<int> ids = _glWidget->getSelectionManager()->getSelectedIds();
    if (!ids.isEmpty())
        applyAssemblySelection(ids);
}

// ---------------------------------------------------------------------------
// Picking mode — arrow buttons
// ---------------------------------------------------------------------------
void ExplodedViewPanel::on_pushButtonSelectAssembly_toggled(bool checked)
{
    if (checked) {
        // Cancel any active anchor picking first (suppress its toggled signal).
        {
            QSignalBlocker b(pushButtonSelectAnchor);
            pushButtonSelectAnchor->setChecked(false);
        }
        cancelPickingMode();

        _pickingTarget = PickingTarget::Assembly;
        lineEditAssembly->setPlaceholderText(tr("Click mesh or node in scene…"));

        _pickingConn = connect(_glWidget, &GLWidget::selectionChanged,
                               this, &ExplodedViewPanel::onPickingSelectionChanged);
    } else {
        cancelPickingMode();
        lineEditAssembly->setPlaceholderText(tr("Select assembly or meshes…"));
    }
}

void ExplodedViewPanel::on_pushButtonSelectAnchor_toggled(bool checked)
{
    if (checked) {
        {
            QSignalBlocker b(pushButtonSelectAssembly);
            pushButtonSelectAssembly->setChecked(false);
        }
        cancelPickingMode();

        _pickingTarget = PickingTarget::Anchor;
        lineEditAnchor->setPlaceholderText(tr("Click mesh or node in scene…"));

        _pickingConn = connect(_glWidget, &GLWidget::selectionChanged,
                               this, &ExplodedViewPanel::onPickingSelectionChanged);
    } else {
        cancelPickingMode();
        lineEditAnchor->setPlaceholderText(tr("Select anchor mesh (optional)…"));
    }
}

void ExplodedViewPanel::onPickingSelectionChanged(const QList<int>& ids)
{
    if (ids.isEmpty())
        return; // Ignore deselection events while waiting for a pick.

    disconnect(_pickingConn);

    if (_pickingTarget == PickingTarget::Assembly) {
        applyAssemblySelection(ids);
        pushButtonSelectAssembly->setChecked(false);
        lineEditAssembly->setPlaceholderText(tr("Select assembly or meshes…"));
    } else if (_pickingTarget == PickingTarget::Anchor) {
        applyAnchorSelection(ids);
        pushButtonSelectAnchor->setChecked(false);
        lineEditAnchor->setPlaceholderText(tr("Select anchor mesh (optional)…"));
    }

    _pickingTarget = PickingTarget::None;
    emit selectionClearRequested();
    updateCaptureButton();
}

void ExplodedViewPanel::cancelPickingMode()
{
    if (_pickingTarget != PickingTarget::None) {
        disconnect(_pickingConn);
        _pickingTarget = PickingTarget::None;
    }
}

// ---------------------------------------------------------------------------
// Selection capture helpers
// ---------------------------------------------------------------------------
void ExplodedViewPanel::applyAssemblySelection(const QList<int>& ids)
{
    _assemblyUuids.clear();
    for (int id : ids)
        _assemblyUuids.insert(_glWidget->getUuidByIndex(id));

    lineEditAssembly->setText(describeAssemblySelection(ids));

    // Anchor must belong to the assembly — clear it if it no longer does.
    if (!_anchorUuid.isNull() && !_assemblyUuids.contains(_anchorUuid)) {
        lineEditAnchor->clear();
        _anchorUuid = QUuid();
    }

    emit explosionParametersChanged();
}

void ExplodedViewPanel::applyAnchorSelection(const QList<int>& ids)
{
    // Anchor is always a single mesh — take the first picked one.
    const QUuid uuid = _glWidget->getUuidByIndex(ids.first());

    // Anchor must be within the assembly selection (if one exists).
    if (!_assemblyUuids.isEmpty() && !_assemblyUuids.contains(uuid)) {
        lineEditAnchor->setPlaceholderText(tr("Anchor must be within the assembly"));
        return;
    }

    _anchorUuid = uuid;

    QString name;
    if (_sceneGraph) {
        const SceneNode* node = _sceneGraph->findNodeForMesh(uuid);
        name = node ? node->name : tr("Mesh");
    } else {
        name = tr("Mesh");
    }
    lineEditAnchor->setText(name);
    emit explosionParametersChanged();
}

QString ExplodedViewPanel::describeAssemblySelection(const QList<int>& ids) const
{
    if (!_sceneGraph || ids.isEmpty())
        return tr("%1 meshes").arg(ids.size());

    // Collect unique owning SceneNodes.
    QSet<const SceneNode*> owningNodes;
    for (int id : ids) {
        const QUuid uuid = _glWidget->getUuidByIndex(id);
        const SceneNode* node = _sceneGraph->findNodeForMesh(uuid);
        if (node)
            owningNodes.insert(node);
    }

    if (owningNodes.isEmpty())
        return tr("%1 meshes").arg(ids.size());

    // Single owning node → use its name directly.
    if (owningNodes.size() == 1)
        return (*owningNodes.begin())->name;

    // Multiple owning nodes — check for a common direct parent.
    const SceneNode* commonParent = nullptr;
    bool allSameParent = true;
    for (const SceneNode* n : owningNodes) {
        if (!commonParent) {
            commonParent = n->parent;
        } else if (n->parent != commonParent) {
            allSameParent = false;
            break;
        }
    }

    if (allSameParent && commonParent && !commonParent->isSynthetic)
        return commonParent->name;

    return tr("%1 meshes").arg(ids.size());
}

// ---------------------------------------------------------------------------
// Combo / slider / buttons
// ---------------------------------------------------------------------------
void ExplodedViewPanel::on_comboBoxMode_currentIndexChanged(int index)
{
    frameVector->setVisible(index == 4); // 4 = Custom Vector
    emit explosionParametersChanged();
}

void ExplodedViewPanel::on_sliderExplosion_valueChanged(int value)
{
    labelDistancePercent->setText(QString("%1%").arg(value));
    updateCaptureButton();
    emit explosionParametersChanged();
}

void ExplodedViewPanel::on_pushButtonCapture_clicked()
{
    // Full implementation in Phase 1 logic pass.
}

void ExplodedViewPanel::on_pushButtonReset_clicked()
{
    cancelPickingMode();

    QSignalBlocker b1(comboBoxMode);
    QSignalBlocker b2(sliderExplosion);
    QSignalBlocker b3(doubleSpinBoxVectorX);
    QSignalBlocker b4(doubleSpinBoxVectorY);
    QSignalBlocker b5(doubleSpinBoxVectorZ);

    lineEditAssembly->clear();
    lineEditAnchor->clear();
    _assemblyUuids.clear();
    _anchorUuid = QUuid();

    pushButtonSelectAssembly->setChecked(false);
    pushButtonSelectAnchor->setChecked(false);
    lineEditAssembly->setPlaceholderText(tr("Select assembly or meshes…"));
    lineEditAnchor->setPlaceholderText(tr("Select anchor mesh (optional)…"));

    comboBoxMode->setCurrentIndex(0);
    frameVector->setVisible(false);

    sliderExplosion->setValue(100);
    labelDistancePercent->setText("100%");

    doubleSpinBoxVectorX->setValue(1.0);
    doubleSpinBoxVectorY->setValue(0.0);
    doubleSpinBoxVectorZ->setValue(0.0);

    updateCaptureButton();

    // Clear exploded view rendering and any lingering viewport selection.
    emit explosionParametersChanged();
    emit selectionClearRequested();
}

void ExplodedViewPanel::updateCaptureButton()
{
    pushButtonCapture->setEnabled(sliderExplosion->value() >= 10);
}
