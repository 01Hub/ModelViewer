#pragma once

#include <QSet>
#include <QUuid>
#include <QVector3D>
#include <QWidget>
#include "ui_ExplodedViewPanel.h"
#include "ExplodedViewManager.h"

class GLWidget;
class SceneGraph;

class ExplodedViewPanel : public QWidget, private Ui::ExplodedViewPanel
{
    Q_OBJECT

public:
    explicit ExplodedViewPanel(GLWidget* parent = nullptr);

    void setSceneGraph(SceneGraph* sg);
    void applyContrastTheme(const QColor& textColor);

    // Called by GLWidget::showExplodedViewPanel(true) to seed the assembly
    // field from whatever is already selected in the viewport / tree.
    void captureCurrentSelection();

    // Stored selection state — consumed by ExplodedViewManager.
    const QSet<QUuid>& assemblyUuids() const { return _assemblyUuids; }
    QUuid               anchorUuid()   const { return _anchorUuid; }
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

private:
    enum class PickingTarget { None, Assembly, Anchor };

    // Receives the selectionChanged signal while in picking mode.
    void onPickingSelectionChanged(const QList<int>& ids);

    // Convert a list of mesh-store indices to display text + populate UUID sets.
    void applyAssemblySelection(const QList<int>& ids);
    void applyAnchorSelection(const QList<int>& ids);

    // Derive a human-readable label for the assembly selection.
    QString describeAssemblySelection(const QList<int>& ids) const;

    void updateCaptureButton();
    void cancelPickingMode();

    GLWidget*    _glWidget   = nullptr;
    SceneGraph*  _sceneGraph = nullptr;

    PickingTarget            _pickingTarget = PickingTarget::None;
    QMetaObject::Connection  _pickingConn;

    QSet<QUuid> _assemblyUuids;
    QUuid       _anchorUuid;
};
