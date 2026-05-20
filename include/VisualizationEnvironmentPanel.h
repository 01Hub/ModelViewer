#pragma once

#include <QWidget>
#include <QVector3D>
#include <QVector4D>
#include <memory>

namespace Ui {
	class VisualizationEnvironmentPanel;
}

class GLWidget;
class ModelViewer;
class MaterialPreviewWidget;

class VisualizationEnvironmentPanel : public QWidget
{
	Q_OBJECT

public:
	explicit VisualizationEnvironmentPanel(QWidget* parent = nullptr);
	~VisualizationEnvironmentPanel();

	// Initialization - MUST be called after panel is created
	void initialize(ModelViewer* modelViewer, GLWidget* glWidget);

	// Public accessors for ModelViewer to manage skybox state
	int getSkyBoxLDRIIndex() const { return _skyBoxLDRIIndex; }
	int getSkyBoxHDRIIndex() const { return _skyBoxHDRIIndex; }
	void setSkyBoxLDRIIndex(int index) { _skyBoxLDRIIndex = index; }
	void setSkyBoxHDRIIndex(int index) { _skyBoxHDRIIndex = index; }

	// Public method for ModelViewer to set PBR mode
	void setPBRLightingMode(bool enable);

	// Set preview widget for updating on environment changes
	void setPreviewWidget(MaterialPreviewWidget* preview) { _previewWidget = preview; }

	// Called when geometry changes to update light position slider ranges
	void updateLightPositionRanges(float range, float offset);

	bool isDetached() { return _detached; }
	void setDetached(bool detached);

signals:
	void detachRequested();

public slots:
	// ===== Light Color Buttons =====
	void onLightAmbientClicked();
	void onLightDiffuseClicked();
	void onLightSpecularClicked();
	void onDefaultLightsClicked();

	// ===== Light Position Sliders =====
	void onLightPosXChanged(int value);
	void onLightPosYChanged(int value);
	void onLightPosZChanged(int value);

	// ===== Lighting Checkboxes =====
	void onDefaultLightsChanged(bool checked);
	void onPunctualLightsChanged(bool checked);
	void onShowLightsChanged(bool checked);
	void onIBLChanged(bool checked);

	// ===== Skybox Controls =====
	void onSkyBoxStateChanged(bool checked);
	void onSkyBoxHDRIChanged(bool checked);
	void onSkyBoxBlurChanged(int value);
	void onSkyBoxFOVChanged(double value);
	void onSkyBoxMapsChanged(int index);
	void onSkyBoxTextureClicked();

	// ===== Shadow Controls =====
	void onShadowMappingStateChanged(bool checked);
	void onSelfShadowsChanged(bool checked);
	void onShadowQualityChanged(int index);

	// ===== Floor Controls =====
	void onFloorStateChanged(bool checked);
	void onFloorTextureStateChanged(bool checked);
	void onReflectionsChanged(bool checked);
	void onEnvMappingChanged(bool checked);
	void onFloorOffsetChanged(double value);
	void onRepeatSChanged(double value);
	void onRepeatTChanged(double value);
	void onFloorTextureClicked();

	// ===== HDR Controls =====
	void onHDRToneMappingStateChanged(bool checked);
	void onHDRToneMappingModeChanged(int index);
	void onEnvMapExposureChanged(double value);
	void onIBLExposureChanged(double value);

	// ===== Gamma Controls =====
	void onGammaCorrectionStateChanged(bool checked);
	void onScreenGammaChanged(double value);

	// ===== Default Values Button =====
	void onDefaultEnvValuesClicked();

	// ===== Skybox Preset Management =====
	void onLoadSkyBoxPresetMaps();

	// ===== Display Mode Changed Signal from GLWidget =====
	void onDisplayModeChanged(int mode);

	// ===== Detach Button =====
	void onDetachButtonClicked();

public:
	// Helper methods
	void connectSignalsAndSlots();
	void updateControlDependencies();
	void updateButtonStyles();
	void reloadSkyBoxPresets();

private:
	// Member variables
	ModelViewer* _modelViewer;
	GLWidget* _glWidget;
	MaterialPreviewWidget* _previewWidget = nullptr;
	std::unique_ptr<Ui::VisualizationEnvironmentPanel> ui;
	bool _isInitialized;

	// State management - moved from ModelViewer
	int _skyBoxLDRIIndex;
	int _skyBoxHDRIIndex;

	bool _detached = false;
};
