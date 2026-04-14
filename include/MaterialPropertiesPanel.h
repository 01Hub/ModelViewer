#pragma once

#include <QWidget>
#include <QIcon>
#include <QHash>
#include <QPushButton>
#include <QLabel>
#include <QToolButton>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QMenu>
#include <QImage>
#include <QPainter>
#include <QVector3D>
#include <QDataStream>
#include <QFileDialog>
#include <QColorDialog>
#include <QComboBox>

#include "GLMaterial.h"

// Forward declarations
class ModelViewer;
class GLWidget;
class MaterialPreviewWidget;

namespace Ui
{
	class MaterialPropertiesPanel;
}

class MaterialPropertiesPanel : public QWidget
{
	Q_OBJECT
public:
	explicit MaterialPropertiesPanel(QWidget* parent = nullptr);
	~MaterialPropertiesPanel() override;

	// Material binding
	void bindMaterial(GLMaterial* material);
	GLMaterial* material() const { return _material; }

	// Initialization
	void initialize(ModelViewer* modelViewer, GLWidget* glWidget);

	// State
	bool isDetached() const { return _detached; }
	void setDetached(bool detached);

	// Property getters - all 37 scalar properties
	QVector3D getAlbedoColor() const;
	float getMetalness() const;
	float getRoughness() const;
	float getIOR() const;
	float getOpacity() const;
	float getEmissiveStrength() const;
	QVector3D getEmissiveColor() const;
	float getClearcoat() const;
	float getClearcoatRoughness() const;
	QVector3D getClearcoatColor() const;
	QVector3D getSheenColor() const;
	float getSheenRoughness() const;
	float getTransmission() const;
	float getThickness() const;
	float getNormalScale() const;
	float getHeightScale() const;
	float getClearcoatNormalScale() const;
	float getOcclusionStrength() const;
	float getAnisotropyStrength() const;
	float getAnisotropyRotation() const;
	float getDiffuseTransmissionFactor() const;
	QVector3D getDiffuseTransmissionColor() const;
	float getSpecularFactor() const;
	QVector3D getSpecularColor() const;
	int getShadingModel() const;
	int getBlendMode() const;
	bool getTwoSided() const;
	bool getWireframe() const;
	float getAlphaThreshold() const;
	bool getUnlit() const;
	float getIridescenceStrength() const;
	float getIridescenceThickness() const;
	float getIridescenceIOR() const;
	float getIridescenceThinFilmThickness() const;

	// Texture path getter
	QString getTexturePath(GLMaterial::TextureType type) const;

signals:
	void materialChanged(GLMaterial* material);
	void materialApplied(const GLMaterial& material);
	void textureSamplerChanged(GLMaterial* material, GLMaterial::TextureType type);
	void textureCacheClearRequested();
	void detachRequested();

private slots:
	// Scalar property handlers
	void onAlbedoColorPicked();
	void onMetallicChanged(double value);
	void onRoughnessChanged(double value);
	void onIORChanged(double value);
	void onOpacityChanged(double value);
	void onEmissiveStrengthChanged(double value);
	void onEmissiveColorPicked();
	void onClearcoatChanged(double value);
	void onClearcoatRoughnessChanged(double value);
	void onClearcoatColorPicked();
	void onSheenColorPicked();
	void onSheenRoughnessChanged(double value);
	void onTransmissionChanged(double value);
	void onThicknessChanged(double value);
	void onNormalScaleChanged(double value);
	void onHeightScaleChanged(double value);
	void onClearcoatNormalScaleChanged(double value);
	void onOcclusionStrengthChanged(double value);
	void onAnisotropyStrengthChanged(double value);
	void onAnisotropyRotationChanged(double value);
	void onDiffuseTransmissionFactorChanged(double value);
	void onDiffuseTransmissionColorPicked();
	void onSpecularFactorChanged(double value);
	void onSpecularColorPicked();
	void onShadingModelChanged(int index);
	void onBlendModeChanged(int index);
	void onTwoSidedToggled(bool checked);
	void onWireframeToggled(bool checked);
	void onAlphaThresholdChanged(double value);
	void onUnlitToggled(bool checked);
	void onIridescenceStrengthChanged(double value);
	void onIridescenceThicknessChanged(double value);
	void onIridescenceIORChanged(double value);
	void onIridescenceThinFilmThicknessChanged(double value);

	// Texture handlers
	void onTextureButtonClicked(GLMaterial::TextureType type);
	void onTextureFactorChanged(GLMaterial::TextureType type);
	void onTextureTransformClicked(GLMaterial::TextureType type);
	void onTextureColorPickerClicked(GLMaterial::TextureType type);
	void onClearAllTextures();

	// Preset handlers
	void onMaterialPresetSelected(const GLMaterial& material);
	void onMaterialDoubleClicked(const GLMaterial& material);
	void onSaveToLibrary();

	void onContextMenu(const QPoint& pos);

protected:
	bool eventFilter(QObject* obj, QEvent* ev) override;

private:
	// Texture management
	struct MapSlot
	{
		QPushButton* button = nullptr;           // square thumbnail button
		QLabel* label = nullptr;                 // text under the button
		QToolButton* gear = nullptr;             // optional channel packing gear
		QToolButton* transformButton = nullptr;  // transform/sampler button
		QDoubleSpinBox* factorSpinBox = nullptr; // numeric factor spin box
		QPushButton* colorPickerButton = nullptr; // color picker for color factors
		QString key;                             // e.g., "albedo", "roughness"
		GLMaterial::TextureType type;            // enum type
	};

	// Texture management helpers
	void registerTextureMaps();
	void connectTextureSignals();
	void applyButtonEmptyIcon(MapSlot& m);
	void applyButtonImageIcon(MapSlot& m, const QString& file);
	QIcon makeIconFromFile(const QString& file, int edge = 90) const;
	static QIcon makeCheckerIcon(int w = 90, int h = 90, int cell = 8);

	// GLMaterial sync
	void setTextureMapPath(GLMaterial::TextureType type, const QString& file);
	void clearTextureMap(GLMaterial::TextureType type);
	void clearAllTexturesMaps();
	QString textureMapPath(GLMaterial::TextureType type) const;

	// Material loading
	void loadMaterialTexturesFromKey(const QString& materialKey);

	// Load factor values from material into UI spin boxes
	void loadFactorValuesFromMaterial();
	void loadScalarValuesFromMaterial();
	void updateScalarUI();

	// Texture operations
	void updateTexturePreview(GLMaterial::TextureType type);
	void openPackingDialogFor(GLMaterial::TextureType type);

	// Slot handlers
	void onColorPickerClicked(GLMaterial::TextureType type);
	void onFactorChanged(GLMaterial::TextureType type);
	void onTransformButtonClicked(GLMaterial::TextureType type);
	void onSearchTextChanged(const QString& text);

	void updatePreview();

	// Preset operations
	void populatePresetTree();
	void selectPresetInTree(const QString& presetName, bool userPreset);
	void applyMaterialPreset(const QString& presetName, const QString& presetFolder = QString(), bool userPreset = false);
	void loadMaterialPresetMetadata(const QString& presetName);
	bool saveCurrentPresetMetadata();
	QString materialLibraryRoot() const;
	QString userMaterialLibraryRoot() const;
	QString currentPresetMetadataPath() const;

	// Material library signal management
	void connectMaterialLibrarySignals();

	// Members
	Ui::MaterialPropertiesPanel* _ui = nullptr;
	GLMaterial* _material = nullptr;
	ModelViewer* _modelViewer = nullptr;
	GLWidget* _glWidget = nullptr;
	MaterialPreviewWidget* _preview = nullptr;

	QHash<GLMaterial::TextureType, MapSlot> _textureSlots;
	QIcon _checkerIcon;

	QString _lastUsedTextureFolder;
	QString _currentPresetName;
	QString _currentPresetFolder;
	bool _currentPresetIsUser = false;

	bool _detached = false;
	bool _updateInProgress = false;
};
