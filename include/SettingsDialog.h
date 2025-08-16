#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QSettings>
#include "ThemeManager.h"


namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    void retranslateUI();

    void blockAllChildWidgetSignals(bool block);

    void setMaxMSAASamples(int maxSamples);
	void setMaxAnisotropy(int maxAnisotropy);

public:
    // General tab
    int generalThemeIndex() const { return general_themeIndex; }
    int generalLanguageIndex() const { return general_languageIndex; }
    bool generalPromptOverwrite() const { return general_promptOverwrite; }
    bool generalRestoreLastFile() const { return general_restoreLastFile; }
    bool generalShowTooltips() const { return general_showTooltips; }
    bool generalConfirmExit() const { return general_confirmExit; }

    // Camera tab
    int cameraProjectionModeIndex() const { return camera_projectionModeIndex; }
    int cameraDefaultViewIndex() const { return camera_defaultViewIndex; }
    int cameraDefaultProjectionIndex() const { return camera_defaultProjectionIndex; }
    bool cameraTrackball() const { return camera_trackball; }
    bool cameraInvertZoom() const { return camera_invertZoom; }
    double cameraZoomFactor() const { return camera_zoomFactor; }

    // Background tab
    int backgroundStyleIndex() const { return background_styleIndex; }
    int backgroundGradientStyleIndex() const { return background_gradientStyleIndex; }
    QColor backgroundTopColor() const { return background_topColor; }
    QColor backgroundBottomColor() const { return background_bottomColor; }

    // Display tab
    bool displayShowBoundingBox() const { return display_showBoundingBox; }
    bool displayShowCornerTrihedron() const { return display_showCornerTrihedron; }
    int  displayCornerTrihedronPosition() const { return display_cornerTrihedronPosition; }
    bool displayShowGrid() const { return display_showGrid; }
    bool displayShowWireframe() const { return display_showWireframe; }
    int displayFieldOfView() const { return display_fieldOfView; }
    double displayNearPlane() const { return display_nearPlane; }
    double displayFarPlane() const { return display_farPlane; }
    bool displayShowCenterTrihedron() const { return display_showCenterTrihedron; }

    // Navigation group
    int navigationModeIndex() const { return navigation_modeIndex; }
    int navigationMouseSensitivity() const { return navigation_mouseSensitivity; }
    int navigationZoomSensitivity() const { return navigation_zoomSensitivity; }
    bool navigationInvertYAxis() const { return navigation_invertYAxis; }
    bool navigationSmoothNavigation() const { return navigation_smoothNavigation; }

    // Rendering tab
    int renderingShadingModeIndex() const { return rendering_shadingModeIndex; }
    bool renderingBackfaceCulling() const { return rendering_backfaceCulling; }
    bool renderingNormalMap() const { return rendering_normalMap; }
    int renderingShaderModelIndex() const { return rendering_shaderModelIndex; }
    int renderingMsaaIndex() const { return rendering_msaaIndex; }
    int renderingAnisotropyIndex() const { return rendering_anisotropyIndex; }

    // Lighting
    bool lightingEnableLighting() const { return lighting_enableLighting; }
    bool lightingEnableShadows() const { return lighting_enableShadows; }
    int lightingAmbient() const { return lighting_ambient; }
    int lightingDiffuse() const { return lighting_diffuse; }
    int lightingSpecular() const { return lighting_specular; }

    // Materials
    int materialsDefaultMaterialIndex() const { return materials_defaultMaterialIndex; }
    QString materialsTextureDir() const { return materials_textureDir; }

    // UV Generation Tab
    int uvMethodIndex() const { return uv_methodIndex; }
    double uvAngleThreshold() const { return uv_angleThreshold; }
    bool uvPreserveUVs() const { return uv_preserveUVs; }
    bool uvAutoPackUVs() const { return uv_autoPackUVs; }
    bool uvRelaxUVs() const { return uv_relaxUVs; }
    bool uvPcaProjection() const { return uv_pcaProjection; }
    bool uvXatlasPackingOnly() const { return uv_xatlasPackingOnly; }
    bool uvRememberUV() const { return uv_rememberUV; }

    // Import/Export Tab
    // OpenCascade
    int importTessellationQuality() const { return import_tessellationQuality; }
    double importLinearDeflection() const { return import_linearDeflection; }
    double importAngularDeflection() const { return import_angularDeflection; }
    bool importOcctUnifyFaces() const { return import_occtUnifyFaces; }
    bool importOcctUnifyEdges() const { return import_occtUnifyEdges; }
    bool importOcctBuildCurves() const { return import_occtBuildCurves; }

    // Assimp
    bool importAssimpTriangulate() const { return import_assimpTriangulate; }
    bool importAssimpGenNormals() const { return import_assimpGenNormals; }
    bool importAssimpSmoothNormals() const { return import_assimpSmoothNormals; }
    bool importAssimpCalcTangents() const { return import_assimpCalcTangents; }
    bool importAssimpOptimizeMesh() const { return import_assimpOptimizeMesh; }
    bool importAssimpRemoveDuplicates() const { return import_assimpRemoveDuplicates; }
    int importAssimpMaxFaceVertices() const { return import_assimpMaxFaceVertices; }

    // Performance Tab
    bool perfMultithreadedLoad() const { return perf_multithreadedLoad; }
    int perfThreadLimit() const { return perf_threadLimit; }
    bool perfSkyboxBlending() const { return perf_skyboxBlending; }
    bool perfProgressiveLoading() const { return perf_progressiveLoading; }
    int perfMaxFps() const { return perf_maxFps; }
    bool perfVsync() const { return perf_vsync; }
    bool perfFrustumCulling() const { return perf_frustumCulling; }
    bool perfBackfaceCulling() const { return perf_backfaceCulling; }
    bool perfLevelOfDetail() const { return perf_levelOfDetail; }
    int perfMaxVertices() const { return perf_maxVertices; }

    // Memory management
    int perfTextureCacheSize() const { return perf_textureCacheSize; }
    int perfGeometryCacheSize() const { return perf_geometryCacheSize; }
    bool perfCompressTextures() const { return perf_compressTextures; }
    bool perfGenerateMipmaps() const { return perf_generateMipmaps; }

    // Advanced Tab
    int advancedOpenGLVersionIndex() const { return advanced_openGLVersionIndex; }
    bool advancedVsync() const { return advanced_vsync; }
    int advancedThreads() const { return advanced_threads; }
    bool advancedShaderHotReload() const { return advanced_shaderHotReload; }
    bool advancedShowFPS() const { return advanced_showFPS; }
    bool advancedLegacyOpenGL() const { return advanced_legacyOpenGL; }

    // Debug Tab
    bool debugShowFps() const { return debug_showFps; }
    bool debugShowMemoryUsage() const { return debug_showMemoryUsage; }
    bool debugShowRenderStats() const { return debug_showRenderStats; }
    bool debugShowOpenGLInfo() const { return debug_showOpenGLInfo; }
    bool debugEnableLogging() const { return debug_enableLogging; }
    int debugLogLevelIndex() const { return debug_logLevelIndex; }
    bool debugCheckOpenGLErrors() const { return debug_checkOpenGLErrors; }
    bool debugValidateShaders() const { return debug_validateShaders; }
    bool debugProfileRendering() const { return debug_profileRendering; }

    ThemeManager* themeManager() const { return _themeManager; }    


signals:
	void settingsChanged(); // Signal to notify that settings have changed

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
    void on_comboBoxCornerTrihedronPosition_currentIndexChanged();
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
	void on_assimpAutoScaleCheckBox_stateChanged();
	void on_assimpAutoOrientCheckBox_stateChanged();
    void on_assimpMaxFaceVerticesSpinBox_valueChanged();
	void on_radioButtonExportScene_toggled(bool checked);
	void on_radioButtonExportMeshes_toggled(bool checked);
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
    void setDefaultValues();
    void applySettings();    
    void loadSettings();
    void saveSettings();	

private:

    // General tab
    int general_themeIndex = 0;
    int general_languageIndex = 0;
    bool general_promptOverwrite = true;
    bool general_restoreLastFile = false;
    bool general_showTooltips = true;
    bool general_confirmExit = false;

    // Camera tab
    int camera_projectionModeIndex = 0;
    int camera_defaultViewIndex = 0;
    int camera_defaultProjectionIndex = 0;
    bool camera_trackball = false;
    bool camera_invertZoom = false;
    double camera_zoomFactor = 1.0;

    // Background tab
    int background_styleIndex = 0;
    int background_gradientStyleIndex = 0;
    // For color buttons, use QColor
    QColor background_topColor = Qt::white;
    QColor background_bottomColor = Qt::white;

    // Display tab
    bool display_showBoundingBox = false;
    bool display_showCornerTrihedron = true;
    int display_cornerTrihedronPosition = 1;
    bool display_showGrid = true;
    bool display_showWireframe = false;
    int display_fieldOfView = 60;
    double display_nearPlane = 0.1;
    double display_farPlane = 1000.0;
    bool display_showCenterTrihedron = false;

    // Navigation group
    int navigation_modeIndex = 0;
    int navigation_mouseSensitivity = 5;
    int navigation_zoomSensitivity = 5;
    bool navigation_invertYAxis = false;
    bool navigation_smoothNavigation = true;

    // Rendering tab
    int rendering_shadingModeIndex = 0;
    bool rendering_backfaceCulling = false;
    bool rendering_normalMap = false;
    int rendering_shaderModelIndex = 0;
    int rendering_msaaIndex = 0;
    int rendering_anisotropyIndex = 0;

    // Lighting
    bool lighting_enableLighting = true;
    bool lighting_enableShadows = false;
    int lighting_ambient = 20;
    int lighting_diffuse = 80;
    int lighting_specular = 50;

    // Materials
    int materials_defaultMaterialIndex = 0;
    QString materials_textureDir;

    // UV Generation Tab
    int uv_methodIndex = 0;
    double uv_angleThreshold = 66.0;
    bool uv_preserveUVs = false;
    bool uv_autoPackUVs = false;
    bool uv_relaxUVs = false;
    bool uv_pcaProjection = false;
    bool uv_xatlasPackingOnly = false;
    bool uv_rememberUV = false;

    // Import/Export Tab
    // OpenCascade
    int import_tessellationQuality = 5;
    double import_linearDeflection = 0.1;
    double import_angularDeflection = 0.1;
    bool import_occtUnifyFaces = false;
    bool import_occtUnifyEdges = false;
    bool import_occtBuildCurves = false;

    // Assimp
    bool import_assimpTriangulate = true;
    bool import_assimpGenNormals = true;
    bool import_assimpSmoothNormals = false;
    bool import_assimpCalcTangents = false;
    bool import_assimpOptimizeMesh = false;
    bool import_assimpRemoveDuplicates = false;
	bool import_assimpAutoScaleModel = true;
	bool import_assimpAutoOrientModel = true;
    int import_assimpMaxFaceVertices = 3;

    bool export_exportScene = true;
    bool export_exportMeshes = false;


    // Performance Tab
    bool perf_multithreadedLoad = false;
    int perf_threadLimit = 4;
    bool perf_skyboxBlending = false;
    bool perf_progressiveLoading = false;
    int perf_maxFps = 60;
    bool perf_vsync = true;
    bool perf_frustumCulling = true;
    bool perf_backfaceCulling = true;
    bool perf_levelOfDetail = false;
    int perf_maxVertices = 1000000;

    // Memory management
    int perf_textureCacheSize = 512;
    int perf_geometryCacheSize = 256;
    bool perf_compressTextures = true;
    bool perf_generateMipmaps = true;

    // Advanced Tab
    int advanced_openGLVersionIndex = 0;
    bool advanced_vsync = false;
    int advanced_threads = 4;
    bool advanced_shaderHotReload = false;
    bool advanced_showFPS = false;
    bool advanced_legacyOpenGL = false;

    // Debug Tab
    bool debug_showFps = false;
    bool debug_showMemoryUsage = false;
    bool debug_showRenderStats = false;
    bool debug_showOpenGLInfo = false;
    bool debug_enableLogging = false;
    int debug_logLevelIndex = 0;
    bool debug_checkOpenGLErrors = false;
    bool debug_validateShaders = false;
    bool debug_profileRendering = false;

	ThemeManager* _themeManager;

    Ui::SettingsDialog *ui;	
};

#endif // SETTINGSDIALOG_H
