#include "ChannelPackingEditorDialog.h"
#include "GLMaterial.h"
#include "LanguageManager.h"
#include "MaterialPreviewWidget.h"
#include "MaterialPropertiesPanel.h"
#include "MaterialTextureLibrary.h"
#include "PathUtils.h"
#include "TextureParametersDialog.h"
#include "ui_MaterialPropertiesPanel.h"
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QCheckBox>
#include <QStandardPaths>

// ============================================================================
// JSON Helper Functions (copied from TextureMappingPanel.cpp)
// ============================================================================

namespace
{
	constexpr int PresetNameRole = Qt::UserRole;
	constexpr int PresetFolderRole = Qt::UserRole + 1;
	constexpr int PresetIsUserRole = Qt::UserRole + 2;

	QString wrapModeToJson(GLenum mode)
	{
		switch (mode)
		{
		case GL_CLAMP_TO_EDGE:   return QStringLiteral("clamp_to_edge");
		case GL_MIRRORED_REPEAT: return QStringLiteral("mirrored_repeat");
		case GL_REPEAT:
		default:                 return QStringLiteral("repeat");
		}
	}

	GLenum wrapModeFromJson(const QString& mode, GLenum fallback = GL_REPEAT)
	{
		const QString lower = mode.trimmed().toLower();
		if (lower == QLatin1String("clamp") || lower == QLatin1String("clamp_to_edge"))
			return GL_CLAMP_TO_EDGE;
		if (lower == QLatin1String("mirror") || lower == QLatin1String("mirrored_repeat"))
			return GL_MIRRORED_REPEAT;
		return fallback;
	}

	QString filterToJson(GLenum filter)
	{
		switch (filter)
		{
		case GL_NEAREST:                return QStringLiteral("nearest");
		case GL_LINEAR:                 return QStringLiteral("linear");
		case GL_NEAREST_MIPMAP_NEAREST: return QStringLiteral("nearest_mipmap_nearest");
		case GL_LINEAR_MIPMAP_NEAREST:  return QStringLiteral("linear_mipmap_nearest");
		case GL_NEAREST_MIPMAP_LINEAR:  return QStringLiteral("nearest_mipmap_linear");
		case GL_LINEAR_MIPMAP_LINEAR:
		default:                        return QStringLiteral("linear_mipmap_linear");
		}
	}

	GLenum filterFromJson(const QString& filter, GLenum fallback)
	{
		const QString lower = filter.trimmed().toLower();
		if (lower == QLatin1String("nearest")) return GL_NEAREST;
		if (lower == QLatin1String("linear")) return GL_LINEAR;
		if (lower == QLatin1String("nearest_mipmap_nearest")) return GL_NEAREST_MIPMAP_NEAREST;
		if (lower == QLatin1String("linear_mipmap_nearest")) return GL_LINEAR_MIPMAP_NEAREST;
		if (lower == QLatin1String("nearest_mipmap_linear")) return GL_NEAREST_MIPMAP_LINEAR;
		if (lower == QLatin1String("linear_mipmap_linear")) return GL_LINEAR_MIPMAP_LINEAR;
		return fallback;
	}

	QJsonArray vec2ToJsonArray(const glm::vec2& v)
	{
		return QJsonArray{ v.x, v.y };
	}

	QJsonArray vec3ToJsonArray(const QVector3D& v)
	{
		return QJsonArray{ v.x(), v.y(), v.z() };
	}

	glm::vec2 vec2FromJson(const QJsonValue& value, const glm::vec2& fallback)
	{
		if (!value.isArray()) return fallback;
		const QJsonArray arr = value.toArray();
		if (arr.size() < 2) return fallback;
		return glm::vec2(
			static_cast<float>(arr.at(0).toDouble(fallback.x)),
			static_cast<float>(arr.at(1).toDouble(fallback.y)));
	}

	QVector3D vec3FromJson(const QJsonValue& value, const QVector3D& fallback)
	{
		if (!value.isArray()) return fallback;
		const QJsonArray arr = value.toArray();
		if (arr.size() < 3) return fallback;
		return QVector3D(
			static_cast<float>(arr.at(0).toDouble(fallback.x())),
			static_cast<float>(arr.at(1).toDouble(fallback.y())),
			static_cast<float>(arr.at(2).toDouble(fallback.z())));
	}
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

MaterialPropertiesPanel::MaterialPropertiesPanel(QWidget* parent)
	: QWidget(parent)
	, _ui(new Ui::MaterialPropertiesPanel)
{
	_ui->setupUi(this);

	connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this, [this]() {
		_ui->retranslateUi(this);
	});

	// Enable right-click context menu
	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &QWidget::customContextMenuRequested,
		this, &MaterialPropertiesPanel::onContextMenu);

	_preview = qobject_cast<MaterialPreviewWidget*>(_ui->previewWidget);
	if (_preview)
	{
		_preview->setPreviewProfile(PreviewProfile::TextureAuthoring);
	}

	_checkerIcon = makeCheckerIcon();
	registerTextureMaps();
	connectTextureSignals();

	// Default checker on all texture buttons
	for (auto it = _textureSlots.begin(); it != _textureSlots.end(); ++it)
		applyButtonEmptyIcon(it.value());

	// Enable drag/drop on texture buttons
	for (auto it = _textureSlots.begin(); it != _textureSlots.end(); ++it)
	{
		if (it.value().button)
		{
			it.value().button->setAcceptDrops(true);
			it.value().button->installEventFilter(this);
		}
	}

	// Initialize material to default and bind
	bindMaterial(new GLMaterial());
}

MaterialPropertiesPanel::~MaterialPropertiesPanel()
{
	delete _ui;
}

// ============================================================================
// Public API - Material Binding
// ============================================================================

void MaterialPropertiesPanel::bindMaterial(GLMaterial* material)
{
	_material = material;
	if (!_material)
		return;

	// Reflect current textures -> button icons
	for (auto it = _textureSlots.begin(); it != _textureSlots.end(); ++it)
	{
		const QString path = textureMapPath(it.key());
		if (path.isEmpty())
			applyButtonEmptyIcon(it.value());
		else
			applyButtonImageIcon(it.value(), path);
	}

	// Load all scalar property values from material
	loadScalarValuesFromMaterial();

	// Load all texture factor values
	loadFactorValuesFromMaterial();

	// Update preview
	updatePreview();

	emit materialChanged(_material);
}

void MaterialPropertiesPanel::initialize(ModelViewer* modelViewer, GLWidget* glWidget)
{
	_modelViewer = modelViewer;
	_glWidget = glWidget;
}

void MaterialPropertiesPanel::setDetached(bool detached)
{
	_detached = detached;
}

// ============================================================================
// Public API - Property Getters (simplified stubs)
// ============================================================================

QVector3D MaterialPropertiesPanel::getAlbedoColor() const
{
	if (!_material) return QVector3D();
	return _material->albedoColor();
}

float MaterialPropertiesPanel::getMetalness() const
{
	if (!_material) return 0.0f;
	return _material->metalness();
}

float MaterialPropertiesPanel::getRoughness() const
{
	if (!_material) return 0.5f;
	return _material->roughness();
}

float MaterialPropertiesPanel::getIOR() const
{
	if (!_material) return 1.5f;
	return _material->ior();
}

float MaterialPropertiesPanel::getOpacity() const
{
	if (!_material) return 1.0f;
	return _material->opacity();
}

float MaterialPropertiesPanel::getEmissiveStrength() const
{
	if (!_material) return 0.0f;
	return _material->emissiveStrength();
}

QVector3D MaterialPropertiesPanel::getEmissiveColor() const
{
	if (!_material) return QVector3D();
	return _material->emissive();
}

float MaterialPropertiesPanel::getClearcoat() const
{
	if (!_material) return 0.0f;
	return _material->clearcoat();
}

float MaterialPropertiesPanel::getClearcoatRoughness() const
{
	if (!_material) return 0.0f;
	return _material->clearcoatRoughness();
}

QVector3D MaterialPropertiesPanel::getClearcoatColor() const
{
	return QVector3D(1.0f, 1.0f, 1.0f);
}

QVector3D MaterialPropertiesPanel::getSheenColor() const
{
	if (!_material) return QVector3D();
	return _material->sheenColor();
}

float MaterialPropertiesPanel::getSheenRoughness() const
{
	if (!_material) return 0.0f;
	return _material->sheenRoughness();
}

float MaterialPropertiesPanel::getTransmission() const
{
	if (!_material) return 0.0f;
	return _material->transmission();
}

float MaterialPropertiesPanel::getThickness() const { return 0.0f; }
float MaterialPropertiesPanel::getNormalScale() const { return _material ? _material->normalScale() : 1.0f; }
float MaterialPropertiesPanel::getHeightScale() const { return _material ? _material->heightScale() : 1.0f; }
float MaterialPropertiesPanel::getClearcoatNormalScale() const { return _material ? _material->clearcoatNormalScale() : 1.0f; }
float MaterialPropertiesPanel::getOcclusionStrength() const { return _material ? _material->occlusionStrength() : 1.0f; }
float MaterialPropertiesPanel::getAnisotropyStrength() const { return 0.0f; }
float MaterialPropertiesPanel::getAnisotropyRotation() const { return 0.0f; }
float MaterialPropertiesPanel::getDiffuseTransmissionFactor() const { return 0.0f; }
QVector3D MaterialPropertiesPanel::getDiffuseTransmissionColor() const { return QVector3D(); }
float MaterialPropertiesPanel::getSpecularFactor() const { return 1.0f; }
QVector3D MaterialPropertiesPanel::getSpecularColor() const { return QVector3D(); }
int MaterialPropertiesPanel::getShadingModel() const { return _material ? static_cast<int>(_material->shadingModel()) : 0; }
int MaterialPropertiesPanel::getBlendMode() const { return _material ? static_cast<int>(_material->blendMode()) : 0; }
bool MaterialPropertiesPanel::getTwoSided() const { return _material ? _material->twoSided() : false; }
bool MaterialPropertiesPanel::getWireframe() const { return _material ? _material->wireframe() : false; }
float MaterialPropertiesPanel::getAlphaThreshold() const { return _material ? _material->alphaThreshold() : 0.5f; }
bool MaterialPropertiesPanel::getUnlit() const { return _material && _material->shadingModel() == GLMaterial::ShadingModel::Unlit; }
float MaterialPropertiesPanel::getIridescenceStrength() const { return 0.0f; }
float MaterialPropertiesPanel::getIridescenceThickness() const { return 0.0f; }
float MaterialPropertiesPanel::getIridescenceIOR() const { return 1.5f; }
float MaterialPropertiesPanel::getIridescenceThinFilmThickness() const { return 0.0f; }

QString MaterialPropertiesPanel::getTexturePath(GLMaterial::TextureType type) const
{
	return textureMapPath(type);
}

// ============================================================================
// Scalar Property Slot Handlers (simplified stubs)
// ============================================================================

void MaterialPropertiesPanel::onAlbedoColorPicked()
{
	if (_updateInProgress || !_material) return;
	QColor color = QColorDialog::getColor(Qt::white, this, tr("Select Albedo Color"));
	if (color.isValid())
	{
		_material->setAlbedoColor(QVector3D(color.redF(), color.greenF(), color.blueF()));
		updateScalarUI();
		updatePreview();
		emit materialChanged(_material);
	}
}

void MaterialPropertiesPanel::onMetallicChanged(double value) { if (_material && !_updateInProgress) { _material->setMetalness(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onRoughnessChanged(double value) { if (_material && !_updateInProgress) { _material->setRoughness(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onIORChanged(double value) { if (_material && !_updateInProgress) { _material->setIOR(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onOpacityChanged(double value) { if (_material && !_updateInProgress) { _material->setOpacity(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onEmissiveStrengthChanged(double value) { if (_material && !_updateInProgress) { _material->setEmissiveStrength(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }

void MaterialPropertiesPanel::onEmissiveColorPicked()
{
	if (_updateInProgress || !_material) return;
	QColor color = QColorDialog::getColor(Qt::white, this, tr("Select Emissive Color"));
	if (color.isValid())
	{
		_material->setEmissive(QVector3D(color.redF(), color.greenF(), color.blueF()));
		updateScalarUI();
		updatePreview();
		emit materialChanged(_material);
	}
}

void MaterialPropertiesPanel::onClearcoatChanged(double value) { if (_material && !_updateInProgress) { _material->setClearcoat(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onClearcoatRoughnessChanged(double value) { if (_material && !_updateInProgress) { _material->setClearcoatRoughness(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onClearcoatColorPicked() {}
void MaterialPropertiesPanel::onSheenColorPicked() {}
void MaterialPropertiesPanel::onSheenRoughnessChanged(double value) { if (_material && !_updateInProgress) { _material->setSheenRoughness(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onTransmissionChanged(double value) { if (_material && !_updateInProgress) { _material->setTransmission(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onThicknessChanged(double value) {}
void MaterialPropertiesPanel::onNormalScaleChanged(double value) { if (_material && !_updateInProgress) { _material->setNormalScale(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onHeightScaleChanged(double value) { if (_material && !_updateInProgress) { _material->setHeightScale(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onClearcoatNormalScaleChanged(double value) { if (_material && !_updateInProgress) { _material->setClearcoatNormalScale(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onOcclusionStrengthChanged(double value) { if (_material && !_updateInProgress) { _material->setOcclusionStrength(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onAnisotropyStrengthChanged(double value) {}
void MaterialPropertiesPanel::onAnisotropyRotationChanged(double value) {}
void MaterialPropertiesPanel::onDiffuseTransmissionFactorChanged(double value) {}
void MaterialPropertiesPanel::onDiffuseTransmissionColorPicked() {}
void MaterialPropertiesPanel::onSpecularFactorChanged(double value) {}
void MaterialPropertiesPanel::onSpecularColorPicked() {}
void MaterialPropertiesPanel::onShadingModelChanged(int index) { if (_material && !_updateInProgress) { _material->setShadingModel(static_cast<GLMaterial::ShadingModel>(index)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onBlendModeChanged(int index) { if (_material && !_updateInProgress) { _material->setBlendMode(static_cast<GLMaterial::BlendMode>(index)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onTwoSidedToggled(bool checked) { if (_material && !_updateInProgress) { _material->setTwoSided(checked); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onWireframeToggled(bool checked) { if (_material && !_updateInProgress) { _material->setWireframe(checked); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onAlphaThresholdChanged(double value) { if (_material && !_updateInProgress) { _material->setAlphaThreshold(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onUnlitToggled(bool checked) {}
void MaterialPropertiesPanel::onIridescenceStrengthChanged(double value) {}
void MaterialPropertiesPanel::onIridescenceThicknessChanged(double value) {}
void MaterialPropertiesPanel::onIridescenceIORChanged(double value) {}
void MaterialPropertiesPanel::onIridescenceThinFilmThicknessChanged(double value) {}

// ============================================================================
// Texture Management
// ============================================================================

void MaterialPropertiesPanel::registerTextureMaps()
{
	if (!_ui) return;

	// Helper lambda to safely insert texture slots
	auto insertSlot = [this](GLMaterial::TextureType type, QPushButton* btn, QLabel* lbl,
							 QToolButton* gear, QToolButton* transform, QDoubleSpinBox* factor,
							 QPushButton* colorPicker, const QString& key)
	{
		if (!btn) return;  // Button is required

		MapSlot slot;
		slot.button = btn;
		slot.label = lbl;
		slot.gear = gear;
		slot.transformButton = transform;
		slot.factorSpinBox = factor;
		slot.colorPickerButton = colorPicker;
		slot.key = key;
		slot.type = type;

		_textureSlots.insert(type, slot);
	};

	// Register texture types that have UI buttons defined in the UI file
	// Only include textures where the button exists
	if (_ui->btnAlbedoTex) insertSlot(GLMaterial::TextureType::Albedo, _ui->btnAlbedoTex, _ui->lblAlbedo, nullptr, _ui->toolButtonAlbedoTexTrsf, nullptr, nullptr, "albedo");
	if (_ui->btnNormalTex) insertSlot(GLMaterial::TextureType::Normal, _ui->btnNormalTex, _ui->lblNormal, nullptr, _ui->toolButtonNormalTexTrsf, nullptr, nullptr, "normal");
	if (_ui->btnMetallicTex) insertSlot(GLMaterial::TextureType::Metallic, _ui->btnMetallicTex, _ui->lblMetallic, _ui->gearMetallic, _ui->toolButtonMetallicTexTrsf, nullptr, nullptr, "metallic");
	if (_ui->btnRoughnessTex) insertSlot(GLMaterial::TextureType::Roughness, _ui->btnRoughnessTex, _ui->lblRoughnessTex, _ui->gearRoughness, _ui->toolButtonRoughTexTrsf, nullptr, nullptr, "roughness");
	if (_ui->btnAOTex) insertSlot(GLMaterial::TextureType::AmbientOcclusion, _ui->btnAOTex, _ui->lblAmbientOcclusion, _ui->gearAO, _ui->toolButtonAOTexTrsf, nullptr, nullptr, "ao");
	if (_ui->btnOpacityTex) insertSlot(GLMaterial::TextureType::Opacity, _ui->btnOpacityTex, _ui->lblOpacityTex, _ui->gearOpacity, _ui->toolButtonOpacTexTrsf, nullptr, nullptr, "opacity");
	if (_ui->btnEmissiveTex) insertSlot(GLMaterial::TextureType::Emissive, _ui->btnEmissiveTex, _ui->lblEmissiveTex, nullptr, _ui->toolButtonEmissiveTexTrsf, nullptr, nullptr, "emissive");
	if (_ui->btnHeightTex) insertSlot(GLMaterial::TextureType::Height, _ui->btnHeightTex, _ui->lblHeightScaleLabel, nullptr, _ui->toolButtonHeightTexTrsf, nullptr, nullptr, "height");
	if (_ui->btnTransmissionTex) insertSlot(GLMaterial::TextureType::Transmission, _ui->btnTransmissionTex, _ui->lblTransmissionTex, nullptr, _ui->toolButtonTransTexTrsf, nullptr, nullptr, "transmission");
	if (_ui->btnIORTex) insertSlot(GLMaterial::TextureType::IOR, _ui->btnIORTex, _ui->lblIOR, nullptr, _ui->toolButtonIORTexTrsf, nullptr, nullptr, "ior");
	if (_ui->btnSheenColorTex) insertSlot(GLMaterial::TextureType::SheenColor, _ui->btnSheenColorTex, _ui->lblSheenColor, nullptr, nullptr, nullptr, nullptr, "sheen_color");
	if (_ui->btnSheenRoughTex) insertSlot(GLMaterial::TextureType::SheenRoughness, _ui->btnSheenRoughTex, nullptr, nullptr, _ui->toolButtonSheenRghTexTrsf, nullptr, nullptr, "sheen_rough");
	if (_ui->btnIridFactorTex) insertSlot(GLMaterial::TextureType::Iridescence, _ui->btnIridFactorTex, nullptr, nullptr, _ui->toolButtonIridColTexTrsf, nullptr, nullptr, "iridescence");
	if (_ui->btnIridescenceThicknessTex) insertSlot(GLMaterial::TextureType::IridescenceThickness, _ui->btnIridescenceThicknessTex, nullptr, nullptr, _ui->toolButtonIridRghTexTrsf, nullptr, nullptr, "iridescence_thickness");
	if (_ui->btnDiffuseTransTex) insertSlot(GLMaterial::TextureType::DiffuseTransmission, _ui->btnDiffuseTransTex, nullptr, nullptr, _ui->toolButtonDiffuseTransTexTrsf, nullptr, nullptr, "diffuse_transmission");
	if (_ui->btnThicknessTex) insertSlot(GLMaterial::TextureType::Thickness, _ui->btnThicknessTex, _ui->lblThicknessTex, nullptr, _ui->toolButtonThicknessTexTrsf, nullptr, nullptr, "thickness");
}

void MaterialPropertiesPanel::connectTextureSignals()
{
	for (auto it = _textureSlots.begin(); it != _textureSlots.end(); ++it)
	{
		auto& slot = it.value();
		if (!slot.button) continue;

		connect(slot.button, &QPushButton::clicked, this, [this, type = it.key()]() {
			onTextureButtonClicked(type);
		});

		if (slot.factorSpinBox)
		{
			connect(slot.factorSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
				this, [this, type = it.key()]() { onFactorChanged(type); });
		}

		if (slot.transformButton)
		{
			connect(slot.transformButton, &QToolButton::clicked, this, [this, type = it.key()]() {
				onTransformButtonClicked(type);
			});
		}

		if (slot.colorPickerButton)
		{
			connect(slot.colorPickerButton, &QPushButton::clicked, this, [this, type = it.key()]() {
				onColorPickerClicked(type);
			});
		}
	}
}

void MaterialPropertiesPanel::applyButtonEmptyIcon(MapSlot& m)
{
	if (!m.button) return;
	m.button->setIcon(_checkerIcon);
	m.button->setIconSize(QSize(90, 90));
	m.button->setText(QString());
	m.button->setToolTip(QString());
}

void MaterialPropertiesPanel::applyButtonImageIcon(MapSlot& m, const QString& file)
{
	if (!m.button) return;
	QIcon icon = makeIconFromFile(file);
	m.button->setIcon(icon);
	m.button->setIconSize(QSize(90, 90));
	m.button->setToolTip(file);
}

QIcon MaterialPropertiesPanel::makeIconFromFile(const QString& file, int edge) const
{
	QImage img(file);
	if (img.isNull())
		return _checkerIcon;
	img = img.scaledToWidth(edge, Qt::SmoothTransformation);
	return QIcon(QPixmap::fromImage(img));
}

QIcon MaterialPropertiesPanel::makeCheckerIcon(int w, int h, int cell)
{
	QImage checkerImg(w, h, QImage::Format_RGB32);
	QPainter painter(&checkerImg);

	for (int y = 0; y < h; y += cell)
	{
		for (int x = 0; x < w; x += cell)
		{
			bool white = ((x / cell + y / cell) % 2) == 0;
			painter.fillRect(x, y, cell, cell, white ? Qt::white : Qt::lightGray);
		}
	}
	return QIcon(QPixmap::fromImage(checkerImg));
}

void MaterialPropertiesPanel::setTextureMapPath(GLMaterial::TextureType type, const QString& file)
{
	if (!_material) return;
	_lastUsedTextureFolder = QFileInfo(file).dir().absolutePath();

	GLMaterial::Texture tex = _material->texture(type);
	tex.path = file.toStdString();
	_material->setTexture(type, tex);

	auto& slot = _textureSlots[type];
	applyButtonImageIcon(slot, file);
	updatePreview();
	emit textureSamplerChanged(_material, type);
}

void MaterialPropertiesPanel::clearTextureMap(GLMaterial::TextureType type)
{
	if (!_material) return;

	GLMaterial::Texture tex = _material->texture(type);
	tex.path = "";
	tex.id = 0;
	_material->setTexture(type, tex);

	auto& slot = _textureSlots[type];
	applyButtonEmptyIcon(slot);
	updatePreview();
	emit textureSamplerChanged(_material, type);
}

void MaterialPropertiesPanel::clearAllTexturesMaps()
{
	if (!_material) return;
	for (auto it = _textureSlots.begin(); it != _textureSlots.end(); ++it)
		clearTextureMap(it.key());
	emit textureCacheClearRequested();
}

QString MaterialPropertiesPanel::textureMapPath(GLMaterial::TextureType type) const
{
	if (!_material) return QString();
	return QString::fromStdString(_material->texture(type).path);
}

void MaterialPropertiesPanel::loadFactorValuesFromMaterial()
{
	if (!_material) return;
	_updateInProgress = true;
	for (auto it = _textureSlots.begin(); it != _textureSlots.end(); ++it)
	{
		if (it.value().factorSpinBox)
			it.value().factorSpinBox->setValue(1.0);
	}
	_updateInProgress = false;
}

void MaterialPropertiesPanel::loadScalarValuesFromMaterial() {}
void MaterialPropertiesPanel::updateScalarUI() {}
void MaterialPropertiesPanel::updateTexturePreview(GLMaterial::TextureType type) {}
void MaterialPropertiesPanel::openPackingDialogFor(GLMaterial::TextureType type) {}
void MaterialPropertiesPanel::onColorPickerClicked(GLMaterial::TextureType type) {}
void MaterialPropertiesPanel::onFactorChanged(GLMaterial::TextureType type) {}
void MaterialPropertiesPanel::onTransformButtonClicked(GLMaterial::TextureType type) {}

void MaterialPropertiesPanel::updatePreview()
{
	if (_preview && _material)
		_preview->setMaterial(*_material);
}

void MaterialPropertiesPanel::populatePresetTree() {}
void MaterialPropertiesPanel::selectPresetInTree(const QString& presetName, bool userPreset) {}
void MaterialPropertiesPanel::applyMaterialPreset(const QString& presetName, const QString& presetFolder, bool userPreset) {}
void MaterialPropertiesPanel::loadMaterialPresetMetadata(const QString& presetName) {}
bool MaterialPropertiesPanel::saveCurrentPresetMetadata() { return false; }

QString MaterialPropertiesPanel::materialLibraryRoot() const
{
	return PathUtils::getDataDirectory() + "/textures/materials";
}

QString MaterialPropertiesPanel::userMaterialLibraryRoot() const
{
	QString appData = PathUtils::getDataDirectory();
	return appData.left(appData.lastIndexOf('/')) + "/user_materials";
}

QString MaterialPropertiesPanel::currentPresetMetadataPath() const
{
	if (_currentPresetName.isEmpty())
		return QString();
	QString root = _currentPresetIsUser ? userMaterialLibraryRoot() : materialLibraryRoot();
	return root + "/" + _currentPresetFolder + "/material.json";
}

void MaterialPropertiesPanel::onTextureButtonClicked(GLMaterial::TextureType type)
{
	QString texFolder = _lastUsedTextureFolder;
	if (texFolder.isEmpty())
		texFolder = PathUtils::getDataDirectory() + "/textures/materials";

	QString file = QFileDialog::getOpenFileName(this, tr("Select Texture"), texFolder,
		tr("Image Files (*.png *.jpg *.jpeg *.bmp *.tga);;All Files (*)"));

	if (!file.isEmpty())
		setTextureMapPath(type, file);
}

void MaterialPropertiesPanel::onTextureFactorChanged(GLMaterial::TextureType type) {}
void MaterialPropertiesPanel::onTextureTransformClicked(GLMaterial::TextureType type) {}
void MaterialPropertiesPanel::onTextureColorPickerClicked(GLMaterial::TextureType type) {}

void MaterialPropertiesPanel::onClearAllTextures()
{
	if (QMessageBox::question(this, tr("Clear All Textures"),
		tr("Are you sure you want to clear all textures?")) == QMessageBox::Yes)
		clearAllTexturesMaps();
}

void MaterialPropertiesPanel::onMaterialPresetSelected(const QString& presetName) {}
void MaterialPropertiesPanel::onSaveToLibrary() {}

void MaterialPropertiesPanel::onContextMenu(const QPoint& pos)
{
	QMenu menu;
	menu.addAction(tr("Clear All Textures"), this, &MaterialPropertiesPanel::onClearAllTextures);
	menu.exec(mapToGlobal(pos));
}

bool MaterialPropertiesPanel::eventFilter(QObject* obj, QEvent* ev)
{
	if (ev->type() == QEvent::Drop)
	{
		QDropEvent* dropEv = static_cast<QDropEvent*>(ev);
		const QMimeData* mime = dropEv->mimeData();

		if (mime->hasUrls())
		{
			QList<QUrl> urls = mime->urls();
			if (!urls.isEmpty())
			{
				QString filePath = urls.first().toLocalFile();

				for (auto it = _textureSlots.begin(); it != _textureSlots.end(); ++it)
				{
					if (it.value().button == obj)
					{
						setTextureMapPath(it.key(), filePath);
						dropEv->acceptProposedAction();
						return true;
					}
				}
			}
		}
	}
	else if (ev->type() == QEvent::DragEnter)
	{
		QDragEnterEvent* dragEv = static_cast<QDragEnterEvent*>(ev);
		if (dragEv->mimeData()->hasUrls())
		{
			dragEv->acceptProposedAction();
			return true;
		}
	}

	return QWidget::eventFilter(obj, ev);
}
