#include "LanguageManager.h"
#include "Logger.h"
#include "SettingsDialog.h"
#include "ModelViewerApplication.h"
#include "ui_SettingsDialog.h"
#include <algorithm>
#include <QMessageBox>
#include <QTimer>

namespace
{
constexpr int kCornerTrihedronTopLeft = 0;
constexpr int kCornerTrihedronTopRight = 1;
constexpr int kCornerTrihedronBottomLeft = 2;
constexpr int kCornerTrihedronBottomRight = 3;

int normalizeCornerTrihedronPositionValue(int value)
{
    return std::clamp(value, kCornerTrihedronTopLeft, kCornerTrihedronBottomRight);
}

void configureCornerTrihedronPositionCombo(QComboBox* comboBox)
{
    if (!comboBox)
        return;

    comboBox->setItemData(0, kCornerTrihedronTopLeft);
    comboBox->setItemData(1, kCornerTrihedronTopRight);
    comboBox->setItemData(2, kCornerTrihedronBottomLeft);
    comboBox->setItemData(3, kCornerTrihedronBottomRight);
}

void setCornerTrihedronPositionSelection(QComboBox* comboBox, int positionValue)
{
    if (!comboBox)
        return;

    const int normalizedValue = normalizeCornerTrihedronPositionValue(positionValue);
    const int index = comboBox->findData(normalizedValue);
    comboBox->setCurrentIndex(index >= 0 ? index : 1);
}

int currentCornerTrihedronPositionValue(const QComboBox* comboBox)
{
    if (!comboBox)
        return kCornerTrihedronTopRight;

    const QVariant data = comboBox->currentData();
    if (data.isValid())
        return normalizeCornerTrihedronPositionValue(data.toInt());

    return normalizeCornerTrihedronPositionValue(comboBox->currentIndex());
}

bool isViewCubeAvailableForCornerPosition(int positionValue)
{
    return normalizeCornerTrihedronPositionValue(positionValue) != kCornerTrihedronBottomRight;
}
}

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog),
    _themeManager(new ThemeManager(this))
{
    ui->setupUi(this);
    configureCornerTrihedronPositionCombo(ui->comboBoxCornerTrihedronPosition);

    retranslateUI();

    // Connect to specific buttons
    QPushButton *okButton = ui->buttonBox->button(QDialogButtonBox::Ok); // Get the "OK" button
    if (okButton) {
        connect(okButton, &QPushButton::clicked, this, &SettingsDialog::onOkClicked);
    }

    QPushButton *cancelButton = ui->buttonBox->button(QDialogButtonBox::Cancel); // Get the "Cancel" button
    if (cancelButton) {
        connect(cancelButton, &QPushButton::clicked, this, &SettingsDialog::onCancelClicked);
    }

    QPushButton *applyButton = ui->buttonBox->button(QDialogButtonBox::Apply); // Get the "Cancel" button
    if (applyButton) {
        connect(applyButton, &QPushButton::clicked, this, &SettingsDialog::onApplyClicked);
    }

    QPushButton* restoreButton = ui->buttonBox->button(QDialogButtonBox::RestoreDefaults);
    if (restoreButton) {
        connect(restoreButton, &QPushButton::clicked, this, &SettingsDialog::onRestoreDefaults);
    }

    loadSettings();

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this]() {
        updateSettingsHint();
    });
    updateSettingsHint();

    connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this, [this]() {
        ui->retranslateUi(this);
        retranslateUI();
        // Second pass: QDialogButtonBox handles QEvent::LanguageChange internally
        // and overwrites our button texts after this slot returns. A deferred call
        // ensures retranslateUI() runs after those events settle.
        // Safe: 'this' as context prevents firing if the dialog is destroyed.
        QTimer::singleShot(0, this, [this]() {
            retranslateUI();
        });
    });
}

SettingsDialog::~SettingsDialog()
{
	blockSignals(true);
    delete ui;
}

void SettingsDialog::retranslateUI()
{
    // Dialog buttons
    if (ui->buttonBox->button(QDialogButtonBox::Ok))
        ui->buttonBox->button(QDialogButtonBox::Ok)->setText(QCoreApplication::translate("SettingsDialog", "OK"));
    if (ui->buttonBox->button(QDialogButtonBox::Cancel))
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(QCoreApplication::translate("SettingsDialog", "Cancel"));
    if (ui->buttonBox->button(QDialogButtonBox::Apply))
        ui->buttonBox->button(QDialogButtonBox::Apply)->setText(QCoreApplication::translate("SettingsDialog", "Apply"));
    if (ui->buttonBox->button(QDialogButtonBox::RestoreDefaults))
        ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)->setText(QCoreApplication::translate("SettingsDialog", "Defaults"));
    updateSettingsHint();

    ui->buttonBox->updateGeometry();
    ui->buttonBox->update();
    ui->buttonBox->repaint();
    this->update();
    this->repaint();
    // MSAA ComboBox
    //setMaxMSAASamples(/* pass current maxSamples value here */);

    // Anisotropy ComboBox
    //setMaxAnisotropy(/* pass current maxAnisotropy value here */);

    // If there are other dynamically set texts, add them here.
}

void SettingsDialog::refreshViewCubeAvailability()
{
    const bool viewCubeAvailable = isViewCubeAvailableForCornerPosition(
        currentCornerTrihedronPositionValue(ui->comboBoxCornerTrihedronPosition));
    ui->showViewCubeCheckBox->setEnabled(viewCubeAvailable);
    if (!viewCubeAvailable)
        ui->showViewCubeCheckBox->setChecked(false);
}

void SettingsDialog::setMaxMSAASamples(int maxSamples)
{
    bool oldState = ui->msaaComboBox->blockSignals(true);
    ui->msaaComboBox->clear();
    ui->msaaComboBox->addItem("None", 0);
    if (maxSamples >= 2) ui->msaaComboBox->addItem("2x", 2);
    if (maxSamples >= 4) ui->msaaComboBox->addItem("4x", 4);
    if (maxSamples >= 8) ui->msaaComboBox->addItem("8x", 8);
    if (maxSamples >= 16) ui->msaaComboBox->addItem("16x", 16);
    if (maxSamples >= 32) ui->msaaComboBox->addItem("32x", 32);

	QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    int val = settings.value("msaaComboBox", ui->msaaComboBox->currentIndex()).toInt();
    ui->msaaComboBox->setCurrentIndex(val);
    ui->msaaComboBox->blockSignals(oldState);
}

void SettingsDialog::setMaxAnisotropy(int maxAnisotropy)
{
    bool oldState = ui->anisotropyComboBox->blockSignals(true);
    ui->anisotropyComboBox->clear();
    ui->anisotropyComboBox->addItem("1x (None)", 1.0f);
    if (maxAnisotropy >= 2) ui->anisotropyComboBox->addItem("2x", 2.0f);
    if (maxAnisotropy >= 4) ui->anisotropyComboBox->addItem("4x", 4.0f);
    if (maxAnisotropy >= 8) ui->anisotropyComboBox->addItem("8x", 8.0f);
    if (maxAnisotropy >= 16) ui->anisotropyComboBox->addItem("16x", 16.0f);
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    int val = settings.value("anisotropyComboBox", ui->anisotropyComboBox->currentIndex()).toInt();
    ui->anisotropyComboBox->setCurrentIndex(val);
    ui->anisotropyComboBox->blockSignals(oldState);
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
    blockAllChildWidgetSignals(true);
	setDefaultValues();
    blockAllChildWidgetSignals(false);
    syncStateFromUi();
    QMessageBox::information(this, tr("Defaults Staged"), tr("Default settings are loaded in the dialog. Click Apply or OK to save them."));
}


void SettingsDialog::applySettings()
{
    syncStateFromUi();

    const bool themeChanged = (_appliedThemeIndex != general_themeIndex);
    const bool languageChanged = (_appliedLanguageIndex != general_languageIndex);
    const bool fileLoggingChanged = (_appliedDebugEnableLogging != debug_enableLogging);
    const bool consoleLoggingChanged = (_appliedDebugEnableConsoleOutput != debug_enableConsoleOutput);
    const bool logLevelChanged = (_appliedDebugLogLevelIndex != debug_logLevelIndex);

    // Apply the settings of the UI elements
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    // General tab
    settings.setValue("comboBoxTheme", general_themeIndex);
    settings.setValue("comboBoxLanguage", general_languageIndex);

    QString langCode = "en"; // Default to English 
    if (general_languageIndex == 0)
        langCode = "en"; // English
    else if (general_languageIndex == 1)
        langCode = "fr"; // French
    else if (general_languageIndex == 2)
        langCode = "de"; // German
    else if (general_languageIndex == 3)
        langCode = "es"; // Spanish
    else if (general_languageIndex == 4)
        langCode = "it"; // Italian    
    settings.setValue("App/Language", langCode);

    if (themeChanged)
    {
        _themeManager->setTheme(static_cast<ThemeManager::Theme>(general_themeIndex));
    }
    if (languageChanged)
    {
        // Defer past QDialog::accept() so the dialog is safely closed before
        // retranslation fires — prevents re-entrant UI modification and
        // timer callbacks racing with dialog destruction.
        QTimer::singleShot(0, [langCode]() {
            LanguageManager::instance().loadLanguage(langCode);
        });
    }

    settings.setValue("checkPromptOverwrite", general_promptOverwrite);
    settings.setValue("checkRestoreLastFile", general_restoreLastFile);
    settings.setValue("checkTooltips", general_showTooltips);
    settings.setValue("checkConfirmExit", general_confirmExit);
	settings.setValue("checkTutorialLaunch", general_showTutorialLauncher);
	settings.setValue("spinBoxUndoLimit", general_undoLimit);

    // Camera tab
    settings.setValue("comboProjectionMode", camera_projectionModeIndex);
    settings.setValue("comboDefaultView", camera_defaultViewIndex);
    settings.setValue("comboDefaultProjection", camera_defaultProjectionIndex);
    settings.setValue("comboCameraUpAxis", camera_defaultUpAxisIndex);
    settings.setValue("checkTrackball", camera_trackball);
    settings.setValue("checkInvertZoom", camera_invertZoom);
    settings.setValue("spinZoomFactor", camera_zoomFactor);

    // Background tab
    settings.setValue("comboBoxBackgroundStyle", background_styleIndex);
    settings.setValue("comboBoxGradientStyle", background_gradientStyleIndex);
    settings.setValue("backgroundTopColor", background_topColor);
    settings.setValue("backgroundBottomColor", background_bottomColor);

    // Display tab
    settings.setValue("showBoundingBoxCheckBox", display_showBoundingBox);
    settings.setValue("showCornerTrihedronCheckBox", display_showCornerTrihedron);
    settings.setValue("showViewCubeCheckBox", display_showViewCube);
    settings.setValue("comboBoxCornerTrihedronPosition", display_cornerTrihedronPosition);
    settings.setValue("showGridCheckBox", display_showGrid);
    settings.setValue("showWireframeCheckBox", display_showWireframe);
    settings.setValue("fieldOfViewSpinBox", display_fieldOfView);
    settings.setValue("nearPlaneSpinBox", display_nearPlane);
    settings.setValue("farPlaneSpinBox", display_farPlane);
    settings.setValue("showCenterTrihedronCheckBox", display_showCenterTrihedron);
    settings.setValue("comboBoxHoverHighlightMode", static_cast<int>(display_hoverHighlightMode));

    // Navigation group
    settings.setValue("navigationModeComboBox", navigation_modeIndex);
    settings.setValue("mouseSensitivitySlider", navigation_mouseSensitivity);
    settings.setValue("zoomSensitivitySlider", navigation_zoomSensitivity);
    settings.setValue("invertYAxisCheckBox", navigation_invertYAxis);
    settings.setValue("smoothNavigationCheckBox", navigation_smoothNavigation);

    // Rendering tab
    settings.setValue("comboShadingMode", rendering_shadingModeIndex);
    settings.setValue("checkBackfaceCulling", rendering_backfaceCulling);
    settings.setValue("checkNormalMap", rendering_normalMap);
    settings.setValue("shaderModelComboBox", rendering_shaderModelIndex);
    settings.setValue("msaaComboBox", rendering_msaaIndex);
    settings.setValue("anisotropyComboBox", rendering_anisotropyIndex);

    // Lighting
    settings.setValue("enableLightingCheckBox", lighting_enableLighting);
    settings.setValue("enableShadowsCheckBox", lighting_enableShadows);
    settings.setValue("ambientLightSlider", lighting_ambient);
    settings.setValue("diffuseLightSlider", lighting_diffuse);
    settings.setValue("specularLightSlider", lighting_specular);

    // Materials
    settings.setValue("comboBoxDefaultMaterial", materials_defaultMaterialIndex);
    settings.setValue("lineEditTextureDir", materials_textureDir);

    // UV Generation Tab
    settings.setValue("comboUVMethod", uv_methodIndex);
    settings.setValue("spinAngleThreshold", uv_angleThreshold);
    settings.setValue("checkPreserveUVs", uv_preserveUVs);
    settings.setValue("checkAutoPackUVs", uv_autoPackUVs);
    settings.setValue("checkRelaxUVs", uv_relaxUVs);
    settings.setValue("checkPCAProjection", uv_pcaProjection);
    settings.setValue("checkXatlasPackingOnly", uv_xatlasPackingOnly);
    settings.setValue("checkRememberUV", uv_rememberUV);

    // Import/Export Tab - OpenCascade
    settings.setValue("tessellationQualitySlider", import_tessellationQuality);
    settings.setValue("linearDeflectionSpinBox", import_linearDeflection);
    settings.setValue("angularDeflectionSpinBox", import_angularDeflection);
    settings.setValue("occtUnifyFacesCheckBox", import_occtUnifyFaces);
    settings.setValue("occtUnifyEdgesCheckBox", import_occtUnifyEdges);
    settings.setValue("occtBuildCurvesCheckBox", import_occtBuildCurves);

    // Import/Export Tab - Assimp
    settings.setValue("assimpTriangulateCheckBox", import_assimpTriangulate);
    settings.setValue("assimpGenNormalsCheckBox", import_assimpGenNormals);
    settings.setValue("assimpSmoothNormalsCheckBox", import_assimpSmoothNormals);
    settings.setValue("assimpCalcTangentsCheckBox", import_assimpCalcTangents);
    settings.setValue("assimpOptimizeMeshCheckBox", import_assimpOptimizeMesh);
    settings.setValue("assimpRemoveDuplicatesCheckBox", import_assimpRemoveDuplicates);
    settings.setValue("assimpAutoOrientCheckBox", import_assimpAutoOrientModel);
    settings.setValue("assimpMaxFaceVerticesSpinBox", import_assimpMaxFaceVertices);

	// Import/Export Tab - Export
	settings.setValue("radioButtonExportScene", export_exportScene);
	settings.setValue("radioButtonExportMeshs", export_exportMeshes);

    // Performance Tab
    settings.setValue("checkMultithreadedLoad", perf_multithreadedLoad);
    settings.setValue("spinThreadLimit", perf_threadLimit);
    settings.setValue("checkSkyboxBlending", perf_skyboxBlending);
    settings.setValue("checkProgressiveLoading", perf_progressiveLoading);
    settings.setValue("maxFpsSpinBox", perf_maxFps);
    settings.setValue("vsyncCheckBox", perf_vsync);
    settings.setValue("frustumCullingCheckBox", perf_frustumCulling);
    settings.setValue("backfaceCullingCheckBox", perf_backfaceCulling);
    settings.setValue("levelOfDetailCheckBox", perf_levelOfDetail);
    settings.setValue("maxVerticesSpinBox", perf_maxVertices);

    // Memory management
    settings.setValue("textureCacheSizeSpinBox", perf_textureCacheSize);
    settings.setValue("geometryCacheSizeSpinBox", perf_geometryCacheSize);
    settings.setValue("compressTexturesCheckBox", perf_compressTextures);
    settings.setValue("generateMipmapsCheckBox", perf_generateMipmaps);

    // Advanced Tab
    settings.setValue("comboBoxOpenGLVersion", advanced_openGLVersionIndex);
    settings.setValue("checkBoxVSync", advanced_vsync);
    settings.setValue("spinBoxThreads", advanced_threads);
    settings.setValue("checkShaderHotReload", advanced_shaderHotReload);
    settings.setValue("checkShowFPS", advanced_showFPS);
    settings.setValue("checkLegacyOpenGL", advanced_legacyOpenGL);

    // Debug Tab
    settings.setValue("showFpsCheckBox", debug_showFps);
    settings.setValue("showMemoryUsageCheckBox", debug_showMemoryUsage);
    settings.setValue("showRenderStatsCheckBox", debug_showRenderStats);
    settings.setValue("showOpenGLInfoCheckBox", debug_showOpenGLInfo);
    settings.setValue("enableLoggingCheckBox", debug_enableLogging);
	settings.setValue("enableConsoleCheckBox", debug_enableConsoleOutput);
    settings.setValue("logLevelComboBox", debug_logLevelIndex);
    settings.setValue("checkOpenGLErrorsCheckBox", debug_checkOpenGLErrors);
    settings.setValue("validateShadersCheckBox", debug_validateShaders);
    settings.setValue("profileRenderingCheckBox", debug_profileRendering);
    settings.setValue("showTextureDebugPanelCheckBox", debug_showTextureDebugPanel);

    if (general_showTutorialLauncher)
    {
        settings.setValue("tutorial/displayMode", "ask");
    }

    if (fileLoggingChanged)
    {
        Logger::instance().setFileEnabled(debug_enableLogging);
    }
    if (consoleLoggingChanged)
    {
        Logger::instance().setConsoleEnabled(debug_enableConsoleOutput);
    }
    if (logLevelChanged)
    {
        Logger::instance().setMinimumLevel(static_cast<Logger::LogLevel>(debug_logLevelIndex));
    }

    captureAppliedState();

    // Notify other parts of the application about the settings change
	emit settingsChanged();

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
	ui->checkTutorialLaunch->setChecked(true);    // Explicitly set to true
	ui->spinBoxUndoLimit->setValue(50);           // Explicitly set

    // Camera tab
    ui->comboProjectionMode->setCurrentIndex(0);       // "Orthographic"
    ui->comboDefaultView->setCurrentIndex(0);          // "Isometric"
    ui->comboDefaultProjection->setCurrentIndex(0);    // "Isometric"
    ui->comboCameraUpAxis->setCurrentIndex(0);         // "Z-Up"
    ui->checkTrackball->setChecked(false);             // No 'checked' property found
    ui->checkInvertZoom->setChecked(false);            // No 'checked' property found
    ui->spinZoomFactor->setValue(1.0);                 // Explicitly set to 1.0

    // Background tab
    ui->comboBoxBackgroundStyle->setCurrentIndex(0);   // "Gradient"
    ui->comboBoxGradientStyle->setCurrentIndex(0);     // "Vertical"
    // pushButtonTopColor and pushButtonBottomColor do not store actual color values - handled elsewhere.

    // Display tab
    ui->showBoundingBoxCheckBox->setChecked(false);    // No 'checked' property found
    ui->showCornerTrihedronCheckBox->setChecked(true); // Explicitly set to true
    ui->showViewCubeCheckBox->setChecked(true);        // Explicitly set to true
	ui->showCenterTrihedronCheckBox->setChecked(true); // Explicitly set to true
    setCornerTrihedronPositionSelection(ui->comboBoxCornerTrihedronPosition, kCornerTrihedronTopRight);
    refreshViewCubeAvailability();
    ui->showGridCheckBox->setChecked(true);            // Explicitly set to true
    ui->showWireframeCheckBox->setChecked(false);      // No 'checked' property found
    ui->fieldOfViewSpinBox->setValue(45);              // Explicitly set
    ui->nearPlaneSpinBox->setValue(0.1);               // Explicitly set
    ui->farPlaneSpinBox->setValue(1000.0);             // Explicitly set
    if (ui->comboBoxHoverHighlightMode)
        ui->comboBoxHoverHighlightMode->setCurrentIndex(0); // Default: Disabled

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
    	
	int maxSamples = ModelViewerApplication::supportedMSAASamples();
	if(maxSamples >= 4)
        ui->msaaComboBox->setCurrentIndex(3);                    // "4x"    
    else
        ui->msaaComboBox->setCurrentIndex(0);                    // "1 (No MSAA)"
    
	int maxAnisotropy = ModelViewerApplication::supportedAnisotropicFilteringLevel();
    if(maxAnisotropy >= 4)
        ui->anisotropyComboBox->setCurrentIndex(3);              // "4x"
	else
        ui->anisotropyComboBox->setCurrentIndex(0);              // "1x (Off)"

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
    ui->assimpSmoothNormalsCheckBox->setChecked(true);
    ui->assimpCalcTangentsCheckBox->setChecked(true);
    ui->assimpOptimizeMeshCheckBox->setChecked(true);
    ui->assimpRemoveDuplicatesCheckBox->setChecked(true);
	ui->assimpAutoOrientCheckBox->setChecked(true);
    ui->assimpMaxFaceVerticesSpinBox->setValue(3);

    // Import/Export Tab - Export
    ui->radioButtonExportScene->setChecked(true);
    ui->radioButtonExportMeshes->setChecked(false);

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
	ui->enableConsoleCheckBox->setChecked(false);
    ui->logLevelComboBox->setCurrentText("Warning");

    ui->checkOpenGLErrorsCheckBox->setChecked(false);
    ui->validateShadersCheckBox->setChecked(false);
    ui->profileRenderingCheckBox->setChecked(false);
    ui->showTextureDebugPanelCheckBox->setChecked(false);
}

void SettingsDialog::captureAppliedState()
{
    _appliedThemeIndex = general_themeIndex;
    _appliedLanguageIndex = general_languageIndex;
    _appliedDebugEnableLogging = debug_enableLogging;
    _appliedDebugEnableConsoleOutput = debug_enableConsoleOutput;
    _appliedDebugLogLevelIndex = debug_logLevelIndex;
}

void SettingsDialog::updateSettingsHint()
{
    if (!ui || !ui->label_5 || !ui->tabWidget)
        return;

    QString hint = QCoreApplication::translate("SettingsDialog",
        "Some settings apply immediately. Others take effect only for newly opened documents or after restarting the application.");

    QWidget* currentTab = ui->tabWidget->currentWidget();
    if (currentTab == ui->tabDisplay || currentTab == ui->tabRendering || currentTab == ui->tabDebug)
    {
        hint = QCoreApplication::translate("SettingsDialog",
            "Most settings on this tab apply immediately. MSAA and some graphics options may still require restarting the application.");
    }
    else if (currentTab == ui->tabImportExport || currentTab == ui->tabPerformance)
    {
        hint = QCoreApplication::translate("SettingsDialog",
            "These settings are primarily used for newly opened documents and future imports/exports. They may not affect models that are already loaded.");
    }
    else if (currentTab == ui->tabAdvanced)
    {
        hint = QCoreApplication::translate("SettingsDialog",
            "Advanced graphics settings often require restarting the application to take full effect.");
    }
    else if (currentTab == ui->tabCamera || currentTab == ui->tabUVGeneration)
    {
        hint = QCoreApplication::translate("SettingsDialog",
            "These settings are mainly used as defaults for future actions and newly opened documents.");
    }

    ui->label_5->setText(hint);
}

void SettingsDialog::blockAllChildWidgetSignals(bool block)
{
    const auto widgets = this->findChildren<QWidget*>();
    for (QWidget* widget : widgets)
        widget->blockSignals(block);
}

void SettingsDialog::syncStateFromUi()
{
    // General tab
    general_themeIndex = ui->comboBoxTheme->currentIndex();
    general_languageIndex = ui->comboBoxLanguage->currentIndex();
    general_promptOverwrite = ui->checkPromptOverwrite->isChecked();
    general_restoreLastFile = ui->checkRestoreLastFile->isChecked();
    general_showTooltips = ui->checkTooltips->isChecked();
    general_confirmExit = ui->checkConfirmExit->isChecked();
    general_showTutorialLauncher = ui->checkTutorialLaunch->isChecked();
    general_undoLimit = ui->spinBoxUndoLimit->value();

    // Camera tab
    camera_projectionModeIndex = ui->comboProjectionMode->currentIndex();
    camera_defaultViewIndex = ui->comboDefaultView->currentIndex();
    camera_defaultProjectionIndex = ui->comboDefaultProjection->currentIndex();
    camera_defaultUpAxisIndex = ui->comboCameraUpAxis->currentIndex();
    camera_trackball = ui->checkTrackball->isChecked();
    camera_invertZoom = ui->checkInvertZoom->isChecked();
    camera_zoomFactor = ui->spinZoomFactor->value();

    // Background tab
    background_styleIndex = ui->comboBoxBackgroundStyle->currentIndex();
    background_gradientStyleIndex = ui->comboBoxGradientStyle->currentIndex();
    background_topColor = ui->pushButtonTopColor->property("color").value<QColor>();
    background_bottomColor = ui->pushButtonBottomColor->property("color").value<QColor>();

    // Display tab
    display_showBoundingBox = ui->showBoundingBoxCheckBox->isChecked();
    display_showCornerTrihedron = ui->showCornerTrihedronCheckBox->isChecked();
    display_cornerTrihedronPosition = currentCornerTrihedronPositionValue(ui->comboBoxCornerTrihedronPosition);
    display_showViewCube = isViewCubeAvailableForCornerPosition(display_cornerTrihedronPosition)
        && ui->showViewCubeCheckBox->isChecked();
    display_showGrid = ui->showGridCheckBox->isChecked();
    display_showWireframe = ui->showWireframeCheckBox->isChecked();
    display_fieldOfView = ui->fieldOfViewSpinBox->value();
    display_nearPlane = ui->nearPlaneSpinBox->value();
    display_farPlane = ui->farPlaneSpinBox->value();
    display_showCenterTrihedron = ui->showCenterTrihedronCheckBox->isChecked();
    if (ui->comboBoxHoverHighlightMode) {
        display_hoverHighlightMode = static_cast<HoverHighlightMode>(ui->comboBoxHoverHighlightMode->currentIndex());
    }

    // Navigation group
    navigation_modeIndex = ui->navigationModeComboBox->currentIndex();
    navigation_mouseSensitivity = ui->mouseSensitivitySlider->value();
    navigation_zoomSensitivity = ui->zoomSensitivitySlider->value();
    navigation_invertYAxis = ui->invertYAxisCheckBox->isChecked();
    navigation_smoothNavigation = ui->smoothNavigationCheckBox->isChecked();

    // Rendering tab
    rendering_shadingModeIndex = ui->comboShadingMode->currentIndex();
    rendering_backfaceCulling = ui->checkBackfaceCulling->isChecked();
    rendering_normalMap = ui->checkNormalMap->isChecked();
    rendering_shaderModelIndex = ui->shaderModelComboBox->currentIndex();
    rendering_msaaIndex = ui->msaaComboBox->currentIndex();
    rendering_anisotropyIndex = ui->anisotropyComboBox->currentIndex();

    // Lighting
    lighting_enableLighting = ui->enableLightingCheckBox->isChecked();
    lighting_enableShadows = ui->enableShadowsCheckBox->isChecked();
    lighting_ambient = ui->ambientLightSlider->value();
    lighting_diffuse = ui->diffuseLightSlider->value();
    lighting_specular = ui->specularLightSlider->value();

    // Materials
    materials_defaultMaterialIndex = ui->comboBoxDefaultMaterial->currentIndex();
    materials_textureDir = ui->lineEditTextureDir->text();

    // UV Generation Tab
    uv_methodIndex = ui->comboUVMethod->currentIndex();
    uv_angleThreshold = ui->spinAngleThreshold->value();
    uv_preserveUVs = ui->checkPreserveUVs->isChecked();
    uv_autoPackUVs = ui->checkAutoPackUVs->isChecked();
    uv_relaxUVs = ui->checkRelaxUVs->isChecked();
    uv_pcaProjection = ui->checkPCAProjection->isChecked();
    uv_xatlasPackingOnly = ui->checkXatlasPackingOnly->isChecked();
    uv_rememberUV = ui->checkRememberUV->isChecked();

    // Import/Export Tab
    import_tessellationQuality = ui->tessellationQualitySlider->value();
    import_linearDeflection = ui->linearDeflectionSpinBox->value();
    import_angularDeflection = ui->angularDeflectionSpinBox->value();
    import_occtUnifyFaces = ui->occtUnifyFacesCheckBox->isChecked();
    import_occtUnifyEdges = ui->occtUnifyEdgesCheckBox->isChecked();
    import_occtBuildCurves = ui->occtBuildCurvesCheckBox->isChecked();
    import_assimpTriangulate = ui->assimpTriangulateCheckBox->isChecked();
    import_assimpGenNormals = ui->assimpGenNormalsCheckBox->isChecked();
    import_assimpSmoothNormals = ui->assimpSmoothNormalsCheckBox->isChecked();
    import_assimpCalcTangents = ui->assimpCalcTangentsCheckBox->isChecked();
    import_assimpOptimizeMesh = ui->assimpOptimizeMeshCheckBox->isChecked();
    import_assimpRemoveDuplicates = ui->assimpRemoveDuplicatesCheckBox->isChecked();
    import_assimpAutoOrientModel = ui->assimpAutoOrientCheckBox->isChecked();
    import_assimpMaxFaceVertices = ui->assimpMaxFaceVerticesSpinBox->value();
    export_exportScene = ui->radioButtonExportScene->isChecked();
    export_exportMeshes = ui->radioButtonExportMeshes->isChecked();

    // Performance Tab
    perf_multithreadedLoad = ui->checkMultithreadedLoad->isChecked();
    perf_threadLimit = ui->spinThreadLimit->value();
    perf_skyboxBlending = ui->checkSkyboxBlending->isChecked();
    perf_progressiveLoading = ui->checkProgressiveLoading->isChecked();
    perf_maxFps = ui->maxFpsSpinBox->value();
    perf_vsync = ui->vsyncCheckBox->isChecked();
    perf_frustumCulling = ui->frustumCullingCheckBox->isChecked();
    perf_backfaceCulling = ui->backfaceCullingCheckBox->isChecked();
    perf_levelOfDetail = ui->levelOfDetailCheckBox->isChecked();
    perf_maxVertices = ui->maxVerticesSpinBox->value();
    perf_textureCacheSize = ui->textureCacheSizeSpinBox->value();
    perf_geometryCacheSize = ui->geometryCacheSizeSpinBox->value();
    perf_compressTextures = ui->compressTexturesCheckBox->isChecked();
    perf_generateMipmaps = ui->generateMipmapsCheckBox->isChecked();

    // Advanced Tab
    advanced_openGLVersionIndex = ui->comboBoxOpenGLVersion->currentIndex();
    advanced_vsync = ui->checkBoxVSync->isChecked();
    advanced_threads = ui->spinBoxThreads->value();
    advanced_shaderHotReload = ui->checkShaderHotReload->isChecked();
    advanced_showFPS = ui->checkShowFPS->isChecked();
    advanced_legacyOpenGL = ui->checkLegacyOpenGL->isChecked();

    // Debug Tab
    debug_showFps = ui->showFpsCheckBox->isChecked();
    debug_showMemoryUsage = ui->showMemoryUsageCheckBox->isChecked();
    debug_showRenderStats = ui->showRenderStatsCheckBox->isChecked();
    debug_showOpenGLInfo = ui->showOpenGLInfoCheckBox->isChecked();
    debug_enableLogging = ui->enableLoggingCheckBox->isChecked();
    debug_enableConsoleOutput = ui->enableConsoleCheckBox->isChecked();
    debug_logLevelIndex = ui->logLevelComboBox->currentIndex();
    debug_checkOpenGLErrors = ui->checkOpenGLErrorsCheckBox->isChecked();
    debug_validateShaders = ui->validateShadersCheckBox->isChecked();
    debug_profileRendering = ui->profileRenderingCheckBox->isChecked();
    debug_showTextureDebugPanel = ui->showTextureDebugPanelCheckBox->isChecked();
}

void SettingsDialog::loadSettings()
{
    blockAllChildWidgetSignals(true);

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
	bVal = settings.value("checkTutorialLaunch", ui->checkTutorialLaunch->isChecked()).toBool();
	ui->checkTutorialLaunch->setChecked(bVal);
	iVal = settings.value("spinBoxUndoLimit", ui->spinBoxUndoLimit->value()).toInt();
	ui->spinBoxUndoLimit->setValue(iVal);
    iVal = settings.value("comboProjectionMode", ui->comboProjectionMode->currentIndex()).toInt();
    ui->comboProjectionMode->setCurrentIndex(iVal);
    iVal = settings.value("comboDefaultView", ui->comboDefaultView->currentIndex()).toInt();
    ui->comboDefaultView->setCurrentIndex(iVal);
    iVal = settings.value("comboDefaultProjection", ui->comboDefaultProjection->currentIndex()).toInt();
    ui->comboDefaultProjection->setCurrentIndex(iVal);
    iVal = settings.value("comboCameraUpAxis", ui->comboCameraUpAxis->currentIndex()).toInt();
    ui->comboCameraUpAxis->setCurrentIndex(iVal);
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
    bVal = settings.value("showViewCubeCheckBox", ui->showViewCubeCheckBox->isChecked()).toBool();
    ui->showViewCubeCheckBox->setChecked(bVal);
    iVal = settings.value("comboBoxCornerTrihedronPosition", kCornerTrihedronTopRight).toInt();
    setCornerTrihedronPositionSelection(ui->comboBoxCornerTrihedronPosition, iVal);
    refreshViewCubeAvailability();
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
    if (ui->comboBoxHoverHighlightMode) {
        iVal = settings.value("comboBoxHoverHighlightMode", ui->comboBoxHoverHighlightMode->currentIndex()).toInt();
        ui->comboBoxHoverHighlightMode->setCurrentIndex(iVal);
    }
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
    bVal = settings.value("assimpAutoOrientCheckBox", ui->assimpAutoOrientCheckBox->isChecked()).toBool();
    ui->assimpAutoOrientCheckBox->setChecked(bVal);
    iVal = settings.value("assimpMaxFaceVerticesSpinBox", ui->assimpMaxFaceVerticesSpinBox->value()).toInt();
    ui->assimpMaxFaceVerticesSpinBox->setValue(iVal);
	bVal = settings.value("radioButtonExportScene", ui->radioButtonExportScene->isChecked()).toBool();
	ui->radioButtonExportScene->setChecked(bVal);
	bVal = settings.value("radioButtonExportMeshes", ui->radioButtonExportMeshes->isChecked()).toBool();
	ui->radioButtonExportMeshes->setChecked(bVal);
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
	bVal = settings.value("enableConsoleCheckBox", ui->enableConsoleCheckBox->isChecked()).toBool();
	ui->enableConsoleCheckBox->setChecked(bVal);
    iVal = settings.value("logLevelComboBox", ui->logLevelComboBox->currentIndex()).toInt();
    ui->logLevelComboBox->setCurrentIndex(iVal);
    bVal = settings.value("checkOpenGLErrorsCheckBox", ui->checkOpenGLErrorsCheckBox->isChecked()).toBool();
    ui->checkOpenGLErrorsCheckBox->setChecked(bVal);
    bVal = settings.value("validateShadersCheckBox", ui->validateShadersCheckBox->isChecked()).toBool();
    ui->validateShadersCheckBox->setChecked(bVal);
    bVal = settings.value("profileRenderingCheckBox", ui->profileRenderingCheckBox->isChecked()).toBool();
    ui->profileRenderingCheckBox->setChecked(bVal);
    bVal = settings.value("showTextureDebugPanelCheckBox", ui->showTextureDebugPanelCheckBox->isChecked()).toBool();
    ui->showTextureDebugPanelCheckBox->setChecked(bVal);

    blockAllChildWidgetSignals(false);
    syncStateFromUi();
    captureAppliedState();
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
		ui->checkRestoreLastFile, ui->checkTooltips, ui->checkConfirmExit, ui->checkTutorialLaunch, ui->spinBoxUndoLimit,
        ui->comboProjectionMode, ui->comboDefaultView, ui->comboDefaultProjection,
        ui->comboCameraUpAxis,
        ui->checkTrackball, ui->checkInvertZoom, ui->spinZoomFactor,
        ui->comboBoxBackgroundStyle, ui->pushButtonTopColor, ui->pushButtonBottomColor,
        ui->comboBoxGradientStyle, ui->showBoundingBoxCheckBox, ui->showCornerTrihedronCheckBox,
        ui->showViewCubeCheckBox,
        ui->comboBoxCornerTrihedronPosition, ui->farPlaneSpinBox, ui->fieldOfViewSpinBox, ui->showGridCheckBox,
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
		ui->assimpAutoOrientCheckBox,
        ui->assimpMaxFaceVerticesSpinBox, ui->radioButtonExportScene, ui->radioButtonExportMeshes,
        ui->checkMultithreadedLoad, ui->spinThreadLimit,
        ui->checkSkyboxBlending, ui->checkProgressiveLoading, ui->maxFpsSpinBox,
        ui->vsyncCheckBox, ui->frustumCullingCheckBox, ui->backfaceCullingCheckBox,
        ui->levelOfDetailCheckBox, ui->maxVerticesSpinBox, ui->textureCacheSizeSpinBox,
        ui->geometryCacheSizeSpinBox, ui->compressTexturesCheckBox, ui->generateMipmapsCheckBox,
        ui->comboBoxOpenGLVersion, ui->checkBoxVSync, ui->checkShaderHotReload,
        ui->checkShowFPS, ui->checkLegacyOpenGL, ui->spinBoxThreads,
        ui->showFpsCheckBox, ui->showMemoryUsageCheckBox, ui->showRenderStatsCheckBox,
        ui->showOpenGLInfoCheckBox, ui->enableLoggingCheckBox, ui->enableConsoleCheckBox, ui->logLevelComboBox,
        ui->checkOpenGLErrorsCheckBox, ui->validateShadersCheckBox, ui->profileRenderingCheckBox,
        ui->showTextureDebugPanelCheckBox,
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

// General tab
void SettingsDialog::on_comboBoxTheme_currentIndexChanged()
{
    general_themeIndex = ui->comboBoxTheme->currentIndex();
}

void SettingsDialog::on_comboBoxLanguage_currentIndexChanged()
{
    general_languageIndex = ui->comboBoxLanguage->currentIndex();    
}

void SettingsDialog::on_checkPromptOverwrite_stateChanged()
{
    general_promptOverwrite = ui->checkPromptOverwrite->isChecked();
}

void SettingsDialog::on_checkRestoreLastFile_stateChanged()
{
    general_restoreLastFile = ui->checkRestoreLastFile->isChecked();
}

void SettingsDialog::on_checkTooltips_stateChanged()
{
    general_showTooltips = ui->checkTooltips->isChecked();
}

void SettingsDialog::on_checkConfirmExit_stateChanged()
{
    general_confirmExit = ui->checkConfirmExit->isChecked();
}

void SettingsDialog::on_checkTutorialLaunch_stateChanged()
{
	general_showTutorialLauncher = ui->checkTutorialLaunch->isChecked();
}

void SettingsDialog::on_spinBoxUndoLimit_valueChanged()
{
	general_undoLimit = ui->spinBoxUndoLimit->value();
}

// Camera tab
void SettingsDialog::on_comboProjectionMode_currentIndexChanged()
{
    camera_projectionModeIndex = ui->comboProjectionMode->currentIndex();
}

void SettingsDialog::on_comboDefaultView_currentIndexChanged()
{
    camera_defaultViewIndex = ui->comboDefaultView->currentIndex();
}

void SettingsDialog::on_comboDefaultProjection_currentIndexChanged()
{
    camera_defaultProjectionIndex = ui->comboDefaultProjection->currentIndex();
}

void SettingsDialog::on_comboCameraUpAxis_currentIndexChanged()
{
    camera_defaultUpAxisIndex = ui->comboCameraUpAxis->currentIndex();
}

void SettingsDialog::on_checkTrackball_stateChanged()
{
    camera_trackball = ui->checkTrackball->isChecked();
}

void SettingsDialog::on_checkInvertZoom_stateChanged()
{
    camera_invertZoom = ui->checkInvertZoom->isChecked();
}

void SettingsDialog::on_spinZoomFactor_valueChanged()
{
    camera_zoomFactor = ui->spinZoomFactor->value();
}

// Background tab
void SettingsDialog::on_comboBoxBackgroundStyle_currentIndexChanged()
{
    background_styleIndex = ui->comboBoxBackgroundStyle->currentIndex();
}

void SettingsDialog::on_pushButtonTopColor_clicked()
{
    // Example: open color dialog and assign
    // background_topColor = QColorDialog::getColor(background_topColor, this);
    // For now, just keep as placeholder
}

void SettingsDialog::on_pushButtonBottomColor_clicked()
{
    // Example: open color dialog and assign
    // background_bottomColor = QColorDialog::getColor(background_bottomColor, this);
    // For now, just keep as placeholder
}

void SettingsDialog::on_comboBoxGradientStyle_currentIndexChanged()
{
    background_gradientStyleIndex = ui->comboBoxGradientStyle->currentIndex();
}

// Display tab
void SettingsDialog::on_showBoundingBoxCheckBox_stateChanged()
{
    display_showBoundingBox = ui->showBoundingBoxCheckBox->isChecked();
}

void SettingsDialog::on_showCornerTrihedronCheckBox_stateChanged()
{
    display_showCornerTrihedron = ui->showCornerTrihedronCheckBox->isChecked();
}

void SettingsDialog::on_showViewCubeCheckBox_stateChanged()
{
    display_showViewCube = ui->showViewCubeCheckBox->isChecked();
}

void SettingsDialog::on_comboBoxCornerTrihedronPosition_currentIndexChanged()
{
    display_cornerTrihedronPosition = currentCornerTrihedronPositionValue(ui->comboBoxCornerTrihedronPosition);
    refreshViewCubeAvailability();
    display_showViewCube = ui->showViewCubeCheckBox->isChecked();
}

void SettingsDialog::on_farPlaneSpinBox_valueChanged()
{
    display_farPlane = ui->farPlaneSpinBox->value();
}

void SettingsDialog::on_fieldOfViewSpinBox_valueChanged()
{
    display_fieldOfView = ui->fieldOfViewSpinBox->value();
}

void SettingsDialog::on_showGridCheckBox_stateChanged()
{
    display_showGrid = ui->showGridCheckBox->isChecked();
}

void SettingsDialog::on_nearPlaneSpinBox_valueChanged()
{
    display_nearPlane = ui->nearPlaneSpinBox->value();
}

void SettingsDialog::on_showWireframeCheckBox_stateChanged()
{
    display_showWireframe = ui->showWireframeCheckBox->isChecked();
}

void SettingsDialog::on_showCenterTrihedronCheckBox_stateChanged()
{
    display_showCenterTrihedron = ui->showCenterTrihedronCheckBox->isChecked();
}

// Navigation group
void SettingsDialog::on_navigationModeComboBox_currentIndexChanged()
{
    navigation_modeIndex = ui->navigationModeComboBox->currentIndex();
}

void SettingsDialog::on_mouseSensitivitySlider_valueChanged()
{
    navigation_mouseSensitivity = ui->mouseSensitivitySlider->value();
}

void SettingsDialog::on_zoomSensitivitySlider_valueChanged()
{
    navigation_zoomSensitivity = ui->zoomSensitivitySlider->value();
}

void SettingsDialog::on_invertYAxisCheckBox_stateChanged()
{
    navigation_invertYAxis = ui->invertYAxisCheckBox->isChecked();
}

void SettingsDialog::on_smoothNavigationCheckBox_stateChanged()
{
    navigation_smoothNavigation = ui->smoothNavigationCheckBox->isChecked();
}

// Rendering tab
void SettingsDialog::on_comboShadingMode_currentIndexChanged()
{
    rendering_shadingModeIndex = ui->comboShadingMode->currentIndex();
}

void SettingsDialog::on_checkBackfaceCulling_stateChanged()
{
    rendering_backfaceCulling = ui->checkBackfaceCulling->isChecked();
}

void SettingsDialog::on_checkNormalMap_stateChanged()
{
    rendering_normalMap = ui->checkNormalMap->isChecked();
}

void SettingsDialog::on_shaderModelComboBox_currentIndexChanged()
{
    rendering_shaderModelIndex = ui->shaderModelComboBox->currentIndex();
}

void SettingsDialog::on_msaaComboBox_currentIndexChanged()
{
    rendering_msaaIndex = ui->msaaComboBox->currentIndex();    
    QMessageBox::information(this, tr("MSAA Change"), tr("Please restart the application for the MSAA change to take effect."));
}

void SettingsDialog::on_anisotropyComboBox_currentIndexChanged()
{
    rendering_anisotropyIndex = ui->anisotropyComboBox->currentIndex();
}

// Lighting
void SettingsDialog::on_enableLightingCheckBox_stateChanged()
{
    lighting_enableLighting = ui->enableLightingCheckBox->isChecked();
}

void SettingsDialog::on_enableShadowsCheckBox_stateChanged()
{
    lighting_enableShadows = ui->enableShadowsCheckBox->isChecked();
}

void SettingsDialog::on_ambientLightSlider_valueChanged()
{
    lighting_ambient = ui->ambientLightSlider->value();
}

void SettingsDialog::on_diffuseLightSlider_valueChanged()
{
    lighting_diffuse = ui->diffuseLightSlider->value();
}

void SettingsDialog::on_specularLightSlider_valueChanged()
{
    lighting_specular = ui->specularLightSlider->value();
}

// Materials
void SettingsDialog::on_comboBoxDefaultMaterial_currentIndexChanged()
{
    materials_defaultMaterialIndex = ui->comboBoxDefaultMaterial->currentIndex();
}

void SettingsDialog::on_lineEditTextureDir_textChanged()
{
    materials_textureDir = ui->lineEditTextureDir->text();
}

// UV Generation Tab
void SettingsDialog::on_comboUVMethod_currentIndexChanged()
{
    uv_methodIndex = ui->comboUVMethod->currentIndex();
}

void SettingsDialog::on_spinAngleThreshold_valueChanged()
{
    uv_angleThreshold = ui->spinAngleThreshold->value();
}

void SettingsDialog::on_checkPreserveUVs_stateChanged()
{
    uv_preserveUVs = ui->checkPreserveUVs->isChecked();
}

void SettingsDialog::on_checkAutoPackUVs_stateChanged()
{
    uv_autoPackUVs = ui->checkAutoPackUVs->isChecked();
}

void SettingsDialog::on_checkRelaxUVs_stateChanged()
{
    uv_relaxUVs = ui->checkRelaxUVs->isChecked();
}

void SettingsDialog::on_checkPCAProjection_stateChanged()
{
    uv_pcaProjection = ui->checkPCAProjection->isChecked();
}

void SettingsDialog::on_checkXatlasPackingOnly_stateChanged()
{
    uv_xatlasPackingOnly = ui->checkXatlasPackingOnly->isChecked();
}

void SettingsDialog::on_checkRememberUV_stateChanged()
{
    uv_rememberUV = ui->checkRememberUV->isChecked();
}

void SettingsDialog::on_buttonResetUVPrompt_clicked()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.remove("UVMethod");
    settings.remove("RememberUVMethod");

    qDebug() << "UV Prompt settings have been reset.";
    QMessageBox::information(this, tr("Settings Reset"), tr("UV Prompt settings have been cleared."));
}

// Import/Export Tab - OpenCascade
void SettingsDialog::on_tessellationQualitySlider_valueChanged()
{
    import_tessellationQuality = ui->tessellationQualitySlider->value();
}

void SettingsDialog::on_linearDeflectionSpinBox_valueChanged()
{
    import_linearDeflection = ui->linearDeflectionSpinBox->value();
}

void SettingsDialog::on_angularDeflectionSpinBox_valueChanged()
{
    import_angularDeflection = ui->angularDeflectionSpinBox->value();
}

void SettingsDialog::on_occtUnifyFacesCheckBox_stateChanged()
{
    import_occtUnifyFaces = ui->occtUnifyFacesCheckBox->isChecked();
}

void SettingsDialog::on_occtUnifyEdgesCheckBox_stateChanged()
{
    import_occtUnifyEdges = ui->occtUnifyEdgesCheckBox->isChecked();
}

void SettingsDialog::on_occtBuildCurvesCheckBox_stateChanged()
{
    import_occtBuildCurves = ui->occtBuildCurvesCheckBox->isChecked();
}

// Import/Export Tab - Assimp
void SettingsDialog::on_assimpTriangulateCheckBox_stateChanged()
{
    import_assimpTriangulate = ui->assimpTriangulateCheckBox->isChecked();
}

void SettingsDialog::on_assimpGenNormalsCheckBox_stateChanged()
{
    import_assimpGenNormals = ui->assimpGenNormalsCheckBox->isChecked();
}

void SettingsDialog::on_assimpSmoothNormalsCheckBox_stateChanged()
{
    import_assimpSmoothNormals = ui->assimpSmoothNormalsCheckBox->isChecked();
}

void SettingsDialog::on_assimpCalcTangentsCheckBox_stateChanged()
{
    import_assimpCalcTangents = ui->assimpCalcTangentsCheckBox->isChecked();
}

void SettingsDialog::on_assimpOptimizeMeshCheckBox_stateChanged()
{
    import_assimpOptimizeMesh = ui->assimpOptimizeMeshCheckBox->isChecked();
}

void SettingsDialog::on_assimpRemoveDuplicatesCheckBox_stateChanged()
{
    import_assimpRemoveDuplicates = ui->assimpRemoveDuplicatesCheckBox->isChecked();
}

void SettingsDialog::on_assimpAutoOrientCheckBox_stateChanged()
{
	import_assimpAutoOrientModel = ui->assimpAutoOrientCheckBox->isChecked();
}

void SettingsDialog::on_assimpMaxFaceVerticesSpinBox_valueChanged()
{
    import_assimpMaxFaceVertices = ui->assimpMaxFaceVerticesSpinBox->value();
}

void SettingsDialog::on_radioButtonExportScene_toggled(bool checked)
{
	export_exportScene = checked;
}

void SettingsDialog::on_radioButtonExportMeshes_toggled(bool checked)
{
	export_exportMeshes = checked;
}

// Performance Tab
void SettingsDialog::on_checkMultithreadedLoad_stateChanged()
{
    perf_multithreadedLoad = ui->checkMultithreadedLoad->isChecked();
}

void SettingsDialog::on_spinThreadLimit_valueChanged()
{
    perf_threadLimit = ui->spinThreadLimit->value();
}

void SettingsDialog::on_checkSkyboxBlending_stateChanged()
{
    perf_skyboxBlending = ui->checkSkyboxBlending->isChecked();
}

void SettingsDialog::on_checkProgressiveLoading_stateChanged()
{
    perf_progressiveLoading = ui->checkProgressiveLoading->isChecked();
}

void SettingsDialog::on_maxFpsSpinBox_valueChanged()
{
    perf_maxFps = ui->maxFpsSpinBox->value();
}

void SettingsDialog::on_vsyncCheckBox_stateChanged()
{
    perf_vsync = ui->vsyncCheckBox->isChecked();
}

void SettingsDialog::on_frustumCullingCheckBox_stateChanged()
{
    perf_frustumCulling = ui->frustumCullingCheckBox->isChecked();
}

void SettingsDialog::on_backfaceCullingCheckBox_stateChanged()
{
    perf_backfaceCulling = ui->backfaceCullingCheckBox->isChecked();
}

void SettingsDialog::on_levelOfDetailCheckBox_stateChanged()
{
    perf_levelOfDetail = ui->levelOfDetailCheckBox->isChecked();
}

void SettingsDialog::on_maxVerticesSpinBox_valueChanged()
{
    perf_maxVertices = ui->maxVerticesSpinBox->value();
}

// Memory management
void SettingsDialog::on_textureCacheSizeSpinBox_valueChanged()
{
    perf_textureCacheSize = ui->textureCacheSizeSpinBox->value();
}

void SettingsDialog::on_geometryCacheSizeSpinBox_valueChanged()
{
    perf_geometryCacheSize = ui->geometryCacheSizeSpinBox->value();
}

void SettingsDialog::on_compressTexturesCheckBox_stateChanged()
{
    perf_compressTextures = ui->compressTexturesCheckBox->isChecked();
}

void SettingsDialog::on_generateMipmapsCheckBox_stateChanged()
{
    perf_generateMipmaps = ui->generateMipmapsCheckBox->isChecked();
}

// Advanced Tab
void SettingsDialog::on_comboBoxOpenGLVersion_currentIndexChanged()
{
    advanced_openGLVersionIndex = ui->comboBoxOpenGLVersion->currentIndex();
}

void SettingsDialog::on_checkBoxVSync_stateChanged()
{
    advanced_vsync = ui->checkBoxVSync->isChecked();
}

void SettingsDialog::on_spinBoxThreads_valueChanged()
{
    advanced_threads = ui->spinBoxThreads->value();
}

void SettingsDialog::on_checkShaderHotReload_stateChanged()
{
    advanced_shaderHotReload = ui->checkShaderHotReload->isChecked();
}

void SettingsDialog::on_checkShowFPS_stateChanged()
{
    advanced_showFPS = ui->checkShowFPS->isChecked();
}

void SettingsDialog::on_checkLegacyOpenGL_stateChanged()
{
    advanced_legacyOpenGL = ui->checkLegacyOpenGL->isChecked();
}

// Debug Tab
void SettingsDialog::on_showFpsCheckBox_stateChanged()
{
    debug_showFps = ui->showFpsCheckBox->isChecked();
}

void SettingsDialog::on_showMemoryUsageCheckBox_stateChanged()
{
    debug_showMemoryUsage = ui->showMemoryUsageCheckBox->isChecked();
}

void SettingsDialog::on_showRenderStatsCheckBox_stateChanged()
{
    debug_showRenderStats = ui->showRenderStatsCheckBox->isChecked();
}

void SettingsDialog::on_showOpenGLInfoCheckBox_stateChanged()
{
    debug_showOpenGLInfo = ui->showOpenGLInfoCheckBox->isChecked();
}

void SettingsDialog::on_enableLoggingCheckBox_stateChanged()
{
    debug_enableLogging = ui->enableLoggingCheckBox->isChecked();
}

void SettingsDialog::on_enableConsoleCheckBox_stateChanged()
{
	debug_enableConsoleOutput = ui->enableConsoleCheckBox->isChecked();
}

void SettingsDialog::on_logLevelComboBox_currentIndexChanged()
{
    debug_logLevelIndex = ui->logLevelComboBox->currentIndex();
}

void SettingsDialog::on_checkOpenGLErrorsCheckBox_stateChanged()
{
    debug_checkOpenGLErrors = ui->checkOpenGLErrorsCheckBox->isChecked();
}

void SettingsDialog::on_validateShadersCheckBox_stateChanged()
{
    debug_validateShaders = ui->validateShadersCheckBox->isChecked();
}

void SettingsDialog::on_profileRenderingCheckBox_stateChanged()
{
    debug_profileRendering = ui->profileRenderingCheckBox->isChecked();
}

void SettingsDialog::on_showTextureDebugPanelCheckBox_stateChanged()
{
    debug_showTextureDebugPanel = ui->showTextureDebugPanelCheckBox->isChecked();
    emit textureDebugPanelVisibilityChanged(debug_showTextureDebugPanel);
}

void SettingsDialog::on_clearCacheButton_clicked()
{
    // TODO: Handle clearCacheButton::clicked()
}

void SettingsDialog::on_resetSettingsButton_clicked()
{
    // TODO: Handle resetSettingsButton::clicked()
}

