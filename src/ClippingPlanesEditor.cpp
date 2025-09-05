#include "ClippingPlanesEditor.h"
#include "ui_ClippingPlanesEditor.h"
#include "LanguageManager.h"
#include "GLWidget.h"
#include "config.h"
#include <QKeyEvent>
#include <QColorDialog>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

// helper: simple extension check (same filters as file dialog)
static bool isImageFileExtension(const QString& path)
{
	const QStringList exts = { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr", ".exr" };
	const QString lower = path.toLower();
	for (const QString& e : exts)
		if (lower.endsWith(e)) return true;
	return false;
}

// Filter that only uses the button and glView pointers it was given.
class TextureButtonDropFilter : public QObject
{
public:
	// pass the specific UI button and the GL view; parent the filter to 'parent'
	explicit TextureButtonDropFilter(QPushButton* button, GLWidget* glView, QObject* parent = nullptr)
		: QObject(parent), _button(button), _glView(glView)
	{
	}

protected:
	bool eventFilter(QObject* obj, QEvent* ev) override
	{
		Q_UNUSED(obj);
		if (ev->type() == QEvent::DragEnter)
		{
			QDragEnterEvent* den = static_cast<QDragEnterEvent*>(ev);
			const QMimeData* md = den->mimeData();
			if (md && md->hasUrls())
			{
				const QList<QUrl> urls = md->urls();
				if (!urls.isEmpty())
				{
					const QString local = urls.first().toLocalFile();
					if (!local.isEmpty() && isImageFileExtension(local))
					{
						den->acceptProposedAction();
						return true;
					}
				}
			}
			return QObject::eventFilter(obj, ev);
		}
		else if (ev->type() == QEvent::Drop)
		{
			QDropEvent* de = static_cast<QDropEvent*>(ev);
			const QMimeData* md = de->mimeData();
			if (md && md->hasUrls())
			{
				const QList<QUrl> urls = md->urls();
				if (!urls.isEmpty())
				{
					const QString local = urls.first().toLocalFile();
					if (!local.isEmpty() && isImageFileExtension(local))
					{
						// Apply same behaviour as on_pushButtonTexture_clicked
						if (_button)
						{
							int thumb = 140;
							_button->setFixedSize(thumb, thumb);

							QPixmap pix(local);
							QIcon icon(pix);
							_button->setIcon(icon);
							_button->setIconSize(_button->size());
							_button->setText(QString());
							_button->setToolTip(QFileInfo(local).fileName());
						}

						if (_glView)
						{
							_glView->setHatchTexture(local);
							_glView->updateClippingPlane();
							_glView->update();
						}

						de->acceptProposedAction();
						return true;
					}
				}
			}
			return QObject::eventFilter(obj, ev);
		}

		return QObject::eventFilter(obj, ev);
	}

private:
	QPushButton* _button = nullptr;
	GLWidget* _glView = nullptr;
};


ClippingPlanesEditor::ClippingPlanesEditor(GLWidget* parent) :
	QWidget(parent),
	_glView(parent)
{
	setupUi(this);
	connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this, [this]() {
		retranslateUi(this);
		});

	// enable drag/drop on the single texture button (no header changes)
	pushButtonTexture->setAcceptDrops(true);
	// parent the filter to 'this' so it will be deleted with the editor
	pushButtonTexture->installEventFilter(new TextureButtonDropFilter(pushButtonTexture, _glView, this));
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

void ClippingPlanesEditor::on_radioButtonProcedural_toggled(bool checked)
{
	_glView->setClippingPlaneHatchMode(checked ? ClippingPlaneHatchMode::PROCEDURAL : ClippingPlaneHatchMode::TEXTURE);
	_glView->updateClippingPlane();
	_glView->update();
}

void ClippingPlanesEditor::on_comboBoxHatchMode_currentIndexChanged(int index)
{
	_glView->setClippingPlaneHatchPattern(static_cast<HatchPattern>(index));
	_glView->updateClippingPlane();
	_glView->update();
}

void ClippingPlanesEditor::on_spinBoxHatchTiling_valueChanged(int val)
{
	_glView->setHatchTiling(val);
	_glView->updateClippingPlane();
	_glView->update();
}

void ClippingPlanesEditor::on_doubleSpinBoxThickness_valueChanged(double val)
{
	_glView->setHatchLineThickness(static_cast<float>(val));
	_glView->updateClippingPlane();
	_glView->update();
}

void ClippingPlanesEditor::on_doubleSpinBoxIntensity_valueChanged(double val)
{
	_glView->setHatchIntensity(static_cast<float>(val));
	_glView->updateClippingPlane();
	_glView->update();
}

void ClippingPlanesEditor::on_pushButtonHatchColor_clicked()
{
	QColor color = QColorDialog::getColor(QColor(0,0,0), this, tr("Select Hatch Color"));
	if (color.isValid())
	{		
		pushButtonHatchColor->setStyleSheet(
			QString("background-color: %1; color: %2;")
			.arg(color.name())
			.arg(color.lightness() < 128 ? "#FFFFFF" : "#000000")
		);
		_glView->setHatchLineColor(color);
		_glView->updateClippingPlane();
		_glView->update();
	}
}

void ClippingPlanesEditor::on_pushButtonTexture_clicked()
{
	const QString path = QString(MODELVIEWER_DATA_DIR) + "/";
	QString filePath = QFileDialog::getOpenFileName(this, tr("Select Hatch Texture"), QString(path + "textures/patterns"),
		tr("Image Files (*.png *.jpg *.bmp)"));
	if (!filePath.isEmpty())
	{
		// make the button a fixed-size square thumbnail (say 48x48)
		int thumb = 140;
		pushButtonTexture->setFixedSize(thumb, thumb);

		// set icon and scale it to full button area
		QPixmap pix(filePath);
		QIcon icon(pix);
		pushButtonTexture->setIcon(icon);
		pushButtonTexture->setIconSize(pushButtonTexture->size());

		// remove text and optional focus/flat look
		pushButtonTexture->setText(QString());		

		// optionally use tooltip for the filename
		pushButtonTexture->setToolTip(QFileInfo(filePath).fileName());

		_glView->setHatchTexture(filePath);
		_glView->updateClippingPlane();
		_glView->update();
	}
	else
	{
		pushButtonTexture->setText(tr("Select Texture"));
	}
}

void ClippingPlanesEditor::on_pushButtonDefaultValues_clicked()
{
	// set default values
	doubleSpinBoxXYCoeff->setValue(0);
	doubleSpinBoxYZCoeff->setValue(0);
	doubleSpinBoxZXCoeff->setValue(0);
	checkBoxXY->setChecked(false);
	checkBoxYZ->setChecked(false);
	checkBoxZX->setChecked(false);
	checkBoxFlipXY->setChecked(false);
	checkBoxFlipYZ->setChecked(false);
	checkBoxFlipZX->setChecked(false);
	checkBoxCapping->setChecked(false);
	radioButtonProcedural->setChecked(true);
	comboBoxHatchMode->setCurrentIndex(0);
	spinBoxHatchTiling->setValue(50);
	doubleSpinBoxThickness->setValue(0.05);
	doubleSpinBoxIntensity->setValue(1.0f);
	pushButtonHatchColor->setStyleSheet("background-color: #000000; color: #FFFFFF;");
	pushButtonTexture->setText(tr("Select Texture"));
}
