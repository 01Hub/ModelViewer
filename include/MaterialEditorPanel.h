#pragma once
#include <QWidget>
#include <QTreeWidget>
#include <QFormLayout>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include "MaterialLibraryWidget.h"
#include "MaterialPreviewWidget.h"

class MaterialEditorPanel : public QWidget
{
    Q_OBJECT
public:
    explicit MaterialEditorPanel(QWidget *parent = nullptr);

signals:
    void materialChanged(const GLMaterial &mat);

private slots:
    void onMaterialSelected(const GLMaterial &mat);

private:
    MaterialLibraryWidget *treeWidget;
    MaterialPreviewWidget *previewWidget;
	QComboBox* modelCombo;
    QPushButton *albedoButton;
    QDoubleSpinBox *metalnessSpin;
    QDoubleSpinBox *roughnessSpin;
    QDoubleSpinBox *opacitySpin;
	QDoubleSpinBox* iorSpin;
	QDoubleSpinBox* clearcoatSpin;
	QDoubleSpinBox* clearcoatRoughnessSpin;
	QPushButton* sheenColorButton;
	QDoubleSpinBox* sheenRoughnessSpin;
	QDoubleSpinBox* transmissionSpin;
	QDoubleSpinBox* alphaThresholdSpin;

    QComboBox *shadingCombo;
    QComboBox *blendCombo;
    QCheckBox *twoSidedCheck;
    QCheckBox *wireframeCheck;

    QPushButton* applyButton;
    QPushButton* saveButton;
    QPushButton* deleteButton;

    GLMaterial _currentMaterial = GLMaterial::METAL_ALUMINUM();
};
