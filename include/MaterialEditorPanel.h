#pragma once
#include <QWidget>
#include <QTreeWidget>
#include <QFormLayout>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QToolButton>
#include "MaterialLibraryWidget.h"
#include "MaterialPreviewWidget.h"

class MaterialEditorPanel : public QWidget
{
	Q_OBJECT
public:
	explicit MaterialEditorPanel(QWidget* parent = nullptr);

	void onSaveButtonClicked();
	void onDeleteButtonClicked();

	bool isDetached() const { return _detached; }
	void setDetached(bool detached);

signals:
	void materialChanged(const GLMaterial& mat);
	void detachRequested();

private slots:
	void onMaterialSelected(const GLMaterial& mat);
	void onDetachButtonClicked();

private:
	MaterialLibraryWidget* treeWidget;
	MaterialPreviewWidget* previewWidget;
	QComboBox* modelCombo;
	QPushButton* albedoButton;
	QDoubleSpinBox* metalnessSpin;
	QDoubleSpinBox* roughnessSpin;
	QDoubleSpinBox* opacitySpin;
	QDoubleSpinBox* emissiveSpin;	
	QDoubleSpinBox* iridescenceFactorSpin;
	QDoubleSpinBox* iridescenceIorSpin;
	QDoubleSpinBox* iridescenceThicknessMinSpin;
	QDoubleSpinBox* iridescenceThicknessMaxSpin;
	QDoubleSpinBox* clearcoatSpin;
	QDoubleSpinBox* clearcoatRoughnessSpin;
	QPushButton* sheenColorButton;
	QDoubleSpinBox* sheenRoughnessSpin;
	QDoubleSpinBox* iorSpin;
	QDoubleSpinBox* transmissionSpin;
	QDoubleSpinBox* thicknessSpin;
	QDoubleSpinBox* attenuationDistanceSpin;
	QPushButton* attenuationColorButton;
	QDoubleSpinBox* dispersionSpin;
	QDoubleSpinBox* alphaThresholdSpin;

	QComboBox* shadingCombo;
	QComboBox* blendCombo;
	QCheckBox* twoSidedCheck;
	QCheckBox* wireframeCheck;

	QPushButton* applyButton;
	QPushButton* saveButton;
	QPushButton* deleteButton;
	QToolButton* detachButton;
	QFrame* separator;

	GLMaterial _currentMaterial = GLMaterial::METAL_ALUMINUM();

	bool _detached = false;
};
