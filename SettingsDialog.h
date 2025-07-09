#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QSettings>

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

private slots:   
    void onOkClicked();
    void onCancelClicked();
    void onApplyClicked();
    void onRestoreDefaults();

    void on_comboBoxTheme_currentIndexChanged();
    void on_comboBoxLanguage_currentIndexChanged();
    void on_checkPromptOverwrite_stateChanged();
    void on_checkRestoreLastFile_stateChanged();
    void on_checkTooltips_stateChanged();
    void on_checkConfirmExit_stateChanged();
    void on_comboProjectionMode_currentIndexChanged();
    void on_comboDefaultView_currentIndexChanged();
    void on_comboDefaultProjection_currentIndexChanged();
    void on_checkTrackball_stateChanged();
    void on_checkInvertZoom_stateChanged();
    void on_spinZoomFactor_valueChanged();
    void on_comboBoxBackgroundStyle_currentIndexChanged();
    void on_pushButtonTopColor_clicked();
    void on_pushButtonBottomColor_clicked();
    void on_comboBoxGradientStyle_currentIndexChanged();
    void on_showBoundingBoxCheckBox_stateChanged();
    void on_showCornerTrihedronCheckBox_stateChanged();
    void on_farPlaneSpinBox_valueChanged();
    void on_fieldOfViewSpinBox_valueChanged();
    void on_showGridCheckBox_stateChanged();
    void on_nearPlaneSpinBox_valueChanged();
    void on_showWireframeCheckBox_stateChanged();
    void on_showCenterTrihedronCheckBox_stateChanged();
    void on_navigationModeComboBox_currentIndexChanged();
    void on_mouseSensitivitySlider_valueChanged();
    void on_zoomSensitivitySlider_valueChanged();
    void on_invertYAxisCheckBox_stateChanged();
    void on_smoothNavigationCheckBox_stateChanged();
    void on_comboShadingMode_currentIndexChanged();
    void on_checkBackfaceCulling_stateChanged();
    void on_checkNormalMap_stateChanged();
    void on_shaderModelComboBox_currentIndexChanged();
    void on_msaaComboBox_currentIndexChanged();
    void on_anisotropyComboBox_currentIndexChanged();
    void on_enableLightingCheckBox_stateChanged();
    void on_enableShadowsCheckBox_stateChanged();
    void on_ambientLightSlider_valueChanged();
    void on_diffuseLightSlider_valueChanged();
    void on_specularLightSlider_valueChanged();
    void on_lineEditTextureDir_textChanged();
    void on_comboBoxDefaultMaterial_currentIndexChanged();
    void on_comboUVMethod_currentIndexChanged();
    void on_spinAngleThreshold_valueChanged();
    void on_checkPreserveUVs_stateChanged();
    void on_checkAutoPackUVs_stateChanged();
    void on_checkRelaxUVs_stateChanged();
    void on_checkPCAProjection_stateChanged();
    void on_checkXatlasPackingOnly_stateChanged();
    void on_checkRememberUV_stateChanged();
    void on_buttonResetUVPrompt_clicked();
    void on_tessellationQualitySlider_valueChanged();
    void on_linearDeflectionSpinBox_valueChanged();
    void on_angularDeflectionSpinBox_valueChanged();
    void on_occtUnifyFacesCheckBox_stateChanged();
    void on_occtUnifyEdgesCheckBox_stateChanged();
    void on_occtBuildCurvesCheckBox_stateChanged();
    void on_assimpTriangulateCheckBox_stateChanged();
    void on_assimpGenNormalsCheckBox_stateChanged();
    void on_assimpSmoothNormalsCheckBox_stateChanged();
    void on_assimpCalcTangentsCheckBox_stateChanged();
    void on_assimpOptimizeMeshCheckBox_stateChanged();
    void on_assimpRemoveDuplicatesCheckBox_stateChanged();
    void on_assimpMaxFaceVerticesSpinBox_valueChanged();
    void on_checkMultithreadedLoad_stateChanged();
    void on_spinThreadLimit_valueChanged();
    void on_checkSkyboxBlending_stateChanged();
    void on_checkProgressiveLoading_stateChanged();
    void on_maxFpsSpinBox_valueChanged();
    void on_vsyncCheckBox_stateChanged();
    void on_frustumCullingCheckBox_stateChanged();
    void on_backfaceCullingCheckBox_stateChanged();
    void on_levelOfDetailCheckBox_stateChanged();
    void on_maxVerticesSpinBox_valueChanged();
    void on_textureCacheSizeSpinBox_valueChanged();
    void on_geometryCacheSizeSpinBox_valueChanged();
    void on_compressTexturesCheckBox_stateChanged();
    void on_generateMipmapsCheckBox_stateChanged();
    void on_comboBoxOpenGLVersion_currentIndexChanged();
    void on_checkBoxVSync_stateChanged();
    void on_checkShaderHotReload_stateChanged();
    void on_checkShowFPS_stateChanged();
    void on_checkLegacyOpenGL_stateChanged();
    void on_spinBoxThreads_valueChanged();
    void on_showFpsCheckBox_stateChanged();
    void on_showMemoryUsageCheckBox_stateChanged();
    void on_showRenderStatsCheckBox_stateChanged();
    void on_showOpenGLInfoCheckBox_stateChanged();
    void on_enableLoggingCheckBox_stateChanged();
    void on_logLevelComboBox_currentIndexChanged();
    void on_checkOpenGLErrorsCheckBox_stateChanged();
    void on_validateShadersCheckBox_stateChanged();
    void on_profileRenderingCheckBox_stateChanged();
    void on_clearCacheButton_clicked();
    void on_resetSettingsButton_clicked();
    void restoreDefaults();	

private:
    void applySettings();    
    void loadSettings();
    void saveSettings();

private:
    Ui::SettingsDialog *ui;

	std::unique_ptr<QSettings> _settings;
};

#endif // SETTINGSDIALOG_H
