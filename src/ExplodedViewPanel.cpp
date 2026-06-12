#include "ExplodedViewPanel.h"

#include "GLWidget.h"
#include "GltfAnimationData.h"
#include "SceneGraph.h"
#include "SceneNode.h"
#include "SelectionManager.h"
#include "TriangleMesh.h"

#include <QListWidgetItem>
#include <QMenu>
#include <QSignalBlocker>
#include <QStyle>
#include <QDebug>
#include <QtGlobal>
#include <algorithm>
#include <assimp/matrix4x4.h>
#include <functional>
#include <utility>

namespace
{
constexpr float kExplosionOffsetTolerance = 1.0e-4f;

QVector3D localTranslationOf(const aiMatrix4x4& matrix)
{
    aiVector3D scaling;
    aiQuaternion rotation;
    aiVector3D position;
    matrix.Decompose(scaling, rotation, position);
    return QVector3D(position.x, position.y, position.z);
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
}

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
    _assemblySelectIdleIcon = pushButtonSelectAssembly->icon();
    _assemblySelectCommitIcon = QIcon(":/icons/res/checkmark.png");
    if (_assemblySelectCommitIcon.isNull())
        _assemblySelectCommitIcon = style()->standardIcon(QStyle::SP_DialogApplyButton);
    updateAssemblyPickButtonVisual(false);

    auto emitParamChanged = [this]() { emit explosionParametersChanged(); };
    connect(doubleSpinBoxVectorX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, emitParamChanged);
    connect(doubleSpinBoxVectorY, qOverload<double>(&QDoubleSpinBox::valueChanged), this, emitParamChanged);
    connect(doubleSpinBoxVectorZ, qOverload<double>(&QDoubleSpinBox::valueChanged), this, emitParamChanged);

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
            _anchorUuid = QUuid();
            pushButtonSelectAnchor->setChecked(false);
            emit explosionParametersChanged();
        });
        menu.exec(lineEditAnchor->mapToGlobal(pos));
    });

    if (listWidgetCapturedViews)
    {
        connect(listWidgetCapturedViews, &QListWidget::currentRowChanged,
                this, [this](int) { updateCaptureButton(); });
    }

    updateCaptureButton();
}

void ExplodedViewPanel::setSceneGraph(SceneGraph* sg)
{
    _sceneGraph = sg;
}

void ExplodedViewPanel::applyContrastTheme(const QColor& textColor)
{
    setStyleSheet(QString("color: %1;").arg(textColor.name()));
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
        applyAssemblySelection(ids);
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

    _assemblyUuids.clear();
    for (int id : ids)
        _assemblyUuids.insert(_glWidget->getUuidByIndex(id));

    lineEditAssembly->setText(describeAssemblySelection(ids));

    if (!_anchorUuid.isNull() && !_assemblyUuids.contains(_anchorUuid)) {
        lineEditAnchor->clear();
        _anchorUuid = QUuid();
    }

    emit explosionParametersChanged();
}

void ExplodedViewPanel::applyAnchorSelection(const QList<int>& ids)
{
    const QUuid uuid = _glWidget->getUuidByIndex(ids.first());

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
    if (captureCurrentExplosionStep())
        updateCaptureButton();
}

void ExplodedViewPanel::on_pushButtonRemoveCapture_clicked()
{
    if (!listWidgetCapturedViews)
        return;

    const int row = listWidgetCapturedViews->currentRow();
    if (row < 0 || row >= _capturedSteps.size())
        return;

    _capturedSteps.removeAt(row);
    updateCapturedViewsList();
    if (listWidgetCapturedViews && !_capturedSteps.isEmpty())
        listWidgetCapturedViews->setCurrentRow(std::min<int>(row, _capturedSteps.size() - 1));
    updateCaptureButton();
}

void ExplodedViewPanel::on_pushButtonCapture_clicked()
{
    createAnimationsFromCapturedSteps();
}

bool ExplodedViewPanel::captureCurrentExplosionStep()
{
    if (!_glWidget || !_sceneGraph || _assemblyUuids.isEmpty())
        return false;

    const SceneGraphWorldTransforms worlds = _sceneGraph->evaluateWorldTransforms();
    CapturedExplosionStep step;
    step.id = QUuid::createUuid();
    step.name = tr("Step %1").arg(_capturedStepCounter++);
    QHash<QUuid, CapturedTransformTrack> aggregatedTracksByNode;
    QHash<QUuid, QVector<QVector3D>> localOffsetsByNode;
    QHash<QUuid, int> selectedMeshCountByNode;
    QHash<QUuid, int> totalMeshCountByNode;

    for (const QUuid& meshUuid : _assemblyUuids)
    {
        TriangleMesh* mesh = _glWidget->getMeshByUuid(meshUuid);
        SceneNode* ownerNode = _sceneGraph->findNodeForMesh(meshUuid);
        if (!mesh || !ownerNode)
            continue;

        const QVector3D worldOffset = mesh->explosionOffset();
        if (worldOffset.lengthSquared() < 1e-8f)
            continue;

        QMatrix4x4 parentWorld;
        parentWorld.setToIdentity();
        if (ownerNode->parent && worlds.nodeWorldByUuid.contains(ownerNode->parent->nodeUuid))
            parentWorld = worlds.nodeWorldByUuid.value(ownerNode->parent->nodeUuid);

        bool invertible = false;
        const QMatrix4x4 parentWorldInv = parentWorld.inverted(&invertible);
        if (!invertible)
            continue;

        const QVector3D startPosition = localTranslationOf(ownerNode->localTransform);
        const QVector3D localOffset = (parentWorldInv * QVector4D(worldOffset, 0.0f)).toVector3D();

        CapturedTransformTrack track;
        track.meshUuid = meshUuid;
        track.ownerNodeUuid = ownerNode->nodeUuid;
        track.sourceFile = mesh->getSourceFile().trimmed();
        if (track.sourceFile.isEmpty())
            track.sourceFile = sourceFileForNode(ownerNode);
        track.targetNodeName = ownerNode->name;
        track.startPosition = startPosition;
        track.endPosition = startPosition;

        if (!aggregatedTracksByNode.contains(ownerNode->nodeUuid))
        {
            aggregatedTracksByNode.insert(ownerNode->nodeUuid, track);
            totalMeshCountByNode.insert(ownerNode->nodeUuid, ownerNode->meshUuids.size());
        }
        localOffsetsByNode[ownerNode->nodeUuid].append(localOffset);
        selectedMeshCountByNode[ownerNode->nodeUuid] = selectedMeshCountByNode.value(ownerNode->nodeUuid) + 1;
    }

    for (auto it = aggregatedTracksByNode.begin(); it != aggregatedTracksByNode.end(); ++it)
    {
        const QVector<QVector3D> offsets = localOffsetsByNode.value(it.key());
        const int count = offsets.size();
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

        const QVector3D referenceOffset = offsets.front();
        bool offsetsCompatible = true;
        for (const QVector3D& offset : offsets)
        {
            if ((offset - referenceOffset).lengthSquared() > kExplosionOffsetTolerance)
            {
                offsetsCompatible = false;
                break;
            }
        }

        if (!offsetsCompatible)
        {
            qWarning() << "[ExplodedView] Skipping animation capture for node"
                       << it->targetNodeName
                       << "because its meshes explode by different offsets and cannot"
                       << "be represented by one node translation track.";
            continue;
        }

        it->endPosition = it->startPosition + referenceOffset;
        step.tracks.append(it.value());
    }

    if (step.tracks.isEmpty())
        return false;

    _capturedSteps.append(step);
    updateCapturedViewsList();
    if (listWidgetCapturedViews)
        listWidgetCapturedViews->setCurrentRow(_capturedSteps.size() - 1);
    return true;
}

bool ExplodedViewPanel::createAnimationsFromCapturedSteps()
{
    if (!_sceneGraph || _capturedSteps.isEmpty())
        return false;

    enum class OutputMode { ParallelSingle, SequentialSingle, Separate };
    const OutputMode mode = (comboBoxAnimationMode && comboBoxAnimationMode->currentIndex() == 1)
        ? OutputMode::SequentialSingle
        : (comboBoxAnimationMode && comboBoxAnimationMode->currentIndex() == 2)
            ? OutputMode::Separate
            : OutputMode::ParallelSingle;

    const double durationSeconds = doubleSpinBoxAnimationDuration
        ? doubleSpinBoxAnimationDuration->value()
        : 3.0;
    const bool loopBack = checkBoxLoopBack && checkBoxLoopBack->isChecked();

    // Build one node-index map per source file. If a file has no persisted node
    // bindings yet, synthesize them from the current SceneGraph hierarchy.
    QHash<QString, GltfAnimationData> animationDataByFile;
    QHash<QString, QHash<QUuid, int>> nodeIndexByUuidByFile;

    QSet<QString> involvedFiles;
    for (const CapturedExplosionStep& step : std::as_const(_capturedSteps))
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

    auto makeForwardKeys = [&](const QVector3D& startPosition,
                               const QVector3D& endPosition) {
        QVector<GltfAnimationVec3Key> keys;
        keys.append({0.0, startPosition});
        keys.append({durationSeconds, endPosition});
        if (loopBack)
            keys.append({durationSeconds * 2.0, startPosition});
        return keys;
    };

    const auto makeClipName = [&](const QString& suffix = QString()) {
        const QString base = tr("Exploded View %1").arg(_createdAnimationCounter++);
        return suffix.isEmpty() ? base : QStringLiteral("%1 - %2").arg(base, suffix);
    };

    if (mode == OutputMode::ParallelSingle)
    {
        QHash<QString, QHash<QUuid, CapturedTransformTrack>> finalTracksByFile;
        for (const CapturedExplosionStep& step : std::as_const(_capturedSteps))
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

        for (const CapturedExplosionStep& step : std::as_const(_capturedSteps))
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
            QVector<GltfAnimationVec3Key> keys;
        };

        QHash<QUuid, NodeState> states;
        for (const CapturedExplosionStep& step : std::as_const(_capturedSteps))
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
            it->keys.append({0.0, it->base});

        const qsizetype stepCount = std::max<qsizetype>(1, _capturedSteps.size());
        const double segmentDuration = durationSeconds / static_cast<double>(stepCount);
        QVector<QHash<QUuid, QVector3D>> forwardSnapshots;
        forwardSnapshots.reserve(_capturedSteps.size() + 1);

        QHash<QUuid, QVector3D> initialSnapshot;
        for (auto it = states.cbegin(); it != states.cend(); ++it)
            initialSnapshot.insert(it.key(), it->base);
        forwardSnapshots.append(initialSnapshot);

        for (int stepIndex = 0; stepIndex < _capturedSteps.size(); ++stepIndex)
        {
            const CapturedExplosionStep& step = _capturedSteps[stepIndex];
            for (const CapturedTransformTrack& track : step.tracks)
            {
                if (track.sourceFile != sourceFile)
                    continue;

                auto it = states.find(track.ownerNodeUuid);
                if (it != states.end())
                {
                    it->info = track;
                    it->current = track.endPosition;
                }
            }

            const double t = segmentDuration * static_cast<double>(stepIndex + 1);
            for (auto it = states.begin(); it != states.end(); ++it)
                it->keys.append({t, it->current});

            QHash<QUuid, QVector3D> snapshot;
            for (auto it = states.cbegin(); it != states.cend(); ++it)
                snapshot.insert(it.key(), it->current);
            forwardSnapshots.append(snapshot);
        }

        if (loopBack)
        {
            for (qsizetype reverseStep = stepCount - 1; reverseStep >= 0; --reverseStep)
            {
                const double t = durationSeconds + segmentDuration * static_cast<double>(stepCount - reverseStep);
                const QHash<QUuid, QVector3D>& snapshot = forwardSnapshots[reverseStep];
                for (auto it = states.begin(); it != states.end(); ++it)
                {
                    const QVector3D position = snapshot.value(it.key(), it->base);
                    it->keys.append({t, position});
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
    frameVector->setVisible(index == 4);
    emit explosionParametersChanged();
}

void ExplodedViewPanel::on_sliderExplosion_valueChanged(int value)
{
    labelDistancePercent->setText(QString("%1%").arg(value));
    updateCaptureButton();
    emit explosionParametersChanged();
}

void ExplodedViewPanel::on_pushButtonReset_clicked()
{
    cancelPickingMode();

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
    _capturedSteps.clear();
    _capturedStepCounter = 1;
    if (listWidgetCapturedViews)
        listWidgetCapturedViews->clear();

    updateCaptureButton();
    emit explosionParametersChanged();
    emit selectionClearRequested();
}

void ExplodedViewPanel::updateCaptureButton()
{
    const bool hasAssembly = !_assemblyUuids.isEmpty();
    const bool canCaptureExplosion = hasAssembly && sliderExplosion->value() >= 10;

    if (pushButtonCaptureView)
        pushButtonCaptureView->setEnabled(canCaptureExplosion);
    if (pushButtonCapture)
        pushButtonCapture->setEnabled(!_capturedSteps.isEmpty());
    if (pushButtonRemoveCapture)
        pushButtonRemoveCapture->setEnabled(listWidgetCapturedViews
            && listWidgetCapturedViews->currentRow() >= 0);
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
    lineEditAnchor->clear();
    _assemblyUuids.clear();
    _anchorUuid = QUuid();
}

void ExplodedViewPanel::updateCapturedViewsList()
{
    if (!listWidgetCapturedViews)
        return;

    listWidgetCapturedViews->clear();
    for (const CapturedExplosionStep& step : std::as_const(_capturedSteps))
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
