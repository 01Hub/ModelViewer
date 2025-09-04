#include "ClippingPlanesEditor.h"
#include "ui_ClippingPlanesEditor.h"
#include "LanguageManager.h"
#include "GLWidget.h"

#include <QKeyEvent>

ClippingPlanesEditor::ClippingPlanesEditor(GLWidget* parent) :
	QWidget(parent),
	_glView(parent)
{
	setupUi(this);
	connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this, [this]() {
		retranslateUi(this);
		});

}

ClippingPlanesEditor::~ClippingPlanesEditor()
{
}

void ClippingPlanesEditor::setCoefficientLimits(double xMin, double xMax, double yMin, double yMax, double zMin, double zMax)
{
	doubleSpinBoxXYCoeff->setRange(xMin, xMax);
	doubleSpinBoxYZCoeff->setRange(yMin, yMax);
	doubleSpinBoxZXCoeff->setRange(zMin, zMax);

	// Set the step as 1/50th of the range
	doubleSpinBoxXYCoeff->setSingleStep((xMax - xMin) / 50.0);
	doubleSpinBoxYZCoeff->setSingleStep((yMax - yMin) / 50.0);
	doubleSpinBoxZXCoeff->setSingleStep((zMax - zMin) / 50.0);
}

void ClippingPlanesEditor::keyPressEvent(QKeyEvent* e)
{
	if (e->key() != Qt::Key_Escape)
		QWidget::keyPressEvent(e);
	else {/* minimize */ }
}

void ClippingPlanesEditor::on_checkBoxXY_toggled(bool checked)
{
	_glView->_clipYZEnabled = checked;
	_glView->updateClippingPlane();
	_glView->update();
}

void ClippingPlanesEditor::on_checkBoxYZ_toggled(bool checked)
{
	_glView->_clipZXEnabled = checked;
	_glView->updateClippingPlane();
	_glView->update();
}

void ClippingPlanesEditor::on_checkBoxZX_toggled(bool checked)
{
	_glView->_clipXYEnabled = checked;
	_glView->updateClippingPlane();
	_glView->update();
}

void ClippingPlanesEditor::on_checkBoxFlipXY_toggled(bool checked)
{
	_glView->_clipXFlipped = checked;
	_glView->updateClippingPlane();
	_glView->update();
}

void ClippingPlanesEditor::on_checkBoxFlipYZ_toggled(bool checked)
{
	_glView->_clipYFlipped = checked;
	_glView->updateClippingPlane();
	_glView->update();
}

void ClippingPlanesEditor::on_checkBoxFlipZX_toggled(bool checked)
{
	_glView->_clipZFlipped = checked;
	_glView->updateClippingPlane();
	_glView->update();
}

void ClippingPlanesEditor::on_checkBoxCapping_toggled(bool checked)
{
	_glView->_cappingEnabled = checked;
	_glView->showFloor(!checked);
	_glView->updateClippingPlane();
	_glView->update();
}

void ClippingPlanesEditor::on_doubleSpinBoxXYCoeff_valueChanged(double val)
{
	_glView->_clipXCoeff = val;
	_glView->updateClippingPlane();
	_glView->update();
}

void ClippingPlanesEditor::on_doubleSpinBoxYZCoeff_valueChanged(double val)
{
	_glView->_clipYCoeff = val;
	_glView->updateClippingPlane();
	_glView->update();
}

void ClippingPlanesEditor::on_doubleSpinBoxZXCoeff_valueChanged(double val)
{
	_glView->_clipZCoeff = val;
	_glView->updateClippingPlane();
	_glView->update();
}

void ClippingPlanesEditor::on_pushButtonResetCoeffs_clicked()
{	
	doubleSpinBoxZXCoeff->setValue(0);
	doubleSpinBoxXYCoeff->setValue(0);
	doubleSpinBoxYZCoeff->setValue(0);
}
