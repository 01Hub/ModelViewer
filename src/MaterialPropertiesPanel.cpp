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
#include <QRegularExpression>
#include <functional>

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

	// Connect search functionality for material library
	if (_ui->searchEdit)
	{
		connect(_ui->searchEdit, &QLineEdit::textChanged, this, &MaterialPropertiesPanel::onSearchTextChanged);
	}

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

	// Connect color picker buttons
	if (_ui->btnAlbedoColor)
		connect(_ui->btnAlbedoColor, &QPushButton::clicked, this, &MaterialPropertiesPanel::onAlbedoColorPicked);
	if (_ui->btnEmissiveColor)
		connect(_ui->btnEmissiveColor, &QPushButton::clicked, this, &MaterialPropertiesPanel::onEmissiveColorPicked);
	if (_ui->btnCCColor)
		connect(_ui->btnCCColor, &QPushButton::clicked, this, &MaterialPropertiesPanel::onClearcoatColorPicked);
	if (_ui->btnSheenColor)
		connect(_ui->btnSheenColor, &QPushButton::clicked, this, &MaterialPropertiesPanel::onSheenColorPicked);
	if (_ui->btnDiffTransColor)
		connect(_ui->btnDiffTransColor, &QPushButton::clicked, this, &MaterialPropertiesPanel::onDiffuseTransmissionColorPicked);
	if (_ui->btnSpecularColor)
		connect(_ui->btnSpecularColor, &QPushButton::clicked, this, &MaterialPropertiesPanel::onSpecularColorPicked);

	// Initialize material to default and bind
	bindMaterial(new GLMaterial());

	// Connect tree widget material selection (SINGLE-CLICK PREVIEW AND DOUBLE-CLICK CONFIRM)
	MaterialLibraryWidget* libraryWidget = qobject_cast<MaterialLibraryWidget*>(_ui->treeWidget);
	if (libraryWidget)
	{
		// Single-click: update preview immediately (for editing)
		connect(libraryWidget, &MaterialLibraryWidget::materialPreview,
			this, [this](const GLMaterial& mat) { onMaterialPresetSelected(mat); });

		// Double-click: apply material to selected mesh in main viewer
		connect(libraryWidget, &MaterialLibraryWidget::materialSelected,
			this, [this](const GLMaterial& mat) { onMaterialDoubleClicked(mat); });

		// Load first material by default on panel initialization
		// NOTE: MaterialLibraryWidget::populateMaterials() already selected the first item
		// during construction. We just need to get it and load it into the panel.
		QTreeWidgetItem* currentItem = libraryWidget->currentItem();
		if (currentItem)
		{
			QString materialKey = currentItem->data(0, Qt::UserRole).toString();
			if (!materialKey.isEmpty())
			{
				qDebug() << "=== DEFAULT MATERIAL LOADED ===";
				qDebug() << "  Material name:" << currentItem->text(0);
				qDebug() << "  Material key:" << materialKey;

				// Get the currently selected material from the library
				if (libraryWidget->sharedMaterialMap().contains(materialKey))
				{
					GLMaterial defaultMat = libraryWidget->sharedMaterialMap()[materialKey]();
					// Call our handler directly to load textures and update preview
					onMaterialPresetSelected(defaultMat);
				}
			}
		}
	}

	// Connect Clear buttons for each texture slot
	connect(_ui->pushButtonClearAllMaps, &QPushButton::clicked,
		this, &MaterialPropertiesPanel::clearAllTexturesMaps);
	// Connect cache clearing button with confirmation dialog
	connect(_ui->pushButtonClearTextureCache, &QPushButton::clicked, this, [this] {
		clearAllTexturesMaps();
		QMessageBox::information(nullptr, "Texture Cache Cleared",
			"The texture cache has been cleared.");
		});

	// Connect Apply button
	if (_ui->applyButton)
	{
		connect(_ui->applyButton, &QPushButton::clicked, this, [this]() {
			if (_material)
				emit materialApplied(*_material);
		});
	}

	// Connect Detach button
	if (_ui->detachButton)
	{
		connect(_ui->detachButton, &QToolButton::clicked, this, &MaterialPropertiesPanel::detachRequested);
	}

	// Connect preview controls to updatePreview
	if (_ui->comboShape)
		connect(_ui->comboShape, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MaterialPropertiesPanel::updatePreview);

	if (_ui->comboEnv)
		connect(_ui->comboEnv, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MaterialPropertiesPanel::updatePreview);

	if (_ui->sliderExposure)
		connect(_ui->sliderExposure, &QSlider::valueChanged, this, &MaterialPropertiesPanel::updatePreview);

	if (_ui->comboBoxTexMode)
		connect(_ui->comboBoxTexMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MaterialPropertiesPanel::updatePreview);

	if (_ui->tintModeCombo)
		connect(_ui->tintModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MaterialPropertiesPanel::updatePreview);

	if (_ui->tintStrengthSpin)
		connect(_ui->tintStrengthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MaterialPropertiesPanel::updatePreview);
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
	qDebug() << "=== bindMaterial CALLED ===";
	_material = material;
	if (!_material)
	{
		qDebug() << "  ERROR: material is null!";
		return;
	}

	qDebug() << "  Material bound, albedo:" << _material->albedoColor();

	// Reflect current textures -> button icons
	qDebug() << "  Updating texture button icons...";
	for (auto it = _textureSlots.begin(); it != _textureSlots.end(); ++it)
	{
		const QString path = textureMapPath(it.key());
		if (path.isEmpty())
			applyButtonEmptyIcon(it.value());
		else
		{
			qDebug() << "    Texture found:" << static_cast<int>(it.key()) << "path:" << path;
			applyButtonImageIcon(it.value(), path);
		}
	}

	// Load all scalar property values from material
	qDebug() << "  Loading scalar values...";
	loadScalarValuesFromMaterial();
	qDebug() << "  Scalar values loaded";

	// Load all texture factor values
	qDebug() << "  Loading factor values...";
	loadFactorValuesFromMaterial();
	qDebug() << "  Factor values loaded";

	// Load texture scale values
	qDebug() << "  Loading scale values...";
	if (_ui->doubleSpinBoxNormalScale)
		_ui->doubleSpinBoxNormalScale->setValue(_material->normalScale());
	if (_ui->doubleSpinBoxHeightScale)
		_ui->doubleSpinBoxHeightScale->setValue(_material->heightScale());
	if (_ui->doubleSpinBoxClearcoatNormalScale)
		_ui->doubleSpinBoxClearcoatNormalScale->setValue(_material->clearcoatNormalScale());
	qDebug() << "  Scale values loaded";

	// Update preview
	qDebug() << "  Calling updatePreview()...";
	updatePreview();
	qDebug() << "  updatePreview() completed";

	emit materialChanged(_material);
	qDebug() << "=== bindMaterial COMPLETED ===";
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

// Helper function to set button background color with contrasting text color
static void setButtonColorWithContrast(QPushButton* btn, const QColor& color)
{
	if (!btn) return;

	// Calculate luminance for text color contrast
	float r = color.redF();
	float g = color.greenF();
	float b = color.blueF();
	float luminance = 0.299f * r + 0.587f * g + 0.114f * b;
	QString textColor = (luminance > 0.5f) ? "black" : "white";

	btn->setStyleSheet(QString("background-color: %1; color: %2;").arg(color.name(), textColor));
}

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
void MaterialPropertiesPanel::onClearcoatColorPicked()
{
	if (!_material || !_ui) return;
	QColor color = QColorDialog::getColor(QColor(255, 255, 255), this);
	if (color.isValid())
	{
		setButtonColorWithContrast(_ui->btnCCColor, color);
		updatePreview();
		emit materialChanged(_material);
	}
}
void MaterialPropertiesPanel::onSheenColorPicked()
{
	if (!_material || !_ui) return;
	QColor color = QColorDialog::getColor(QColor(128, 128, 128), this);
	if (color.isValid())
	{
		_material->setSheenColor(QVector3D(color.redF(), color.greenF(), color.blueF()));
		setButtonColorWithContrast(_ui->btnSheenColor, color);
		updatePreview();
		emit materialChanged(_material);
	}
}
void MaterialPropertiesPanel::onSheenRoughnessChanged(double value) { if (_material && !_updateInProgress) { _material->setSheenRoughness(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onTransmissionChanged(double value) { if (_material && !_updateInProgress) { _material->setTransmission(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onThicknessChanged(double value) { if (_material && !_updateInProgress) { _material->setThicknessFactor(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onNormalScaleChanged(double value) { if (_material && !_updateInProgress) { _material->setNormalScale(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onHeightScaleChanged(double value) { if (_material && !_updateInProgress) { _material->setHeightScale(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onClearcoatNormalScaleChanged(double value) { if (_material && !_updateInProgress) { _material->setClearcoatNormalScale(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onOcclusionStrengthChanged(double value) { if (_material && !_updateInProgress) { _material->setOcclusionStrength(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onAnisotropyStrengthChanged(double value) { if (_material && !_updateInProgress) { _material->setAnisotropyStrength(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onAnisotropyRotationChanged(double value) { if (_material && !_updateInProgress) { _material->setAnisotropyRotation(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onDiffuseTransmissionFactorChanged(double value) { if (_material && !_updateInProgress) { _material->setDiffuseTransmissionFactor(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onDiffuseTransmissionColorPicked()
{
	if (!_material || !_ui) return;
	QColor color = QColorDialog::getColor(QColor(200, 200, 200), this);
	if (color.isValid())
	{
		_material->setDiffuseTransmissionColorFactor(QVector3D(color.redF(), color.greenF(), color.blueF()));
		setButtonColorWithContrast(_ui->btnDiffTransColor, color);
		updatePreview();
		emit materialChanged(_material);
	}
}
void MaterialPropertiesPanel::onSpecularFactorChanged(double value) { if (_material && !_updateInProgress) { _material->setSpecularFactor(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onSpecularColorPicked()
{
	if (!_material || !_ui) return;
	QColor color = QColorDialog::getColor(QColor(255, 255, 255), this);
	if (color.isValid())
	{
		_material->setSpecularColor(QVector3D(color.redF(), color.greenF(), color.blueF()));
		setButtonColorWithContrast(_ui->btnSpecularColor, color);
		updatePreview();
		emit materialChanged(_material);
	}
}
void MaterialPropertiesPanel::onShadingModelChanged(int index) { if (_material && !_updateInProgress) { _material->setShadingModel(static_cast<GLMaterial::ShadingModel>(index)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onBlendModeChanged(int index) { if (_material && !_updateInProgress) { _material->setBlendMode(static_cast<GLMaterial::BlendMode>(index)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onTwoSidedToggled(bool checked) { if (_material && !_updateInProgress) { _material->setTwoSided(checked); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onWireframeToggled(bool checked) { if (_material && !_updateInProgress) { _material->setWireframe(checked); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onAlphaThresholdChanged(double value) { if (_material && !_updateInProgress) { _material->setAlphaThreshold(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onUnlitToggled(bool checked) { if (_material && !_updateInProgress) { _material->setUnlit(checked); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onIridescenceStrengthChanged(double value) { if (_material && !_updateInProgress) { _material->setIridescenceFactor(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onIridescenceThicknessChanged(double value) { if (_material && !_updateInProgress) { _material->setIridescenceThicknessMin(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onIridescenceIORChanged(double value) { if (_material && !_updateInProgress) { _material->setIridescenceIor(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }
void MaterialPropertiesPanel::onIridescenceThinFilmThicknessChanged(double value) { if (_material && !_updateInProgress) { _material->setIridescenceThicknessMax(static_cast<float>(value)); updatePreview(); emit materialChanged(_material); } }

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
	if (_ui->btnSheenColorTex) insertSlot(GLMaterial::TextureType::SheenColor, _ui->btnSheenColorTex, _ui->lblSheenColor, nullptr, _ui->toolButtonSheenColTexTrsf, nullptr, nullptr, "sheen_color");
	if (_ui->btnSheenRoughTex) insertSlot(GLMaterial::TextureType::SheenRoughness, _ui->btnSheenRoughTex, nullptr, nullptr, _ui->toolButtonSheenRghTexTrsf, nullptr, nullptr, "sheen_rough");
	if (_ui->btnCCColorTex) insertSlot(GLMaterial::TextureType::ClearcoatColor, _ui->btnCCColorTex, nullptr, nullptr, _ui->toolButtonClearCColTexTrsf, nullptr, nullptr, "clearcoat_color");
	if (_ui->btnCCRoughTex) insertSlot(GLMaterial::TextureType::ClearcoatRoughness, _ui->btnCCRoughTex, nullptr, nullptr, _ui->toolButtonClearCRghTexTrsf, nullptr, nullptr, "clearcoat_rough");
	if (_ui->btnCCNormalTex) insertSlot(GLMaterial::TextureType::ClearcoatNormal, _ui->btnCCNormalTex, nullptr, nullptr, _ui->toolButtonClearCNorTexTrsf, nullptr, nullptr, "clearcoat_normal");
	if (_ui->btnIridFactorTex) insertSlot(GLMaterial::TextureType::Iridescence, _ui->btnIridFactorTex, nullptr, nullptr, _ui->toolButtonIridColTexTrsf, nullptr, nullptr, "iridescence");
	if (_ui->btnIridescenceThicknessTex) insertSlot(GLMaterial::TextureType::IridescenceThickness, _ui->btnIridescenceThicknessTex, nullptr, nullptr, _ui->toolButtonIridRghTexTrsf, nullptr, nullptr, "iridescence_thickness");
	if (_ui->btnSpecFactorColorTex) insertSlot(GLMaterial::TextureType::SpecularFactor, _ui->btnSpecFactorColorTex, nullptr, nullptr, _ui->toolButtonSpecFactorTexTrsf, nullptr, nullptr, "specular_factor");
	if (_ui->btnSpecColorColorTex) insertSlot(GLMaterial::TextureType::SpecularColor, _ui->btnSpecColorColorTex, nullptr, nullptr, _ui->toolButtonSpecColorTexTrsf, nullptr, nullptr, "specular_color");
	if (_ui->btnAnisotropyColorTex) insertSlot(GLMaterial::TextureType::Anisotropy, _ui->btnAnisotropyColorTex, nullptr, nullptr, _ui->toolButtonAnisotropyTexTrsf, nullptr, nullptr, "anisotropy");
	if (_ui->btnDiffuseTransTex) insertSlot(GLMaterial::TextureType::DiffuseTransmission, _ui->btnDiffuseTransTex, nullptr, nullptr, _ui->toolButtonDiffuseTransTexTrsf, nullptr, nullptr, "diffuse_transmission");
	if (_ui->btnDiffuseTransColorTex) insertSlot(GLMaterial::TextureType::DiffuseTransmissionColor, _ui->btnDiffuseTransColorTex, nullptr, nullptr, _ui->toolButtonDiffuseTransColorTexTrsf, nullptr, nullptr, "diffuse_transmission_color");
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

	// Connect gear buttons for channel packing
	for (auto it = _textureSlots.begin(); it != _textureSlots.end(); ++it)
	{
		if (it.value().gear)
		{
			connect(it.value().gear, &QToolButton::clicked, this, [this, type = it.key()]() {
				openPackingDialogFor(type);
			});
		}
	}

	// Connect scale spinboxes for texture coordinate scaling
	if (_ui->doubleSpinBoxNormalScale)
	{
		connect(_ui->doubleSpinBoxNormalScale, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, [this](double value) {
				if (!_material) return;
				_material->setNormalScale(static_cast<float>(value));
				updatePreview();
				emit materialChanged(_material);
			});
	}

	if (_ui->doubleSpinBoxHeightScale)
	{
		connect(_ui->doubleSpinBoxHeightScale, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, [this](double value) {
				if (!_material) return;
				_material->setHeightScale(static_cast<float>(value));
				updatePreview();
				emit materialChanged(_material);
			});
	}

	if (_ui->doubleSpinBoxClearcoatNormalScale)
	{
		connect(_ui->doubleSpinBoxClearcoatNormalScale, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, [this](double value) {
				if (!_material) return;
				_material->setClearcoatNormalScale(static_cast<float>(value));
				updatePreview();
				emit materialChanged(_material);
			});
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

	// Clear using NEW API (for preview and new code paths)
	// Follow TextureMappingPanel's pattern: create fresh texture with proper type
	GLMaterial::Texture resetTex;
	resetTex.type = GLMaterial::textureTypeToString(type).toStdString();
	_material->setTexture(type, resetTex);

	// ALSO clear using OLD API (for compatibility with onCustomMaterialApplied)
	// The old API stores texture paths in separate fields that are used when applying materials
	switch (type)
	{
		case GLMaterial::TextureType::Albedo:
			_material->clearAlbedoMap();
			break;
		case GLMaterial::TextureType::Normal:
			_material->clearNormalMap();
			break;
		case GLMaterial::TextureType::Metallic:
			_material->clearMetallicMap();
			break;
		case GLMaterial::TextureType::Roughness:
			_material->clearRoughnessMap();
			break;
		case GLMaterial::TextureType::AmbientOcclusion:
			_material->clearAOMap();
			break;
		case GLMaterial::TextureType::Opacity:
			_material->clearOpacityMap();
			break;
		case GLMaterial::TextureType::Emissive:
			_material->clearEmissiveMap();
			break;
		case GLMaterial::TextureType::Height:
			_material->clearHeightMap();
			break;
		case GLMaterial::TextureType::Transmission:
			_material->clearTransmissionMap();
			break;
		case GLMaterial::TextureType::IOR:
			_material->clearIORMap();
			break;
		case GLMaterial::TextureType::SheenColor:
			_material->clearSheenColorMap();
			break;
		case GLMaterial::TextureType::SheenRoughness:
			_material->clearSheenRoughnessMap();
			break;
		case GLMaterial::TextureType::ClearcoatColor:
			_material->clearClearcoatColorMap();
			break;
		case GLMaterial::TextureType::ClearcoatRoughness:
			_material->clearClearcoatRoughnessMap();
			break;
		case GLMaterial::TextureType::ClearcoatNormal:
			_material->clearClearcoatNormalMap();
			break;
		case GLMaterial::TextureType::Iridescence:
			_material->clearIridescenceMap();
			break;
		case GLMaterial::TextureType::IridescenceThickness:
			_material->clearIridescenceThicknessMap();
			break;
		case GLMaterial::TextureType::SpecularFactor:
			_material->clearSpecularFactorMap();
			break;
		case GLMaterial::TextureType::SpecularColor:
			_material->clearSpecularColorMap();
			break;
		case GLMaterial::TextureType::Anisotropy:
			_material->clearAnisotropyMap();
			break;
		case GLMaterial::TextureType::DiffuseTransmission:
			_material->clearDiffuseTransmissionMap();
			break;
		case GLMaterial::TextureType::DiffuseTransmissionColor:
			_material->clearDiffuseTransmissionColorMap();
			break;
		case GLMaterial::TextureType::Thickness:
			_material->clearThicknessMap();
			break;
		default:
			break;
	}

	auto& slot = _textureSlots[type];
	applyButtonEmptyIcon(slot);
	// Note: updatePreview() is NOT called here. It's called once at the end of clearAllTexturesMaps()
	// to avoid multiple unnecessary preview updates
	emit textureSamplerChanged(_material, type);
}

void MaterialPropertiesPanel::clearAllTexturesMaps()
{
	if (!_material) return;

	qDebug() << "=== clearAllTexturesMaps CALLED ===";

	// Clear all texture maps and update UI
	for (auto it = _textureSlots.begin(); it != _textureSlots.end(); ++it)
	{
		clearTextureMap(it.key());
		qDebug() << "  Cleared texture type:" << static_cast<int>(it.key());
	}

	// Update preview to show cleared state
	updatePreview();

	// Reapply the cleared material to selected meshes in the viewer
	qDebug() << "  Emitting materialApplied to apply cleared material to mesh...";
	emit materialApplied(*_material);

	// Clear GPU texture cache to free memory
	qDebug() << "  Clearing GPU texture cache...";
	emit textureCacheClearRequested();

	qDebug() << "=== clearAllTexturesMaps COMPLETED ===";
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

void MaterialPropertiesPanel::loadScalarValuesFromMaterial()
{
	if (!_material || !_ui) return;

	_updateInProgress = true;

	// Scalar numeric properties
	if (_ui->metalnessSpin) _ui->metalnessSpin->setValue(_material->metalness());
	if (_ui->roughnessSpin) _ui->roughnessSpin->setValue(_material->roughness());
	if (_ui->iorSpin) _ui->iorSpin->setValue(_material->ior());
	if (_ui->opacitySpin) _ui->opacitySpin->setValue(_material->opacity());
	if (_ui->emissiveSpin) _ui->emissiveSpin->setValue(_material->emissiveStrength());
	if (_ui->clearcoatSpin) _ui->clearcoatSpin->setValue(_material->clearcoat());
	if (_ui->clearcoatRoughnessSpin) _ui->clearcoatRoughnessSpin->setValue(_material->clearcoatRoughness());
	if (_ui->sheenRoughnessSpin) _ui->sheenRoughnessSpin->setValue(_material->sheenRoughness());
	if (_ui->transmissionSpin) _ui->transmissionSpin->setValue(_material->transmission());
	if (_ui->thicknessSpin) _ui->thicknessSpin->setValue(_material->thicknessFactor());
	if (_ui->alphaThresholdSpin) _ui->alphaThresholdSpin->setValue(_material->alphaThreshold());
	if (_ui->occlusionStrengthSpin) _ui->occlusionStrengthSpin->setValue(_material->occlusionStrength());
	if (_ui->aoSpin) _ui->aoSpin->setValue(_material->occlusionStrength());
	if (_ui->dispersionSpin) _ui->dispersionSpin->setValue(_material->dispersion());
	if (_ui->attenuationDistanceSpin) _ui->attenuationDistanceSpin->setValue(_material->attenuationDistance());
	if (_ui->iridescenceFactorSpin) _ui->iridescenceFactorSpin->setValue(_material->iridescenceFactor());
	if (_ui->iridescenceIorSpin) _ui->iridescenceIorSpin->setValue(_material->iridescenceIor());
	if (_ui->iridescenceThicknessMinSpin) _ui->iridescenceThicknessMinSpin->setValue(_material->iridescenceThicknessMin());
	if (_ui->iridescenceThicknessMaxSpin) _ui->iridescenceThicknessMaxSpin->setValue(_material->iridescenceThicknessMax());
	if (_ui->doubleSpinBoxAnisotropyStrength) _ui->doubleSpinBoxAnisotropyStrength->setValue(_material->anisotropyStrength());
	if (_ui->doubleSpinBoxAnisotropyRotation) _ui->doubleSpinBoxAnisotropyRotation->setValue(_material->anisotropyRotation());
	if (_ui->doubleSpinBoxDiffuseTransmissionFactor) _ui->doubleSpinBoxDiffuseTransmissionFactor->setValue(_material->diffuseTransmissionFactor());
	if (_ui->doubleSpinBoxSpecularFactor) _ui->doubleSpinBoxSpecularFactor->setValue(_material->specularFactor());
	if (_ui->doubleSpinBoxNormalScale) _ui->doubleSpinBoxNormalScale->setValue(_material->normalScale());
	if (_ui->doubleSpinBoxHeightScale) _ui->doubleSpinBoxHeightScale->setValue(_material->heightScale());
	if (_ui->doubleSpinBoxClearcoatNormalScale) _ui->doubleSpinBoxClearcoatNormalScale->setValue(_material->clearcoatNormalScale());

	// Color properties
	auto setColorButton = [this](QPushButton* btn, const QVector3D& color) {
		if (!btn) return;
		QColor qcolor(
			int(std::clamp(color.x(), 0.0f, 1.0f) * 255),
			int(std::clamp(color.y(), 0.0f, 1.0f) * 255),
			int(std::clamp(color.z(), 0.0f, 1.0f) * 255)
		);
		// Calculate luminance to determine text color contrast
		// Using standard relative luminance formula for sRGB
		float luminance = 0.299f * color.x() + 0.587f * color.y() + 0.114f * color.z();
		QString textColor = (luminance > 0.5f) ? "black" : "white";
		btn->setStyleSheet(QString("background-color: %1; color: %2;").arg(qcolor.name(), textColor));
	};

	setColorButton(_ui->btnAlbedoColor, _material->albedoColor());
	setColorButton(_ui->btnEmissiveColor, _material->emissive());
	setColorButton(_ui->btnSheenColor, _material->sheenColor());
	setColorButton(_ui->btnDiffTransColor, _material->diffuseTransmissionColorFactor());
	setColorButton(_ui->btnSpecularColor, _material->specularColor());
	setColorButton(_ui->btnAttenuationColor, _material->attenuationColor());

	// Combo boxes
	if (_ui->shadingCombo) _ui->shadingCombo->setCurrentIndex(static_cast<int>(_material->shadingModel()));
	if (_ui->blendCombo) _ui->blendCombo->setCurrentIndex(static_cast<int>(_material->blendMode()));

	// Checkboxes
	if (_ui->twoSidedCheck) _ui->twoSidedCheck->setChecked(_material->twoSided());
	if (_ui->wireframeCheck) _ui->wireframeCheck->setChecked(_material->wireframe());

	_updateInProgress = false;
}
void MaterialPropertiesPanel::updateScalarUI()
{
	// This function is called after color changes to update the color button UI
	// Simply call loadScalarValuesFromMaterial to refresh all scalar controls
	loadScalarValuesFromMaterial();
}
void MaterialPropertiesPanel::updateTexturePreview(GLMaterial::TextureType type) {}
void MaterialPropertiesPanel::openPackingDialogFor(GLMaterial::TextureType type)
{
	if (!_material) return;

	// Convert TextureType to string key used by material packing
	auto keyFromType = [](GLMaterial::TextureType t)->QString {
		if (t == GLMaterial::TextureType::Metallic) return "metallic";
		if (t == GLMaterial::TextureType::Roughness) return "roughness";
		if (t == GLMaterial::TextureType::AmbientOcclusion) return "ao";
		if (t == GLMaterial::TextureType::Opacity) return "opacity";
		return QString();
	};

	// Pretty name for the window title
	auto pretty = [](GLMaterial::TextureType t)->QString {
		if (t == GLMaterial::TextureType::Metallic) return tr("Metallic");
		if (t == GLMaterial::TextureType::Roughness) return tr("Roughness");
		if (t == GLMaterial::TextureType::AmbientOcclusion) return tr("Ambient Occlusion");
		if (t == GLMaterial::TextureType::Opacity) return tr("Opacity");
		return tr("Texture");
	};

	QString key = keyFromType(type);
	if (key.isEmpty())
		return;  // Unsupported texture type for packing

	ChannelPackingEditorDialog dlg(this);

	// Show dialog with current (or default) packing
	GLMaterial::ChannelPacking cur{};
	if (_material) cur = _material->packingFor(key);
	dlg.setCurrentPacking(cur, pretty(type));

	if (dlg.exec() == QDialog::Accepted)
	{
		if (_material)
		{
			_material->setPackingFor(key, dlg.packing());
			updatePreview();
			emit materialChanged(_material);
		}
	}
}
void MaterialPropertiesPanel::onColorPickerClicked(GLMaterial::TextureType type) {}
void MaterialPropertiesPanel::onFactorChanged(GLMaterial::TextureType type) {}
void MaterialPropertiesPanel::onTransformButtonClicked(GLMaterial::TextureType type)
{
	if (!_material) return;

	// Get current texture data
	auto tex = _material->texture(type);

	// Check if texture has been loaded
	if (tex.path == "")
	{
		QMessageBox::warning(this, "No Texture",
			"Please load a texture first before editing its transform and sampler parameters.");
		return;
	}

	// Create and show dialog
	TextureParametersDialog dialog(this);

	// Load current values into dialog
	TextureParametersDialog::SamplerSettings samplers{
		tex.wrapS,
		tex.wrapT,
		tex.magFilter,
		tex.minFilter
	};
	dialog.setSamplerSettings(samplers);

	TextureParametersDialog::TextureTransform texTransform{
		tex.texCoordIndex,
		QVector2D(tex.scale.x, tex.scale.y),
		QVector2D(tex.offset.x, tex.offset.y),
		tex.rotation
	};
	dialog.setTransform(texTransform);

	// Show dialog and wait for result
	if (dialog.exec() == QDialog::Accepted)
	{
		// Get new values from dialog
		auto [newTexCoord, newScale, newOffset, newRotation] = dialog.getTransform();
		auto [newWrapS, newWrapT, newMagF, newMinF] = dialog.getSamplerSettings();

		// Update texture struct with new values
		tex.wrapS = newWrapS;
		tex.wrapT = newWrapT;
		tex.magFilter = newMagF;
		tex.minFilter = newMinF;
		tex.scale = glm::vec2(newScale.x(), newScale.y());
		tex.offset = glm::vec2(newOffset.x(), newOffset.y());
		tex.rotation = newRotation;
		tex.texCoordIndex = newTexCoord;

		// Store in material (cascades to unified storage via setTexture)
		_material->setTexture(type, tex);

		// Update preview
		updatePreview();

		// Emit signal for sampler change
		emit textureSamplerChanged(_material, type);
		emit materialChanged(_material);
	}
}

void MaterialPropertiesPanel::updatePreview()
{
	qDebug() << "=== updatePreview CALLED ===";

	if (!_preview)
	{
		qDebug() << "  ERROR: _preview is null!";
		return;
	}
	if (!_material)
	{
		qDebug() << "  ERROR: _material is null!";
		return;
	}

	qDebug() << "  Preview and material valid";

	// Update preview with material
	qDebug() << "  Calling _preview->setMaterial()...";
	_preview->setMaterial(*_material);
	qDebug() << "  Material set on preview";

	// Update preview shape, environment, and exposure from UI controls
	if (_ui->comboShape)
	{
		int shapeIdx = _ui->comboShape->currentIndex();
		qDebug() << "  Setting preview shape:" << shapeIdx;
		_preview->setPreviewShape(static_cast<PreviewShape>(shapeIdx));
	}

	if (_ui->comboEnv)
	{
		int envIdx = _ui->comboEnv->currentIndex();
		qDebug() << "  Setting environment:" << envIdx;
		_preview->setEnvironment(static_cast<EnvMode>(envIdx));
	}

	if (_ui->sliderExposure)
	{
		float exposure = _ui->sliderExposure->value() / 10.0f;
		qDebug() << "  Setting exposure:" << exposure;
		_preview->setExposureEV(exposure);
	}

	if (_ui->comboBoxTexMode)
	{
		int texModeIdx = _ui->comboBoxTexMode->currentIndex();
		qDebug() << "  Setting texture view mode:" << texModeIdx;
		_preview->setTextureViewMode(static_cast<TexViewMode>(texModeIdx));
	}

	// Trigger re-render
	qDebug() << "  Calling _preview->update()...";
	_preview->update();
	qDebug() << "=== updatePreview COMPLETED ===";
}

void MaterialPropertiesPanel::loadMaterialTexturesFromKey(const QString& materialKey)
{
	qDebug() << "=== loadMaterialTexturesFromKey CALLED ===";
	qDebug() << "  Material key:" << materialKey;

	if (!_material)
	{
		qDebug() << "  ERROR: _material is null!";
		return;
	}

	// Load unified materials.json
	QString jsonPath = PathUtils::getDataDirectory() + "/data/catalogs/materials.json";
	qDebug() << "  JSON path:" << jsonPath;
	QFile file(jsonPath);

	if (!file.open(QIODevice::ReadOnly))
	{
		qDebug() << "  ERROR: Cannot open materials.json!";
		return;
	}
	qDebug() << "  JSON file opened successfully";

	QJsonObject catalog = QJsonDocument::fromJson(file.readAll()).object();
	file.close();

	// Find material in groups
	QJsonObject materialObj;
	for (const QJsonValue& groupVal : catalog["groups"].toArray())
	{
		for (const QJsonValue& matVal : groupVal.toObject()["items"].toArray())
		{
			if (matVal.toObject()["key"].toString() == materialKey)
			{
				materialObj = matVal.toObject();
				break;
			}
		}
		if (!materialObj.isEmpty())
			break;
	}

	if (materialObj.isEmpty())
	{
		qDebug() << "  ERROR: Material not found in JSON for key:" << materialKey;
		return;
	}

	qDebug() << "  Material found in JSON";
	QString baseDir = PathUtils::getDataDirectory() + "/";

	// Texture type mapping: JSON key -> texture path key -> GLMaterial::TextureType
	static const QMap<QString, QPair<QString, GLMaterial::TextureType>> textureMap = {
		{"Albedo", {"albedoMapPath", GLMaterial::TextureType::Albedo}},
		{"Normal", {"normalMapPath", GLMaterial::TextureType::Normal}},
		{"Metallic", {"metallicMapPath", GLMaterial::TextureType::Metallic}},
		{"Roughness", {"roughnessMapPath", GLMaterial::TextureType::Roughness}},
		{"Height", {"heightMapPath", GLMaterial::TextureType::Height}},
		{"AmbientOcclusion", {"aoMapPath", GLMaterial::TextureType::AmbientOcclusion}},
		{"Emissive", {"emissiveMapPath", GLMaterial::TextureType::Emissive}},
		{"Opacity", {"opacityMapPath", GLMaterial::TextureType::Opacity}},
		{"Transmission", {"transmissionMapPath", GLMaterial::TextureType::Transmission}},
		{"IOR", {"iorMapPath", GLMaterial::TextureType::IOR}},
		{"SheenColor", {"sheenColorMapPath", GLMaterial::TextureType::SheenColor}},
		{"SheenRoughness", {"sheenRoughnessMapPath", GLMaterial::TextureType::SheenRoughness}},
		{"ClearcoatColor", {"clearcoatColorMapPath", GLMaterial::TextureType::ClearcoatColor}},
		{"ClearcoatRoughness", {"clearcoatRoughnessMapPath", GLMaterial::TextureType::ClearcoatRoughness}},
		{"ClearcoatNormal", {"clearcoatNormalMapPath", GLMaterial::TextureType::ClearcoatNormal}},
		{"SpecularFactor", {"specularFactorMapPath", GLMaterial::TextureType::SpecularFactor}},
		{"SpecularColor", {"specularColorMapPath", GLMaterial::TextureType::SpecularColor}},
		{"Anisotropy", {"anisotropyMapPath", GLMaterial::TextureType::Anisotropy}},
		{"Iridescence", {"iridescenceMapPath", GLMaterial::TextureType::Iridescence}},
		{"IridescenceThickness", {"iridescenceThicknessMapPath", GLMaterial::TextureType::IridescenceThickness}},
		{"DiffuseTransmission", {"diffuseTransmissionMapPath", GLMaterial::TextureType::DiffuseTransmission}},
		{"DiffuseTransmissionColor", {"diffuseTransmissionColorMapPath", GLMaterial::TextureType::DiffuseTransmissionColor}},
		{"Thickness", {"thicknessMapPath", GLMaterial::TextureType::Thickness}},
		{"Diffuse", {"diffuseMapPath", GLMaterial::TextureType::Diffuse}},
		{"SpecularGlossiness", {"specularGlossinessMapPath", GLMaterial::TextureType::SpecularGlossiness}}
	};

	// Load each texture from JSON using OLD API setters (for compatibility with onCustomMaterialApplied)
	qDebug() << "  Loading textures...";
	int texturesLoaded = 0;
	for (auto it = textureMap.constBegin(); it != textureMap.constEnd(); ++it)
	{
		const QString& jsonKey = it.value().first;
		const GLMaterial::TextureType type = it.value().second;

		if (materialObj.contains(jsonKey))
		{
			QString relativePath = materialObj[jsonKey].toString();
			if (!relativePath.isEmpty())
			{
				QString fullPath = baseDir + relativePath;
				if (QFile::exists(fullPath))
				{
					// Set BOTH old and new API to ensure compatibility with all texture-reading code paths:
					// - Old API (setAlbedoMap, etc.): Used by onCustomMaterialApplied() handler
					// - New API (setTexture with TextureType): Used by GLWidget::setTexturesToObjects() for main viewer

					// First, set the OLD API (for onCustomMaterialApplied handler)
					switch (type)
					{
						case GLMaterial::TextureType::Albedo:
							_material->setAlbedoMap(fullPath);
							break;
						case GLMaterial::TextureType::Metallic:
							_material->setMetallicMap(fullPath);
							break;
						case GLMaterial::TextureType::Roughness:
							_material->setRoughnessMap(fullPath);
							break;
						case GLMaterial::TextureType::Normal:
							_material->setNormalMap(fullPath);
							break;
						case GLMaterial::TextureType::AmbientOcclusion:
							_material->setAOMap(fullPath);
							break;
						case GLMaterial::TextureType::Opacity:
							_material->setOpacityMap(fullPath);
							break;
						case GLMaterial::TextureType::Emissive:
							_material->setEmissiveMap(fullPath);
							break;
						case GLMaterial::TextureType::Height:
							_material->setHeightMap(fullPath);
							break;
						case GLMaterial::TextureType::Transmission:
							_material->setTransmissionMap(fullPath);
							break;
						case GLMaterial::TextureType::IOR:
							_material->setIORMap(fullPath);
							break;
						case GLMaterial::TextureType::SheenColor:
							_material->setSheenColorMap(fullPath);
							break;
						case GLMaterial::TextureType::SheenRoughness:
							_material->setSheenRoughnessMap(fullPath);
							break;
						case GLMaterial::TextureType::ClearcoatColor:
							_material->setClearcoatColorMap(fullPath);
							break;
						case GLMaterial::TextureType::ClearcoatRoughness:
							_material->setClearcoatRoughnessMap(fullPath);
							break;
						case GLMaterial::TextureType::ClearcoatNormal:
							_material->setClearcoatNormalMap(fullPath);
							break;
						case GLMaterial::TextureType::Iridescence:
							_material->setIridescenceMap(fullPath);
							break;
						case GLMaterial::TextureType::IridescenceThickness:
							_material->setIridescenceThicknessMap(fullPath);
							break;
						case GLMaterial::TextureType::SpecularFactor:
							_material->setSpecularFactorMap(fullPath);
							break;
						case GLMaterial::TextureType::SpecularColor:
							_material->setSpecularColorMap(fullPath);
							break;
						case GLMaterial::TextureType::Anisotropy:
							_material->setAnisotropyMap(fullPath);
							break;
						case GLMaterial::TextureType::DiffuseTransmission:
							_material->setDiffuseTransmissionMap(fullPath);
							break;
						case GLMaterial::TextureType::DiffuseTransmissionColor:
							_material->setDiffuseTransmissionColorMap(fullPath);
							break;
						case GLMaterial::TextureType::Thickness:
							_material->setThicknessMap(fullPath);
							break;
						case GLMaterial::TextureType::Diffuse:
							_material->setDiffuseMap(fullPath);
							break;
						case GLMaterial::TextureType::SpecularGlossiness:
							_material->setSpecularGlossinessMap(fullPath);
							break;
						default:
							qDebug() << "    WARNING: Unknown texture type:" << static_cast<int>(type);
							continue;
					}

					// Also set the NEW API (for GLWidget::setTexturesToObjects used in main viewer)
					auto tex = _material->texture(type);
					tex.path = fullPath.toStdString();
					_material->setTexture(type, tex);
					qDebug() << "    Loaded texture:" << jsonKey << "path:" << fullPath;
					texturesLoaded++;
				}
				else
				{
					qDebug() << "    Texture file not found:" << fullPath;
				}
			}
		}
	}
	qDebug() << "  Textures loaded:" << texturesLoaded;
	qDebug() << "=== loadMaterialTexturesFromKey COMPLETED ===";
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

void MaterialPropertiesPanel::onMaterialPresetSelected(const GLMaterial& mat)
{
	qDebug() << "=== onMaterialPresetSelected CALLED ===";
	qDebug() << "  Material received from tree widget";

	// Material is already populated with scalar properties from tree widget
	// Now load textures and bind the material
	if (!_ui)
	{
		qDebug() << "  ERROR: _ui is null!";
		return;
	}

	// Create a copy of the material to work with
	if (!_material) _material = new GLMaterial();
	*_material = mat;
	qDebug() << "  Material copied to _material";

	// Get the material key from the selected tree item to load textures
	MaterialLibraryWidget* libraryWidget = qobject_cast<MaterialLibraryWidget*>(_ui->treeWidget);
	if (libraryWidget)
	{
		QList<QTreeWidgetItem*> selected = libraryWidget->selectedItems();
		qDebug() << "  Selected items count:" << selected.size();
		if (!selected.isEmpty())
		{
			QString materialKey = selected.first()->data(0, Qt::UserRole).toString();
			qDebug() << "  Material key:" << materialKey;
			if (!materialKey.isEmpty())
			{
				// Load textures from materials.json by material key
				qDebug() << "  Loading textures from JSON for key:" << materialKey;
				loadMaterialTexturesFromKey(materialKey);
				qDebug() << "  Textures loaded";
			}
			else
			{
				qDebug() << "  WARNING: materialKey is empty";
			}
		}
		else
		{
			qDebug() << "  WARNING: No tree items selected";
		}
	}
	else
	{
		qDebug() << "  WARNING: Tree widget cast failed or doesn't exist";
	}

	// Bind material and update UI
	qDebug() << "  Calling bindMaterial()...";
	bindMaterial(_material);
	qDebug() << "  bindMaterial() completed";

	// NOTE: Do NOT emit materialApplied here!
	// materialApplied should only be emitted when user clicks Apply button
	// Selecting from tree just loads the material for editing/preview
	qDebug() << "=== onMaterialPresetSelected COMPLETED ===";
}

void MaterialPropertiesPanel::onMaterialDoubleClicked(const GLMaterial& mat)
{
	qDebug() << "=== onMaterialDoubleClicked CALLED (APPLY TO MESH) ===";
	qDebug() << "  Received material scalar values:";
	qDebug() << "    Metalness:" << mat.metalness();
	qDebug() << "    Roughness:" << mat.roughness();
	qDebug() << "    IOR:" << mat.ior();
	qDebug() << "    Transmission:" << mat.transmission();

	// Load and bind material for preview (same as single-click)
	if (!_material) _material = new GLMaterial();
	*_material = mat;
	qDebug() << "  After copying to _material:";
	qDebug() << "    Metalness:" << _material->metalness();
	qDebug() << "    Roughness:" << _material->roughness();

	// Get material key from selected tree item to load textures
	MaterialLibraryWidget* libraryWidget = qobject_cast<MaterialLibraryWidget*>(_ui->treeWidget);
	if (libraryWidget)
	{
		QList<QTreeWidgetItem*> selected = libraryWidget->selectedItems();
		qDebug() << "  Selected items count:" << selected.size();
		if (!selected.isEmpty())
		{
			QString materialKey = selected.first()->data(0, Qt::UserRole).toString();
			qDebug() << "  Material key:" << materialKey;
			if (!materialKey.isEmpty())
			{
				// Load textures from materials.json by material key
				qDebug() << "  Loading textures from JSON for double-click apply...";
				loadMaterialTexturesFromKey(materialKey);
				qDebug() << "  Textures loaded";
			}
		}
	}

	// Bind material and update UI
	qDebug() << "  Calling bindMaterial()...";
	bindMaterial(_material);
	qDebug() << "  bindMaterial() completed";

	// EMIT SIGNAL TO APPLY TO MESH (this is the key difference from single-click)
	qDebug() << "  Emitting materialApplied signal to apply to selected mesh...";
	qDebug() << "  Material about to be emitted has:";
	qDebug() << "    Metalness:" << _material->metalness();
	qDebug() << "    Roughness:" << _material->roughness();
	qDebug() << "    IOR:" << _material->ior();
	qDebug() << "    Transmission:" << _material->transmission();
	emit materialApplied(*_material);

	qDebug() << "=== onMaterialDoubleClicked COMPLETED ===";
}

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

void MaterialPropertiesPanel::onSearchTextChanged(const QString& text)
{
	// Get the tree widget
	QTreeWidget* tw = qobject_cast<QTreeWidget*>(_ui->treeWidget);
	if (!tw) return;

	const QString needle = text.trimmed().toLower();
	const bool hasFilter = !needle.isEmpty();
	QStringList tokens;
	if (hasFilter) tokens = needle.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
	int matchCount = 0;

	// Recursively walk and update each item
	std::function<bool(QTreeWidgetItem*, bool)> matchAndShow = [&](QTreeWidgetItem* item, bool parentMatched) -> bool {
		const QString label = item->text(0).toLower();

		// Only consider self-match when a filter is active
		bool selfMatched = false;
		if (hasFilter)
		{
			selfMatched = true;
			for (const QString& t : tokens)
			{
				if (!label.contains(t)) { selfMatched = false; break; }
			}
		}

		// Always recurse children so we can reset their fonts/foreground when clearing
		// Pass down whether this item or any ancestor matched
		bool anyChildMatched = false;
		bool shouldShowChildren = parentMatched || selfMatched;
		for (int i = 0; i < item->childCount(); ++i)
		{
			if (matchAndShow(item->child(i), shouldShowChildren)) anyChildMatched = true;
		}

		// Item should be shown if:
		// 1. No filter is active (show all), OR
		// 2. This item matched the search, OR
		// 3. Any child matched the search, OR
		// 4. A parent/ancestor matched the search
		const bool matched = hasFilter ? (selfMatched || anyChildMatched || parentMatched) : true;
		item->setHidden(hasFilter ? !matched : false);

		// Expand only when filtering and the item matched
		if (hasFilter && matched) item->setExpanded(true);

		// Foreground color: use subtle blue when self matched; reset otherwise
		if (hasFilter && selfMatched)
		{
			item->setForeground(0, QBrush(QColor(0x1e88e5)));
		}
		else
		{
			item->setForeground(0, QBrush());  // reset to default
		}

		if (matched) ++matchCount;
		return matched;
	};

	// Start recursion from top-level items with parentMatched = false
	for (int i = 0; i < tw->topLevelItemCount(); ++i)
		matchAndShow(tw->topLevelItem(i), false);

	// Red border when no matches and query non-empty; reset otherwise
	if (!hasFilter)
	{
		_ui->searchEdit->setStyleSheet("");
		_ui->searchEdit->setToolTip("");
	}
	else if (matchCount == 0)
	{
		_ui->searchEdit->setStyleSheet("QLineEdit { border: 1px solid #e53935; }");
		_ui->searchEdit->setToolTip("No matches");
	}
	else
	{
		_ui->searchEdit->setStyleSheet("");
		_ui->searchEdit->setToolTip("");
	}
}
