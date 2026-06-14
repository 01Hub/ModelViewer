#pragma once

#include <QIcon>
#include <QElapsedTimer>
#include <QHash>
#include <QMatrix4x4>
#include <QSet>
#include <QString>
#include <QUuid>
#include <QVector>
#include <QVector3D>
#include <QQuaternion>
#include <QWidget>

#include "ui_ExplodedViewPanel.h"
#include "ExplodedViewManager.h"

class GLWidget;
class SceneGraph;
class QTimer;

class ExplodedViewPanel : public QWidget, private Ui::ExplodedViewPanel
{
    Q_OBJECT

public:
    explicit ExplodedViewPanel(GLWidget* parent = nullptr);

    void setSceneGraph(SceneGraph* sg);
    void applyContrastTheme(const QColor& textColor);
    void applyBackgroundTheme(const QColor& topColor, const QColor& bottomColor);
    void deactivateInteractiveState();

    // Called by GLWidget::showExplodedViewPanel(true) to seed the assembly
    // field from whatever is already selected in the viewport / tree.
    void captureCurrentSelection();

    // Stored selection state — consumed by ExplodedViewManager.
    const QSet<QUuid>& assemblyUuids() const;
    QUuid               anchorUuid()   const;
    ExplodedViewManager::Mode mode()   const;
    QVector3D           userVector()   const;
    float               factor()       const;  // sliderValue / 100.0f

signals:
    // Emitted after picking capture so ModelViewer can clear the screen selection.
    void selectionClearRequested();
    // Emitted when any parameter that affects the explosion changes.
    void explosionParametersChanged();

private slots:
    void on_comboBoxMode_currentIndexChanged(int index);
    void on_sliderExplosion_valueChanged(int value);
    void on_pushButtonSelectAssembly_toggled(bool checked);
    void on_pushButtonSelectAnchor_toggled(bool checked);
    void on_pushButtonCapture_clicked();
    void on_pushButtonReset_clicked();
    void on_pushButtonPresetNew_clicked();
    void on_pushButtonPresetDuplicate_clicked();
    void on_pushButtonPresetActions_clicked();
    void on_pushButtonStartManualPlacement_clicked();
    void on_pushButtonFinishManualPlacement_clicked();
    void on_pushButtonClearManualPlacement_clicked();

private:
    enum class PickingTarget { None, Assembly, Anchor };

    struct CapturedTransformTrack
    {
        QUuid     meshUuid;
        QUuid     ownerNodeUuid;
        QString   sourceFile;
        QString   targetNodeName;
        int       targetNodeIndex = -1;
        QVector3D startPosition;
        QVector3D endPosition;
        QQuaternion startRotation;
        QQuaternion endRotation;
    };

    struct CapturedExplosionStep
    {
        QUuid                         id;
        QString                       name;
        QVector<CapturedTransformTrack> tracks;
    };

    struct ExplodedViewPreset
    {
        QUuid id;
        QString name;
        QSet<QUuid> assemblyUuids;
        QUuid anchorUuid;
        ExplodedViewManager::Mode mode = ExplodedViewManager::Mode::Auto;
        QVector3D userVector = QVector3D(1.0f, 0.0f, 0.0f);
        float factor = 1.0f;
        QVector<CapturedExplosionStep> capturedSteps;
        int capturedStepCounter = 1;
        int outputMode = 0;
        double durationSeconds = 3.0;
        bool loopBack = true;
    };

    struct PreviewMeshState
    {
        QMatrix4x4 sceneRenderTransform;
        QVector3D explosionOffset;
        QVector3D translation;
        QVector3D rotation;
        QVector3D scale = QVector3D(1.0f, 1.0f, 1.0f);
        QQuaternion rotationQuat;
        bool hasExactRotation = false;
    };

    struct SuspendedAnimationState
    {
        bool valid = false;
        QString sourceFile;
        int clipIndex = -1;
        double timeSeconds = 0.0;
        bool wasPlaying = false;
    };

    // Receives the selectionChanged signal while in picking mode.
    void onPickingSelectionChanged(const QList<int>& ids);

    // Convert a list of mesh-store indices to display text + populate UUID sets.
    void applyAssemblySelection(const QList<int>& ids);
    void applyAnchorSelection(const QList<int>& ids);
    void updateAssemblySelectionDisplay(const QList<int>& ids);

    // Derive a human-readable label for the assembly selection.
    QString describeAssemblySelection(const QList<int>& ids) const;

    void updateCaptureButton();
    void cancelPickingMode();
    void updateAuthoringModeUi();
    void updateManualPlacementUi();
    void updatePreviewControls();
    void stopDraftPreview(bool restoreScene = true);
    bool ensureDraftPreviewSession();
    void applyDraftPreviewPose(double timeSeconds);
    double currentDraftPreviewDuration() const;
    void syncCapturedStepOrderFromList();
    void updateAssemblyPickButtonVisual(bool awaitingCommit);
    void clearAssemblySelection();
    void updateCapturedViewsList();
    void applyPopupMenuStyle(QMenu& menu) const;
    bool captureCurrentExplosionStep();
    bool createAnimationsFromCapturedSteps();
    void onPreviewPlayPauseClicked();
    void onPreviewStopClicked();
    void onPreviewLoopToggled(bool checked);
    void onPreviewSliderPressed();
    void onPreviewSliderReleased();
    void onPreviewSliderValueChanged(int value);
    void initializeDefaultPreset();
    void refreshPresetCombo();
    void loadPresetIntoUi(int index);
    void syncActivePresetFromUi();
    QString nextPresetName() const;
    QString nextDuplicatedPresetName(const QString& sourceName) const;
    ExplodedViewPreset* activePreset();
    const ExplodedViewPreset* activePreset() const;
    ExplodedViewPreset& ensureActivePreset();
    QVector<CapturedExplosionStep>& activeCapturedSteps();
    const QVector<CapturedExplosionStep>& activeCapturedSteps() const;

    GLWidget*    _glWidget   = nullptr;
    SceneGraph*  _sceneGraph = nullptr;

    PickingTarget            _pickingTarget = PickingTarget::None;
    QMetaObject::Connection  _pickingConn;

    QIcon       _assemblySelectIdleIcon;
    QIcon       _assemblySelectCommitIcon;
    QString _popupMenuStyleSheet;
    QVector<ExplodedViewPreset> _presets;
    int _activePresetIndex = -1;
    int _createdAnimationCounter = 1;
    bool _syncingPresetUi = false;
    bool _syncingPreviewControls = false;
    bool _previewScrubbing = false;
    bool _draftPreviewActive = false;
    bool _draftPreviewPlaying = false;
    bool _draftPreviewLoopPlayback = true;
    double _draftPreviewCurrentTime = 0.0;
    QElapsedTimer _draftPreviewElapsed;
    QTimer* _draftPreviewTimer = nullptr;
    QSet<QString> _draftPreviewFiles;
    QHash<QUuid, PreviewMeshState> _draftPreviewMeshStates;
    SuspendedAnimationState _draftPreviewSuspendedAnimation;
    QSet<QUuid> _emptyAssemblyUuids;

private slots:
    void on_pushButtonCaptureView_clicked();
    void on_pushButtonRemoveCapture_clicked();
    void onDraftPreviewTick();
};
