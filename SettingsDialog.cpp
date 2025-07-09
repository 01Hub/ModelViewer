#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include <QMessageBox>


SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);

	_settings = std::make_unique<QSettings>(new QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName()));

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
}

SettingsDialog::~SettingsDialog()
{
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
    _settings->clear();
	restoreDefaults();
    qDebug() << "All settings have been reset to defaults.";
    QMessageBox::information(this, "Settings Reset", "All settings have been cleared.");
}

void SettingsDialog::applySettings()
{

}

void SettingsDialog::loadSettings()
{    
    if (ui->comboBoxTheme) ui->comboBoxTheme->setProperty("value", _settings->value("comboBoxTheme", ui->comboBoxTheme->property("value")));
    if (ui->comboBoxLanguage) ui->comboBoxLanguage->setProperty("value", _settings->value("comboBoxLanguage", ui->comboBoxLanguage->property("value")));
    if (ui->checkPromptOverwrite) ui->checkPromptOverwrite->setProperty("value", _settings->value("checkPromptOverwrite", ui->checkPromptOverwrite->property("value")));
    if (ui->checkRestoreLastFile) ui->checkRestoreLastFile->setProperty("value", _settings->value("checkRestoreLastFile", ui->checkRestoreLastFile->property("value")));
    if (ui->checkTooltips) ui->checkTooltips->setProperty("value", _settings->value("checkTooltips", ui->checkTooltips->property("value")));
    if (ui->checkConfirmExit) ui->checkConfirmExit->setProperty("value", _settings->value("checkConfirmExit", ui->checkConfirmExit->property("value")));
    if (ui->comboProjectionMode) ui->comboProjectionMode->setProperty("value", _settings->value("comboProjectionMode", ui->comboProjectionMode->property("value")));
    if (ui->comboDefaultView) ui->comboDefaultView->setProperty("value", _settings->value("comboDefaultView", ui->comboDefaultView->property("value")));
    if (ui->comboDefaultProjection) ui->comboDefaultProjection->setProperty("value", _settings->value("comboDefaultProjection", ui->comboDefaultProjection->property("value")));
    if (ui->checkTrackball) ui->checkTrackball->setProperty("value", _settings->value("checkTrackball", ui->checkTrackball->property("value")));
    if (ui->checkInvertZoom) ui->checkInvertZoom->setProperty("value", _settings->value("checkInvertZoom", ui->checkInvertZoom->property("value")));
    if (ui->spinZoomFactor) ui->spinZoomFactor->setProperty("value", _settings->value("spinZoomFactor", ui->spinZoomFactor->property("value")));
    if (ui->comboBoxBackgroundStyle) ui->comboBoxBackgroundStyle->setProperty("value", _settings->value("comboBoxBackgroundStyle", ui->comboBoxBackgroundStyle->property("value")));
    if (ui->pushButtonTopColor) ui->pushButtonTopColor->setProperty("value", _settings->value("pushButtonTopColor", ui->pushButtonTopColor->property("value")));
    if (ui->pushButtonBottomColor) ui->pushButtonBottomColor->setProperty("value", _settings->value("pushButtonBottomColor", ui->pushButtonBottomColor->property("value")));
    if (ui->comboBoxGradientStyle) ui->comboBoxGradientStyle->setProperty("value", _settings->value("comboBoxGradientStyle", ui->comboBoxGradientStyle->property("value")));
    if (ui->showBoundingBoxCheckBox) ui->showBoundingBoxCheckBox->setProperty("value", _settings->value("showBoundingBoxCheckBox", ui->showBoundingBoxCheckBox->property("value")));
    if (ui->showCornerTrihedronCheckBox) ui->showCornerTrihedronCheckBox->setProperty("value", _settings->value("showCornerTrihedronCheckBox", ui->showCornerTrihedronCheckBox->property("value")));
    if (ui->farPlaneSpinBox) ui->farPlaneSpinBox->setProperty("value", _settings->value("farPlaneSpinBox", ui->farPlaneSpinBox->property("value")));
    if (ui->fieldOfViewSpinBox) ui->fieldOfViewSpinBox->setProperty("value", _settings->value("fieldOfViewSpinBox", ui->fieldOfViewSpinBox->property("value")));
    if (ui->showGridCheckBox) ui->showGridCheckBox->setProperty("value", _settings->value("showGridCheckBox", ui->showGridCheckBox->property("value")));
    if (ui->nearPlaneSpinBox) ui->nearPlaneSpinBox->setProperty("value", _settings->value("nearPlaneSpinBox", ui->nearPlaneSpinBox->property("value")));
    if (ui->showWireframeCheckBox) ui->showWireframeCheckBox->setProperty("value", _settings->value("showWireframeCheckBox", ui->showWireframeCheckBox->property("value")));
    if (ui->showCenterTrihedronCheckBox) ui->showCenterTrihedronCheckBox->setProperty("value", _settings->value("showCenterTrihedronCheckBox", ui->showCenterTrihedronCheckBox->property("value")));
    if (ui->navigationModeComboBox) ui->navigationModeComboBox->setProperty("value", _settings->value("navigationModeComboBox", ui->navigationModeComboBox->property("value")));
    if (ui->mouseSensitivitySlider) ui->mouseSensitivitySlider->setProperty("value", _settings->value("mouseSensitivitySlider", ui->mouseSensitivitySlider->property("value")));
    if (ui->zoomSensitivitySlider) ui->zoomSensitivitySlider->setProperty("value", _settings->value("zoomSensitivitySlider", ui->zoomSensitivitySlider->property("value")));
    if (ui->invertYAxisCheckBox) ui->invertYAxisCheckBox->setProperty("value", _settings->value("invertYAxisCheckBox", ui->invertYAxisCheckBox->property("value")));
    if (ui->smoothNavigationCheckBox) ui->smoothNavigationCheckBox->setProperty("value", _settings->value("smoothNavigationCheckBox", ui->smoothNavigationCheckBox->property("value")));
    if (ui->comboShadingMode) ui->comboShadingMode->setProperty("value", _settings->value("comboShadingMode", ui->comboShadingMode->property("value")));
    if (ui->checkBackfaceCulling) ui->checkBackfaceCulling->setProperty("value", _settings->value("checkBackfaceCulling", ui->checkBackfaceCulling->property("value")));
    if (ui->checkNormalMap) ui->checkNormalMap->setProperty("value", _settings->value("checkNormalMap", ui->checkNormalMap->property("value")));
    if (ui->shaderModelComboBox) ui->shaderModelComboBox->setProperty("value", _settings->value("shaderModelComboBox", ui->shaderModelComboBox->property("value")));
    if (ui->msaaComboBox) ui->msaaComboBox->setProperty("value", _settings->value("msaaComboBox", ui->msaaComboBox->property("value")));
    if (ui->anisotropyComboBox) ui->anisotropyComboBox->setProperty("value", _settings->value("anisotropyComboBox", ui->anisotropyComboBox->property("value")));
    if (ui->enableLightingCheckBox) ui->enableLightingCheckBox->setProperty("value", _settings->value("enableLightingCheckBox", ui->enableLightingCheckBox->property("value")));
    if (ui->enableShadowsCheckBox) ui->enableShadowsCheckBox->setProperty("value", _settings->value("enableShadowsCheckBox", ui->enableShadowsCheckBox->property("value")));
    if (ui->ambientLightSlider) ui->ambientLightSlider->setProperty("value", _settings->value("ambientLightSlider", ui->ambientLightSlider->property("value")));
    if (ui->diffuseLightSlider) ui->diffuseLightSlider->setProperty("value", _settings->value("diffuseLightSlider", ui->diffuseLightSlider->property("value")));
    if (ui->specularLightSlider) ui->specularLightSlider->setProperty("value", _settings->value("specularLightSlider", ui->specularLightSlider->property("value")));
    if (ui->lineEditTextureDir) ui->lineEditTextureDir->setProperty("value", _settings->value("lineEditTextureDir", ui->lineEditTextureDir->property("value")));
    if (ui->comboBoxDefaultMaterial) ui->comboBoxDefaultMaterial->setProperty("value", _settings->value("comboBoxDefaultMaterial", ui->comboBoxDefaultMaterial->property("value")));
    if (ui->comboUVMethod) ui->comboUVMethod->setProperty("value", _settings->value("comboUVMethod", ui->comboUVMethod->property("value")));
    if (ui->spinAngleThreshold) ui->spinAngleThreshold->setProperty("value", _settings->value("spinAngleThreshold", ui->spinAngleThreshold->property("value")));
    if (ui->checkPreserveUVs) ui->checkPreserveUVs->setProperty("value", _settings->value("checkPreserveUVs", ui->checkPreserveUVs->property("value")));
    if (ui->checkAutoPackUVs) ui->checkAutoPackUVs->setProperty("value", _settings->value("checkAutoPackUVs", ui->checkAutoPackUVs->property("value")));
    if (ui->checkRelaxUVs) ui->checkRelaxUVs->setProperty("value", _settings->value("checkRelaxUVs", ui->checkRelaxUVs->property("value")));
    if (ui->checkPCAProjection) ui->checkPCAProjection->setProperty("value", _settings->value("checkPCAProjection", ui->checkPCAProjection->property("value")));
    if (ui->checkXatlasPackingOnly) ui->checkXatlasPackingOnly->setProperty("value", _settings->value("checkXatlasPackingOnly", ui->checkXatlasPackingOnly->property("value")));
    if (ui->checkRememberUV) ui->checkRememberUV->setProperty("value", _settings->value("checkRememberUV", ui->checkRememberUV->property("value")));
    if (ui->buttonResetUVPrompt) ui->buttonResetUVPrompt->setProperty("value", _settings->value("buttonResetUVPrompt", ui->buttonResetUVPrompt->property("value")));
    if (ui->tessellationQualitySlider) ui->tessellationQualitySlider->setProperty("value", _settings->value("tessellationQualitySlider", ui->tessellationQualitySlider->property("value")));
    if (ui->linearDeflectionSpinBox) ui->linearDeflectionSpinBox->setProperty("value", _settings->value("linearDeflectionSpinBox", ui->linearDeflectionSpinBox->property("value")));
    if (ui->angularDeflectionSpinBox) ui->angularDeflectionSpinBox->setProperty("value", _settings->value("angularDeflectionSpinBox", ui->angularDeflectionSpinBox->property("value")));
    if (ui->occtUnifyFacesCheckBox) ui->occtUnifyFacesCheckBox->setProperty("value", _settings->value("occtUnifyFacesCheckBox", ui->occtUnifyFacesCheckBox->property("value")));
    if (ui->occtUnifyEdgesCheckBox) ui->occtUnifyEdgesCheckBox->setProperty("value", _settings->value("occtUnifyEdgesCheckBox", ui->occtUnifyEdgesCheckBox->property("value")));
    if (ui->occtBuildCurvesCheckBox) ui->occtBuildCurvesCheckBox->setProperty("value", _settings->value("occtBuildCurvesCheckBox", ui->occtBuildCurvesCheckBox->property("value")));
    if (ui->assimpTriangulateCheckBox) ui->assimpTriangulateCheckBox->setProperty("value", _settings->value("assimpTriangulateCheckBox", ui->assimpTriangulateCheckBox->property("value")));
    if (ui->assimpGenNormalsCheckBox) ui->assimpGenNormalsCheckBox->setProperty("value", _settings->value("assimpGenNormalsCheckBox", ui->assimpGenNormalsCheckBox->property("value")));
    if (ui->assimpSmoothNormalsCheckBox) ui->assimpSmoothNormalsCheckBox->setProperty("value", _settings->value("assimpSmoothNormalsCheckBox", ui->assimpSmoothNormalsCheckBox->property("value")));
    if (ui->assimpCalcTangentsCheckBox) ui->assimpCalcTangentsCheckBox->setProperty("value", _settings->value("assimpCalcTangentsCheckBox", ui->assimpCalcTangentsCheckBox->property("value")));
    if (ui->assimpOptimizeMeshCheckBox) ui->assimpOptimizeMeshCheckBox->setProperty("value", _settings->value("assimpOptimizeMeshCheckBox", ui->assimpOptimizeMeshCheckBox->property("value")));
    if (ui->assimpRemoveDuplicatesCheckBox) ui->assimpRemoveDuplicatesCheckBox->setProperty("value", _settings->value("assimpRemoveDuplicatesCheckBox", ui->assimpRemoveDuplicatesCheckBox->property("value")));
    if (ui->assimpMaxFaceVerticesSpinBox) ui->assimpMaxFaceVerticesSpinBox->setProperty("value", _settings->value("assimpMaxFaceVerticesSpinBox", ui->assimpMaxFaceVerticesSpinBox->property("value")));
    if (ui->checkMultithreadedLoad) ui->checkMultithreadedLoad->setProperty("value", _settings->value("checkMultithreadedLoad", ui->checkMultithreadedLoad->property("value")));
    if (ui->spinThreadLimit) ui->spinThreadLimit->setProperty("value", _settings->value("spinThreadLimit", ui->spinThreadLimit->property("value")));
    if (ui->checkSkyboxBlending) ui->checkSkyboxBlending->setProperty("value", _settings->value("checkSkyboxBlending", ui->checkSkyboxBlending->property("value")));
    if (ui->checkProgressiveLoading) ui->checkProgressiveLoading->setProperty("value", _settings->value("checkProgressiveLoading", ui->checkProgressiveLoading->property("value")));
    if (ui->maxFpsSpinBox) ui->maxFpsSpinBox->setProperty("value", _settings->value("maxFpsSpinBox", ui->maxFpsSpinBox->property("value")));
    if (ui->vsyncCheckBox) ui->vsyncCheckBox->setProperty("value", _settings->value("vsyncCheckBox", ui->vsyncCheckBox->property("value")));
    if (ui->frustumCullingCheckBox) ui->frustumCullingCheckBox->setProperty("value", _settings->value("frustumCullingCheckBox", ui->frustumCullingCheckBox->property("value")));
    if (ui->backfaceCullingCheckBox) ui->backfaceCullingCheckBox->setProperty("value", _settings->value("backfaceCullingCheckBox", ui->backfaceCullingCheckBox->property("value")));
    if (ui->levelOfDetailCheckBox) ui->levelOfDetailCheckBox->setProperty("value", _settings->value("levelOfDetailCheckBox", ui->levelOfDetailCheckBox->property("value")));
    if (ui->maxVerticesSpinBox) ui->maxVerticesSpinBox->setProperty("value", _settings->value("maxVerticesSpinBox", ui->maxVerticesSpinBox->property("value")));
    if (ui->textureCacheSizeSpinBox) ui->textureCacheSizeSpinBox->setProperty("value", _settings->value("textureCacheSizeSpinBox", ui->textureCacheSizeSpinBox->property("value")));
    if (ui->geometryCacheSizeSpinBox) ui->geometryCacheSizeSpinBox->setProperty("value", _settings->value("geometryCacheSizeSpinBox", ui->geometryCacheSizeSpinBox->property("value")));
    if (ui->compressTexturesCheckBox) ui->compressTexturesCheckBox->setProperty("value", _settings->value("compressTexturesCheckBox", ui->compressTexturesCheckBox->property("value")));
    if (ui->generateMipmapsCheckBox) ui->generateMipmapsCheckBox->setProperty("value", _settings->value("generateMipmapsCheckBox", ui->generateMipmapsCheckBox->property("value")));
    if (ui->comboBoxOpenGLVersion) ui->comboBoxOpenGLVersion->setProperty("value", _settings->value("comboBoxOpenGLVersion", ui->comboBoxOpenGLVersion->property("value")));
    if (ui->checkBoxVSync) ui->checkBoxVSync->setProperty("value", _settings->value("checkBoxVSync", ui->checkBoxVSync->property("value")));
    if (ui->checkShaderHotReload) ui->checkShaderHotReload->setProperty("value", _settings->value("checkShaderHotReload", ui->checkShaderHotReload->property("value")));
    if (ui->checkShowFPS) ui->checkShowFPS->setProperty("value", _settings->value("checkShowFPS", ui->checkShowFPS->property("value")));
    if (ui->checkLegacyOpenGL) ui->checkLegacyOpenGL->setProperty("value", _settings->value("checkLegacyOpenGL", ui->checkLegacyOpenGL->property("value")));
    if (ui->spinBoxThreads) ui->spinBoxThreads->setProperty("value", _settings->value("spinBoxThreads", ui->spinBoxThreads->property("value")));
    if (ui->showFpsCheckBox) ui->showFpsCheckBox->setProperty("value", _settings->value("showFpsCheckBox", ui->showFpsCheckBox->property("value")));
    if (ui->showMemoryUsageCheckBox) ui->showMemoryUsageCheckBox->setProperty("value", _settings->value("showMemoryUsageCheckBox", ui->showMemoryUsageCheckBox->property("value")));
    if (ui->showRenderStatsCheckBox) ui->showRenderStatsCheckBox->setProperty("value", _settings->value("showRenderStatsCheckBox", ui->showRenderStatsCheckBox->property("value")));
    if (ui->showOpenGLInfoCheckBox) ui->showOpenGLInfoCheckBox->setProperty("value", _settings->value("showOpenGLInfoCheckBox", ui->showOpenGLInfoCheckBox->property("value")));
    if (ui->enableLoggingCheckBox) ui->enableLoggingCheckBox->setProperty("value", _settings->value("enableLoggingCheckBox", ui->enableLoggingCheckBox->property("value")));
    if (ui->logLevelComboBox) ui->logLevelComboBox->setProperty("value", _settings->value("logLevelComboBox", ui->logLevelComboBox->property("value")));
    if (ui->checkOpenGLErrorsCheckBox) ui->checkOpenGLErrorsCheckBox->setProperty("value", _settings->value("checkOpenGLErrorsCheckBox", ui->checkOpenGLErrorsCheckBox->property("value")));
    if (ui->validateShadersCheckBox) ui->validateShadersCheckBox->setProperty("value", _settings->value("validateShadersCheckBox", ui->validateShadersCheckBox->property("value")));
    if (ui->profileRenderingCheckBox) ui->profileRenderingCheckBox->setProperty("value", _settings->value("profileRenderingCheckBox", ui->profileRenderingCheckBox->property("value")));
    if (ui->clearCacheButton) ui->clearCacheButton->setProperty("value", _settings->value("clearCacheButton", ui->clearCacheButton->property("value")));
    if (ui->resetSettingsButton) ui->resetSettingsButton->setProperty("value", _settings->value("resetSettingsButton", ui->resetSettingsButton->property("value")));
}

void SettingsDialog::saveSettings()
{    
    if (ui->comboBoxTheme) _settings->setValue("comboBoxTheme", ui->comboBoxTheme->property("value"));
    if (ui->comboBoxLanguage) _settings->setValue("comboBoxLanguage", ui->comboBoxLanguage->property("value"));
    if (ui->checkPromptOverwrite) _settings->setValue("checkPromptOverwrite", ui->checkPromptOverwrite->property("value"));
    if (ui->checkRestoreLastFile) _settings->setValue("checkRestoreLastFile", ui->checkRestoreLastFile->property("value"));
    if (ui->checkTooltips) _settings->setValue("checkTooltips", ui->checkTooltips->property("value"));
    if (ui->checkConfirmExit) _settings->setValue("checkConfirmExit", ui->checkConfirmExit->property("value"));
    if (ui->comboProjectionMode) _settings->setValue("comboProjectionMode", ui->comboProjectionMode->property("value"));
    if (ui->comboDefaultView) _settings->setValue("comboDefaultView", ui->comboDefaultView->property("value"));
    if (ui->comboDefaultProjection) _settings->setValue("comboDefaultProjection", ui->comboDefaultProjection->property("value"));
    if (ui->checkTrackball) _settings->setValue("checkTrackball", ui->checkTrackball->property("value"));
    if (ui->checkInvertZoom) _settings->setValue("checkInvertZoom", ui->checkInvertZoom->property("value"));
    if (ui->spinZoomFactor) _settings->setValue("spinZoomFactor", ui->spinZoomFactor->property("value"));
    if (ui->comboBoxBackgroundStyle) _settings->setValue("comboBoxBackgroundStyle", ui->comboBoxBackgroundStyle->property("value"));
    if (ui->pushButtonTopColor) _settings->setValue("pushButtonTopColor", ui->pushButtonTopColor->property("value"));
    if (ui->pushButtonBottomColor) _settings->setValue("pushButtonBottomColor", ui->pushButtonBottomColor->property("value"));
    if (ui->comboBoxGradientStyle) _settings->setValue("comboBoxGradientStyle", ui->comboBoxGradientStyle->property("value"));
    if (ui->showBoundingBoxCheckBox) _settings->setValue("showBoundingBoxCheckBox", ui->showBoundingBoxCheckBox->property("value"));
    if (ui->showCornerTrihedronCheckBox) _settings->setValue("showCornerTrihedronCheckBox", ui->showCornerTrihedronCheckBox->property("value"));
    if (ui->farPlaneSpinBox) _settings->setValue("farPlaneSpinBox", ui->farPlaneSpinBox->property("value"));
    if (ui->fieldOfViewSpinBox) _settings->setValue("fieldOfViewSpinBox", ui->fieldOfViewSpinBox->property("value"));
    if (ui->showGridCheckBox) _settings->setValue("showGridCheckBox", ui->showGridCheckBox->property("value"));
    if (ui->nearPlaneSpinBox) _settings->setValue("nearPlaneSpinBox", ui->nearPlaneSpinBox->property("value"));
    if (ui->showWireframeCheckBox) _settings->setValue("showWireframeCheckBox", ui->showWireframeCheckBox->property("value"));
    if (ui->showCenterTrihedronCheckBox) _settings->setValue("showCenterTrihedronCheckBox", ui->showCenterTrihedronCheckBox->property("value"));
    if (ui->navigationModeComboBox) _settings->setValue("navigationModeComboBox", ui->navigationModeComboBox->property("value"));
    if (ui->mouseSensitivitySlider) _settings->setValue("mouseSensitivitySlider", ui->mouseSensitivitySlider->property("value"));
    if (ui->zoomSensitivitySlider) _settings->setValue("zoomSensitivitySlider", ui->zoomSensitivitySlider->property("value"));
    if (ui->invertYAxisCheckBox) _settings->setValue("invertYAxisCheckBox", ui->invertYAxisCheckBox->property("value"));
    if (ui->smoothNavigationCheckBox) _settings->setValue("smoothNavigationCheckBox", ui->smoothNavigationCheckBox->property("value"));
    if (ui->comboShadingMode) _settings->setValue("comboShadingMode", ui->comboShadingMode->property("value"));
    if (ui->checkBackfaceCulling) _settings->setValue("checkBackfaceCulling", ui->checkBackfaceCulling->property("value"));
    if (ui->checkNormalMap) _settings->setValue("checkNormalMap", ui->checkNormalMap->property("value"));
    if (ui->shaderModelComboBox) _settings->setValue("shaderModelComboBox", ui->shaderModelComboBox->property("value"));
    if (ui->msaaComboBox) _settings->setValue("msaaComboBox", ui->msaaComboBox->property("value"));
    if (ui->anisotropyComboBox) _settings->setValue("anisotropyComboBox", ui->anisotropyComboBox->property("value"));
    if (ui->enableLightingCheckBox) _settings->setValue("enableLightingCheckBox", ui->enableLightingCheckBox->property("value"));
    if (ui->enableShadowsCheckBox) _settings->setValue("enableShadowsCheckBox", ui->enableShadowsCheckBox->property("value"));
    if (ui->ambientLightSlider) _settings->setValue("ambientLightSlider", ui->ambientLightSlider->property("value"));
    if (ui->diffuseLightSlider) _settings->setValue("diffuseLightSlider", ui->diffuseLightSlider->property("value"));
    if (ui->specularLightSlider) _settings->setValue("specularLightSlider", ui->specularLightSlider->property("value"));
    if (ui->lineEditTextureDir) _settings->setValue("lineEditTextureDir", ui->lineEditTextureDir->property("value"));
    if (ui->comboBoxDefaultMaterial) _settings->setValue("comboBoxDefaultMaterial", ui->comboBoxDefaultMaterial->property("value"));
    if (ui->comboUVMethod) _settings->setValue("comboUVMethod", ui->comboUVMethod->property("value"));
    if (ui->spinAngleThreshold) _settings->setValue("spinAngleThreshold", ui->spinAngleThreshold->property("value"));
    if (ui->checkPreserveUVs) _settings->setValue("checkPreserveUVs", ui->checkPreserveUVs->property("value"));
    if (ui->checkAutoPackUVs) _settings->setValue("checkAutoPackUVs", ui->checkAutoPackUVs->property("value"));
    if (ui->checkRelaxUVs) _settings->setValue("checkRelaxUVs", ui->checkRelaxUVs->property("value"));
    if (ui->checkPCAProjection) _settings->setValue("checkPCAProjection", ui->checkPCAProjection->property("value"));
    if (ui->checkXatlasPackingOnly) _settings->setValue("checkXatlasPackingOnly", ui->checkXatlasPackingOnly->property("value"));
    if (ui->checkRememberUV) _settings->setValue("checkRememberUV", ui->checkRememberUV->property("value"));
    if (ui->buttonResetUVPrompt) _settings->setValue("buttonResetUVPrompt", ui->buttonResetUVPrompt->property("value"));
    if (ui->tessellationQualitySlider) _settings->setValue("tessellationQualitySlider", ui->tessellationQualitySlider->property("value"));
    if (ui->linearDeflectionSpinBox) _settings->setValue("linearDeflectionSpinBox", ui->linearDeflectionSpinBox->property("value"));
    if (ui->angularDeflectionSpinBox) _settings->setValue("angularDeflectionSpinBox", ui->angularDeflectionSpinBox->property("value"));
    if (ui->occtUnifyFacesCheckBox) _settings->setValue("occtUnifyFacesCheckBox", ui->occtUnifyFacesCheckBox->property("value"));
    if (ui->occtUnifyEdgesCheckBox) _settings->setValue("occtUnifyEdgesCheckBox", ui->occtUnifyEdgesCheckBox->property("value"));
    if (ui->occtBuildCurvesCheckBox) _settings->setValue("occtBuildCurvesCheckBox", ui->occtBuildCurvesCheckBox->property("value"));
    if (ui->assimpTriangulateCheckBox) _settings->setValue("assimpTriangulateCheckBox", ui->assimpTriangulateCheckBox->property("value"));
    if (ui->assimpGenNormalsCheckBox) _settings->setValue("assimpGenNormalsCheckBox", ui->assimpGenNormalsCheckBox->property("value"));
    if (ui->assimpSmoothNormalsCheckBox) _settings->setValue("assimpSmoothNormalsCheckBox", ui->assimpSmoothNormalsCheckBox->property("value"));
    if (ui->assimpCalcTangentsCheckBox) _settings->setValue("assimpCalcTangentsCheckBox", ui->assimpCalcTangentsCheckBox->property("value"));
    if (ui->assimpOptimizeMeshCheckBox) _settings->setValue("assimpOptimizeMeshCheckBox", ui->assimpOptimizeMeshCheckBox->property("value"));
    if (ui->assimpRemoveDuplicatesCheckBox) _settings->setValue("assimpRemoveDuplicatesCheckBox", ui->assimpRemoveDuplicatesCheckBox->property("value"));
    if (ui->assimpMaxFaceVerticesSpinBox) _settings->setValue("assimpMaxFaceVerticesSpinBox", ui->assimpMaxFaceVerticesSpinBox->property("value"));
    if (ui->checkMultithreadedLoad) _settings->setValue("checkMultithreadedLoad", ui->checkMultithreadedLoad->property("value"));
    if (ui->spinThreadLimit) _settings->setValue("spinThreadLimit", ui->spinThreadLimit->property("value"));
    if (ui->checkSkyboxBlending) _settings->setValue("checkSkyboxBlending", ui->checkSkyboxBlending->property("value"));
    if (ui->checkProgressiveLoading) _settings->setValue("checkProgressiveLoading", ui->checkProgressiveLoading->property("value"));
    if (ui->maxFpsSpinBox) _settings->setValue("maxFpsSpinBox", ui->maxFpsSpinBox->property("value"));
    if (ui->vsyncCheckBox) _settings->setValue("vsyncCheckBox", ui->vsyncCheckBox->property("value"));
    if (ui->frustumCullingCheckBox) _settings->setValue("frustumCullingCheckBox", ui->frustumCullingCheckBox->property("value"));
    if (ui->backfaceCullingCheckBox) _settings->setValue("backfaceCullingCheckBox", ui->backfaceCullingCheckBox->property("value"));
    if (ui->levelOfDetailCheckBox) _settings->setValue("levelOfDetailCheckBox", ui->levelOfDetailCheckBox->property("value"));
    if (ui->maxVerticesSpinBox) _settings->setValue("maxVerticesSpinBox", ui->maxVerticesSpinBox->property("value"));
    if (ui->textureCacheSizeSpinBox) _settings->setValue("textureCacheSizeSpinBox", ui->textureCacheSizeSpinBox->property("value"));
    if (ui->geometryCacheSizeSpinBox) _settings->setValue("geometryCacheSizeSpinBox", ui->geometryCacheSizeSpinBox->property("value"));
    if (ui->compressTexturesCheckBox) _settings->setValue("compressTexturesCheckBox", ui->compressTexturesCheckBox->property("value"));
    if (ui->generateMipmapsCheckBox) _settings->setValue("generateMipmapsCheckBox", ui->generateMipmapsCheckBox->property("value"));
    if (ui->comboBoxOpenGLVersion) _settings->setValue("comboBoxOpenGLVersion", ui->comboBoxOpenGLVersion->property("value"));
    if (ui->checkBoxVSync) _settings->setValue("checkBoxVSync", ui->checkBoxVSync->property("value"));
    if (ui->checkShaderHotReload) _settings->setValue("checkShaderHotReload", ui->checkShaderHotReload->property("value"));
    if (ui->checkShowFPS) _settings->setValue("checkShowFPS", ui->checkShowFPS->property("value"));
    if (ui->checkLegacyOpenGL) _settings->setValue("checkLegacyOpenGL", ui->checkLegacyOpenGL->property("value"));
    if (ui->spinBoxThreads) _settings->setValue("spinBoxThreads", ui->spinBoxThreads->property("value"));
    if (ui->showFpsCheckBox) _settings->setValue("showFpsCheckBox", ui->showFpsCheckBox->property("value"));
    if (ui->showMemoryUsageCheckBox) _settings->setValue("showMemoryUsageCheckBox", ui->showMemoryUsageCheckBox->property("value"));
    if (ui->showRenderStatsCheckBox) _settings->setValue("showRenderStatsCheckBox", ui->showRenderStatsCheckBox->property("value"));
    if (ui->showOpenGLInfoCheckBox) _settings->setValue("showOpenGLInfoCheckBox", ui->showOpenGLInfoCheckBox->property("value"));
    if (ui->enableLoggingCheckBox) _settings->setValue("enableLoggingCheckBox", ui->enableLoggingCheckBox->property("value"));
    if (ui->logLevelComboBox) _settings->setValue("logLevelComboBox", ui->logLevelComboBox->property("value"));
    if (ui->checkOpenGLErrorsCheckBox) _settings->setValue("checkOpenGLErrorsCheckBox", ui->checkOpenGLErrorsCheckBox->property("value"));
    if (ui->validateShadersCheckBox) _settings->setValue("validateShadersCheckBox", ui->validateShadersCheckBox->property("value"));
    if (ui->profileRenderingCheckBox) _settings->setValue("profileRenderingCheckBox", ui->profileRenderingCheckBox->property("value"));
    if (ui->clearCacheButton) _settings->setValue("clearCacheButton", ui->clearCacheButton->property("value"));
    if (ui->resetSettingsButton) _settings->setValue("resetSettingsButton", ui->resetSettingsButton->property("value"));
}

void SettingsDialog::restoreDefaults()
{
    if (ui->comboBoxTheme) ui->comboBoxTheme->setProperty("value", ui->comboBoxTheme->property("defaultValue"));
    if (ui->comboBoxLanguage) ui->comboBoxLanguage->setProperty("value", ui->comboBoxLanguage->property("defaultValue"));
    if (ui->checkPromptOverwrite) ui->checkPromptOverwrite->setProperty("value", ui->checkPromptOverwrite->property("defaultValue"));
    if (ui->checkRestoreLastFile) ui->checkRestoreLastFile->setProperty("value", ui->checkRestoreLastFile->property("defaultValue"));
    if (ui->checkTooltips) ui->checkTooltips->setProperty("value", ui->checkTooltips->property("defaultValue"));
    if (ui->checkConfirmExit) ui->checkConfirmExit->setProperty("value", ui->checkConfirmExit->property("defaultValue"));
    if (ui->comboProjectionMode) ui->comboProjectionMode->setProperty("value", ui->comboProjectionMode->property("defaultValue"));
    if (ui->comboDefaultView) ui->comboDefaultView->setProperty("value", ui->comboDefaultView->property("defaultValue"));
    if (ui->comboDefaultProjection) ui->comboDefaultProjection->setProperty("value", ui->comboDefaultProjection->property("defaultValue"));
    if (ui->checkTrackball) ui->checkTrackball->setProperty("value", ui->checkTrackball->property("defaultValue"));
    if (ui->checkInvertZoom) ui->checkInvertZoom->setProperty("value", ui->checkInvertZoom->property("defaultValue"));
    if (ui->spinZoomFactor) ui->spinZoomFactor->setProperty("value", ui->spinZoomFactor->property("defaultValue"));
    if (ui->comboBoxBackgroundStyle) ui->comboBoxBackgroundStyle->setProperty("value", ui->comboBoxBackgroundStyle->property("defaultValue"));
    if (ui->pushButtonTopColor) ui->pushButtonTopColor->setProperty("value", ui->pushButtonTopColor->property("defaultValue"));
    if (ui->pushButtonBottomColor) ui->pushButtonBottomColor->setProperty("value", ui->pushButtonBottomColor->property("defaultValue"));
    if (ui->comboBoxGradientStyle) ui->comboBoxGradientStyle->setProperty("value", ui->comboBoxGradientStyle->property("defaultValue"));
    if (ui->showBoundingBoxCheckBox) ui->showBoundingBoxCheckBox->setProperty("value", ui->showBoundingBoxCheckBox->property("defaultValue"));
    if (ui->showCornerTrihedronCheckBox) ui->showCornerTrihedronCheckBox->setProperty("value", ui->showCornerTrihedronCheckBox->property("defaultValue"));
    if (ui->farPlaneSpinBox) ui->farPlaneSpinBox->setProperty("value", ui->farPlaneSpinBox->property("defaultValue"));
    if (ui->fieldOfViewSpinBox) ui->fieldOfViewSpinBox->setProperty("value", ui->fieldOfViewSpinBox->property("defaultValue"));
    if (ui->showGridCheckBox) ui->showGridCheckBox->setProperty("value", ui->showGridCheckBox->property("defaultValue"));
    if (ui->nearPlaneSpinBox) ui->nearPlaneSpinBox->setProperty("value", ui->nearPlaneSpinBox->property("defaultValue"));
    if (ui->showWireframeCheckBox) ui->showWireframeCheckBox->setProperty("value", ui->showWireframeCheckBox->property("defaultValue"));
    if (ui->showCenterTrihedronCheckBox) ui->showCenterTrihedronCheckBox->setProperty("value", ui->showCenterTrihedronCheckBox->property("defaultValue"));
    if (ui->navigationModeComboBox) ui->navigationModeComboBox->setProperty("value", ui->navigationModeComboBox->property("defaultValue"));
    if (ui->mouseSensitivitySlider) ui->mouseSensitivitySlider->setProperty("value", ui->mouseSensitivitySlider->property("defaultValue"));
    if (ui->zoomSensitivitySlider) ui->zoomSensitivitySlider->setProperty("value", ui->zoomSensitivitySlider->property("defaultValue"));
    if (ui->invertYAxisCheckBox) ui->invertYAxisCheckBox->setProperty("value", ui->invertYAxisCheckBox->property("defaultValue"));
    if (ui->smoothNavigationCheckBox) ui->smoothNavigationCheckBox->setProperty("value", ui->smoothNavigationCheckBox->property("defaultValue"));
    if (ui->comboShadingMode) ui->comboShadingMode->setProperty("value", ui->comboShadingMode->property("defaultValue"));
    if (ui->checkBackfaceCulling) ui->checkBackfaceCulling->setProperty("value", ui->checkBackfaceCulling->property("defaultValue"));
    if (ui->checkNormalMap) ui->checkNormalMap->setProperty("value", ui->checkNormalMap->property("defaultValue"));
    if (ui->shaderModelComboBox) ui->shaderModelComboBox->setProperty("value", ui->shaderModelComboBox->property("defaultValue"));
    if (ui->msaaComboBox) ui->msaaComboBox->setProperty("value", ui->msaaComboBox->property("defaultValue"));
    if (ui->anisotropyComboBox) ui->anisotropyComboBox->setProperty("value", ui->anisotropyComboBox->property("defaultValue"));
    if (ui->enableLightingCheckBox) ui->enableLightingCheckBox->setProperty("value", ui->enableLightingCheckBox->property("defaultValue"));
    if (ui->enableShadowsCheckBox) ui->enableShadowsCheckBox->setProperty("value", ui->enableShadowsCheckBox->property("defaultValue"));
    if (ui->ambientLightSlider) ui->ambientLightSlider->setProperty("value", ui->ambientLightSlider->property("defaultValue"));
    if (ui->diffuseLightSlider) ui->diffuseLightSlider->setProperty("value", ui->diffuseLightSlider->property("defaultValue"));
    if (ui->specularLightSlider) ui->specularLightSlider->setProperty("value", ui->specularLightSlider->property("defaultValue"));
    if (ui->lineEditTextureDir) ui->lineEditTextureDir->setProperty("value", ui->lineEditTextureDir->property("defaultValue"));
    if (ui->comboBoxDefaultMaterial) ui->comboBoxDefaultMaterial->setProperty("value", ui->comboBoxDefaultMaterial->property("defaultValue"));
    if (ui->comboUVMethod) ui->comboUVMethod->setProperty("value", ui->comboUVMethod->property("defaultValue"));
    if (ui->spinAngleThreshold) ui->spinAngleThreshold->setProperty("value", ui->spinAngleThreshold->property("defaultValue"));
    if (ui->checkPreserveUVs) ui->checkPreserveUVs->setProperty("value", ui->checkPreserveUVs->property("defaultValue"));
    if (ui->checkAutoPackUVs) ui->checkAutoPackUVs->setProperty("value", ui->checkAutoPackUVs->property("defaultValue"));
    if (ui->checkRelaxUVs) ui->checkRelaxUVs->setProperty("value", ui->checkRelaxUVs->property("defaultValue"));
    if (ui->checkPCAProjection) ui->checkPCAProjection->setProperty("value", ui->checkPCAProjection->property("defaultValue"));
    if (ui->checkXatlasPackingOnly) ui->checkXatlasPackingOnly->setProperty("value", ui->checkXatlasPackingOnly->property("defaultValue"));
    if (ui->checkRememberUV) ui->checkRememberUV->setProperty("value", ui->checkRememberUV->property("defaultValue"));
    if (ui->buttonResetUVPrompt) ui->buttonResetUVPrompt->setProperty("value", ui->buttonResetUVPrompt->property("defaultValue"));
    if (ui->tessellationQualitySlider) ui->tessellationQualitySlider->setProperty("value", ui->tessellationQualitySlider->property("defaultValue"));
    if (ui->linearDeflectionSpinBox) ui->linearDeflectionSpinBox->setProperty("value", ui->linearDeflectionSpinBox->property("defaultValue"));
    if (ui->angularDeflectionSpinBox) ui->angularDeflectionSpinBox->setProperty("value", ui->angularDeflectionSpinBox->property("defaultValue"));
    if (ui->occtUnifyFacesCheckBox) ui->occtUnifyFacesCheckBox->setProperty("value", ui->occtUnifyFacesCheckBox->property("defaultValue"));
    if (ui->occtUnifyEdgesCheckBox) ui->occtUnifyEdgesCheckBox->setProperty("value", ui->occtUnifyEdgesCheckBox->property("defaultValue"));
    if (ui->occtBuildCurvesCheckBox) ui->occtBuildCurvesCheckBox->setProperty("value", ui->occtBuildCurvesCheckBox->property("defaultValue"));
    if (ui->assimpTriangulateCheckBox) ui->assimpTriangulateCheckBox->setProperty("value", ui->assimpTriangulateCheckBox->property("defaultValue"));
    if (ui->assimpGenNormalsCheckBox) ui->assimpGenNormalsCheckBox->setProperty("value", ui->assimpGenNormalsCheckBox->property("defaultValue"));
    if (ui->assimpSmoothNormalsCheckBox) ui->assimpSmoothNormalsCheckBox->setProperty("value", ui->assimpSmoothNormalsCheckBox->property("defaultValue"));
    if (ui->assimpCalcTangentsCheckBox) ui->assimpCalcTangentsCheckBox->setProperty("value", ui->assimpCalcTangentsCheckBox->property("defaultValue"));
    if (ui->assimpOptimizeMeshCheckBox) ui->assimpOptimizeMeshCheckBox->setProperty("value", ui->assimpOptimizeMeshCheckBox->property("defaultValue"));
    if (ui->assimpRemoveDuplicatesCheckBox) ui->assimpRemoveDuplicatesCheckBox->setProperty("value", ui->assimpRemoveDuplicatesCheckBox->property("defaultValue"));
    if (ui->assimpMaxFaceVerticesSpinBox) ui->assimpMaxFaceVerticesSpinBox->setProperty("value", ui->assimpMaxFaceVerticesSpinBox->property("defaultValue"));
    if (ui->checkMultithreadedLoad) ui->checkMultithreadedLoad->setProperty("value", ui->checkMultithreadedLoad->property("defaultValue"));
    if (ui->spinThreadLimit) ui->spinThreadLimit->setProperty("value", ui->spinThreadLimit->property("defaultValue"));
    if (ui->checkSkyboxBlending) ui->checkSkyboxBlending->setProperty("value", ui->checkSkyboxBlending->property("defaultValue"));
    if (ui->checkProgressiveLoading) ui->checkProgressiveLoading->setProperty("value", ui->checkProgressiveLoading->property("defaultValue"));
    if (ui->maxFpsSpinBox) ui->maxFpsSpinBox->setProperty("value", ui->maxFpsSpinBox->property("defaultValue"));
    if (ui->vsyncCheckBox) ui->vsyncCheckBox->setProperty("value", ui->vsyncCheckBox->property("defaultValue"));
    if (ui->frustumCullingCheckBox) ui->frustumCullingCheckBox->setProperty("value", ui->frustumCullingCheckBox->property("defaultValue"));
    if (ui->backfaceCullingCheckBox) ui->backfaceCullingCheckBox->setProperty("value", ui->backfaceCullingCheckBox->property("defaultValue"));
    if (ui->levelOfDetailCheckBox) ui->levelOfDetailCheckBox->setProperty("value", ui->levelOfDetailCheckBox->property("defaultValue"));
    if (ui->maxVerticesSpinBox) ui->maxVerticesSpinBox->setProperty("value", ui->maxVerticesSpinBox->property("defaultValue"));
    if (ui->textureCacheSizeSpinBox) ui->textureCacheSizeSpinBox->setProperty("value", ui->textureCacheSizeSpinBox->property("defaultValue"));
    if (ui->geometryCacheSizeSpinBox) ui->geometryCacheSizeSpinBox->setProperty("value", ui->geometryCacheSizeSpinBox->property("defaultValue"));
    if (ui->compressTexturesCheckBox) ui->compressTexturesCheckBox->setProperty("value", ui->compressTexturesCheckBox->property("defaultValue"));
    if (ui->generateMipmapsCheckBox) ui->generateMipmapsCheckBox->setProperty("value", ui->generateMipmapsCheckBox->property("defaultValue"));
    if (ui->comboBoxOpenGLVersion) ui->comboBoxOpenGLVersion->setProperty("value", ui->comboBoxOpenGLVersion->property("defaultValue"));
    if (ui->checkBoxVSync) ui->checkBoxVSync->setProperty("value", ui->checkBoxVSync->property("defaultValue"));
    if (ui->checkShaderHotReload) ui->checkShaderHotReload->setProperty("value", ui->checkShaderHotReload->property("defaultValue"));
    if (ui->checkShowFPS) ui->checkShowFPS->setProperty("value", ui->checkShowFPS->property("defaultValue"));
    if (ui->checkLegacyOpenGL) ui->checkLegacyOpenGL->setProperty("value", ui->checkLegacyOpenGL->property("defaultValue"));
    if (ui->spinBoxThreads) ui->spinBoxThreads->setProperty("value", ui->spinBoxThreads->property("defaultValue"));
    if (ui->showFpsCheckBox) ui->showFpsCheckBox->setProperty("value", ui->showFpsCheckBox->property("defaultValue"));
    if (ui->showMemoryUsageCheckBox) ui->showMemoryUsageCheckBox->setProperty("value", ui->showMemoryUsageCheckBox->property("defaultValue"));
    if (ui->showRenderStatsCheckBox) ui->showRenderStatsCheckBox->setProperty("value", ui->showRenderStatsCheckBox->property("defaultValue"));
    if (ui->showOpenGLInfoCheckBox) ui->showOpenGLInfoCheckBox->setProperty("value", ui->showOpenGLInfoCheckBox->property("defaultValue"));
    if (ui->enableLoggingCheckBox) ui->enableLoggingCheckBox->setProperty("value", ui->enableLoggingCheckBox->property("defaultValue"));
    if (ui->logLevelComboBox) ui->logLevelComboBox->setProperty("value", ui->logLevelComboBox->property("defaultValue"));
    if (ui->checkOpenGLErrorsCheckBox) ui->checkOpenGLErrorsCheckBox->setProperty("value", ui->checkOpenGLErrorsCheckBox->property("defaultValue"));
    if (ui->validateShadersCheckBox) ui->validateShadersCheckBox->setProperty("value", ui->validateShadersCheckBox->property("defaultValue"));
    if (ui->profileRenderingCheckBox) ui->profileRenderingCheckBox->setProperty("value", ui->profileRenderingCheckBox->property("defaultValue"));
    if (ui->clearCacheButton) ui->clearCacheButton->setProperty("value", ui->clearCacheButton->property("defaultValue"));
    if (ui->resetSettingsButton) ui->resetSettingsButton->setProperty("value", ui->resetSettingsButton->property("defaultValue"));
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
    _settings->remove("UVMethod");
    _settings->remove("RememberUVMethod");

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
    // TODO: Handle checkProgressiveLoading::stateChanged(int)
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


