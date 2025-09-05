#ifndef CLIPPINGPLANESEDITOR_H
#define CLIPPINGPLANESEDITOR_H

#include <QDialog>
#include "ui_ClippingPlanesEditor.h"

class GLWidget;
class ClippingPlanesEditor : public QWidget, Ui::ClippingPlanesEditor
{
	Q_OBJECT

public:
	explicit ClippingPlanesEditor(GLWidget* parent = nullptr);
	~ClippingPlanesEditor();

	void setCoefficientLimits(double xMin, double xMax, double yMin, double yMax, double zMin, double zMax);

protected slots:
	void keyPressEvent(QKeyEvent* e);
	void on_checkBoxXY_toggled(bool checked);
	void on_checkBoxYZ_toggled(bool checked);
	void on_checkBoxZX_toggled(bool checked);
	void on_checkBoxFlipXY_toggled(bool checked);
	void on_checkBoxFlipYZ_toggled(bool checked);
	void on_checkBoxFlipZX_toggled(bool checked);
	void on_checkBoxCapping_toggled(bool checked);
	void on_doubleSpinBoxXYCoeff_valueChanged(double val);
	void on_doubleSpinBoxYZCoeff_valueChanged(double val);
	void on_doubleSpinBoxZXCoeff_valueChanged(double val);
	void on_pushButtonResetCoeffs_clicked();
	void on_radioButtonProcedural_toggled(bool checked);
	void on_comboBoxHatchMode_currentIndexChanged(int index);
	void on_spinBoxHatchTiling_valueChanged(int val);
	void on_doubleSpinBoxThickness_valueChanged(double val);
	void on_doubleSpinBoxIntensity_valueChanged(double val);
	void on_pushButtonHatchColor_clicked();
	void on_pushButtonTexture_clicked();
	void on_pushButtonDefaultValues_clicked();


private:
	GLWidget* _glView;
};

#endif // CLIPPINGPLANESEDITOR_H
