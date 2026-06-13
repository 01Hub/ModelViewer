#include "ExplodedViewPanel.h"

#include "GLWidget.h"
#include "GltfAnimationData.h"
#include "SceneGraph.h"
#include "SceneNode.h"
#include "SelectionManager.h"
#include "TriangleMesh.h"

#include <QAbstractItemModel>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStyle>
#include <QTimer>
#include <QDebug>
#include <QtGlobal>
#include <algorithm>
#include <assimp/matrix4x4.h>
#include <functional>
#include <utility>

namespace
{
constexpr float kExplosionOffsetTolerance = 1.0e-4f;
constexpr float kRotationDotTolerance = 1.0e-4f;

struct LocalNodeTransform
{
    QVector3D translation;
    QQuaternion rotation;
    QVector3D scale = QVector3D(1.0f, 1.0f, 1.0f);
};

QMatrix4x4 aiToQMatrix(const aiMatrix4x4& matrix)
{
    QMatrix4x4 out;
    out.setRow(0, QVector4D(matrix.a1, matrix.a2, matrix.a3, matrix.a4));
    out.setRow(1, QVector4D(matrix.b1, matrix.b2, matrix.b3, matrix.b4));
    out.setRow(2, QVector4D(matrix.c1, matrix.c2, matrix.c3, matrix.c4));
    out.setRow(3, QVector4D(matrix.d1, matrix.d2, matrix.d3, matrix.d4));
    return out;
}

aiMatrix4x4 qToAiMatrix(const QMatrix4x4& matrix)
{
    aiMatrix4x4 out;
    out.a1 = matrix(0, 0); out.a2 = matrix(0, 1); out.a3 = matrix(0, 2); out.a4 = matrix(0, 3);
    out.b1 = matrix(1, 0); out.b2 = matrix(1, 1); out.b3 = matrix(1, 2); out.b4 = matrix(1, 3);
    out.c1 = matrix(2, 0); out.c2 = matrix(2, 1); out.c3 = matrix(2, 2); out.c4 = matrix(2, 3);
    out.d1 = matrix(3, 0); out.d2 = matrix(3, 1); out.d3 = matrix(3, 2); out.d4 = matrix(3, 3);
    return out;
}

LocalNodeTransform decomposeLocalNodeTransform(const QMatrix4x4& matrix)
{
    aiVector3D scaling;
    aiQuaternion rotation;
    aiVector3D position;
    qToAiMatrix(matrix).Decompose(scaling, rotation, position);

    LocalNodeTransform result;
    result.translation = QVector3D(position.x, position.y, position.z);
    result.rotation = QQuaternion(rotation.w, rotation.x, rotation.y, rotation.z).normalized();
    result.scale = QVector3D(scaling.x, scaling.y, scaling.z);
    return result;
}

QMatrix4x4 composeLocalNodeTransform(const LocalNodeTransform& transform)
{
    QMatrix4x4 matrix;
    matrix.setToIdentity();
    matrix.translate(transform.translation);
    matrix.rotate(transform.rotation);
    matrix.scale(transform.scale);
    return matrix;
}

bool rotationsNearlyEqual(const QQuaternion& a, const QQuaternion& b)
{
    return std::abs(QQuaternion::dotProduct(a.normalized(), b.normalized())) >= (1.0f - kRotationDotTolerance);
}

QString formatPreviewTime(double seconds)
{
    const int totalMs = qMax(0, static_cast<int>(seconds * 1000.0));
    const int minutes = totalMs / 60000;
    const int secs = (totalMs / 1000) % 60;
    const int centiseconds = (totalMs / 10) % 100;
    return QStringLiteral("%1:%2.%3")
        .arg(minutes)
        .arg(secs, 2, 10, QLatin1Char('0'))
        .arg(centiseconds, 2, 10, QLatin1Char('0'));
}

void collectNodeBindings(SceneNode* node,
                         QVector<GltfAnimationNodeBinding>& out,
                         QHash<QUuid, int>& nodeIndexByUuid,
                         const QVector<int>& aiChildPath = {})
{
    if (!node)
        return;

    GltfAnimationNodeBinding binding;
    binding.nodeIndex = out.size();
    binding.nodeName = node->name;
    binding.hasAiChildPath = true;
    binding.aiChildPath = aiChildPath;
    out.append(binding);
    nodeIndexByUuid.insert(node->nodeUuid, binding.nodeIndex);

    for (int childIndex = 0; childIndex < node->children.size(); ++childIndex)
    {
        SceneNode* child = node->children[childIndex];
        QVector<int> childPath = aiChildPath;
        childPath.append(childIndex);
        collectNodeBindings(child, out, nodeIndexByUuid, childPath);
    }
}

QString sourceFileForNode(SceneNode* node)
{
    for (SceneNode* cur = node; cur; cur = cur->parent)
    {
        if (!cur->sourceFile.isEmpty())
            return cur->sourceFile;
    }
    return QString();
}

SceneNode* findSceneNodeByAiChildPath(SceneNode* aiRootNode, const QVector<int>& aiChildPath)
{
    SceneNode* node = aiRootNode;
    for (int childIndex : aiChildPath)
    {
        if (!node || childIndex < 0 || childIndex >= node->children.size())
            return nullptr;
        node = node->children[childIndex];
    }
    return node;
}

ExplodedViewManager::Mode modeFromComboIndex(int index)
{
    switch (index)
    {
    case 1: return ExplodedViewManager::Mode::AxisX;
    case 2: return ExplodedViewManager::Mode::AxisY;
    case 3: return ExplodedViewManager::Mode::AxisZ;
    case 4: return ExplodedViewManager::Mode::Vector;
    default: return ExplodedViewManager::Mode::Auto;
    }
}

int comboIndexFromMode(ExplodedViewManager::Mode mode)
{
    switch (mode)
    {
    case ExplodedViewManager::Mode::AxisX: return 1;
    case ExplodedViewManager::Mode::AxisY: return 2;
    case ExplodedViewManager::Mode::AxisZ: return 3;
    case ExplodedViewManager::Mode::Vector: return 4;
    case ExplodedViewManager::Mode::Auto:
    default:
        return 0;
    }
}
}

// ---------------------------------------------------------------------------
// Parameter accessors
// ---------------------------------------------------------------------------
ExplodedViewManager::Mode ExplodedViewPanel::mode() const
{
    if (comboBoxMode)
        return modeFromComboIndex(comboBoxMode->currentIndex());
    if (const ExplodedViewPreset* preset = activePreset())
        return preset->mode;
    return ExplodedViewManager::Mode::Auto;
}

QVector3D ExplodedViewPanel::userVector() const
{
    if (doubleSpinBoxVectorX && doubleSpinBoxVectorY && doubleSpinBoxVectorZ)
    {
        return QVector3D(
            static_cast<float>(doubleSpinBoxVectorX->value()),
            static_cast<float>(doubleSpinBoxVectorY->value()),
            static_cast<float>(doubleSpinBoxVectorZ->value()));
    }
    if (const ExplodedViewPreset* preset = activePreset())
        return preset->userVector;
    return QVector3D(1.0f, 0.0f, 0.0f);
}

float ExplodedViewPanel::factor() const
{
    if (sliderExplosion)
        return sliderExplosion->value() / 100.0f;
    if (const ExplodedViewPreset* preset = activePreset())
        return preset->factor;
    return 1.0f;
}

const QSet<QUuid>& ExplodedViewPanel::assemblyUuids() const
{
    if (const ExplodedViewPreset* preset = activePreset())
        return preset->assemblyUuids;
    return _emptyAssemblyUuids;
}

QUuid ExplodedViewPanel::anchorUuid() const
{
    if (const ExplodedViewPreset* preset = activePreset())
        return preset->anchorUuid;
    return QUuid();
}

ExplodedViewPanel::ExplodedViewPreset* ExplodedViewPanel::activePreset()
{
    if (_activePresetIndex < 0 || _activePresetIndex >= _presets.size())
        return nullptr;
    return &_presets[_activePresetIndex];
}

const ExplodedViewPanel::ExplodedViewPreset* ExplodedViewPanel::activePreset() const
{
    if (_activePresetIndex < 0 || _activePresetIndex >= _presets.size())
        return nullptr;
    return &_presets[_activePresetIndex];
}

ExplodedViewPanel::ExplodedViewPreset& ExplodedViewPanel::ensureActivePreset()
{
    if (_activePresetIndex < 0 || _activePresetIndex >= _presets.size())
    {
        initializeDefaultPreset();
        if (_activePresetIndex < 0 || _activePresetIndex >= _presets.size())
            _activePresetIndex = 0;
    }
    return _presets[_activePresetIndex];
}

QVector<ExplodedViewPanel::CapturedExplosionStep>& ExplodedViewPanel::activeCapturedSteps()
{
    return ensureActivePreset().capturedSteps;
}

const QVector<ExplodedViewPanel::CapturedExplosionStep>& ExplodedViewPanel::activeCapturedSteps() const
{
    static const QVector<CapturedExplosionStep> kEmptySteps;
    if (const ExplodedViewPreset* preset = activePreset())
        return preset->capturedSteps;
    return kEmptySteps;
}

void ExplodedViewPanel::refreshPresetCombo()
{
    if (!comboBoxPreset)
        return;

    QSignalBlocker blocker(comboBoxPreset);
    comboBoxPreset->clear();
    for (const ExplodedViewPreset& preset : std::as_const(_presets))
        comboBoxPreset->addItem(preset.name, preset.id);

    if (_activePresetIndex >= 0 && _activePresetIndex < comboBoxPreset->count())
        comboBoxPreset->setCurrentIndex(_activePresetIndex);
}

void ExplodedViewPanel::syncActivePresetFromUi()
{
    if (_syncingPresetUi)
        return;

    ExplodedViewPreset& preset = ensureActivePreset();
    preset.mode = modeFromComboIndex(comboBoxMode ? comboBoxMode->currentIndex() : 0);
    preset.userVector = QVector3D(
        static_cast<float>(doubleSpinBoxVectorX ? doubleSpinBoxVectorX->value() : 1.0),
        static_cast<float>(doubleSpinBoxVectorY ? doubleSpinBoxVectorY->value() : 0.0),
        static_cast<float>(doubleSpinBoxVectorZ ? doubleSpinBoxVectorZ->value() : 0.0));
    preset.factor = sliderExplosion ? sliderExplosion->value() / 100.0f : 1.0f;
    preset.outputMode = comboBoxAnimationMode ? comboBoxAnimationMode->currentIndex() : 0;
    preset.durationSeconds = doubleSpinBoxAnimationDuration ? doubleSpinBoxAnimationDuration->value() : 3.0;
    preset.loopBack = checkBoxLoopBack && checkBoxLoopBack->isChecked();
}

void ExplodedViewPanel::loadPresetIntoUi(int index)
{
    if (index < 0 || index >= _presets.size())
        return;

    stopDraftPreview();
    _activePresetIndex = index;
    const ExplodedViewPreset& preset = _presets[index];

    _syncingPresetUi = true;
    {
        QSignalBlocker b1(comboBoxPreset);
        QSignalBlocker b2(comboBoxMode);
        QSignalBlocker b3(sliderExplosion);
        QSignalBlocker b4(doubleSpinBoxVectorX);
        QSignalBlocker b5(doubleSpinBoxVectorY);
        QSignalBlocker b6(doubleSpinBoxVectorZ);
        QSignalBlocker b7(comboBoxAnimationMode);
        QSignalBlocker b8(doubleSpinBoxAnimationDuration);
        QSignalBlocker b9(checkBoxLoopBack);

        if (comboBoxPreset)
            comboBoxPreset->setCurrentIndex(index);
        if (comboBoxMode)
            comboBoxMode->setCurrentIndex(comboIndexFromMode(preset.mode));
        if (sliderExplosion)
            sliderExplosion->setValue(qRound(preset.factor * 100.0f));
        if (doubleSpinBoxVectorX)
            doubleSpinBoxVectorX->setValue(preset.userVector.x());
        if (doubleSpinBoxVectorY)
            doubleSpinBoxVectorY->setValue(preset.userVector.y());
        if (doubleSpinBoxVectorZ)
            doubleSpinBoxVectorZ->setValue(preset.userVector.z());
        if (comboBoxAnimationMode)
            comboBoxAnimationMode->setCurrentIndex(qBound(0, preset.outputMode, comboBoxAnimationMode->count() - 1));
        if (doubleSpinBoxAnimationDuration)
            doubleSpinBoxAnimationDuration->setValue(preset.durationSeconds);
        if (checkBoxLoopBack)
            checkBoxLoopBack->setChecked(preset.loopBack);
    }
    _syncingPresetUi = false;

    labelDistancePercent->setText(QString("%1%").arg(qRound(preset.factor * 100.0f)));
    if (frameVector)
        frameVector->setVisible((!radioButtonModeManual || !radioButtonModeManual->isChecked()) && comboBoxMode && comboBoxMode->currentIndex() == 4);

    QList<int> selectionIds;
    selectionIds.reserve(preset.assemblyUuids.size());
    if (_glWidget)
    {
        for (const QUuid& uuid : preset.assemblyUuids)
        {
            const int indexByUuid = _glWidget->getIndexByUuid(uuid);
            if (indexByUuid >= 0)
                selectionIds.append(indexByUuid);
        }
    }

    if (selectionIds.isEmpty())
    {
        lineEditAssembly->clear();
        if (lineEditManualSelection)
            lineEditManualSelection->clear();
    }
    else
    {
        const QString selectionText = describeAssemblySelection(selectionIds);
        lineEditAssembly->setText(selectionText);
        if (lineEditManualSelection)
            lineEditManualSelection->setText(selectionText);
    }

    if (!preset.anchorUuid.isNull() && _glWidget)
    {
        if (_sceneGraph)
        {
            const SceneNode* anchorNode = _sceneGraph->findNodeForMesh(preset.anchorUuid);
            if (anchorNode)
                lineEditAnchor->setText(anchorNode->name);
            else
                lineEditAnchor->setText(preset.anchorUuid.toString(QUuid::WithoutBraces));
        }
        else
            lineEditAnchor->setText(preset.anchorUuid.toString(QUuid::WithoutBraces));
    }
    else
    {
        lineEditAnchor->clear();
    }

    updateCapturedViewsList();
    if (listWidgetCapturedViews && !preset.capturedSteps.isEmpty())
        listWidgetCapturedViews->setCurrentRow(0);
    updateCaptureButton();
    updatePreviewControls();
    emit explosionParametersChanged();
}

void ExplodedViewPanel::initializeDefaultPreset()
{
    if (!_presets.isEmpty())
        return;

    ExplodedViewPreset preset;
    preset.id = QUuid::createUuid();
    preset.name = tr("Explosion 1");
    preset.loopBack = checkBoxLoopBack ? checkBoxLoopBack->isChecked() : true;
    preset.durationSeconds = doubleSpinBoxAnimationDuration ? doubleSpinBoxAnimationDuration->value() : 3.0;
    preset.outputMode = comboBoxAnimationMode ? comboBoxAnimationMode->currentIndex() : 0;
    _presets.append(preset);
    _activePresetIndex = 0;
    refreshPresetCombo();
    loadPresetIntoUi(0);
}

ExplodedViewPanel::ExplodedViewPanel(GLWidget* parent)
    : QWidget(parent)
    , _glWidget(parent)
{
    setupUi(this);
    _draftPreviewTimer = new QTimer(this);
    _draftPreviewTimer->setInterval(16);
    frameVector->setVisible(false);
    _draftPreviewLoopPlayback = true;
    checkBoxPreviewLoop->setChecked(_draftPreviewLoopPlayback);
    _assemblySelectIdleIcon = pushButtonSelectAssembly->icon();
    _assemblySelectCommitIcon = QIcon(":/icons/res/checkmark.png");
    if (_assemblySelectCommitIcon.isNull())
        _assemblySelectCommitIcon = style()->standardIcon(QStyle::SP_DialogApplyButton);
    updateAssemblyPickButtonVisual(false);

    auto emitParamChanged = [this]() { emit explosionParametersChanged(); };
    connect(doubleSpinBoxVectorX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, emitParamChanged);
    connect(doubleSpinBoxVectorY, qOverload<double>(&QDoubleSpinBox::valueChanged), this, emitParamChanged);
    connect(doubleSpinBoxVectorZ, qOverload<double>(&QDoubleSpinBox::valueChanged), this, emitParamChanged);
    connect(_draftPreviewTimer, &QTimer::timeout, this, &ExplodedViewPanel::onDraftPreviewTick);

    lineEditAssembly->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(lineEditAssembly, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (lineEditAssembly->text().isEmpty())
            return;
        QMenu menu(this);
        connect(menu.addAction(tr("Clear Selection")), &QAction::triggered, this, [this]() {
            cancelPickingMode();
            clearAssemblySelection();
            {
                QSignalBlocker b1(pushButtonSelectAssembly);
                QSignalBlocker b2(pushButtonSelectAnchor);
                pushButtonSelectAssembly->setChecked(false);
                pushButtonSelectAnchor->setChecked(false);
            }
            updateAssemblyPickButtonVisual(false);
            updateCaptureButton();
            emit explosionParametersChanged();
            emit selectionClearRequested();
        });
        menu.exec(lineEditAssembly->mapToGlobal(pos));
    });

    lineEditAnchor->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(lineEditAnchor, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (lineEditAnchor->text().isEmpty())
            return;
        QMenu menu(this);
        connect(menu.addAction(tr("Clear Anchor")), &QAction::triggered, this, [this]() {
            lineEditAnchor->clear();
            ensureActivePreset().anchorUuid = QUuid();
            pushButtonSelectAnchor->setChecked(false);
            emit explosionParametersChanged();
        });
        menu.exec(lineEditAnchor->mapToGlobal(pos));
    });

    if (listWidgetCapturedViews)
    {
        listWidgetCapturedViews->setDragEnabled(true);
        listWidgetCapturedViews->setAcceptDrops(true);
        listWidgetCapturedViews->setDropIndicatorShown(true);
        listWidgetCapturedViews->setDragDropMode(QAbstractItemView::InternalMove);
        listWidgetCapturedViews->setDefaultDropAction(Qt::MoveAction);
        connect(listWidgetCapturedViews, &QListWidget::currentRowChanged,
                this, [this](int) {
            stopDraftPreview();
            updateCaptureButton();
            updatePreviewControls();
        });
        connect(listWidgetCapturedViews->model(), &QAbstractItemModel::rowsMoved,
                this, [this]() {
            stopDraftPreview();
            syncCapturedStepOrderFromList();
            updateCaptureButton();
            updatePreviewControls();
        });
    }

    connect(radioButtonModeAuto, &QRadioButton::toggled, this, [this](bool) {
        updateAuthoringModeUi();
    });
    connect(radioButtonModeManual, &QRadioButton::toggled, this, [this](bool) {
        updateAuthoringModeUi();
    });
    connect(pushButtonPreviewPlayPause, &QPushButton::clicked,
            this, &ExplodedViewPanel::onPreviewPlayPauseClicked);
    connect(pushButtonPreviewStop, &QPushButton::clicked,
            this, &ExplodedViewPanel::onPreviewStopClicked);
    connect(checkBoxPreviewLoop, &QCheckBox::toggled,
            this, &ExplodedViewPanel::onPreviewLoopToggled);
    connect(sliderPreviewTimeline, &QSlider::sliderPressed,
            this, &ExplodedViewPanel::onPreviewSliderPressed);
    connect(sliderPreviewTimeline, &QSlider::sliderReleased,
            this, &ExplodedViewPanel::onPreviewSliderReleased);
    connect(sliderPreviewTimeline, &QSlider::valueChanged,
            this, &ExplodedViewPanel::onPreviewSliderValueChanged);
    connect(comboBoxAnimationMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        stopDraftPreview();
        updatePreviewControls();
    });
    connect(doubleSpinBoxAnimationDuration, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        stopDraftPreview();
        updatePreviewControls();
    });
    connect(checkBoxLoopBack, &QCheckBox::toggled, this, [this](bool) {
        syncActivePresetFromUi();
        stopDraftPreview();
        updatePreviewControls();
    });
    if (comboBoxPreset)
    {
        connect(comboBoxPreset, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
            if (_syncingPresetUi)
                return;
            loadPresetIntoUi(index);
        });
    }
    connect(comboBoxMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        syncActivePresetFromUi();
    });
    connect(sliderExplosion, &QSlider::valueChanged, this, [this](int) {
        syncActivePresetFromUi();
    });
    connect(doubleSpinBoxVectorX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        syncActivePresetFromUi();
    });
    connect(doubleSpinBoxVectorY, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        syncActivePresetFromUi();
    });
    connect(doubleSpinBoxVectorZ, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        syncActivePresetFromUi();
    });
    connect(comboBoxAnimationMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        syncActivePresetFromUi();
    });
    connect(doubleSpinBoxAnimationDuration, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        syncActivePresetFromUi();
    });

    initializeDefaultPreset();
    updateAuthoringModeUi();
    updatePreviewControls();
    updateCaptureButton();
}

void ExplodedViewPanel::setSceneGraph(SceneGraph* sg)
{
    stopDraftPreview();
    _sceneGraph = sg;
    updatePreviewControls();
}

void ExplodedViewPanel::applyContrastTheme(const QColor& textColor)
{
    setStyleSheet(QString("color: %1;").arg(textColor.name()));
}

void ExplodedViewPanel::deactivateInteractiveState()
{
    stopDraftPreview();
    cancelPickingMode();
    {
        QSignalBlocker b1(pushButtonSelectAssembly);
        QSignalBlocker b2(pushButtonSelectAnchor);
        pushButtonSelectAssembly->setChecked(false);
        pushButtonSelectAnchor->setChecked(false);
    }
    updateAssemblyPickButtonVisual(false);
    updateCaptureButton();
}

void ExplodedViewPanel::captureCurrentSelection()
{
    if (!_glWidget)
        return;
    const QList<int> ids = _glWidget->getSelectionManager()->getSelectedIds();
    if (!ids.isEmpty())
        applyAssemblySelection(ids);
}

// ---------------------------------------------------------------------------
// Picking mode
// ---------------------------------------------------------------------------
void ExplodedViewPanel::on_pushButtonSelectAssembly_toggled(bool checked)
{
    if (checked) {
        {
            QSignalBlocker b(pushButtonSelectAnchor);
            pushButtonSelectAnchor->setChecked(false);
        }
        cancelPickingMode();

        _pickingTarget = PickingTarget::Assembly;
        lineEditAssembly->setPlaceholderText(tr("Adjust selection, then click again to confirm..."));
        updateAssemblyPickButtonVisual(true);

        _pickingConn = connect(_glWidget, &GLWidget::selectionChanged,
                               this, &ExplodedViewPanel::onPickingSelectionChanged);
    } else {
        const bool committingSelection = (_pickingTarget == PickingTarget::Assembly);
        cancelPickingMode();
        lineEditAssembly->setPlaceholderText(tr("Select assembly or meshes..."));
        updateAssemblyPickButtonVisual(false);

        if (committingSelection)
        {
            applyAssemblySelection(_glWidget->getSelectionManager()->getSelectedIds());
            emit selectionClearRequested();
            updateCaptureButton();
        }
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
        updateAssemblyPickButtonVisual(false);

        _pickingTarget = PickingTarget::Anchor;
        lineEditAnchor->setPlaceholderText(tr("Click mesh or node in scene..."));

        _pickingConn = connect(_glWidget, &GLWidget::selectionChanged,
                               this, &ExplodedViewPanel::onPickingSelectionChanged);
    } else {
        cancelPickingMode();
        lineEditAnchor->setPlaceholderText(tr("Select anchor mesh (optional)..."));
    }
}

void ExplodedViewPanel::onPickingSelectionChanged(const QList<int>& ids)
{
    if (_pickingTarget == PickingTarget::Assembly)
    {
        updateAssemblySelectionDisplay(ids);
        updateCaptureButton();
        return;
    }

    if (ids.isEmpty())
        return;

    disconnect(_pickingConn);

    if (_pickingTarget == PickingTarget::Anchor)
    {
        applyAnchorSelection(ids);
        pushButtonSelectAnchor->setChecked(false);
        lineEditAnchor->setPlaceholderText(tr("Select anchor mesh (optional)..."));
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
// Selection helpers
// ---------------------------------------------------------------------------
void ExplodedViewPanel::applyAssemblySelection(const QList<int>& ids)
{
    if (ids.isEmpty())
    {
        clearAssemblySelection();
        emit explosionParametersChanged();
        return;
    }

    ExplodedViewPreset& preset = ensureActivePreset();
    preset.assemblyUuids.clear();
    for (int id : ids)
        preset.assemblyUuids.insert(_glWidget->getUuidByIndex(id));

    updateAssemblySelectionDisplay(ids);

    if (!preset.anchorUuid.isNull() && !preset.assemblyUuids.contains(preset.anchorUuid)) {
        lineEditAnchor->clear();
        preset.anchorUuid = QUuid();
    }

    emit explosionParametersChanged();
}

void ExplodedViewPanel::updateAssemblySelectionDisplay(const QList<int>& ids)
{
    if (ids.isEmpty())
    {
        lineEditAssembly->clear();
        if (lineEditManualSelection)
            lineEditManualSelection->clear();
        return;
    }

    const QString selectionText = describeAssemblySelection(ids);
    lineEditAssembly->setText(selectionText);
    if (lineEditManualSelection)
        lineEditManualSelection->setText(selectionText);
}

void ExplodedViewPanel::applyAnchorSelection(const QList<int>& ids)
{
    const QUuid uuid = _glWidget->getUuidByIndex(ids.first());

    ExplodedViewPreset& preset = ensureActivePreset();
    if (!preset.assemblyUuids.isEmpty() && !preset.assemblyUuids.contains(uuid)) {
        lineEditAnchor->setPlaceholderText(tr("Anchor must be within the assembly"));
        return;
    }

    preset.anchorUuid = uuid;

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

    QSet<const SceneNode*> owningNodes;
    for (int id : ids) {
        const QUuid uuid = _glWidget->getUuidByIndex(id);
        const SceneNode* node = _sceneGraph->findNodeForMesh(uuid);
        if (node)
            owningNodes.insert(node);
    }

    if (owningNodes.isEmpty())
        return tr("%1 meshes").arg(ids.size());

    if (owningNodes.size() == 1)
        return (*owningNodes.begin())->name;

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
// Capture / create animation
// ---------------------------------------------------------------------------
void ExplodedViewPanel::on_pushButtonCaptureView_clicked()
{
    stopDraftPreview();
    if (captureCurrentExplosionStep())
    {
        updateCaptureButton();
        updatePreviewControls();
    }
}

void ExplodedViewPanel::on_pushButtonRemoveCapture_clicked()
{
    stopDraftPreview();
    if (!listWidgetCapturedViews)
        return;

    const int row = listWidgetCapturedViews->currentRow();
    QVector<CapturedExplosionStep>& capturedSteps = activeCapturedSteps();
    if (row < 0 || row >= capturedSteps.size())
        return;

    capturedSteps.removeAt(row);
    updateCapturedViewsList();
    if (listWidgetCapturedViews && !capturedSteps.isEmpty())
        listWidgetCapturedViews->setCurrentRow(std::min<int>(row, capturedSteps.size() - 1));
    updateCaptureButton();
    updatePreviewControls();
}

void ExplodedViewPanel::on_pushButtonCapture_clicked()
{
    stopDraftPreview();
    createAnimationsFromCapturedSteps();
}

bool ExplodedViewPanel::captureCurrentExplosionStep()
{
    const bool manualMode = radioButtonModeManual && radioButtonModeManual->isChecked();
    const QSet<QUuid> captureUuids = manualMode && _glWidget
        ? _glWidget->explodedViewManualPlacementUuids()
        : assemblyUuids();

    if (!_glWidget || !_sceneGraph || captureUuids.isEmpty())
        return false;

    ExplodedViewPreset& preset = ensureActivePreset();
    const SceneGraphWorldTransforms worlds = _sceneGraph->evaluateWorldTransforms();
    CapturedExplosionStep step;
    step.id = QUuid::createUuid();
    step.name = tr("Step %1").arg(preset.capturedStepCounter++);
    QHash<QUuid, CapturedTransformTrack> aggregatedTracksByNode;
    QHash<QUuid, QVector<LocalNodeTransform>> endTransformsByNode;
    QHash<QUuid, int> selectedMeshCountByNode;
    QHash<QUuid, int> totalMeshCountByNode;

    for (const QUuid& meshUuid : captureUuids)
    {
        TriangleMesh* mesh = _glWidget->getMeshByUuid(meshUuid);
        SceneNode* ownerNode = _sceneGraph->findNodeForMesh(meshUuid);
        if (!mesh || !ownerNode)
            continue;

        const QVector3D worldOffset = mesh->explosionOffset();
        const TransformState meshState(
            mesh->getTranslation(),
            mesh->getRotation(),
            mesh->getScaling(),
            mesh->getRotationQuaternion());
        const bool hasManualTranslation = meshState.translation.lengthSquared() > 1.0e-8f;
        const QQuaternion identityRotation(1.0f, 0.0f, 0.0f, 0.0f);
        const bool hasManualRotation = meshState.hasExactRotation
            ? !rotationsNearlyEqual(meshState.rotationQuat, identityRotation)
            : meshState.rotation.lengthSquared() > 1.0e-8f;
        if (worldOffset.lengthSquared() < 1e-8f && !hasManualTranslation && !hasManualRotation)
            continue;

        QMatrix4x4 parentWorld;
        parentWorld.setToIdentity();
        if (ownerNode->parent && worlds.nodeWorldByUuid.contains(ownerNode->parent->nodeUuid))
            parentWorld = worlds.nodeWorldByUuid.value(ownerNode->parent->nodeUuid);

        bool invertible = false;
        const QMatrix4x4 parentWorldInv = parentWorld.inverted(&invertible);
        if (!invertible)
            continue;

        const LocalNodeTransform startTransform = decomposeLocalNodeTransform(aiToQMatrix(ownerNode->localTransform));
        const QMatrix4x4 currentWorld = mesh->combinedRenderTransform();
        const LocalNodeTransform endTransform = decomposeLocalNodeTransform(parentWorldInv * currentWorld);

        CapturedTransformTrack track;
        track.meshUuid = meshUuid;
        track.ownerNodeUuid = ownerNode->nodeUuid;
        track.sourceFile = mesh->getSourceFile().trimmed();
        if (track.sourceFile.isEmpty())
            track.sourceFile = sourceFileForNode(ownerNode);
        track.targetNodeName = ownerNode->name;
        track.startPosition = startTransform.translation;
        track.endPosition = startTransform.translation;
        track.startRotation = startTransform.rotation;
        track.endRotation = startTransform.rotation;

        if (!aggregatedTracksByNode.contains(ownerNode->nodeUuid))
        {
            aggregatedTracksByNode.insert(ownerNode->nodeUuid, track);
            totalMeshCountByNode.insert(ownerNode->nodeUuid, ownerNode->meshUuids.size());
        }
        endTransformsByNode[ownerNode->nodeUuid].append(endTransform);
        selectedMeshCountByNode[ownerNode->nodeUuid] = selectedMeshCountByNode.value(ownerNode->nodeUuid) + 1;
    }

    for (auto it = aggregatedTracksByNode.begin(); it != aggregatedTracksByNode.end(); ++it)
    {
        const QVector<LocalNodeTransform> endTransforms = endTransformsByNode.value(it.key());
        const int count = endTransforms.size();
        if (count <= 0)
            continue;

        const int selectedCount = selectedMeshCountByNode.value(it.key(), 0);
        const int totalCount = totalMeshCountByNode.value(it.key(), selectedCount);
        if (selectedCount != totalCount)
        {
            qWarning() << "[ExplodedView] Skipping animation capture for node"
                       << it->targetNodeName
                       << "because only" << selectedCount << "of" << totalCount
                       << "direct meshes are part of the exploded selection.";
            continue;
        }

        const LocalNodeTransform& referenceTransform = endTransforms.front();
        bool transformsCompatible = true;
        for (const LocalNodeTransform& endTransform : endTransforms)
        {
            if ((endTransform.translation - referenceTransform.translation).lengthSquared() > kExplosionOffsetTolerance
                || !rotationsNearlyEqual(endTransform.rotation, referenceTransform.rotation))
            {
                transformsCompatible = false;
                break;
            }
        }

        if (!transformsCompatible)
        {
            qWarning() << "[ExplodedView] Skipping animation capture for node"
                       << it->targetNodeName
                       << "because its meshes resolve to different node transforms and cannot"
                       << "be represented by one node animation track.";
            continue;
        }

        it->endPosition = referenceTransform.translation;
        it->endRotation = referenceTransform.rotation;
        step.tracks.append(it.value());
    }

    if (step.tracks.isEmpty())
        return false;

    preset.capturedSteps.append(step);
    updateCapturedViewsList();
    if (listWidgetCapturedViews)
        listWidgetCapturedViews->setCurrentRow(preset.capturedSteps.size() - 1);
    return true;
}

bool ExplodedViewPanel::createAnimationsFromCapturedSteps()
{
    const QVector<CapturedExplosionStep>& capturedSteps = activeCapturedSteps();
    if (!_sceneGraph || capturedSteps.isEmpty())
        return false;

    const ExplodedViewPreset& preset = ensureActivePreset();
    enum class OutputMode { ParallelSingle, SequentialSingle, Separate };
    const OutputMode mode = (preset.outputMode == 1)
        ? OutputMode::SequentialSingle
        : (preset.outputMode == 2)
            ? OutputMode::Separate
            : OutputMode::ParallelSingle;

    const double durationSeconds = preset.durationSeconds;
    const bool loopBack = preset.loopBack;

    // Build one node-index map per source file. If a file has no persisted node
    // bindings yet, synthesize them from the current SceneGraph hierarchy.
    QHash<QString, GltfAnimationData> animationDataByFile;
    QHash<QString, QHash<QUuid, int>> nodeIndexByUuidByFile;

    QSet<QString> involvedFiles;
    for (const CapturedExplosionStep& step : capturedSteps)
        for (const CapturedTransformTrack& track : step.tracks)
            if (!track.sourceFile.isEmpty())
                involvedFiles.insert(track.sourceFile);

    for (const QString& sourceFile : involvedFiles)
    {
        GltfAnimationData data = _sceneGraph->animationDataForFile(sourceFile);
        if (data.sourceFile.isEmpty())
            data.sourceFile = sourceFile;

        QHash<QUuid, int> nodeIndexByUuid;
        if (data.nodeBindings.isEmpty())
        {
            SceneNode* fileNode = _sceneGraph->findFileNode(sourceFile);
            if (fileNode)
            {
                if (fileNode->isSynthetic && !fileNode->children.isEmpty())
                {
                    SceneNode* aiRootNode = fileNode->children.first();
                    collectNodeBindings(aiRootNode, data.nodeBindings, nodeIndexByUuid);
                }
                else
                {
                    collectNodeBindings(fileNode, data.nodeBindings, nodeIndexByUuid);
                }
            }
        }
        else
        {
            SceneNode* fileNode = _sceneGraph->findFileNode(sourceFile);
            if (fileNode)
            {
                SceneNode* aiRootNode = fileNode->children.isEmpty() ? nullptr : fileNode->children.first();
                for (const GltfAnimationNodeBinding& binding : std::as_const(data.nodeBindings))
                {
                    SceneNode* targetNode = nullptr;
                    if (binding.hasAiChildPath)
                        targetNode = findSceneNodeByAiChildPath(aiRootNode, binding.aiChildPath);

                    if (!targetNode && !binding.nodeName.isEmpty())
                    {
                        std::function<SceneNode*(SceneNode*)> findByName = [&](SceneNode* node) -> SceneNode*
                        {
                            if (!node)
                                return nullptr;
                            if (node->name == binding.nodeName)
                                return node;
                            for (SceneNode* child : node->children)
                            {
                                if (SceneNode* match = findByName(child))
                                    return match;
                            }
                            return nullptr;
                        };

                        for (SceneNode* child : fileNode->children)
                        {
                            targetNode = findByName(child);
                            if (targetNode)
                                break;
                        }
                    }

                    if (targetNode && binding.nodeIndex >= 0 && !nodeIndexByUuid.contains(targetNode->nodeUuid))
                        nodeIndexByUuid.insert(targetNode->nodeUuid, binding.nodeIndex);
                }
            }
        }

        animationDataByFile.insert(sourceFile, data);
        nodeIndexByUuidByFile.insert(sourceFile, nodeIndexByUuid);
    }

    auto makeChannel = [&](const CapturedTransformTrack& track,
                           int nodeIndex,
                           const QVector<GltfAnimationVec3Key>& keys) {
        GltfAnimationChannel channel;
        channel.targetPath = GltfAnimationTargetPath::Translation;
        channel.targetNodeName = track.targetNodeName;
        channel.targetNodeIndex = nodeIndex;
        channel.vec3Keys = keys;
        return channel;
    };

    auto makeRotationChannel = [&](const CapturedTransformTrack& track,
                                   int nodeIndex,
                                   const QVector<GltfAnimationQuatKey>& keys) {
        GltfAnimationChannel channel;
        channel.targetPath = GltfAnimationTargetPath::Rotation;
        channel.targetNodeName = track.targetNodeName;
        channel.targetNodeIndex = nodeIndex;
        channel.quatKeys = keys;
        return channel;
    };

    auto makeForwardKeys = [&](const QVector3D& startPosition,
                               const QVector3D& endPosition) {
        QVector<GltfAnimationVec3Key> keys;
        keys.append({0.0, startPosition});
        keys.append({durationSeconds, endPosition});
        if (loopBack)
            keys.append({durationSeconds * 2.0, startPosition});
        return keys;
    };

    auto makeForwardRotationKeys = [&](const QQuaternion& startRotation,
                                       const QQuaternion& endRotation) {
        QVector<GltfAnimationQuatKey> keys;
        keys.append({0.0, startRotation});
        keys.append({durationSeconds, endRotation});
        if (loopBack)
            keys.append({durationSeconds * 2.0, startRotation});
        return keys;
    };

    const auto makeClipName = [&](const QString& suffix = QString()) {
        const QString base = tr("Exploded View %1").arg(_createdAnimationCounter++);
        return suffix.isEmpty() ? base : QStringLiteral("%1 - %2").arg(base, suffix);
    };

    if (mode == OutputMode::ParallelSingle)
    {
        QHash<QString, QHash<QUuid, CapturedTransformTrack>> finalTracksByFile;
        for (const CapturedExplosionStep& step : capturedSteps)
        {
            for (const CapturedTransformTrack& track : step.tracks)
            {
                if (!track.sourceFile.isEmpty())
                    finalTracksByFile[track.sourceFile].insert(track.ownerNodeUuid, track);
            }
        }

        QString firstActivatedFile;
        int firstActivatedClip = -1;

        for (auto fileIt = finalTracksByFile.cbegin(); fileIt != finalTracksByFile.cend(); ++fileIt)
        {
            GltfAnimationData data = animationDataByFile.value(fileIt.key());
            GltfAnimationClip clip;
            clip.name = makeClipName(tr("Parallel"));
            clip.durationSeconds = loopBack ? durationSeconds * 2.0 : durationSeconds;
            clip.hasNodeTransforms = true;

            const QHash<QUuid, int> nodeIndexByUuid = nodeIndexByUuidByFile.value(fileIt.key());
            for (const CapturedTransformTrack& track : fileIt.value())
            {
                const int nodeIndex = nodeIndexByUuid.value(track.ownerNodeUuid, track.targetNodeIndex);
                clip.channels.append(makeChannel(track,
                    nodeIndex,
                    makeForwardKeys(track.startPosition, track.endPosition)));
                if (!rotationsNearlyEqual(track.startRotation, track.endRotation))
                {
                    clip.channels.append(makeRotationChannel(track,
                        nodeIndex,
                        makeForwardRotationKeys(track.startRotation, track.endRotation)));
                }
            }

            if (clip.channels.isEmpty())
                continue;

            data.clips.append(clip);
            data.hasNodeAnimations = true;
            _sceneGraph->setAnimationData(fileIt.key(), data);
            _sceneGraph->setActiveAnimationClip(fileIt.key(), data.clips.size() - 1);

            if (firstActivatedFile.isEmpty())
            {
                firstActivatedFile = fileIt.key();
                firstActivatedClip = data.clips.size() - 1;
            }
        }

        if (!firstActivatedFile.isEmpty())
            _glWidget->setActiveAnimation(firstActivatedFile, firstActivatedClip);
        return true;
    }

    if (mode == OutputMode::Separate)
    {
        QString firstActivatedFile;
        int firstActivatedClip = -1;

        for (const CapturedExplosionStep& step : capturedSteps)
        {
            QHash<QString, QHash<QUuid, CapturedTransformTrack>> stepTracksByFile;
            for (const CapturedTransformTrack& track : step.tracks)
            {
                if (!track.sourceFile.isEmpty())
                    stepTracksByFile[track.sourceFile].insert(track.ownerNodeUuid, track);
            }

            for (auto fileIt = stepTracksByFile.cbegin(); fileIt != stepTracksByFile.cend(); ++fileIt)
            {
                GltfAnimationData data = _sceneGraph->animationDataForFile(fileIt.key());
                if (data.sourceFile.isEmpty())
                    data.sourceFile = fileIt.key();
                if (data.nodeBindings.isEmpty())
                    data = animationDataByFile.value(fileIt.key(), data);

                GltfAnimationClip clip;
                clip.name = step.name;
                clip.durationSeconds = loopBack ? durationSeconds * 2.0 : durationSeconds;
                clip.hasNodeTransforms = true;

                const QHash<QUuid, int> nodeIndexByUuid = nodeIndexByUuidByFile.value(fileIt.key());
                for (const CapturedTransformTrack& track : fileIt.value())
                {
                    const int nodeIndex = nodeIndexByUuid.value(track.ownerNodeUuid, track.targetNodeIndex);
                    clip.channels.append(makeChannel(track,
                        nodeIndex,
                        makeForwardKeys(track.startPosition, track.endPosition)));
                    if (!rotationsNearlyEqual(track.startRotation, track.endRotation))
                    {
                        clip.channels.append(makeRotationChannel(track,
                            nodeIndex,
                            makeForwardRotationKeys(track.startRotation, track.endRotation)));
                    }
                }

                if (clip.channels.isEmpty())
                    continue;

                data.clips.append(clip);
                data.hasNodeAnimations = true;
                _sceneGraph->setAnimationData(fileIt.key(), data);
                _sceneGraph->setActiveAnimationClip(fileIt.key(), data.clips.size() - 1);

                if (firstActivatedFile.isEmpty())
                {
                    firstActivatedFile = fileIt.key();
                    firstActivatedClip = data.clips.size() - 1;
                }
            }
        }

        if (!firstActivatedFile.isEmpty())
            _glWidget->setActiveAnimation(firstActivatedFile, firstActivatedClip);
        return true;
    }

    // Sequential single animation: sample the cumulative state after each step.
    QString firstActivatedFile;
    int firstActivatedClip = -1;

    for (const QString& sourceFile : involvedFiles)
    {
        struct NodeState
        {
            CapturedTransformTrack info;
            QVector3D base;
            QVector3D current;
            QQuaternion baseRotation;
            QQuaternion currentRotation;
            QVector<GltfAnimationVec3Key> keys;
            QVector<GltfAnimationQuatKey> rotationKeys;
        };

        QHash<QUuid, NodeState> states;
        for (const CapturedExplosionStep& step : capturedSteps)
        {
            for (const CapturedTransformTrack& track : step.tracks)
            {
                if (track.sourceFile != sourceFile)
                    continue;

                auto it = states.find(track.ownerNodeUuid);
                if (it == states.end())
                {
                    NodeState state;
                    state.info = track;
                    state.base = track.startPosition;
                    state.current = track.startPosition;
                    state.baseRotation = track.startRotation;
                    state.currentRotation = track.startRotation;
                    states.insert(track.ownerNodeUuid, state);
                }
                else
                {
                    it->info = track;
                }
            }
        }

        if (states.isEmpty())
            continue;

        for (auto it = states.begin(); it != states.end(); ++it)
        {
            it->keys.append({0.0, it->base});
            it->rotationKeys.append({0.0, it->baseRotation});
        }

        const qsizetype stepCount = std::max<qsizetype>(1, capturedSteps.size());
        const double segmentDuration = durationSeconds / static_cast<double>(stepCount);
        QVector<QHash<QUuid, QVector3D>> forwardSnapshots;
        QVector<QHash<QUuid, QQuaternion>> forwardRotationSnapshots;
        forwardSnapshots.reserve(capturedSteps.size() + 1);
        forwardRotationSnapshots.reserve(capturedSteps.size() + 1);

        QHash<QUuid, QVector3D> initialSnapshot;
        QHash<QUuid, QQuaternion> initialRotationSnapshot;
        for (auto it = states.cbegin(); it != states.cend(); ++it)
        {
            initialSnapshot.insert(it.key(), it->base);
            initialRotationSnapshot.insert(it.key(), it->baseRotation);
        }
        forwardSnapshots.append(initialSnapshot);
        forwardRotationSnapshots.append(initialRotationSnapshot);

        for (int stepIndex = 0; stepIndex < capturedSteps.size(); ++stepIndex)
        {
            const CapturedExplosionStep& step = capturedSteps[stepIndex];
            for (const CapturedTransformTrack& track : step.tracks)
            {
                if (track.sourceFile != sourceFile)
                    continue;

                auto it = states.find(track.ownerNodeUuid);
                if (it != states.end())
                {
                    it->info = track;
                    it->current = track.endPosition;
                    it->currentRotation = track.endRotation;
                }
            }

            const double t = segmentDuration * static_cast<double>(stepIndex + 1);
            for (auto it = states.begin(); it != states.end(); ++it) {
                it->keys.append({t, it->current});
                it->rotationKeys.append({t, it->currentRotation});
            }

            QHash<QUuid, QVector3D> snapshot;
            QHash<QUuid, QQuaternion> rotationSnapshot;
            for (auto it = states.cbegin(); it != states.cend(); ++it)
            {
                snapshot.insert(it.key(), it->current);
                rotationSnapshot.insert(it.key(), it->currentRotation);
            }
            forwardSnapshots.append(snapshot);
            forwardRotationSnapshots.append(rotationSnapshot);
        }

        if (loopBack)
        {
            for (qsizetype reverseStep = stepCount - 1; reverseStep >= 0; --reverseStep)
            {
                const double t = durationSeconds + segmentDuration * static_cast<double>(stepCount - reverseStep);
                const QHash<QUuid, QVector3D>& snapshot = forwardSnapshots[reverseStep];
                const QHash<QUuid, QQuaternion>& rotationSnapshot = forwardRotationSnapshots[reverseStep];
                for (auto it = states.begin(); it != states.end(); ++it)
                {
                    const QVector3D position = snapshot.value(it.key(), it->base);
                    it->keys.append({t, position});
                    const QQuaternion rotation = rotationSnapshot.value(it.key(), it->baseRotation);
                    it->rotationKeys.append({t, rotation});
                }
            }
        }

        GltfAnimationData data = _sceneGraph->animationDataForFile(sourceFile);
        if (data.sourceFile.isEmpty())
            data.sourceFile = sourceFile;
        if (data.nodeBindings.isEmpty())
            data = animationDataByFile.value(sourceFile, data);

        GltfAnimationClip clip;
        clip.name = makeClipName(tr("Sequential"));
        clip.durationSeconds = loopBack ? durationSeconds * 2.0 : durationSeconds;
        clip.hasNodeTransforms = true;

        const QHash<QUuid, int> nodeIndexByUuid = nodeIndexByUuidByFile.value(sourceFile);
        for (auto it = states.cbegin(); it != states.cend(); ++it)
        {
            const int nodeIndex = nodeIndexByUuid.value(it.key(), it->info.targetNodeIndex);
            clip.channels.append(makeChannel(it->info, nodeIndex, it->keys));
            bool hasRotationAnimation = false;
            for (const GltfAnimationQuatKey& key : it->rotationKeys)
            {
                if (!rotationsNearlyEqual(it->baseRotation, key.value))
                {
                    hasRotationAnimation = true;
                    break;
                }
            }
            if (hasRotationAnimation)
                clip.channels.append(makeRotationChannel(it->info, nodeIndex, it->rotationKeys));
        }

        if (clip.channels.isEmpty())
            continue;

        data.clips.append(clip);
        data.hasNodeAnimations = true;
        _sceneGraph->setAnimationData(sourceFile, data);
        _sceneGraph->setActiveAnimationClip(sourceFile, data.clips.size() - 1);

        if (firstActivatedFile.isEmpty())
        {
            firstActivatedFile = sourceFile;
            firstActivatedClip = data.clips.size() - 1;
        }
    }

    if (!firstActivatedFile.isEmpty())
        _glWidget->setActiveAnimation(firstActivatedFile, firstActivatedClip);
    return !firstActivatedFile.isEmpty();
}

// ---------------------------------------------------------------------------
// UI state
// ---------------------------------------------------------------------------
void ExplodedViewPanel::on_comboBoxMode_currentIndexChanged(int index)
{
    const bool autoMode = !radioButtonModeManual || !radioButtonModeManual->isChecked();
    frameVector->setVisible(autoMode && index == 4);
    syncActivePresetFromUi();
    emit explosionParametersChanged();
}

void ExplodedViewPanel::on_sliderExplosion_valueChanged(int value)
{
    labelDistancePercent->setText(QString("%1%").arg(value));
    syncActivePresetFromUi();
    updateCaptureButton();
    emit explosionParametersChanged();
}

void ExplodedViewPanel::on_pushButtonReset_clicked()
{
    const ExplodedViewPreset* preset = activePreset();
    const bool hasSelection = preset && (!preset->assemblyUuids.isEmpty() || !preset->anchorUuid.isNull());
    const bool hasNonDefaultExplodeState = comboBoxMode
        && sliderExplosion
        && doubleSpinBoxVectorX
        && doubleSpinBoxVectorY
        && doubleSpinBoxVectorZ
        && checkBoxLoopBack
        && comboBoxAnimationMode
        && doubleSpinBoxAnimationDuration
        && (comboBoxMode->currentIndex() != 0
            || sliderExplosion->value() != 100
            || !qFuzzyCompare(doubleSpinBoxVectorX->value(), 1.0)
            || !qFuzzyCompare(doubleSpinBoxVectorY->value(), 0.0)
            || !qFuzzyCompare(doubleSpinBoxVectorZ->value(), 0.0)
            || comboBoxAnimationMode->currentIndex() != 0
            || !qFuzzyCompare(doubleSpinBoxAnimationDuration->value(), 3.0)
            || !checkBoxLoopBack->isChecked());
    const bool hasCapturedSteps = preset && !preset->capturedSteps.isEmpty();
    const bool hasManualPlacement = _glWidget
        && (_glWidget->isExplodedViewManualPlacementActive()
            || _glWidget->hasExplodedViewManualPlacement()
            || _glWidget->hasExplodedViewManualTransformChanges());

    if (hasSelection || hasNonDefaultExplodeState || hasCapturedSteps || hasManualPlacement)
    {
        const auto response = QMessageBox::warning(
            window(),
            tr("Reset Exploded View"),
            tr("This will clear the current exploded-view selection, settings, manual placement, and captured steps for the active preset. Continue?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (response != QMessageBox::Yes)
            return;
    }

    stopDraftPreview();
    cancelPickingMode();
    if (_glWidget)
        _glWidget->clearExplodedViewManualPlacement();

    QSignalBlocker b1(comboBoxMode);
    QSignalBlocker b2(sliderExplosion);
    QSignalBlocker b3(doubleSpinBoxVectorX);
    QSignalBlocker b4(doubleSpinBoxVectorY);
    QSignalBlocker b5(doubleSpinBoxVectorZ);

    clearAssemblySelection();

    {
        QSignalBlocker b6(pushButtonSelectAssembly);
        QSignalBlocker b7(pushButtonSelectAnchor);
        pushButtonSelectAssembly->setChecked(false);
        pushButtonSelectAnchor->setChecked(false);
    }
    updateAssemblyPickButtonVisual(false);
    lineEditAssembly->setPlaceholderText(tr("Select assembly or meshes..."));
    lineEditAnchor->setPlaceholderText(tr("Select anchor mesh (optional)..."));

    comboBoxMode->setCurrentIndex(0);
    frameVector->setVisible(false);

    sliderExplosion->setValue(100);
    labelDistancePercent->setText("100%");

    doubleSpinBoxVectorX->setValue(1.0);
    doubleSpinBoxVectorY->setValue(0.0);
    doubleSpinBoxVectorZ->setValue(0.0);
    ExplodedViewPreset& active = ensureActivePreset();
    active.mode = ExplodedViewManager::Mode::Auto;
    active.userVector = QVector3D(1.0f, 0.0f, 0.0f);
    active.factor = 1.0f;
    active.capturedSteps.clear();
    active.capturedStepCounter = 1;
    active.outputMode = 0;
    active.durationSeconds = 3.0;
    active.loopBack = true;
    comboBoxAnimationMode->setCurrentIndex(active.outputMode);
    doubleSpinBoxAnimationDuration->setValue(active.durationSeconds);
    checkBoxLoopBack->setChecked(active.loopBack);
    if (listWidgetCapturedViews)
        listWidgetCapturedViews->clear();

    updateCaptureButton();
    updateManualPlacementUi();
    updatePreviewControls();
    emit explosionParametersChanged();
    emit selectionClearRequested();
}

void ExplodedViewPanel::updateCaptureButton()
{
    const bool hasAssembly = !assemblyUuids().isEmpty();
    const bool manualMode = radioButtonModeManual && radioButtonModeManual->isChecked();
    const bool hasManualCaptureSet = _glWidget && _glWidget->hasExplodedViewManualTransformChanges();
    const bool canCaptureExplosion = manualMode
        ? hasManualCaptureSet
        : (hasAssembly && sliderExplosion->value() >= 10);

    if (pushButtonCaptureView)
        pushButtonCaptureView->setEnabled(canCaptureExplosion);
    if (pushButtonCapture)
        pushButtonCapture->setEnabled(!activeCapturedSteps().isEmpty());
    if (pushButtonRemoveCapture)
        pushButtonRemoveCapture->setEnabled(listWidgetCapturedViews
            && listWidgetCapturedViews->currentRow() >= 0);
}

void ExplodedViewPanel::updateAuthoringModeUi()
{
    stopDraftPreview();
    const bool autoMode = !radioButtonModeManual || !radioButtonModeManual->isChecked();

    if (!autoMode)
    {
        cancelPickingMode();
        {
            QSignalBlocker b1(pushButtonSelectAssembly);
            QSignalBlocker b2(pushButtonSelectAnchor);
            pushButtonSelectAssembly->setChecked(false);
            pushButtonSelectAnchor->setChecked(false);
        }
        updateAssemblyPickButtonVisual(false);
    }
    else if (_glWidget && _glWidget->isExplodedViewManualPlacementActive())
    {
        _glWidget->finishExplodedViewManualPlacement();
    }

    if (stackedWidgetAuthoring)
        stackedWidgetAuthoring->setCurrentWidget(autoMode ? pageAutoExplode : pageManualPlacement);

    if (frameVector)
        frameVector->setVisible(autoMode && comboBoxMode && comboBoxMode->currentIndex() == 4);

    updateManualPlacementUi();
    updateCaptureButton();
}

void ExplodedViewPanel::updateManualPlacementUi()
{
    if (!pushButtonStartManualPlacement || !_glWidget)
        return;

    const bool manualMode = radioButtonModeManual && radioButtonModeManual->isChecked();
    const bool active = _glWidget->isExplodedViewManualPlacementActive();
    const bool hasPlacement = _glWidget->hasExplodedViewManualPlacement();

    pushButtonStartManualPlacement->setEnabled(manualMode && !active);
    pushButtonFinishManualPlacement->setEnabled(manualMode && active);
    pushButtonClearManualPlacement->setEnabled(manualMode && hasPlacement);

    if (labelManualPlacementStatus)
    {
        if (active)
            labelManualPlacementStatus->setText(tr("Manual placement is active. Translate or rotate the selected meshes to refine the staged exploded pose."));
        else if (hasPlacement)
            labelManualPlacementStatus->setText(tr("Manual placement is staged. You can resume placement for another selection or capture the current exploded pose."));
        else
            labelManualPlacementStatus->setText(tr("Manual placement will reuse the transform gizmo to stage exploded poses without changing the real model transform."));
    }
}

double ExplodedViewPanel::currentDraftPreviewDuration() const
{
    const ExplodedViewPreset* preset = activePreset();
    if (!preset || preset->capturedSteps.isEmpty())
        return 0.0;

    const double durationSeconds = preset->durationSeconds;
    const bool loopBack = preset->loopBack;

    return loopBack ? durationSeconds * 2.0 : durationSeconds;
}

bool ExplodedViewPanel::ensureDraftPreviewSession()
{
    const QVector<CapturedExplosionStep>& capturedSteps = activeCapturedSteps();
    if (_draftPreviewActive || !_glWidget || !_sceneGraph || capturedSteps.isEmpty())
        return _draftPreviewActive;

    _draftPreviewFiles.clear();
    for (const CapturedExplosionStep& step : capturedSteps)
    {
        for (const CapturedTransformTrack& track : step.tracks)
        {
            if (!track.sourceFile.isEmpty())
                _draftPreviewFiles.insert(track.sourceFile);
        }
    }

    if (_draftPreviewFiles.isEmpty())
        return false;

    _draftPreviewMeshStates.clear();
    for (const QString& sourceFile : std::as_const(_draftPreviewFiles))
    {
        SceneNode* fileNode = _sceneGraph->findFileNode(sourceFile);
        if (!fileNode)
            continue;

        const QList<QUuid> meshUuids = _sceneGraph->collectMeshUuids(fileNode);
        for (const QUuid& meshUuid : meshUuids)
        {
            TriangleMesh* mesh = _glWidget->getMeshByUuid(meshUuid);
            if (!mesh || _draftPreviewMeshStates.contains(meshUuid))
                continue;

            PreviewMeshState state;
            state.sceneRenderTransform = mesh->getSceneRenderTransform();
            state.explosionOffset = mesh->explosionOffset();
            state.translation = mesh->getTranslation();
            state.rotation = mesh->getRotation();
            state.scale = mesh->getScaling();
            state.rotationQuat = mesh->getRotationQuaternion();
            state.hasExactRotation = true;
            _draftPreviewMeshStates.insert(meshUuid, state);
        }
    }

    _draftPreviewSuspendedAnimation = {};
    if (!_glWidget->activeAnimationFile().isEmpty() && _glWidget->activeAnimationClip() >= 0)
    {
        _draftPreviewSuspendedAnimation.valid = true;
        _draftPreviewSuspendedAnimation.sourceFile = _glWidget->activeAnimationFile();
        _draftPreviewSuspendedAnimation.clipIndex = _glWidget->activeAnimationClip();
        _draftPreviewSuspendedAnimation.timeSeconds = _glWidget->currentAnimationTimeSeconds();
        _draftPreviewSuspendedAnimation.wasPlaying = _glWidget->isAnimationPlaying();
        _glWidget->setAnimationPlaying(false);
        _glWidget->seekAnimation(0.0);
    }

    const QQuaternion identityQuat(1.0f, 0.0f, 0.0f, 0.0f);
    for (auto it = _draftPreviewMeshStates.begin(); it != _draftPreviewMeshStates.end(); ++it)
    {
        if (TriangleMesh* mesh = _glWidget->getMeshByUuid(it.key()))
        {
            mesh->setExplosionOffset(QVector3D());
            mesh->setTranslation(QVector3D());
            mesh->setRotationQuaternion(identityQuat, QVector3D());
            mesh->setScaling(QVector3D(1.0f, 1.0f, 1.0f));
        }
    }

    _draftPreviewCurrentTime = 0.0;
    _draftPreviewActive = true;
    _draftPreviewPlaying = false;
    applyDraftPreviewPose(0.0);
    return true;
}

void ExplodedViewPanel::stopDraftPreview(bool restoreScene)
{
    if (_draftPreviewTimer)
        _draftPreviewTimer->stop();
    _draftPreviewPlaying = false;

    if (!_draftPreviewActive)
    {
        updatePreviewControls();
        return;
    }

    if (restoreScene && _glWidget)
    {
        for (auto it = _draftPreviewMeshStates.cbegin(); it != _draftPreviewMeshStates.cend(); ++it)
        {
            TriangleMesh* mesh = _glWidget->getMeshByUuid(it.key());
            if (!mesh)
                continue;

            const PreviewMeshState& state = it.value();
            mesh->setSceneRenderTransform(state.sceneRenderTransform);
            mesh->setExplosionOffset(state.explosionOffset);
            mesh->setTranslation(state.translation);
            if (state.hasExactRotation)
                mesh->setRotationQuaternion(state.rotationQuat, state.rotation);
            else
                mesh->setRotation(state.rotation);
            mesh->setScaling(state.scale);
        }

        if (_draftPreviewSuspendedAnimation.valid)
        {
            _glWidget->setActiveAnimation(_draftPreviewSuspendedAnimation.sourceFile,
                                          _draftPreviewSuspendedAnimation.clipIndex);
            _glWidget->seekAnimation(_draftPreviewSuspendedAnimation.timeSeconds);
            _glWidget->setAnimationPlaying(_draftPreviewSuspendedAnimation.wasPlaying);
        }

        _glWidget->update();
    }

    _draftPreviewActive = false;
    _draftPreviewCurrentTime = 0.0;
    _draftPreviewFiles.clear();
    _draftPreviewMeshStates.clear();
    _draftPreviewSuspendedAnimation = {};
    updatePreviewControls();
}

void ExplodedViewPanel::applyDraftPreviewPose(double timeSeconds)
{
    if (!_draftPreviewActive || !_glWidget || !_sceneGraph)
        return;

    const ExplodedViewPreset& preset = ensureActivePreset();
    const QVector<CapturedExplosionStep>& capturedSteps = activeCapturedSteps();
    const double durationSeconds = currentDraftPreviewDuration();
    if (durationSeconds <= 0.0)
        return;

    const double clampedTime = qBound(0.0, timeSeconds, durationSeconds);
    const double forwardDuration = preset.durationSeconds;
    const bool loopBack = preset.loopBack;

    enum class OutputMode { ParallelSingle, SequentialSingle, SeparateClips };
    const OutputMode mode = (preset.outputMode == 1)
        ? OutputMode::SequentialSingle
        : (preset.outputMode == 2)
            ? OutputMode::SeparateClips
            : OutputMode::ParallelSingle;

    struct SampledNodeState
    {
        QVector3D translation;
        QQuaternion rotation;
        QVector3D scale = QVector3D(1.0f, 1.0f, 1.0f);
    };

    QHash<QUuid, SampledNodeState> sampledByNode;

    if (mode == OutputMode::ParallelSingle)
    {
        QHash<QUuid, CapturedTransformTrack> finalTracksByNode;
        for (const CapturedExplosionStep& step : capturedSteps)
        {
            for (const CapturedTransformTrack& track : step.tracks)
                finalTracksByNode.insert(track.ownerNodeUuid, track);
        }

        double localTime = clampedTime;
        bool reversing = false;
        if (loopBack && localTime > forwardDuration)
        {
            localTime -= forwardDuration;
            reversing = true;
        }
        const double segmentDuration = qMax(1.0e-6, forwardDuration);
        const float t = static_cast<float>(qBound(0.0, localTime / segmentDuration, 1.0));

        for (auto it = finalTracksByNode.cbegin(); it != finalTracksByNode.cend(); ++it)
        {
            const CapturedTransformTrack& track = it.value();
            const SceneNode* node = _sceneGraph->findNodeByUuid(track.ownerNodeUuid);
            if (!node)
                continue;

            const LocalNodeTransform base = decomposeLocalNodeTransform(aiToQMatrix(node->localTransform));
            SampledNodeState state;
            state.scale = base.scale;
            if (!reversing)
            {
                state.translation = track.startPosition * (1.0f - t) + track.endPosition * t;
                state.rotation = QQuaternion::slerp(track.startRotation, track.endRotation, t).normalized();
            }
            else
            {
                state.translation = track.endPosition * (1.0f - t) + track.startPosition * t;
                state.rotation = QQuaternion::slerp(track.endRotation, track.startRotation, t).normalized();
            }
            sampledByNode.insert(track.ownerNodeUuid, state);
        }
    }
    else if (mode == OutputMode::SequentialSingle)
    {
        struct NodeState
        {
            QVector3D basePosition;
            QVector3D currentPosition;
            QQuaternion baseRotation;
            QQuaternion currentRotation;
            QVector3D scale = QVector3D(1.0f, 1.0f, 1.0f);
        };

        QHash<QUuid, NodeState> states;
        for (const CapturedExplosionStep& step : capturedSteps)
        {
            for (const CapturedTransformTrack& track : step.tracks)
            {
                if (states.contains(track.ownerNodeUuid))
                    continue;

                const SceneNode* node = _sceneGraph->findNodeByUuid(track.ownerNodeUuid);
                if (!node)
                    continue;

                const LocalNodeTransform base = decomposeLocalNodeTransform(aiToQMatrix(node->localTransform));
                NodeState state;
                state.basePosition = track.startPosition;
                state.currentPosition = track.startPosition;
                state.baseRotation = track.startRotation;
                state.currentRotation = track.startRotation;
                state.scale = base.scale;
                states.insert(track.ownerNodeUuid, state);
            }
        }

        const qsizetype stepCount = std::max<qsizetype>(1, capturedSteps.size());
        const double segmentDuration = forwardDuration / static_cast<double>(stepCount);

        QVector<QHash<QUuid, QVector3D>> positionSnapshots;
        QVector<QHash<QUuid, QQuaternion>> rotationSnapshots;
        positionSnapshots.reserve(capturedSteps.size() + 1);
        rotationSnapshots.reserve(capturedSteps.size() + 1);

        QHash<QUuid, QVector3D> initialPositions;
        QHash<QUuid, QQuaternion> initialRotations;
        for (auto it = states.cbegin(); it != states.cend(); ++it)
        {
            initialPositions.insert(it.key(), it->basePosition);
            initialRotations.insert(it.key(), it->baseRotation);
        }
        positionSnapshots.append(initialPositions);
        rotationSnapshots.append(initialRotations);

        for (const CapturedExplosionStep& step : capturedSteps)
        {
            for (const CapturedTransformTrack& track : step.tracks)
            {
                auto it = states.find(track.ownerNodeUuid);
                if (it == states.end())
                    continue;
                it->currentPosition = track.endPosition;
                it->currentRotation = track.endRotation;
            }

            QHash<QUuid, QVector3D> snapshotPositions;
            QHash<QUuid, QQuaternion> snapshotRotations;
            for (auto it = states.cbegin(); it != states.cend(); ++it)
            {
                snapshotPositions.insert(it.key(), it->currentPosition);
                snapshotRotations.insert(it.key(), it->currentRotation);
            }
            positionSnapshots.append(snapshotPositions);
            rotationSnapshots.append(snapshotRotations);
        }

        auto sampleSequential = [&](double sampleTime) {
            const double bounded = qBound(0.0, sampleTime, forwardDuration);
            int segmentIndex = segmentDuration > 1.0e-6
                ? qMin<int>(static_cast<int>(bounded / segmentDuration), static_cast<int>(stepCount) - 1)
                : 0;
            if (bounded >= forwardDuration)
                segmentIndex = static_cast<int>(stepCount) - 1;

            const double segmentStart = segmentDuration * static_cast<double>(segmentIndex);
            const double segmentEnd = segmentDuration * static_cast<double>(segmentIndex + 1);
            const float t = segmentEnd > segmentStart
                ? static_cast<float>((bounded - segmentStart) / (segmentEnd - segmentStart))
                : 1.0f;

            const QHash<QUuid, QVector3D>& fromPositions = positionSnapshots[segmentIndex];
            const QHash<QUuid, QVector3D>& toPositions = positionSnapshots[segmentIndex + 1];
            const QHash<QUuid, QQuaternion>& fromRotations = rotationSnapshots[segmentIndex];
            const QHash<QUuid, QQuaternion>& toRotations = rotationSnapshots[segmentIndex + 1];

            for (auto it = states.cbegin(); it != states.cend(); ++it)
            {
                SampledNodeState state;
                state.scale = it->scale;
                const QVector3D fromPosition = fromPositions.value(it.key(), it->basePosition);
                const QVector3D toPosition = toPositions.value(it.key(), fromPosition);
                const QQuaternion fromRotation = fromRotations.value(it.key(), it->baseRotation);
                const QQuaternion toRotation = toRotations.value(it.key(), fromRotation);
                state.translation = fromPosition * (1.0f - t) + toPosition * t;
                state.rotation = QQuaternion::slerp(fromRotation, toRotation, t).normalized();
                sampledByNode.insert(it.key(), state);
            }
        };

        if (loopBack && clampedTime > forwardDuration)
            sampleSequential(forwardDuration - (clampedTime - forwardDuration));
        else
            sampleSequential(clampedTime);
    }
    else
    {
        const int selectedRow = listWidgetCapturedViews ? listWidgetCapturedViews->currentRow() : -1;
        const int stepIndex = (selectedRow >= 0 && selectedRow < capturedSteps.size()) ? selectedRow : 0;
        if (stepIndex < capturedSteps.size())
        {
            const CapturedExplosionStep& step = capturedSteps[stepIndex];
            double localTime = clampedTime;
            bool reversing = false;
            if (loopBack && localTime > forwardDuration)
            {
                localTime -= forwardDuration;
                reversing = true;
            }
            const float t = static_cast<float>(qBound(0.0, localTime / qMax(1.0e-6, forwardDuration), 1.0));
            for (const CapturedTransformTrack& track : step.tracks)
            {
                const SceneNode* node = _sceneGraph->findNodeByUuid(track.ownerNodeUuid);
                if (!node)
                    continue;

                const LocalNodeTransform base = decomposeLocalNodeTransform(aiToQMatrix(node->localTransform));
                SampledNodeState state;
                state.scale = base.scale;
                if (!reversing)
                {
                    state.translation = track.startPosition * (1.0f - t) + track.endPosition * t;
                    state.rotation = QQuaternion::slerp(track.startRotation, track.endRotation, t).normalized();
                }
                else
                {
                    state.translation = track.endPosition * (1.0f - t) + track.startPosition * t;
                    state.rotation = QQuaternion::slerp(track.endRotation, track.startRotation, t).normalized();
                }
                sampledByNode.insert(track.ownerNodeUuid, state);
            }
        }
    }

    for (const QString& sourceFile : std::as_const(_draftPreviewFiles))
    {
        SceneNode* fileNode = _sceneGraph->findFileNode(sourceFile);
        if (!fileNode)
            continue;

        std::function<void(SceneNode*, const QMatrix4x4&)> applyNode =
            [&](SceneNode* node, const QMatrix4x4& parentWorld)
        {
            if (!node)
                return;

            QMatrix4x4 localMatrix = aiToQMatrix(node->localTransform);
            if (sampledByNode.contains(node->nodeUuid))
            {
                const SampledNodeState& sampledState = sampledByNode.value(node->nodeUuid);
                LocalNodeTransform localTransform;
                localTransform.translation = sampledState.translation;
                localTransform.rotation = sampledState.rotation;
                localTransform.scale = sampledState.scale;
                localMatrix = composeLocalNodeTransform(localTransform);
            }

            const QMatrix4x4 world = parentWorld * localMatrix;
            for (const QUuid& meshUuid : node->meshUuids)
            {
                if (TriangleMesh* mesh = _glWidget->getMeshByUuid(meshUuid))
                {
                    mesh->setExplosionOffset(QVector3D());
                    mesh->setSceneRenderTransformFast(world);
                }
            }

            for (SceneNode* child : node->children)
                applyNode(child, world);
        };

        for (SceneNode* child : fileNode->children)
            applyNode(child, QMatrix4x4());
    }

    _draftPreviewCurrentTime = clampedTime;
    _glWidget->update();
}

void ExplodedViewPanel::updatePreviewControls()
{
    if (!pushButtonPreviewPlayPause || !pushButtonPreviewStop || !checkBoxPreviewLoop
        || !sliderPreviewTimeline || !labelPreviewTime || !_glWidget)
        return;

    _syncingPreviewControls = true;

    const double durationSeconds = currentDraftPreviewDuration();
    const bool manualPlacementActive = _glWidget->isExplodedViewManualPlacementActive();
    const bool enabled = !activeCapturedSteps().isEmpty() && durationSeconds > 0.0 && !manualPlacementActive;

    pushButtonPreviewPlayPause->setEnabled(enabled);
    pushButtonPreviewPlayPause->setText(_draftPreviewPlaying ? tr("Pause") : tr("Play"));
    pushButtonPreviewStop->setEnabled(enabled && (_draftPreviewActive || _draftPreviewCurrentTime > 0.0));
    checkBoxPreviewLoop->setEnabled(enabled);
    checkBoxPreviewLoop->setChecked(_draftPreviewLoopPlayback);
    sliderPreviewTimeline->setEnabled(enabled);

    if (!_previewScrubbing)
    {
        const int sliderValue = enabled
            ? qRound((_draftPreviewCurrentTime / durationSeconds) * sliderPreviewTimeline->maximum())
            : 0;
        QSignalBlocker blocker(sliderPreviewTimeline);
        sliderPreviewTimeline->setValue(qBound(0, sliderValue, sliderPreviewTimeline->maximum()));
    }

    labelPreviewTime->setText(QStringLiteral("%1 / %2")
        .arg(formatPreviewTime(_draftPreviewCurrentTime))
        .arg(formatPreviewTime(durationSeconds)));

    _syncingPreviewControls = false;
}

void ExplodedViewPanel::onPreviewPlayPauseClicked()
{
    if (activeCapturedSteps().isEmpty())
        return;

    if (_draftPreviewPlaying)
    {
        _draftPreviewPlaying = false;
        if (_draftPreviewTimer)
            _draftPreviewTimer->stop();
        updatePreviewControls();
        return;
    }

    if (!ensureDraftPreviewSession())
        return;

    if (_draftPreviewCurrentTime >= currentDraftPreviewDuration())
        applyDraftPreviewPose(0.0);

    _draftPreviewPlaying = true;
    _draftPreviewElapsed.restart();
    if (_draftPreviewTimer)
        _draftPreviewTimer->start();
    updatePreviewControls();
}

void ExplodedViewPanel::onPreviewStopClicked()
{
    stopDraftPreview(true);
}

void ExplodedViewPanel::onPreviewLoopToggled(bool checked)
{
    if (_syncingPreviewControls)
        return;

    _draftPreviewLoopPlayback = checked;
}

void ExplodedViewPanel::onPreviewSliderPressed()
{
    _previewScrubbing = true;
}

void ExplodedViewPanel::onPreviewSliderReleased()
{
    _previewScrubbing = false;
    onPreviewSliderValueChanged(sliderPreviewTimeline ? sliderPreviewTimeline->value() : 0);
}

void ExplodedViewPanel::onPreviewSliderValueChanged(int value)
{
    if (_syncingPreviewControls || !_glWidget || !sliderPreviewTimeline)
        return;

    const double durationSeconds = currentDraftPreviewDuration();
    if (durationSeconds <= 0.0)
        return;

    const double timeSeconds = (static_cast<double>(value) / static_cast<double>(sliderPreviewTimeline->maximum())) * durationSeconds;
    labelPreviewTime->setText(QStringLiteral("%1 / %2")
        .arg(formatPreviewTime(timeSeconds))
        .arg(formatPreviewTime(durationSeconds)));

    if (!ensureDraftPreviewSession())
        return;

    applyDraftPreviewPose(timeSeconds);
    if (_draftPreviewPlaying)
    {
        _draftPreviewPlaying = false;
        if (_draftPreviewTimer)
            _draftPreviewTimer->stop();
    }
    updatePreviewControls();
}

void ExplodedViewPanel::onDraftPreviewTick()
{
    if (!_draftPreviewPlaying)
        return;

    const double durationSeconds = currentDraftPreviewDuration();
    if (durationSeconds <= 0.0)
    {
        stopDraftPreview();
        return;
    }

    const double deltaSeconds = _draftPreviewElapsed.isValid()
        ? static_cast<double>(_draftPreviewElapsed.restart()) / 1000.0
        : 0.016;
    double nextTime = _draftPreviewCurrentTime + deltaSeconds;
    if (nextTime >= durationSeconds)
    {
        if (_draftPreviewLoopPlayback)
            nextTime = std::fmod(nextTime, durationSeconds);
        else
        {
            applyDraftPreviewPose(durationSeconds);
            _draftPreviewPlaying = false;
            if (_draftPreviewTimer)
                _draftPreviewTimer->stop();
            updatePreviewControls();
            return;
        }
    }

    applyDraftPreviewPose(nextTime);
    updatePreviewControls();
}

void ExplodedViewPanel::syncCapturedStepOrderFromList()
{
    QVector<CapturedExplosionStep>& capturedSteps = activeCapturedSteps();
    if (!listWidgetCapturedViews || capturedSteps.isEmpty())
        return;

    QHash<QUuid, CapturedExplosionStep> stepsById;
    for (const CapturedExplosionStep& step : capturedSteps)
        stepsById.insert(step.id, step);

    QVector<CapturedExplosionStep> reorderedSteps;
    reorderedSteps.reserve(capturedSteps.size());
    for (int row = 0; row < listWidgetCapturedViews->count(); ++row)
    {
        QListWidgetItem* item = listWidgetCapturedViews->item(row);
        if (!item)
            continue;

        const QUuid stepId = item->data(Qt::UserRole).toUuid();
        const auto it = stepsById.constFind(stepId);
        if (it != stepsById.cend())
            reorderedSteps.append(it.value());
    }

    if (reorderedSteps.size() == capturedSteps.size())
        capturedSteps = reorderedSteps;
}

void ExplodedViewPanel::updateAssemblyPickButtonVisual(bool awaitingCommit)
{
    pushButtonSelectAssembly->setIcon(awaitingCommit ? _assemblySelectCommitIcon
                                                     : _assemblySelectIdleIcon);
    pushButtonSelectAssembly->setToolTip(awaitingCommit
        ? tr("Adjust scene or tree selection, then click again to confirm")
        : tr("Pick meshes from scene or tree"));
}

void ExplodedViewPanel::clearAssemblySelection()
{
    lineEditAssembly->clear();
    if (lineEditManualSelection)
        lineEditManualSelection->clear();
    lineEditAnchor->clear();
    ExplodedViewPreset& preset = ensureActivePreset();
    preset.assemblyUuids.clear();
    preset.anchorUuid = QUuid();
}

void ExplodedViewPanel::on_pushButtonStartManualPlacement_clicked()
{
    stopDraftPreview();
    if (_glWidget && _glWidget->beginExplodedViewManualPlacement())
    {
        updateManualPlacementUi();
        updateCaptureButton();
        updatePreviewControls();
    }
}

void ExplodedViewPanel::on_pushButtonFinishManualPlacement_clicked()
{
    stopDraftPreview();
    if (_glWidget)
    {
        _glWidget->finishExplodedViewManualPlacement();
        updateManualPlacementUi();
        updateCaptureButton();
        updatePreviewControls();
    }
}

void ExplodedViewPanel::on_pushButtonClearManualPlacement_clicked()
{
    stopDraftPreview();
    if (_glWidget)
    {
        _glWidget->clearExplodedViewManualPlacement();
        updateManualPlacementUi();
        updateCaptureButton();
        updatePreviewControls();
        emit explosionParametersChanged();
    }
}

void ExplodedViewPanel::updateCapturedViewsList()
{
    if (!listWidgetCapturedViews)
        return;

    listWidgetCapturedViews->clear();
    for (const CapturedExplosionStep& step : activeCapturedSteps())
    {
        auto* item = new QListWidgetItem(
            QStringLiteral("%1 - %2 mesh%3")
                .arg(step.name)
                .arg(step.tracks.size())
                .arg(step.tracks.size() == 1 ? QString() : QStringLiteral("es")),
            listWidgetCapturedViews);
        item->setData(Qt::UserRole, step.id);
    }
}
