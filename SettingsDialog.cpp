#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include <QMessageBox>


SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    	
    // Connect to specific buttons
    QPushButton* okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    if (okButton)
    {
        connect(okButton, &QPushButton::clicked, this, &SettingsDialog::onOkClicked);
    }

    QPushButton* cancelButton = ui->buttonBox->button(QDialogButtonBox::Cancel);
    if (cancelButton)
    {
        connect(cancelButton, &QPushButton::clicked, this, &SettingsDialog::onCancelClicked);
    }

    QPushButton* applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);
    if (applyButton)
    {
        connect(applyButton, &QPushButton::clicked, this, &SettingsDialog::onApplyClicked);
    }

    QPushButton* restoreButton = ui->buttonBox->button(QDialogButtonBox::RestoreDefaults);
    if (restoreButton)
    {
        connect(restoreButton, &QPushButton::clicked, this, &SettingsDialog::onRestoreDefaults);
    }

    loadSettings();
}

SettingsDialog::~SettingsDialog()
{
    saveSettings();
    delete ui;
}

void SettingsDialog::onOkClicked()
{
	applySettings();
	QDialog::accept();
}

void SettingsDialog::onCancelClicked()
{
	QDialog::reject();
}

void SettingsDialog::onApplyClicked()
{
	applySettings();
}

void SettingsDialog::onRestoreDefaults()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
	qDebug() << "Restoring default settings...";
    settings.clear();
	setDefaultValues();
    qDebug() << "All settings have been reset to defaults.";
    QMessageBox::information(this, "Settings Reset", "All settings have been cleared.");
}

void SettingsDialog::applySettings()
{
	// TODO: First apply the settings to the UI elements

	// then save them to the settings file
	saveSettings();
}

void SettingsDialog::setDefaultValues()
{
    // General tab
    ui->checkRestoreLastFile->setChecked(false);  // No 'checked' property found
    ui->checkTooltips->setChecked(true);          // Explicitly set to true
    ui->comboBoxTheme->setCurrentIndex(0);        // Default: "System Default"
    ui->comboBoxLanguage->setCurrentIndex(0);     // Default: "English"
    ui->checkPromptOverwrite->setChecked(true);   // Explicitly set to true
    ui->checkConfirmExit->setChecked(false);      // No 'checked' property found

    // Camera tab
    ui->comboProjectionMode->setCurrentIndex(0);       // "Orthographic"
    ui->comboDefaultView->setCurrentIndex(0);          // "Isometric"
    ui->comboDefaultProjection->setCurrentIndex(0);    // "Isometric"
    ui->checkTrackball->setChecked(false);             // No 'checked' property found
    ui->checkInvertZoom->setChecked(false);            // No 'checked' property found
    ui->spinZoomFactor->setValue(1.0);                 // Explicitly set to 1.0

    // Background tab
    ui->comboBoxBackgroundStyle->setCurrentIndex(0);   // "Gradient"
    ui->comboBoxGradientStyle->setCurrentIndex(0);     // "Vertical"
    // pushButtonTopColor and pushButtonBottomColor do not store actual color values — handled elsewhere.

    // Display tab
    ui->showBoundingBoxCheckBox->setChecked(false);    // No 'checked' property found
    ui->showCornerTrihedronCheckBox->setChecked(true); // Explicitly set to true
    ui->showGridCheckBox->setChecked(true);            // Explicitly set to true
    ui->showWireframeCheckBox->setChecked(false);      // No 'checked' property found
    ui->fieldOfViewSpinBox->setValue(60);              // Explicitly set
    ui->nearPlaneSpinBox->setValue(0.1);               // Explicitly set
    ui->farPlaneSpinBox->setValue(1000.0);             // Explicitly set

    // Navigation group
    ui->navigationModeComboBox->setCurrentIndex(0);          // "Orbit"
    ui->mouseSensitivitySlider->setValue(5);                 // Explicitly set
    ui->zoomSensitivitySlider->setValue(5);                  // Explicitly set
    ui->invertYAxisCheckBox->setChecked(false);              // Not set
    ui->smoothNavigationCheckBox->setChecked(true);          // Explicitly set

    // Rendering tab
    ui->comboShadingMode->setCurrentIndex(0);                // "Shaded"
    ui->checkBackfaceCulling->setChecked(false);             // Not set
    ui->checkNormalMap->setChecked(false);                   // Not set
    ui->shaderModelComboBox->setCurrentIndex(0);             // "Blinn-Phong"
    ui->msaaComboBox->setCurrentIndex(0);                // "1 (No MSAA)"
    ui->anisotropyComboBox->setCurrentIndex(0);          // "1x (Off)"

    // Lighting
    ui->enableLightingCheckBox->setChecked(true);
    ui->enableShadowsCheckBox->setChecked(false);
    ui->ambientLightSlider->setValue(20);
    ui->diffuseLightSlider->setValue(80);
    ui->specularLightSlider->setValue(50);

    // Materials
    ui->comboBoxDefaultMaterial->setCurrentIndex(0);     // "Plastic"
    ui->lineEditTextureDir->clear();                     // Default: empty

    // --- UV Generation Tab ---
    ui->comboUVMethod->setCurrentIndex(0);               // "Angle-Based Smart UV"
    ui->spinAngleThreshold->setValue(66.0);
    ui->checkPreserveUVs->setChecked(false);
    ui->checkAutoPackUVs->setChecked(false);
    ui->checkRelaxUVs->setChecked(false);
    ui->checkPCAProjection->setChecked(false);
    ui->checkXatlasPackingOnly->setChecked(false);
    ui->checkRememberUV->setChecked(false);

    // --- Import/Export Tab ---    
    // OpenCascade settings
    ui->tessellationQualitySlider->setValue(5);
    ui->linearDeflectionSpinBox->setValue(0.1);
    ui->angularDeflectionSpinBox->setValue(0.1);
    ui->occtUnifyFacesCheckBox->setChecked(false);
    ui->occtUnifyEdgesCheckBox->setChecked(false);
    ui->occtBuildCurvesCheckBox->setChecked(false);

    // Assimp settings
    ui->assimpTriangulateCheckBox->setChecked(true);
    ui->assimpGenNormalsCheckBox->setChecked(true);
    ui->assimpSmoothNormalsCheckBox->setChecked(false);
    ui->assimpCalcTangentsCheckBox->setChecked(false);
    ui->assimpOptimizeMeshCheckBox->setChecked(false);
    ui->assimpRemoveDuplicatesCheckBox->setChecked(false);
    ui->assimpMaxFaceVerticesSpinBox->setValue(3);

    // --- Performance Tab ---
    ui->checkMultithreadedLoad->setChecked(false);
    ui->spinThreadLimit->setValue(4);
    ui->checkSkyboxBlending->setChecked(false);
    ui->checkProgressiveLoading->setChecked(false);

    // Rendering performance
    ui->maxFpsSpinBox->setValue(60);
    ui->vsyncCheckBox->setChecked(true);
    ui->frustumCullingCheckBox->setChecked(true);
    ui->backfaceCullingCheckBox->setChecked(true);
    ui->levelOfDetailCheckBox->setChecked(false);
    ui->maxVerticesSpinBox->setValue(1000000);

    // Memory management
    ui->textureCacheSizeSpinBox->setValue(512);
    ui->geometryCacheSizeSpinBox->setValue(256);
    ui->compressTexturesCheckBox->setChecked(true);
    ui->generateMipmapsCheckBox->setChecked(true);

    // --- Advanced Tab ---
    ui->comboBoxOpenGLVersion->setCurrentText("4.5 Core");
    ui->checkBoxVSync->setChecked(false);
    ui->spinBoxThreads->setValue(4);
    ui->checkShaderHotReload->setChecked(false);
    ui->checkShowFPS->setChecked(false);
    ui->checkLegacyOpenGL->setChecked(false);

    // --- Debug Tab ---
    ui->showFpsCheckBox->setChecked(false);
    ui->showMemoryUsageCheckBox->setChecked(false);
    ui->showRenderStatsCheckBox->setChecked(false);
    ui->showOpenGLInfoCheckBox->setChecked(false);
    ui->enableLoggingCheckBox->setChecked(false);
    ui->logLevelComboBox->setCurrentText("Warning");

    ui->checkOpenGLErrorsCheckBox->setChecked(false);
    ui->validateShadersCheckBox->setChecked(false);
    ui->profileRenderingCheckBox->setChecked(false);
}


void SettingsDialog::loadSettings()
{
	qDebug() << "Loading settings from QSettings...";
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    int iVal = settings.value("comboBoxTheme", ui->comboBoxTheme->currentIndex()).toInt();
    ui->comboBoxTheme->setCurrentIndex(iVal);
    iVal = settings.value("comboBoxLanguage", ui->comboBoxLanguage->currentIndex()).toInt();
    ui->comboBoxLanguage->setCurrentIndex(iVal);
    bool bVal = settings.value("checkPromptOverwrite", ui->checkPromptOverwrite->isChecked()).toBool();
    ui->checkPromptOverwrite->setChecked(bVal);
    bVal = settings.value("checkRestoreLastFile", ui->checkRestoreLastFile->isChecked()).toBool();
    ui->checkRestoreLastFile->setChecked(bVal);
    bVal = settings.value("checkTooltips", ui->checkTooltips->isChecked()).toBool();
    ui->checkTooltips->setChecked(bVal);
    bVal = settings.value("checkConfirmExit", ui->checkConfirmExit->isChecked()).toBool();
    ui->checkConfirmExit->setChecked(bVal);
    iVal = settings.value("comboProjectionMode", ui->comboProjectionMode->currentIndex()).toInt();
    ui->comboProjectionMode->setCurrentIndex(iVal);
    iVal = settings.value("comboDefaultView", ui->comboDefaultView->currentIndex()).toInt();
    ui->comboDefaultView->setCurrentIndex(iVal);
    iVal = settings.value("comboDefaultProjection", ui->comboDefaultProjection->currentIndex()).toInt();
    ui->comboDefaultProjection->setCurrentIndex(iVal);
    bVal = settings.value("checkTrackball", ui->checkTrackball->isChecked()).toBool();
    ui->checkTrackball->setChecked(bVal);
    bVal = settings.value("checkInvertZoom", ui->checkInvertZoom->isChecked()).toBool();
    ui->checkInvertZoom->setChecked(bVal);
    double dVal = settings.value("spinZoomFactor", ui->spinZoomFactor->value()).toDouble();
    ui->spinZoomFactor->setValue(dVal);
    iVal = settings.value("comboBoxBackgroundStyle", ui->comboBoxBackgroundStyle->currentIndex()).toInt();
    ui->comboBoxBackgroundStyle->setCurrentIndex(iVal);
    iVal = settings.value("comboBoxGradientStyle", ui->comboBoxGradientStyle->currentIndex()).toInt();
    ui->comboBoxGradientStyle->setCurrentIndex(iVal);
    bVal = settings.value("showBoundingBoxCheckBox", ui->showBoundingBoxCheckBox->isChecked()).toBool();
    ui->showBoundingBoxCheckBox->setChecked(bVal);
    bVal = settings.value("showCornerTrihedronCheckBox", ui->showCornerTrihedronCheckBox->isChecked()).toBool();
    ui->showCornerTrihedronCheckBox->setChecked(bVal);
    dVal = settings.value("farPlaneSpinBox", ui->farPlaneSpinBox->value()).toDouble();
    ui->farPlaneSpinBox->setValue(dVal);
    iVal = settings.value("fieldOfViewSpinBox", ui->fieldOfViewSpinBox->value()).toInt();
    ui->fieldOfViewSpinBox->setValue(iVal);
    bVal = settings.value("showGridCheckBox", ui->showGridCheckBox->isChecked()).toBool();
    ui->showGridCheckBox->setChecked(bVal);
    dVal = settings.value("nearPlaneSpinBox", ui->nearPlaneSpinBox->value()).toDouble();
    ui->nearPlaneSpinBox->setValue(dVal);
    bVal = settings.value("showWireframeCheckBox", ui->showWireframeCheckBox->isChecked()).toBool();
    ui->showWireframeCheckBox->setChecked(bVal);
    bVal = settings.value("showCenterTrihedronCheckBox", ui->showCenterTrihedronCheckBox->isChecked()).toBool();
    ui->showCenterTrihedronCheckBox->setChecked(bVal);
    iVal = settings.value("navigationModeComboBox", ui->navigationModeComboBox->currentIndex()).toInt();
    ui->navigationModeComboBox->setCurrentIndex(iVal);
    iVal = settings.value("mouseSensitivitySlider", ui->mouseSensitivitySlider->value()).toInt();
    ui->mouseSensitivitySlider->setValue(iVal);
    iVal = settings.value("zoomSensitivitySlider", ui->zoomSensitivitySlider->value()).toInt();
    ui->zoomSensitivitySlider->setValue(iVal);
    bVal = settings.value("invertYAxisCheckBox", ui->invertYAxisCheckBox->isChecked()).toBool();
    ui->invertYAxisCheckBox->setChecked(bVal);
    bVal = settings.value("smoothNavigationCheckBox", ui->smoothNavigationCheckBox->isChecked()).toBool();
    ui->smoothNavigationCheckBox->setChecked(bVal);
    iVal = settings.value("comboShadingMode", ui->comboShadingMode->currentIndex()).toInt();
    ui->comboShadingMode->setCurrentIndex(iVal);
    bVal = settings.value("checkBackfaceCulling", ui->checkBackfaceCulling->isChecked()).toBool();
    ui->checkBackfaceCulling->setChecked(bVal);
    bVal = settings.value("checkNormalMap", ui->checkNormalMap->isChecked()).toBool();
    ui->checkNormalMap->setChecked(bVal);
    iVal = settings.value("shaderModelComboBox", ui->shaderModelComboBox->currentIndex()).toInt();
    ui->shaderModelComboBox->setCurrentIndex(iVal);
    iVal = settings.value("msaaComboBox", ui->msaaComboBox->currentIndex()).toInt();
    ui->msaaComboBox->setCurrentIndex(iVal);
    iVal = settings.value("anisotropyComboBox", ui->anisotropyComboBox->currentIndex()).toInt();
    ui->anisotropyComboBox->setCurrentIndex(iVal);
    bVal = settings.value("enableLightingCheckBox", ui->enableLightingCheckBox->isChecked()).toBool();
    ui->enableLightingCheckBox->setChecked(bVal);
    bVal = settings.value("enableShadowsCheckBox", ui->enableShadowsCheckBox->isChecked()).toBool();
    ui->enableShadowsCheckBox->setChecked(bVal);
    iVal = settings.value("ambientLightSlider", ui->ambientLightSlider->value()).toInt();
    ui->ambientLightSlider->setValue(iVal);
    iVal = settings.value("diffuseLightSlider", ui->diffuseLightSlider->value()).toInt();
    ui->diffuseLightSlider->setValue(iVal);
    iVal = settings.value("specularLightSlider", ui->specularLightSlider->value()).toInt();
    ui->specularLightSlider->setValue(iVal);
    QString sval = settings.value("lineEditTextureDir", ui->lineEditTextureDir->text()).toString();
    ui->lineEditTextureDir->setText(sval);
    iVal = settings.value("comboBoxDefaultMaterial", ui->comboBoxDefaultMaterial->currentIndex()).toInt();
    ui->comboBoxDefaultMaterial->setCurrentIndex(iVal);
    iVal = settings.value("comboUVMethod", ui->comboUVMethod->currentIndex()).toInt();
    ui->comboUVMethod->setCurrentIndex(iVal);
    dVal = settings.value("spinAngleThreshold", ui->spinAngleThreshold->value()).toDouble();
    ui->spinAngleThreshold->setValue(dVal);
    bVal = settings.value("checkPreserveUVs", ui->checkPreserveUVs->isChecked()).toBool();
    ui->checkPreserveUVs->setChecked(bVal);
    bVal = settings.value("checkAutoPackUVs", ui->checkAutoPackUVs->isChecked()).toBool();
    ui->checkAutoPackUVs->setChecked(bVal);
    bVal = settings.value("checkRelaxUVs", ui->checkRelaxUVs->isChecked()).toBool();
    ui->checkRelaxUVs->setChecked(bVal);
    bVal = settings.value("checkPCAProjection", ui->checkPCAProjection->isChecked()).toBool();
    ui->checkPCAProjection->setChecked(bVal);
    bVal = settings.value("checkXatlasPackingOnly", ui->checkXatlasPackingOnly->isChecked()).toBool();
    ui->checkXatlasPackingOnly->setChecked(bVal);
    bVal = settings.value("checkRememberUV", ui->checkRememberUV->isChecked()).toBool();
    ui->checkRememberUV->setChecked(bVal);
    iVal = settings.value("tessellationQualitySlider", ui->tessellationQualitySlider->value()).toInt();
    ui->tessellationQualitySlider->setValue(iVal);
    dVal = settings.value("linearDeflectionSpinBox", ui->linearDeflectionSpinBox->value()).toDouble();
    ui->linearDeflectionSpinBox->setValue(dVal);
    dVal = settings.value("angularDeflectionSpinBox", ui->angularDeflectionSpinBox->value()).toDouble();
    ui->angularDeflectionSpinBox->setValue(dVal);
    bVal = settings.value("occtUnifyFacesCheckBox", ui->occtUnifyFacesCheckBox->isChecked()).toBool();
    ui->occtUnifyFacesCheckBox->setChecked(bVal);
    bVal = settings.value("occtUnifyEdgesCheckBox", ui->occtUnifyEdgesCheckBox->isChecked()).toBool();
    ui->occtUnifyEdgesCheckBox->setChecked(bVal);
    bVal = settings.value("occtBuildCurvesCheckBox", ui->occtBuildCurvesCheckBox->isChecked()).toBool();
    ui->occtBuildCurvesCheckBox->setChecked(bVal);
    bVal = settings.value("assimpTriangulateCheckBox", ui->assimpTriangulateCheckBox->isChecked()).toBool();
    ui->assimpTriangulateCheckBox->setChecked(bVal);
    bVal = settings.value("assimpGenNormalsCheckBox", ui->assimpGenNormalsCheckBox->isChecked()).toBool();
    ui->assimpGenNormalsCheckBox->setChecked(bVal);
    bVal = settings.value("assimpSmoothNormalsCheckBox", ui->assimpSmoothNormalsCheckBox->isChecked()).toBool();
    ui->assimpSmoothNormalsCheckBox->setChecked(bVal);
    bVal = settings.value("assimpCalcTangentsCheckBox", ui->assimpCalcTangentsCheckBox->isChecked()).toBool();
    ui->assimpCalcTangentsCheckBox->setChecked(bVal);
    bVal = settings.value("assimpOptimizeMeshCheckBox", ui->assimpOptimizeMeshCheckBox->isChecked()).toBool();
    ui->assimpOptimizeMeshCheckBox->setChecked(bVal);
    bVal = settings.value("assimpRemoveDuplicatesCheckBox", ui->assimpRemoveDuplicatesCheckBox->isChecked()).toBool();
    ui->assimpRemoveDuplicatesCheckBox->setChecked(bVal);
    iVal = settings.value("assimpMaxFaceVerticesSpinBox", ui->assimpMaxFaceVerticesSpinBox->value()).toInt();
    ui->assimpMaxFaceVerticesSpinBox->setValue(iVal);
    bVal = settings.value("checkMultithreadedLoad", ui->checkMultithreadedLoad->isChecked()).toBool();
    ui->checkMultithreadedLoad->setChecked(bVal);
    iVal = settings.value("spinThreadLimit", ui->spinThreadLimit->value()).toInt();
    ui->spinThreadLimit->setValue(iVal);
    bVal = settings.value("checkSkyboxBlending", ui->checkSkyboxBlending->isChecked()).toBool();
    ui->checkSkyboxBlending->setChecked(bVal);
    bVal = settings.value("checkProgressiveLoading", ui->checkProgressiveLoading->isChecked()).toBool();
    ui->checkProgressiveLoading->setChecked(bVal);
    iVal = settings.value("maxFpsSpinBox", ui->maxFpsSpinBox->value()).toInt();
    ui->maxFpsSpinBox->setValue(iVal);
    bVal = settings.value("vsyncCheckBox", ui->vsyncCheckBox->isChecked()).toBool();
    ui->vsyncCheckBox->setChecked(bVal);
    bVal = settings.value("frustumCullingCheckBox", ui->frustumCullingCheckBox->isChecked()).toBool();
    ui->frustumCullingCheckBox->setChecked(bVal);
    bVal = settings.value("backfaceCullingCheckBox", ui->backfaceCullingCheckBox->isChecked()).toBool();
    ui->backfaceCullingCheckBox->setChecked(bVal);
    bVal = settings.value("levelOfDetailCheckBox", ui->levelOfDetailCheckBox->isChecked()).toBool();
    ui->levelOfDetailCheckBox->setChecked(bVal);
    iVal = settings.value("maxVerticesSpinBox", ui->maxVerticesSpinBox->value()).toInt();
    ui->maxVerticesSpinBox->setValue(iVal);
    iVal = settings.value("textureCacheSizeSpinBox", ui->textureCacheSizeSpinBox->value()).toInt();
    ui->textureCacheSizeSpinBox->setValue(iVal);
    iVal = settings.value("geometryCacheSizeSpinBox", ui->geometryCacheSizeSpinBox->value()).toInt();
    ui->geometryCacheSizeSpinBox->setValue(iVal);
    bVal = settings.value("compressTexturesCheckBox", ui->compressTexturesCheckBox->isChecked()).toBool();
    ui->compressTexturesCheckBox->setChecked(bVal);
    bVal = settings.value("generateMipmapsCheckBox", ui->generateMipmapsCheckBox->isChecked()).toBool();
    ui->generateMipmapsCheckBox->setChecked(bVal);
    iVal = settings.value("comboBoxOpenGLVersion", ui->comboBoxOpenGLVersion->currentIndex()).toInt();
    ui->comboBoxOpenGLVersion->setCurrentIndex(iVal);
    bVal = settings.value("checkBoxVSync", ui->checkBoxVSync->isChecked()).toBool();
    ui->checkBoxVSync->setChecked(bVal);
    bVal = settings.value("checkShaderHotReload", ui->checkShaderHotReload->isChecked()).toBool();
    ui->checkShaderHotReload->setChecked(bVal);
    bVal = settings.value("checkShowFPS", ui->checkShowFPS->isChecked()).toBool();
    ui->checkShowFPS->setChecked(bVal);
    bVal = settings.value("checkLegacyOpenGL", ui->checkLegacyOpenGL->isChecked()).toBool();
    ui->checkLegacyOpenGL->setChecked(bVal);
    iVal = settings.value("spinBoxThreads", ui->spinBoxThreads->value()).toInt();
    ui->spinBoxThreads->setValue(iVal);
    bVal = settings.value("showFpsCheckBox", ui->showFpsCheckBox->isChecked()).toBool();
    ui->showFpsCheckBox->setChecked(bVal);
    bVal = settings.value("showMemoryUsageCheckBox", ui->showMemoryUsageCheckBox->isChecked()).toBool();
    ui->showMemoryUsageCheckBox->setChecked(bVal);
    bVal = settings.value("showRenderStatsCheckBox", ui->showRenderStatsCheckBox->isChecked()).toBool();
    ui->showRenderStatsCheckBox->setChecked(bVal);
    bVal = settings.value("showOpenGLInfoCheckBox", ui->showOpenGLInfoCheckBox->isChecked()).toBool();
    ui->showOpenGLInfoCheckBox->setChecked(bVal);
    bVal = settings.value("enableLoggingCheckBox", ui->enableLoggingCheckBox->isChecked()).toBool();
    ui->enableLoggingCheckBox->setChecked(bVal);
    iVal = settings.value("logLevelComboBox", ui->logLevelComboBox->currentIndex()).toInt();
    ui->logLevelComboBox->setCurrentIndex(iVal);
    bVal = settings.value("checkOpenGLErrorsCheckBox", ui->checkOpenGLErrorsCheckBox->isChecked()).toBool();
    ui->checkOpenGLErrorsCheckBox->setChecked(bVal);
    bVal = settings.value("validateShadersCheckBox", ui->validateShadersCheckBox->isChecked()).toBool();
    ui->validateShadersCheckBox->setChecked(bVal);
    bVal = settings.value("profileRenderingCheckBox", ui->profileRenderingCheckBox->isChecked()).toBool();
    ui->profileRenderingCheckBox->setChecked(bVal);
}

void SettingsDialog::saveSettings()
{
	qDebug() << "Saving settings to QSettings...";
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue("comboBoxTheme", ui->comboBoxTheme->currentIndex());
    settings.setValue("comboBoxLanguage", ui->comboBoxLanguage->currentIndex());
    settings.setValue("checkPromptOverwrite", ui->checkPromptOverwrite->isChecked());
    settings.setValue("checkRestoreLastFile", ui->checkRestoreLastFile->isChecked());
    settings.setValue("checkTooltips", ui->checkTooltips->isChecked());
    settings.setValue("checkConfirmExit", ui->checkConfirmExit->isChecked());
    settings.setValue("comboProjectionMode", ui->comboProjectionMode->currentIndex());
    settings.setValue("comboDefaultView", ui->comboDefaultView->currentIndex());
    settings.setValue("comboDefaultProjection", ui->comboDefaultProjection->currentIndex());
    settings.setValue("checkTrackball", ui->checkTrackball->isChecked());
    settings.setValue("checkInvertZoom", ui->checkInvertZoom->isChecked());
    settings.setValue("spinZoomFactor", ui->spinZoomFactor->value());
    settings.setValue("comboBoxBackgroundStyle", ui->comboBoxBackgroundStyle->currentIndex());
    settings.setValue("comboBoxGradientStyle", ui->comboBoxGradientStyle->currentIndex());
    settings.setValue("showBoundingBoxCheckBox", ui->showBoundingBoxCheckBox->isChecked());
    settings.setValue("showCornerTrihedronCheckBox", ui->showCornerTrihedronCheckBox->isChecked());
    settings.setValue("farPlaneSpinBox", ui->farPlaneSpinBox->value());
    settings.setValue("fieldOfViewSpinBox", ui->fieldOfViewSpinBox->value());
    settings.setValue("showGridCheckBox", ui->showGridCheckBox->isChecked());
    settings.setValue("nearPlaneSpinBox", ui->nearPlaneSpinBox->value());
    settings.setValue("showWireframeCheckBox", ui->showWireframeCheckBox->isChecked());
    settings.setValue("showCenterTrihedronCheckBox", ui->showCenterTrihedronCheckBox->isChecked());
    settings.setValue("navigationModeComboBox", ui->navigationModeComboBox->currentIndex());
    settings.setValue("mouseSensitivitySlider", ui->mouseSensitivitySlider->value());
    settings.setValue("zoomSensitivitySlider", ui->zoomSensitivitySlider->value());
    settings.setValue("invertYAxisCheckBox", ui->invertYAxisCheckBox->isChecked());
    settings.setValue("smoothNavigationCheckBox", ui->smoothNavigationCheckBox->isChecked());
    settings.setValue("comboShadingMode", ui->comboShadingMode->currentIndex());
    settings.setValue("checkBackfaceCulling", ui->checkBackfaceCulling->isChecked());
    settings.setValue("checkNormalMap", ui->checkNormalMap->isChecked());
    settings.setValue("shaderModelComboBox", ui->shaderModelComboBox->currentIndex());
    settings.setValue("msaaComboBox", ui->msaaComboBox->currentIndex());
    settings.setValue("anisotropyComboBox", ui->anisotropyComboBox->currentIndex());
    settings.setValue("enableLightingCheckBox", ui->enableLightingCheckBox->isChecked());
    settings.setValue("enableShadowsCheckBox", ui->enableShadowsCheckBox->isChecked());
    settings.setValue("ambientLightSlider", ui->ambientLightSlider->value());
    settings.setValue("diffuseLightSlider", ui->diffuseLightSlider->value());
    settings.setValue("specularLightSlider", ui->specularLightSlider->value());
    settings.setValue("lineEditTextureDir", ui->lineEditTextureDir->text());
    settings.setValue("comboBoxDefaultMaterial", ui->comboBoxDefaultMaterial->currentIndex());
    settings.setValue("comboUVMethod", ui->comboUVMethod->currentIndex());
    settings.setValue("spinAngleThreshold", ui->spinAngleThreshold->value());
    settings.setValue("checkPreserveUVs", ui->checkPreserveUVs->isChecked());
    settings.setValue("checkAutoPackUVs", ui->checkAutoPackUVs->isChecked());
    settings.setValue("checkRelaxUVs", ui->checkRelaxUVs->isChecked());
    settings.setValue("checkPCAProjection", ui->checkPCAProjection->isChecked());
    settings.setValue("checkXatlasPackingOnly", ui->checkXatlasPackingOnly->isChecked());
    settings.setValue("checkRememberUV", ui->checkRememberUV->isChecked());
    settings.setValue("tessellationQualitySlider", ui->tessellationQualitySlider->value());
    settings.setValue("linearDeflectionSpinBox", ui->linearDeflectionSpinBox->value());
    settings.setValue("angularDeflectionSpinBox", ui->angularDeflectionSpinBox->value());
    settings.setValue("occtUnifyFacesCheckBox", ui->occtUnifyFacesCheckBox->isChecked());
    settings.setValue("occtUnifyEdgesCheckBox", ui->occtUnifyEdgesCheckBox->isChecked());
    settings.setValue("occtBuildCurvesCheckBox", ui->occtBuildCurvesCheckBox->isChecked());
    settings.setValue("assimpTriangulateCheckBox", ui->assimpTriangulateCheckBox->isChecked());
    settings.setValue("assimpGenNormalsCheckBox", ui->assimpGenNormalsCheckBox->isChecked());
    settings.setValue("assimpSmoothNormalsCheckBox", ui->assimpSmoothNormalsCheckBox->isChecked());
    settings.setValue("assimpCalcTangentsCheckBox", ui->assimpCalcTangentsCheckBox->isChecked());
    settings.setValue("assimpOptimizeMeshCheckBox", ui->assimpOptimizeMeshCheckBox->isChecked());
    settings.setValue("assimpRemoveDuplicatesCheckBox", ui->assimpRemoveDuplicatesCheckBox->isChecked());
    settings.setValue("assimpMaxFaceVerticesSpinBox", ui->assimpMaxFaceVerticesSpinBox->value());
    settings.setValue("checkMultithreadedLoad", ui->checkMultithreadedLoad->isChecked());
    settings.setValue("spinThreadLimit", ui->spinThreadLimit->value());
    settings.setValue("checkSkyboxBlending", ui->checkSkyboxBlending->isChecked());
    settings.setValue("checkProgressiveLoading", ui->checkProgressiveLoading->isChecked());
    settings.setValue("maxFpsSpinBox", ui->maxFpsSpinBox->value());
    settings.setValue("vsyncCheckBox", ui->vsyncCheckBox->isChecked());
    settings.setValue("frustumCullingCheckBox", ui->frustumCullingCheckBox->isChecked());
    settings.setValue("backfaceCullingCheckBox", ui->backfaceCullingCheckBox->isChecked());
    settings.setValue("levelOfDetailCheckBox", ui->levelOfDetailCheckBox->isChecked());
    settings.setValue("maxVerticesSpinBox", ui->maxVerticesSpinBox->value());
    settings.setValue("textureCacheSizeSpinBox", ui->textureCacheSizeSpinBox->value());
    settings.setValue("geometryCacheSizeSpinBox", ui->geometryCacheSizeSpinBox->value());
    settings.setValue("compressTexturesCheckBox", ui->compressTexturesCheckBox->isChecked());
    settings.setValue("generateMipmapsCheckBox", ui->generateMipmapsCheckBox->isChecked());
    settings.setValue("comboBoxOpenGLVersion", ui->comboBoxOpenGLVersion->currentIndex());
    settings.setValue("checkBoxVSync", ui->checkBoxVSync->isChecked());
    settings.setValue("checkShaderHotReload", ui->checkShaderHotReload->isChecked());
    settings.setValue("checkShowFPS", ui->checkShowFPS->isChecked());
    settings.setValue("checkLegacyOpenGL", ui->checkLegacyOpenGL->isChecked());
    settings.setValue("spinBoxThreads", ui->spinBoxThreads->value());
    settings.setValue("showFpsCheckBox", ui->showFpsCheckBox->isChecked());
    settings.setValue("showMemoryUsageCheckBox", ui->showMemoryUsageCheckBox->isChecked());
    settings.setValue("showRenderStatsCheckBox", ui->showRenderStatsCheckBox->isChecked());
    settings.setValue("showOpenGLInfoCheckBox", ui->showOpenGLInfoCheckBox->isChecked());
    settings.setValue("enableLoggingCheckBox", ui->enableLoggingCheckBox->isChecked());
    settings.setValue("logLevelComboBox", ui->logLevelComboBox->currentIndex());
    settings.setValue("checkOpenGLErrorsCheckBox", ui->checkOpenGLErrorsCheckBox->isChecked());
    settings.setValue("validateShadersCheckBox", ui->validateShadersCheckBox->isChecked());
    settings.setValue("profileRenderingCheckBox", ui->profileRenderingCheckBox->isChecked());
}

void SettingsDialog::restoreDefaults()
{
    qDebug() << "Restoring default settings...";
    // Helper lambda to safely restore widget values
    auto restoreWidget = [](QWidget* widget, const QVariant& defaultValue) {
        if (!widget || !defaultValue.isValid()) return;

        // Handle different widget types appropriately
        if (auto* comboBox = qobject_cast<QComboBox*>(widget))
        {
            int index = defaultValue.toInt();
            if (index >= 0 && index < comboBox->count())
            {
                comboBox->setCurrentIndex(index);
            }
        }
        else if (auto* checkBox = qobject_cast<QCheckBox*>(widget))
        {
            checkBox->setChecked(defaultValue.toBool());
        }
        else if (auto* spinBox = qobject_cast<QSpinBox*>(widget))
        {
            spinBox->setValue(defaultValue.toInt());
        }
        else if (auto* doubleSpinBox = qobject_cast<QDoubleSpinBox*>(widget))
        {
            doubleSpinBox->setValue(defaultValue.toDouble());
        }
        else if (auto* slider = qobject_cast<QSlider*>(widget))
        {
            slider->setValue(defaultValue.toInt());
        }
        else if (auto* lineEdit = qobject_cast<QLineEdit*>(widget))
        {
            lineEdit->setText(defaultValue.toString());
        }
        else if (auto* pushButton = qobject_cast<QPushButton*>(widget))
        {
            // For color buttons, restore the color property
            if (widget->objectName().contains("Color"))
            {
                QColor color = defaultValue.value<QColor>();
                if (color.isValid())
                {
                    widget->setStyleSheet(QString("background-color: %1").arg(color.name()));
                    widget->setProperty("color", color);
                }
            }
        }
        };

    // List of all widgets that need to be restored
    QList<QWidget*> widgetsToRestore = {
        ui->comboBoxTheme, ui->comboBoxLanguage, ui->checkPromptOverwrite,
        ui->checkRestoreLastFile, ui->checkTooltips, ui->checkConfirmExit,
        ui->comboProjectionMode, ui->comboDefaultView, ui->comboDefaultProjection,
        ui->checkTrackball, ui->checkInvertZoom, ui->spinZoomFactor,
        ui->comboBoxBackgroundStyle, ui->pushButtonTopColor, ui->pushButtonBottomColor,
        ui->comboBoxGradientStyle, ui->showBoundingBoxCheckBox, ui->showCornerTrihedronCheckBox,
        ui->farPlaneSpinBox, ui->fieldOfViewSpinBox, ui->showGridCheckBox,
        ui->nearPlaneSpinBox, ui->showWireframeCheckBox, ui->showCenterTrihedronCheckBox,
        ui->navigationModeComboBox, ui->mouseSensitivitySlider, ui->zoomSensitivitySlider,
        ui->invertYAxisCheckBox, ui->smoothNavigationCheckBox, ui->comboShadingMode,
        ui->checkBackfaceCulling, ui->checkNormalMap, ui->shaderModelComboBox,
        ui->msaaComboBox, ui->anisotropyComboBox, ui->enableLightingCheckBox,
        ui->enableShadowsCheckBox, ui->ambientLightSlider, ui->diffuseLightSlider,
        ui->specularLightSlider, ui->lineEditTextureDir, ui->comboBoxDefaultMaterial,
        ui->comboUVMethod, ui->spinAngleThreshold, ui->checkPreserveUVs,
        ui->checkAutoPackUVs, ui->checkRelaxUVs, ui->checkPCAProjection,
        ui->checkXatlasPackingOnly, ui->checkRememberUV, ui->buttonResetUVPrompt,
        ui->tessellationQualitySlider, ui->linearDeflectionSpinBox, ui->angularDeflectionSpinBox,
        ui->occtUnifyFacesCheckBox, ui->occtUnifyEdgesCheckBox, ui->occtBuildCurvesCheckBox,
        ui->assimpTriangulateCheckBox, ui->assimpGenNormalsCheckBox, ui->assimpSmoothNormalsCheckBox,
        ui->assimpCalcTangentsCheckBox, ui->assimpOptimizeMeshCheckBox, ui->assimpRemoveDuplicatesCheckBox,
        ui->assimpMaxFaceVerticesSpinBox, ui->checkMultithreadedLoad, ui->spinThreadLimit,
        ui->checkSkyboxBlending, ui->checkProgressiveLoading, ui->maxFpsSpinBox,
        ui->vsyncCheckBox, ui->frustumCullingCheckBox, ui->backfaceCullingCheckBox,
        ui->levelOfDetailCheckBox, ui->maxVerticesSpinBox, ui->textureCacheSizeSpinBox,
        ui->geometryCacheSizeSpinBox, ui->compressTexturesCheckBox, ui->generateMipmapsCheckBox,
        ui->comboBoxOpenGLVersion, ui->checkBoxVSync, ui->checkShaderHotReload,
        ui->checkShowFPS, ui->checkLegacyOpenGL, ui->spinBoxThreads,
        ui->showFpsCheckBox, ui->showMemoryUsageCheckBox, ui->showRenderStatsCheckBox,
        ui->showOpenGLInfoCheckBox, ui->enableLoggingCheckBox, ui->logLevelComboBox,
        ui->checkOpenGLErrorsCheckBox, ui->validateShadersCheckBox, ui->profileRenderingCheckBox,
        ui->clearCacheButton, ui->resetSettingsButton
    };

    // Restore all widgets using the helper function
    for (QWidget* widget : widgetsToRestore)
    {
        if (widget)
        {
            restoreWidget(widget, widget->property("defaultValue"));
        }
    }

    qDebug() << "Default settings restored successfully.";
}

void SettingsDialog::on_comboBoxTheme_currentIndexChanged()
{
    // TODO: Handle comboBoxTheme::currentIndexChanged(int)
}

void SettingsDialog::on_comboBoxLanguage_currentIndexChanged()
{
    // TODO: Handle comboBoxLanguage::currentIndexChanged(int)
}

void SettingsDialog::on_checkPromptOverwrite_stateChanged()
{
    // TODO: Handle checkPromptOverwrite::stateChanged(int)
}

void SettingsDialog::on_checkRestoreLastFile_stateChanged()
{
    // TODO: Handle checkRestoreLastFile::stateChanged(int)
}

void SettingsDialog::on_checkTooltips_stateChanged()
{
    // TODO: Handle checkTooltips::stateChanged(int)
}

void SettingsDialog::on_checkConfirmExit_stateChanged()
{
    // TODO: Handle checkConfirmExit::stateChanged(int)
}

void SettingsDialog::on_comboProjectionMode_currentIndexChanged()
{
    // TODO: Handle comboProjectionMode::currentIndexChanged(int)
}

void SettingsDialog::on_comboDefaultView_currentIndexChanged()
{
    // TODO: Handle comboDefaultView::currentIndexChanged(int)
}

void SettingsDialog::on_comboDefaultProjection_currentIndexChanged()
{
    // TODO: Handle comboDefaultProjection::currentIndexChanged(int)
}

void SettingsDialog::on_checkTrackball_stateChanged()
{
    // TODO: Handle checkTrackball::stateChanged(int)
}

void SettingsDialog::on_checkInvertZoom_stateChanged()
{
    // TODO: Handle checkInvertZoom::stateChanged(int)
}

void SettingsDialog::on_spinZoomFactor_valueChanged()
{
    // TODO: Handle spinZoomFactor::valueChanged(double)
}

void SettingsDialog::on_comboBoxBackgroundStyle_currentIndexChanged()
{
    // TODO: Handle comboBoxBackgroundStyle::currentIndexChanged(int)
}

void SettingsDialog::on_pushButtonTopColor_clicked()
{
    // TODO: Handle pushButtonTopColor::clicked()
}

void SettingsDialog::on_pushButtonBottomColor_clicked()
{
    // TODO: Handle pushButtonBottomColor::clicked()
}

void SettingsDialog::on_comboBoxGradientStyle_currentIndexChanged()
{
    // TODO: Handle comboBoxGradientStyle::currentIndexChanged(int)
}

void SettingsDialog::on_showBoundingBoxCheckBox_stateChanged()
{
    // TODO: Handle showBoundingBoxCheckBox::stateChanged(int)
}

void SettingsDialog::on_showCornerTrihedronCheckBox_stateChanged()
{
    // TODO: Handle showCornerTrihedronCheckBox::stateChanged(int)
}

void SettingsDialog::on_farPlaneSpinBox_valueChanged()
{
    // TODO: Handle farPlaneSpinBox::valueChanged(double)
}

void SettingsDialog::on_fieldOfViewSpinBox_valueChanged()
{
    // TODO: Handle fieldOfViewSpinBox::valueChanged(int)
}

void SettingsDialog::on_showGridCheckBox_stateChanged()
{
    // TODO: Handle showGridCheckBox::stateChanged(int)
}

void SettingsDialog::on_nearPlaneSpinBox_valueChanged()
{
    // TODO: Handle nearPlaneSpinBox::valueChanged(double)
}

void SettingsDialog::on_showWireframeCheckBox_stateChanged()
{
    // TODO: Handle showWireframeCheckBox::stateChanged(int)
}

void SettingsDialog::on_showCenterTrihedronCheckBox_stateChanged()
{
    // TODO: Handle showCenterTrihedronCheckBox::stateChanged(int)
}

void SettingsDialog::on_navigationModeComboBox_currentIndexChanged()
{
    // TODO: Handle navigationModeComboBox::currentIndexChanged(int)
}

void SettingsDialog::on_mouseSensitivitySlider_valueChanged()
{
    // TODO: Handle mouseSensitivitySlider::valueChanged(int)
}

void SettingsDialog::on_zoomSensitivitySlider_valueChanged()
{
    // TODO: Handle zoomSensitivitySlider::valueChanged(int)
}

void SettingsDialog::on_invertYAxisCheckBox_stateChanged()
{
    // TODO: Handle invertYAxisCheckBox::stateChanged(int)
}

void SettingsDialog::on_smoothNavigationCheckBox_stateChanged()
{
    // TODO: Handle smoothNavigationCheckBox::stateChanged(int)
}

void SettingsDialog::on_comboShadingMode_currentIndexChanged()
{
    // TODO: Handle comboShadingMode::currentIndexChanged(int)
}

void SettingsDialog::on_checkBackfaceCulling_stateChanged()
{
    // TODO: Handle checkBackfaceCulling::stateChanged(int)
}

void SettingsDialog::on_checkNormalMap_stateChanged()
{
    // TODO: Handle checkNormalMap::stateChanged(int)
}

void SettingsDialog::on_shaderModelComboBox_currentIndexChanged()
{
    // TODO: Handle shaderModelComboBox::currentIndexChanged(int)
}

void SettingsDialog::on_msaaComboBox_currentIndexChanged()
{
    // TODO: Handle msaaComboBox::currentIndexChanged(int)
}

void SettingsDialog::on_anisotropyComboBox_currentIndexChanged()
{
    // TODO: Handle anisotropyComboBox::currentIndexChanged(int)
}

void SettingsDialog::on_enableLightingCheckBox_stateChanged()
{
    // TODO: Handle enableLightingCheckBox::stateChanged(int)
}

void SettingsDialog::on_enableShadowsCheckBox_stateChanged()
{
    // TODO: Handle enableShadowsCheckBox::stateChanged(int)
}

void SettingsDialog::on_ambientLightSlider_valueChanged()
{
    // TODO: Handle ambientLightSlider::valueChanged(int)
}

void SettingsDialog::on_diffuseLightSlider_valueChanged()
{
    // TODO: Handle diffuseLightSlider::valueChanged(int)
}

void SettingsDialog::on_specularLightSlider_valueChanged()
{
    // TODO: Handle specularLightSlider::valueChanged(int)
}

void SettingsDialog::on_lineEditTextureDir_textChanged()
{
    // TODO: Handle lineEditTextureDir::textChanged(QString)
}

void SettingsDialog::on_comboBoxDefaultMaterial_currentIndexChanged()
{
    // TODO: Handle comboBoxDefaultMaterial::currentIndexChanged(int)
}

void SettingsDialog::on_comboUVMethod_currentIndexChanged()
{
    // TODO: Handle comboUVMethod::currentIndexChanged(int)
}

void SettingsDialog::on_spinAngleThreshold_valueChanged()
{
    // TODO: Handle spinAngleThreshold::valueChanged(double)
}

void SettingsDialog::on_checkPreserveUVs_stateChanged()
{
    // TODO: Handle checkPreserveUVs::stateChanged(int)
}

void SettingsDialog::on_checkAutoPackUVs_stateChanged()
{
    // TODO: Handle checkAutoPackUVs::stateChanged(int)
}

void SettingsDialog::on_checkRelaxUVs_stateChanged()
{
    // TODO: Handle checkRelaxUVs::stateChanged(int)
}

void SettingsDialog::on_checkPCAProjection_stateChanged()
{
    // TODO: Handle checkPCAProjection::stateChanged(int)
}

void SettingsDialog::on_checkXatlasPackingOnly_stateChanged()
{
    // TODO: Handle checkXatlasPackingOnly::stateChanged(int)
}

void SettingsDialog::on_checkRememberUV_stateChanged()
{
    // TODO: Handle checkRememberUV::stateChanged(int)
}

void SettingsDialog::on_buttonResetUVPrompt_clicked()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.remove("UVMethod");
    settings.remove("RememberUVMethod");

    qDebug() << "UV Prompt settings have been reset.";
    QMessageBox::information(this, "Settings Reset", "UV Prompt settings have been cleared.");
}

void SettingsDialog::on_tessellationQualitySlider_valueChanged()
{
    // TODO: Handle tessellationQualitySlider::valueChanged(int)
}

void SettingsDialog::on_linearDeflectionSpinBox_valueChanged()
{
    // TODO: Handle linearDeflectionSpinBox::valueChanged(double)
}

void SettingsDialog::on_angularDeflectionSpinBox_valueChanged()
{
    // TODO: Handle angularDeflectionSpinBox::valueChanged(double)
}

void SettingsDialog::on_occtUnifyFacesCheckBox_stateChanged()
{
    // TODO: Handle occtUnifyFacesCheckBox::stateChanged(int)
}

void SettingsDialog::on_occtUnifyEdgesCheckBox_stateChanged()
{
    // TODO: Handle occtUnifyEdgesCheckBox::stateChanged(int)
}

void SettingsDialog::on_occtBuildCurvesCheckBox_stateChanged()
{
    // TODO: Handle occtBuildCurvesCheckBox::stateChanged(int)
}

void SettingsDialog::on_assimpTriangulateCheckBox_stateChanged()
{
    // TODO: Handle assimpTriangulateCheckBox::stateChanged(int)
}

void SettingsDialog::on_assimpGenNormalsCheckBox_stateChanged()
{
    // TODO: Handle assimpGenNormalsCheckBox::stateChanged(int)
}

void SettingsDialog::on_assimpSmoothNormalsCheckBox_stateChanged()
{
    // TODO: Handle assimpSmoothNormalsCheckBox::stateChanged(int)
}

void SettingsDialog::on_assimpCalcTangentsCheckBox_stateChanged()
{
    // TODO: Handle assimpCalcTangentsCheckBox::stateChanged(int)
}

void SettingsDialog::on_assimpOptimizeMeshCheckBox_stateChanged()
{
    // TODO: Handle assimpOptimizeMeshCheckBox::stateChanged(int)
}

void SettingsDialog::on_assimpRemoveDuplicatesCheckBox_stateChanged()
{
    // TODO: Handle assimpRemoveDuplicatesCheckBox::stateChanged(int)
}

void SettingsDialog::on_assimpMaxFaceVerticesSpinBox_valueChanged()
{
    // TODO: Handle assimpMaxFaceVerticesSpinBox::valueChanged(int)
}

void SettingsDialog::on_checkMultithreadedLoad_stateChanged()
{
    // TODO: Handle checkMultithreadedLoad::stateChanged(int)
}

void SettingsDialog::on_spinThreadLimit_valueChanged()
{
    // TODO: Handle spinThreadLimit::valueChanged(int)
}

void SettingsDialog::on_checkSkyboxBlending_stateChanged()
{
    // TODO: Handle checkSkyboxBlending::stateChanged(int)
}

void SettingsDialog::on_checkProgressiveLoading_stateChanged()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
	settings.setValue("checkProgressiveLoading", ui->checkProgressiveLoading->isChecked());
}

void SettingsDialog::on_maxFpsSpinBox_valueChanged()
{
    // TODO: Handle maxFpsSpinBox::valueChanged(int)
}

void SettingsDialog::on_vsyncCheckBox_stateChanged()
{
    // TODO: Handle vsyncCheckBox::stateChanged(int)
}

void SettingsDialog::on_frustumCullingCheckBox_stateChanged()
{
    // TODO: Handle frustumCullingCheckBox::stateChanged(int)
}

void SettingsDialog::on_backfaceCullingCheckBox_stateChanged()
{
    // TODO: Handle backfaceCullingCheckBox::stateChanged(int)
}

void SettingsDialog::on_levelOfDetailCheckBox_stateChanged()
{
    // TODO: Handle levelOfDetailCheckBox::stateChanged(int)
}

void SettingsDialog::on_maxVerticesSpinBox_valueChanged()
{
    // TODO: Handle maxVerticesSpinBox::valueChanged(int)
}

void SettingsDialog::on_textureCacheSizeSpinBox_valueChanged()
{
    // TODO: Handle textureCacheSizeSpinBox::valueChanged(int)
}

void SettingsDialog::on_geometryCacheSizeSpinBox_valueChanged()
{
    // TODO: Handle geometryCacheSizeSpinBox::valueChanged(int)
}

void SettingsDialog::on_compressTexturesCheckBox_stateChanged()
{
    // TODO: Handle compressTexturesCheckBox::stateChanged(int)
}

void SettingsDialog::on_generateMipmapsCheckBox_stateChanged()
{
    // TODO: Handle generateMipmapsCheckBox::stateChanged(int)
}

void SettingsDialog::on_comboBoxOpenGLVersion_currentIndexChanged()
{
    // TODO: Handle comboBoxOpenGLVersion::currentIndexChanged(int)
}

void SettingsDialog::on_checkBoxVSync_stateChanged()
{
    // TODO: Handle checkBoxVSync::stateChanged(int)
}

void SettingsDialog::on_checkShaderHotReload_stateChanged()
{
    // TODO: Handle checkShaderHotReload::stateChanged(int)
}

void SettingsDialog::on_checkShowFPS_stateChanged()
{
    // TODO: Handle checkShowFPS::stateChanged(int)
}

void SettingsDialog::on_checkLegacyOpenGL_stateChanged()
{
    // TODO: Handle checkLegacyOpenGL::stateChanged(int)
}

void SettingsDialog::on_spinBoxThreads_valueChanged()
{
    // TODO: Handle spinBoxThreads::valueChanged(int)
}

void SettingsDialog::on_showFpsCheckBox_stateChanged()
{
    // TODO: Handle showFpsCheckBox::stateChanged(int)
}

void SettingsDialog::on_showMemoryUsageCheckBox_stateChanged()
{
    // TODO: Handle showMemoryUsageCheckBox::stateChanged(int)
}

void SettingsDialog::on_showRenderStatsCheckBox_stateChanged()
{
    // TODO: Handle showRenderStatsCheckBox::stateChanged(int)
}

void SettingsDialog::on_showOpenGLInfoCheckBox_stateChanged()
{
    // TODO: Handle showOpenGLInfoCheckBox::stateChanged(int)
}

void SettingsDialog::on_enableLoggingCheckBox_stateChanged()
{
    // TODO: Handle enableLoggingCheckBox::stateChanged(int)
}

void SettingsDialog::on_logLevelComboBox_currentIndexChanged()
{
    // TODO: Handle logLevelComboBox::currentIndexChanged(int)
}

void SettingsDialog::on_checkOpenGLErrorsCheckBox_stateChanged()
{
    // TODO: Handle checkOpenGLErrorsCheckBox::stateChanged(int)
}

void SettingsDialog::on_validateShadersCheckBox_stateChanged()
{
    // TODO: Handle validateShadersCheckBox::stateChanged(int)
}

void SettingsDialog::on_profileRenderingCheckBox_stateChanged()
{
    // TODO: Handle profileRenderingCheckBox::stateChanged(int)
}

void SettingsDialog::on_clearCacheButton_clicked()
{
    // TODO: Handle clearCacheButton::clicked()
}

void SettingsDialog::on_resetSettingsButton_clicked()
{
    // TODO: Handle resetSettingsButton::clicked()
}

