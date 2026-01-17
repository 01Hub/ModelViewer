#include "VisualizationEnvironmentPanel.h"
#include "ui_VisualizationEnvironmentPanel.h"
#include "GLWidget.h"
#include "ModelViewer.h"
#include "PathUtils.h"
#include <QColorDialog>
#include <QFileDialog>
#include <QImage>
#include <QDir>

VisualizationEnvironmentPanel::VisualizationEnvironmentPanel(QWidget* parent)
	: QWidget(parent),
	_modelViewer(nullptr),
	_glWidget(nullptr),
	_isInitialized(false),
	_skyBoxLDRIIndex(0),
	_skyBoxHDRIIndex(0)
{
	ui = std::make_unique<Ui::VisualizationEnvironmentPanel>();
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose);
	ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

VisualizationEnvironmentPanel::~VisualizationEnvironmentPanel() = default;

void VisualizationEnvironmentPanel::initialize(ModelViewer* modelViewer, GLWidget* glWidget)
{
	if (_isInitialized)
		return;

	_modelViewer = modelViewer;
	_glWidget = glWidget;

	// Load state from ModelViewer
	if (_modelViewer)
	{
		_skyBoxLDRIIndex = _modelViewer->getSkyBoxLDRIIndex();
		_skyBoxHDRIIndex = _modelViewer->getSkyBoxHDRIIndex();
	}

	connectSignalsAndSlots();
	updateControlDependencies();

	_isInitialized = true;
}

void VisualizationEnvironmentPanel::connectSignalsAndSlots()
{
	if (!ui)
		return;

	// ===== Light Color Buttons =====
	connect(ui->pushButtonLightAmbient, &QPushButton::clicked, this, &VisualizationEnvironmentPanel::onLightAmbientClicked);
	connect(ui->pushButtonLightDiffuse, &QPushButton::clicked, this, &VisualizationEnvironmentPanel::onLightDiffuseClicked);
	connect(ui->pushButtonLightSpecular, &QPushButton::clicked, this, &VisualizationEnvironmentPanel::onLightSpecularClicked);
	connect(ui->pushButtonDefaultLights, &QPushButton::clicked, this, &VisualizationEnvironmentPanel::onDefaultLightsClicked);

	// ===== Light Position Sliders =====
	connect(ui->sliderLightPosX, QOverload<int>::of(&QSlider::valueChanged), this, &VisualizationEnvironmentPanel::onLightPosXChanged);
	connect(ui->sliderLightPosY, QOverload<int>::of(&QSlider::valueChanged), this, &VisualizationEnvironmentPanel::onLightPosYChanged);
	connect(ui->sliderLightPosZ, QOverload<int>::of(&QSlider::valueChanged), this, &VisualizationEnvironmentPanel::onLightPosZChanged);

	// ===== Lighting Checkboxes =====
	connect(ui->checkBoxDefaultLights, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onDefaultLightsChanged);
	connect(ui->checkBoxPunctualLights, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onPunctualLightsChanged);
	connect(ui->checkBoxShowLights, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onShowLightsChanged);
	connect(ui->checkBoxIBL, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onIBLChanged);

	// ===== Skybox Controls =====
	connect(ui->checkBoxSkyBox, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onSkyBoxStateChanged);
	connect(ui->checkBoxSkyBoxHDRI, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onSkyBoxHDRIChanged);
	connect(ui->checkBoxSkyBoxHDRI, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onLoadSkyBoxPresetMaps);
	connect(ui->checkBoxSkyBoxBlurred, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onSkyBoxBlurredChanged);
	connect(ui->doubleSpinBoxSkyBoxFOV, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VisualizationEnvironmentPanel::onSkyBoxFOVChanged);
	connect(ui->comboBoxSkyBoxMaps, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VisualizationEnvironmentPanel::onSkyBoxMapsChanged);
	connect(ui->pushButtonSkyBoxTex, &QPushButton::clicked, this, &VisualizationEnvironmentPanel::onSkyBoxTextureClicked);

	// ===== Shadow Controls =====
	connect(ui->checkBoxShadowMapping, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onShadowMappingStateChanged);
	connect(ui->checkBoxSelfShadows, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onSelfShadowsChanged);
	connect(ui->comboBoxShadowQuality, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VisualizationEnvironmentPanel::onShadowQualityChanged);

	// ===== Floor Controls =====
	connect(ui->checkBoxFloor, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onFloorStateChanged);
	connect(ui->checkBoxFloorTexture, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onFloorTextureStateChanged);
	connect(ui->checkBoxReflections, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onReflectionsChanged);
	connect(ui->checkBoxEnvMapping, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onEnvMappingChanged);
	connect(ui->doubleSpinBoxFloorOffset, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VisualizationEnvironmentPanel::onFloorOffsetChanged);
	connect(ui->doubleSpinBoxRepeatS, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VisualizationEnvironmentPanel::onRepeatSChanged);
	connect(ui->doubleSpinBoxRepeatT, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VisualizationEnvironmentPanel::onRepeatTChanged);
	connect(ui->pushButtonFloorTexture, &QPushButton::clicked, this, &VisualizationEnvironmentPanel::onFloorTextureClicked);

	// ===== HDR Controls =====
	connect(ui->checkBoxHDRToneMapping, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onHDRToneMappingStateChanged);
	connect(ui->comboBoxHDRToneMappingMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VisualizationEnvironmentPanel::onHDRToneMappingModeChanged);
	connect(ui->doubleSpinBoxEnvMapExposure, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VisualizationEnvironmentPanel::onEnvMapExposureChanged);
	connect(ui->doubleSpinBoxIBLExposure, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VisualizationEnvironmentPanel::onIBLExposureChanged);

	// ===== Gamma Controls =====
	connect(ui->checkBoxGammaCorrection, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onGammaCorrectionStateChanged);
	connect(ui->doubleSpinBoxScreenGamma, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VisualizationEnvironmentPanel::onScreenGammaChanged);

	// ===== Default Values Button =====
	connect(ui->pushButtonDefaultEnvValues, &QPushButton::clicked, this, &VisualizationEnvironmentPanel::onDefaultEnvValuesClicked);

	// ===== Detach Button =====
	connect(ui->toolButtonDetach, &QToolButton::clicked, this, &VisualizationEnvironmentPanel::onDetachButtonClicked);
}

void VisualizationEnvironmentPanel::updateControlDependencies()
{
	if (!ui)
		return;

	bool skyBoxEnabled = ui->checkBoxSkyBox->isChecked();
	bool floorEnabled = ui->checkBoxFloor->isChecked();
	bool shadowsEnabled = ui->checkBoxShadowMapping->isChecked();
	bool hdrEnabled = ui->checkBoxHDRToneMapping->isChecked();
	bool gammaEnabled = ui->checkBoxGammaCorrection->isChecked();
	bool skyBoxHDRIEnabled = skyBoxEnabled && ui->checkBoxSkyBoxHDRI->isChecked();
	bool floorTextureEnabled = floorEnabled && ui->checkBoxFloorTexture->isChecked();

	// Skybox dependencies
	ui->checkBoxSkyBoxHDRI->setEnabled(skyBoxEnabled);
	ui->checkBoxSkyBoxBlurred->setEnabled(skyBoxEnabled);
	ui->doubleSpinBoxSkyBoxFOV->setEnabled(skyBoxEnabled);
	ui->comboBoxSkyBoxMaps->setEnabled(skyBoxEnabled);
	ui->pushButtonSkyBoxTex->setEnabled(skyBoxEnabled);
	ui->labelFOV->setEnabled(skyBoxEnabled);

	// Floor dependencies
	ui->checkBoxReflections->setEnabled(floorEnabled);
	ui->checkBoxFloorTexture->setEnabled(floorEnabled);
	ui->labelFloorOffset->setEnabled(floorEnabled);
	ui->doubleSpinBoxFloorOffset->setEnabled(floorEnabled);
	ui->labelRepeatS->setEnabled(floorTextureEnabled);
	ui->labelRepeatT->setEnabled(floorTextureEnabled);
	ui->doubleSpinBoxRepeatS->setEnabled(floorTextureEnabled);
	ui->doubleSpinBoxRepeatT->setEnabled(floorTextureEnabled);	
	ui->pushButtonFloorTexture->setEnabled(floorTextureEnabled);

	// Shadow dependencies
	ui->checkBoxSelfShadows->setEnabled(shadowsEnabled);
	ui->labelShadowQuality->setEnabled(shadowsEnabled);
	ui->comboBoxShadowQuality->setEnabled(shadowsEnabled);

	// HDR dependencies	
	ui->labelToneMappingMode->setEnabled(hdrEnabled);
	ui->labelEnvMapExposure->setEnabled(hdrEnabled);
	ui->labelIBLExposure->setEnabled(hdrEnabled);
	ui->comboBoxHDRToneMappingMode->setEnabled(hdrEnabled);
	ui->doubleSpinBoxEnvMapExposure->setEnabled(hdrEnabled);
	ui->doubleSpinBoxIBLExposure->setEnabled(hdrEnabled);

	// Gamma dependencies
	ui->labelScreenGamma->setEnabled(gammaEnabled);
	ui->doubleSpinBoxScreenGamma->setEnabled(gammaEnabled);
}

void VisualizationEnvironmentPanel::updateButtonStyles()
{
	if (!_glWidget || !ui)
		return;

	// Update light color button background colors to match current colors
	QVector4D ambientLight = _glWidget->getAmbientLight();
	QVector4D diffuseLight = _glWidget->getDiffuseLight();
	QVector4D specularLight = _glWidget->getSpecularLight();

	// Set ambient button color	
	QColor ambientColor = QColor::fromRgbF(ambientLight.x(), ambientLight.y(), ambientLight.z());
	QString ambientStyle = QString("background-color: %1; color: %2; border: 1px solid gray;")
		.arg(ambientColor.name(), ambientColor.lightness() < 75 ? QColor(Qt::white).name() : QColor(Qt::black).name());
	ui->pushButtonLightAmbient->setStyleSheet(ambientStyle);

	// Set diffuse button color	
	QColor diffuseColor = QColor::fromRgbF(diffuseLight.x(), diffuseLight.y(), diffuseLight.z());
	QString diffuseStyle = QString("background-color: %1; color: %2; border: 1px solid gray;")
		.arg(diffuseColor.name(), diffuseColor.lightness() < 75 ? QColor(Qt::white).name() : QColor(Qt::black).name());
	ui->pushButtonLightDiffuse->setStyleSheet(diffuseStyle);

	// Set specular button color	
	QColor specularColor = QColor::fromRgbF(specularLight.x(), specularLight.y(), specularLight.z());
	QString specularStyle = QString("background-color: %1; color: %2; border: 1px solid gray;")
		.arg(specularColor.name(), specularColor.lightness() < 75 ? QColor(Qt::white).name() : QColor(Qt::black).name());
	ui->pushButtonLightSpecular->setStyleSheet(specularStyle);
}

// ==================== LIGHT COLOR BUTTONS ====================

void VisualizationEnvironmentPanel::onLightAmbientClicked()
{
	if (!_glWidget || !ui)
		return;

	QVector4D ambientLight = _glWidget->getAmbientLight();
	QColor c = QColorDialog::getColor(QColor::fromRgbF(ambientLight.x(), ambientLight.y(), ambientLight.z(), ambientLight.w()), this, "Ambient Light Color");
	if (c.isValid())
	{
		_glWidget->setAmbientLight(QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
		updateButtonStyles();
		_glWidget->updateView();
	}
}

void VisualizationEnvironmentPanel::onLightDiffuseClicked()
{
	if (!_glWidget || !ui)
		return;

	QVector4D diffuseLight = _glWidget->getDiffuseLight();
	QColor c = QColorDialog::getColor(QColor::fromRgbF(diffuseLight.x(), diffuseLight.y(), diffuseLight.z(), diffuseLight.w()), this, "Diffuse Light Color");
	if (c.isValid())
	{
		_glWidget->setDiffuseLight(QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
		updateButtonStyles();
		_glWidget->updateView();
	}
}

void VisualizationEnvironmentPanel::onLightSpecularClicked()
{
	if (!_glWidget || !ui)
		return;

	QVector4D specularLight = _glWidget->getSpecularLight();
	QColor c = QColorDialog::getColor(QColor::fromRgbF(specularLight.x(), specularLight.y(), specularLight.z(), specularLight.w()), this, "Specular Light Color");
	if (c.isValid())
	{
		_glWidget->setSpecularLight(QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
		updateButtonStyles();
		_glWidget->updateView();
	}
}

void VisualizationEnvironmentPanel::onDefaultLightsClicked()
{
	if (!_glWidget || !ui)
		return;

	// Set light colors
	_glWidget->setAmbientLight(QVector4D(0.0f, 0.0f, 0.0f, 1.0f));
	_glWidget->setDiffuseLight(QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
	_glWidget->setSpecularLight(QVector4D(0.5f, 0.5f, 0.5f, 1.0f));

	// Set light position sliders - block signals to prevent cascading during set
	ui->sliderLightPosX->blockSignals(true);
	ui->sliderLightPosY->blockSignals(true);
	ui->sliderLightPosZ->blockSignals(true);

	ui->sliderLightPosX->setValue((ui->sliderLightPosX->maximum() + ui->sliderLightPosX->minimum()) / 2);
	ui->sliderLightPosY->setValue((ui->sliderLightPosY->maximum() + ui->sliderLightPosY->minimum()) / 2);

	float range = _glWidget->getBoundingSphere().getRadius() * 4.0f;
	ui->sliderLightPosZ->setValue(static_cast<int>((-range / 3 + range / 2) / 2));

	ui->sliderLightPosX->blockSignals(false);
	ui->sliderLightPosY->blockSignals(false);
	ui->sliderLightPosZ->blockSignals(false);

	// Manually update light offset
	_glWidget->setLightOffset(QVector3D(
		static_cast<float>(ui->sliderLightPosX->value()),
		static_cast<float>(ui->sliderLightPosY->value()),
		static_cast<float>(ui->sliderLightPosZ->value())));

	// Set lighting checkboxes - block signals during set
	ui->checkBoxDefaultLights->blockSignals(true);
	ui->checkBoxPunctualLights->blockSignals(true);
	ui->checkBoxIBL->blockSignals(true);
	ui->checkBoxShowLights->blockSignals(true);

	ui->checkBoxDefaultLights->setChecked(true);
	ui->checkBoxPunctualLights->setChecked(true);
	ui->checkBoxIBL->setChecked(true);
	ui->checkBoxShowLights->setChecked(false);

	ui->checkBoxDefaultLights->blockSignals(false);
	ui->checkBoxPunctualLights->blockSignals(false);
	ui->checkBoxIBL->blockSignals(false);
	ui->checkBoxShowLights->blockSignals(false);

	// Manually trigger GLWidget calls
	_glWidget->useDefaultLights(true);
	_glWidget->usePunctualLights(true);
	_glWidget->useIBL(true);
	_glWidget->showLights(false);

	updateButtonStyles();
	_glWidget->updateView();
}

// ==================== LIGHT POSITION SLIDERS ====================

void VisualizationEnvironmentPanel::onLightPosXChanged(int value)
{
	if (!_glWidget || !ui)
		return;

	_glWidget->setLightOffset(QVector3D(
		static_cast<float>(ui->sliderLightPosX->value()),
		static_cast<float>(ui->sliderLightPosY->value()),
		static_cast<float>(ui->sliderLightPosZ->value())));
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onLightPosYChanged(int value)
{
	if (!_glWidget || !ui)
		return;

	_glWidget->setLightOffset(QVector3D(
		static_cast<float>(ui->sliderLightPosX->value()),
		static_cast<float>(ui->sliderLightPosY->value()),
		static_cast<float>(ui->sliderLightPosZ->value())));
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onLightPosZChanged(int value)
{
	if (!_glWidget || !ui)
		return;

	_glWidget->setLightOffset(QVector3D(
		static_cast<float>(ui->sliderLightPosX->value()),
		static_cast<float>(ui->sliderLightPosY->value()),
		static_cast<float>(ui->sliderLightPosZ->value())));
	_glWidget->updateView();
}

// ==================== LIGHTING CHECKBOXES ====================

void VisualizationEnvironmentPanel::onDefaultLightsChanged(bool checked)
{
	if (!_glWidget)
		return;

	_glWidget->useDefaultLights(checked);
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onPunctualLightsChanged(bool checked)
{
	if (!_glWidget)
		return;

	_glWidget->usePunctualLights(checked);
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onShowLightsChanged(bool checked)
{
	if (!_glWidget)
		return;

	_glWidget->showLights(checked);
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onIBLChanged(bool checked)
{
	if (!_glWidget)
		return;

	_glWidget->useIBL(checked);
	_glWidget->updateView();
}

// ==================== SKYBOX CONTROLS ====================

void VisualizationEnvironmentPanel::onSkyBoxStateChanged(bool checked)
{
	if (!_glWidget)
		return;

	_glWidget->showSkyBox(checked);
	updateControlDependencies();

	// Load presets if checkbox just enabled and combo is empty
	if (checked && ui->comboBoxSkyBoxMaps->count() == 0)
		onLoadSkyBoxPresetMaps();

	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onSkyBoxHDRIChanged(bool checked)
{
	if (!_glWidget)
		return;

	_glWidget->setSkyBoxTextureHDRI(checked);
	// Note: onLoadSkyBoxPresetMaps is connected to this signal and will be called
}

void VisualizationEnvironmentPanel::onSkyBoxBlurredChanged(bool checked)
{
	if (!_glWidget)
		return;

	_glWidget->blurSkyBox(checked);
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onSkyBoxFOVChanged(double value)
{
	if (!_glWidget)
		return;

	_glWidget->setSkyBoxFOV(value);
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onSkyBoxMapsChanged(int index)
{
	if (!_glWidget || !ui)
		return;

	QString selectedPath = ui->comboBoxSkyBoxMaps->itemData(index).toString();
	if (!selectedPath.isEmpty())
	{
		_glWidget->setSkyBoxTextureFolder(selectedPath);
		_glWidget->updateView();
	}
}

void VisualizationEnvironmentPanel::onSkyBoxTextureClicked()
{
	if (!_glWidget || !ui)
		return;

	QString texpath = ui->checkBoxSkyBoxHDRI->isChecked() ? "/textures/envmap/skyboxes/HDRI" : "/textures/envmap/skyboxes/LDRI";
	QString appPath = PathUtils::getDataDirectory();
	QString dir = QFileDialog::getExistingDirectory(this, tr("Select Skybox Texture Folder"),
		appPath + texpath,
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	if (!dir.isEmpty())
	{
		_glWidget->setSkyBoxTextureFolder(dir);
		_glWidget->updateView();
	}
}

// ==================== SHADOW CONTROLS ====================

void VisualizationEnvironmentPanel::onShadowMappingStateChanged(bool checked)
{
	if (!_glWidget)
		return;

	_glWidget->showShadows(checked);
	updateControlDependencies();
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onSelfShadowsChanged(bool checked)
{
	if (!_glWidget)
		return;

	_glWidget->showSelfShadows(checked);
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onShadowQualityChanged(int index)
{
	if (!_glWidget)
		return;

	_glWidget->setShadowQuality(static_cast<AdaptiveShadowMapper::QualityLevel>(index));
	_glWidget->updateView();
}

// ==================== FLOOR CONTROLS ====================

void VisualizationEnvironmentPanel::onFloorStateChanged(bool checked)
{
	if (!_glWidget)
		return;

	_glWidget->showFloor(checked);
	updateControlDependencies();
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onFloorTextureStateChanged(bool checked)
{
	if (!_glWidget)
		return;

	_glWidget->showFloorTexture(checked);
	updateControlDependencies();
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onReflectionsChanged(bool checked)
{
	if (!_glWidget)
		return;

	_glWidget->showReflections(checked);
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onEnvMappingChanged(bool checked)
{
	if (!_glWidget)
		return;

	_glWidget->showEnvironment(checked);
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onFloorOffsetChanged(double value)
{
	if (!_glWidget)
		return;

	_glWidget->setFloorOffsetPercent(value);
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onRepeatSChanged(double value)
{
	if (!_glWidget)
		return;

	_glWidget->setFloorTexRepeatS(value);
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onRepeatTChanged(double value)
{
	if (!_glWidget)
		return;

	_glWidget->setFloorTexRepeatT(value);
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onFloorTextureClicked()
{
	if (!_glWidget || !ui)
		return;

	QString appPath = PathUtils::getDataDirectory();
	QString filter = "Image Files (*.png *.jpg *.jpeg *.bmp *.tiff);;All Files (*)";
	QString fileName = QFileDialog::getOpenFileName(this, "Choose an image for floor texture", appPath + "/textures/envmap/floor", filter);
	if (!fileName.isEmpty())
	{
		QImage buf;
		if (!buf.load(fileName))
		{
			// Fallback to dummy image if load fails
			QImage dummy(128, 128, QImage::Format_ARGB32);
			dummy.fill(1);
			buf = dummy;
		}
		_glWidget->setFloorTexture(buf);
		_glWidget->updateView();
	}
}

// ==================== HDR CONTROLS ====================

void VisualizationEnvironmentPanel::onHDRToneMappingStateChanged(bool checked)
{
	if (!_glWidget)
		return;

	_glWidget->enableHDRToneMapping(checked);
	updateControlDependencies();
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onHDRToneMappingModeChanged(int index)
{
	if (!_glWidget)
		return;

	_glWidget->setHDRToneMappingMode(static_cast<HDRToneMapMode>(index));
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onEnvMapExposureChanged(double value)
{
	if (!_glWidget)
		return;

	_glWidget->setEnvMapExposure(value);
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onIBLExposureChanged(double value)
{
	if (!_glWidget)
		return;

	_glWidget->setIBLExposure(value);
	_glWidget->updateView();
}

// ==================== GAMMA CONTROLS ====================

void VisualizationEnvironmentPanel::onGammaCorrectionStateChanged(bool checked)
{
	if (!_glWidget)
		return;

	_glWidget->enableGammaCorrection(checked);
	updateControlDependencies();
	_glWidget->updateView();
}

void VisualizationEnvironmentPanel::onScreenGammaChanged(double value)
{
	if (!_glWidget)
		return;

	_glWidget->setScreenGamma(value);
	_glWidget->updateView();
}

// ==================== DEFAULT VALUES BUTTON ====================

void VisualizationEnvironmentPanel::onDefaultEnvValuesClicked()
{
	if (!ui || !_glWidget)
		return;

	ui->doubleSpinBoxSkyBoxFOV->setValue(45.0);
	ui->comboBoxShadowQuality->setCurrentIndex(1);
	ui->doubleSpinBoxFloorOffset->setValue(0.0);
	ui->doubleSpinBoxRepeatS->setValue(1.0);
	ui->doubleSpinBoxRepeatT->setValue(1.0);
	ui->comboBoxHDRToneMappingMode->setCurrentIndex(0);
	ui->doubleSpinBoxEnvMapExposure->setValue(0.0);
	ui->doubleSpinBoxIBLExposure->setValue(0.0);
	ui->doubleSpinBoxScreenGamma->setValue(2.2);

	updateControlDependencies();
	_glWidget->updateView();
}

// ==================== SKYBOX PRESET MANAGEMENT ====================

void VisualizationEnvironmentPanel::onLoadSkyBoxPresetMaps()
{
	reloadSkyBoxPresets();
}

void VisualizationEnvironmentPanel::reloadSkyBoxPresets()
{
	if (!_modelViewer || !_glWidget || !ui)
		return;

	bool isHDRI = ui->checkBoxSkyBoxHDRI->isChecked();
	QString appPath = PathUtils::getDataDirectory();
	QString texPath = appPath + (isHDRI ? "/textures/envmap/skyboxes/HDRI" : "/textures/envmap/skyboxes/LDRI");

	// Store current index before clearing
	int currentIndex = ui->comboBoxSkyBoxMaps->currentIndex();
	if (isHDRI)
		_skyBoxLDRIIndex = std::max(0, currentIndex);
	else
		_skyBoxHDRIIndex = std::max(0, currentIndex);

	// Update ModelViewer state
	_modelViewer->setSkyBoxLDRIIndex(_skyBoxLDRIIndex);
	_modelViewer->setSkyBoxHDRIIndex(_skyBoxHDRIIndex);

	// Clear and populate combo box
	ui->comboBoxSkyBoxMaps->blockSignals(true);
	ui->comboBoxSkyBoxMaps->clear();

	QDir dir(texPath);
	QStringList folderList = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

	for (const QString& folderName : folderList)
	{
		QString fullPath = dir.absoluteFilePath(folderName);
		ui->comboBoxSkyBoxMaps->addItem(folderName, fullPath);
	}

	ui->comboBoxSkyBoxMaps->blockSignals(false);

	updateControlDependencies();

	// Restore previous index for this mode
	int indexToRestore = isHDRI ? _skyBoxHDRIIndex : _skyBoxLDRIIndex;
	if (indexToRestore >= 0 && indexToRestore < ui->comboBoxSkyBoxMaps->count())
	{
		ui->comboBoxSkyBoxMaps->setCurrentIndex(indexToRestore);
	}

	// Load texture folder if available
	QString selectedPath = ui->comboBoxSkyBoxMaps->itemData(ui->comboBoxSkyBoxMaps->currentIndex()).toString();
	if (!selectedPath.isEmpty())
	{
		_glWidget->setSkyBoxTextureFolder(selectedPath);
		_glWidget->updateView();
	}
}

// ==================== DISPLAY MODE SYNCHRONIZATION ====================

void VisualizationEnvironmentPanel::onDisplayModeChanged(int mode)
{
	if (!_glWidget || !ui)
		return;

	bool realShaded = (mode == static_cast<int>(DisplayMode::REALSHADED));
	bool pbrLighting = (_glWidget->getRenderingMode() == RenderingMode::PHYSICALLY_BASED_RENDERING);

	// Block signals to prevent cascading updates
	blockSignals(true);

	ui->checkBoxEnvMapping->setChecked(realShaded || pbrLighting);
	ui->checkBoxShadowMapping->setChecked(realShaded);
	ui->checkBoxSelfShadows->setChecked(realShaded);
	ui->checkBoxReflections->setChecked(realShaded);
	ui->checkBoxFloor->setChecked(realShaded);
	ui->checkBoxSkyBoxHDRI->setChecked(ui->checkBoxSkyBoxHDRI->isChecked() || (realShaded && pbrLighting));

	bool skyBoxHDRIChecked = ui->checkBoxSkyBoxHDRI->isChecked();
	ui->checkBoxHDRToneMapping->setChecked(skyBoxHDRIChecked);
	ui->checkBoxGammaCorrection->setChecked(skyBoxHDRIChecked);

	blockSignals(false);

	updateControlDependencies();
	_glWidget->setSkyBoxTextureHDRI(skyBoxHDRIChecked);
}

// ==================== PBR LIGHTING MODE ====================

void VisualizationEnvironmentPanel::setPBRLightingMode(bool enable)
{
	if (!enable || !_glWidget || !ui)
		return;

	// Block signals during state changes
	ui->checkBoxSkyBoxHDRI->blockSignals(true);
	ui->checkBoxHDRToneMapping->blockSignals(true);
	ui->checkBoxGammaCorrection->blockSignals(true);

	ui->checkBoxSkyBoxHDRI->setChecked(true);
	ui->checkBoxHDRToneMapping->setChecked(true);
	ui->checkBoxGammaCorrection->setChecked(true);

	ui->checkBoxSkyBoxHDRI->blockSignals(false);
	ui->checkBoxHDRToneMapping->blockSignals(false);
	ui->checkBoxGammaCorrection->blockSignals(false);

	// Trigger updates
	onSkyBoxHDRIChanged(true);
	_glWidget->enableHDRToneMapping(true);
	_glWidget->enableGammaCorrection(true);

	updateControlDependencies();
}

// ==================== LIGHT POSITION RANGE UPDATES (from ModelViewer geometry changes) ====================

void VisualizationEnvironmentPanel::updateLightPositionRanges(float range, float offset)
{
	if (!ui)
		return;

	// Update X slider
	ui->sliderLightPosX->blockSignals(true);
	ui->sliderLightPosX->setRange(static_cast<int>(-range), static_cast<int>(range - offset));
	ui->sliderLightPosX->setValue((ui->sliderLightPosX->maximum() + ui->sliderLightPosX->minimum()) / 2);
	ui->sliderLightPosX->blockSignals(false);

	// Update Y slider
	ui->sliderLightPosY->blockSignals(true);
	ui->sliderLightPosY->setRange(static_cast<int>(-range), static_cast<int>(range - offset));
	ui->sliderLightPosY->setValue((ui->sliderLightPosY->maximum() + ui->sliderLightPosY->minimum()) / 2);
	ui->sliderLightPosY->blockSignals(false);

	// Update Z slider
	ui->sliderLightPosZ->blockSignals(true);
	ui->sliderLightPosZ->setRange(static_cast<int>(-range / 3), static_cast<int>(range / 2));
	ui->sliderLightPosZ->setValue((ui->sliderLightPosZ->maximum() + ui->sliderLightPosZ->minimum()) / 2);
	ui->sliderLightPosZ->blockSignals(false);

	// Manually trigger light offset update
	if (_glWidget)
	{
		_glWidget->setLightOffset(QVector3D(
			static_cast<float>(ui->sliderLightPosX->value()),
			static_cast<float>(ui->sliderLightPosY->value()),
			static_cast<float>(ui->sliderLightPosZ->value())));
		_glWidget->updateView();
	}
}

void VisualizationEnvironmentPanel::setDetached(bool detached)
{
	_detached = detached;
	ui->toolButtonDetach->setVisible(!_detached);
	ui->lineSeparator->setVisible(!_detached);
	if(detached)
		ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	else
		ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

// ==================== DETACH BUTTON ====================

void VisualizationEnvironmentPanel::onDetachButtonClicked()
{
	emit detachRequested();
}
