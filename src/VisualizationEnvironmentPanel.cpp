#include "VisualizationEnvironmentPanel.h"
#include "ui_VisualizationEnvironmentPanel.h"
#include "GLWidget.h"
#include "LanguageManager.h"
#include "ModelViewer.h"
#include "MaterialPreviewWidget.h"
#include "PathUtils.h"
#include "SceneGraph.h"
#include <QColorDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QImage>
#include <QDir>
#include <QDebug>
#include <QMouseEvent>
#include <QTreeWidget>

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

	// Embed the enum value in each combo item's UserRole so the handler
	// never depends on positional index — reordering the UI is always safe.
	{
		auto* combo = ui->comboBoxHDRToneMappingMode;
		combo->setItemData(0, static_cast<int>(HDRToneMapMode::KhronosPbrNeutral));
		combo->setItemData(1, static_cast<int>(HDRToneMapMode::ACES_Narkowicz));
		combo->setItemData(2, static_cast<int>(HDRToneMapMode::ACES_Hill));
		combo->setItemData(3, static_cast<int>(HDRToneMapMode::AECS_Hill_Exposure_Boost));
		combo->setItemData(4, static_cast<int>(HDRToneMapMode::Uncharted2ToneMapping));
		combo->setItemData(5, static_cast<int>(HDRToneMapMode::Reinhard));
	}

	connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this, [this]() {
		ui->retranslateUi(this);
		});

	setAttribute(Qt::WA_DeleteOnClose);
	ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		
	QTimer::singleShot(0, this, &VisualizationEnvironmentPanel::onLoadSkyBoxPresetMaps);
}

VisualizationEnvironmentPanel::~VisualizationEnvironmentPanel() = default;

bool VisualizationEnvironmentPanel::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == _lightTreeResizeHandle)
	{
		auto* me = static_cast<QMouseEvent*>(event);
		switch (event->type())
		{
		case QEvent::MouseButtonPress:
			if (me->button() == Qt::LeftButton)
			{
				_lightTreeDragStartY = me->globalPosition().y();
				_lightTreeDragStartH = ui->treePunctualLights->height();
				return true;
			}
			break;

		case QEvent::MouseMove:
			if (me->buttons() & Qt::LeftButton)
			{
				constexpr int kMinH = 48;
				const int delta = static_cast<int>(me->globalPosition().y() - _lightTreeDragStartY);
				const int newH  = qMax(kMinH, _lightTreeDragStartH + delta);
				// setFixedHeight (sets both min and max) forces the layout to
				// allocate exactly newH pixels — setMaximumHeight alone is
				// insufficient when the tree is already at its natural size.
				ui->treePunctualLights->setFixedHeight(newH);
				ui->groupBoxPunctualLights->updateGeometry();
				return true;
			}
			break;

		case QEvent::MouseButtonRelease:
			return true;

		default:
			break;
		}
	}
	return QWidget::eventFilter(watched, event);
}

void VisualizationEnvironmentPanel::initialize(ModelViewer* modelViewer, GLWidget* glWidget)
{
	if (_isInitialized)
		return;

	_modelViewer = modelViewer;
	_glWidget    = glWidget;
	_sceneGraph  = _modelViewer ? _modelViewer->sceneGraph() : nullptr;

	// Load state from ModelViewer
	if (_modelViewer)
	{
		_skyBoxLDRIIndex = _modelViewer->getSkyBoxLDRIIndex();
		_skyBoxHDRIIndex = _modelViewer->getSkyBoxHDRIIndex();
	}

	connectSignalsAndSlots();

	// Refresh punctual lights tree whenever SceneGraph light data changes
	// (model loaded/unloaded, or individual light toggled from elsewhere).
	if (_sceneGraph)
	{
		connect(_sceneGraph, &SceneGraph::lightDataChanged,
		        this, &VisualizationEnvironmentPanel::refreshPunctualLightsTree);
	}
	updateControlDependencies();

	// Keep GLWidget in sync with the UI defaults on first startup as well.
	// Without this, the viewer could retain an internal floor offset that
	// differs from the spin box value until the user touches the control.
	if (_glWidget && ui)
	{
		_glWidget->setGroundMode(ui->radioButtonGroundFloor->isChecked()
			? GroundMode::Floor
			: (ui->radioButtonGroundGrid->isChecked() ? GroundMode::Grid : GroundMode::None));
		_glWidget->setFloorOffsetPercent(ui->doubleSpinBoxFloorOffset->value());
	}

	// ── Punctual-lights tree resize handle ─────────────────────────────────
	// Give the tree a compact default height (shows ~4–5 items); users drag
	// the handle strip below it to reveal more.  The tree's own scroll bar
	// handles any overflow.
	{
		// Fix the tree at a compact default height; the user can drag the handle
		// down to expand.  setFixedHeight is used so the layout immediately
		// honours the size regardless of content count.
		constexpr int kDefaultTreeH = 110;
		ui->treePunctualLights->setFixedHeight(kDefaultTreeH);

		// Thin drag-handle strip inserted below the tree in the group box layout.
		auto* handle = new QFrame(ui->groupBoxPunctualLights);
		handle->setObjectName("treeLightResizeHandle");
		handle->setFrameShape(QFrame::HLine);
		handle->setFrameShadow(QFrame::Sunken);
		handle->setCursor(Qt::SizeVerCursor);
		handle->setFixedHeight(6);
		handle->setToolTip(tr("Drag to resize the lights list"));

		auto* gl = qobject_cast<QGridLayout*>(ui->groupBoxPunctualLights->layout());
		if (gl)
			gl->addWidget(handle, 1, 0);

		handle->installEventFilter(this);
		_lightTreeResizeHandle = handle;
	}
	// ────────────────────────────────────────────────────────────────────────

	_isInitialized = true;
}

void VisualizationEnvironmentPanel::connectSignalsAndSlots()
{
	if (!ui)
		return;

	// ===== Light Color Buttons =====
	connect(ui->pushButtonLightColor, &QPushButton::clicked, this, &VisualizationEnvironmentPanel::onLightColorClicked);
	connect(ui->pushButtonDefaultLights, &QPushButton::clicked, this, &VisualizationEnvironmentPanel::onDefaultLightsClicked);

	// ===== Light Position Sliders =====
	connect(ui->sliderLightPosX, QOverload<int>::of(&QSlider::valueChanged), this, &VisualizationEnvironmentPanel::onLightPosXChanged);
	connect(ui->sliderLightPosY, QOverload<int>::of(&QSlider::valueChanged), this, &VisualizationEnvironmentPanel::onLightPosYChanged);
	connect(ui->sliderLightPosZ, QOverload<int>::of(&QSlider::valueChanged), this, &VisualizationEnvironmentPanel::onLightPosZChanged);

	// ===== Lighting Checkboxes =====
	connect(ui->checkBoxDefaultLights, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onDefaultLightsChanged);
	connect(ui->checkBoxShowLights, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onShowLightsChanged);
	connect(ui->checkBoxIBL, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onIBLChanged);

	// ===== Punctual Lights Tree =====
	connect(ui->treePunctualLights, &QTreeWidget::itemChanged,
	        this, &VisualizationEnvironmentPanel::onPunctualLightItemChanged);

	// Group box hidden until a model with lights is loaded
	ui->groupBoxPunctualLights->setVisible(false);

	// ===== Skybox Controls =====
	connect(_glWidget, &GLWidget::cameraUpAxisChanged,
	        this, &VisualizationEnvironmentPanel::updateSkyBoxRotationLabels);
	updateSkyBoxRotationLabels(_glWidget->isCameraUpAxisZUp());
	connect(ui->checkBoxSkyBox, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onSkyBoxStateChanged);
	connect(ui->checkBoxSkyBoxHDRI, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onSkyBoxHDRIChanged);
	connect(ui->checkBoxSkyBoxHDRI, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onLoadSkyBoxPresetMaps);
	connect(ui->sliderSkyBoxBlur, QOverload<int>::of(&QSlider::valueChanged), this, &VisualizationEnvironmentPanel::onSkyBoxBlurChanged);
	connect(ui->doubleSpinBoxSkyBoxFOV, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VisualizationEnvironmentPanel::onSkyBoxFOVChanged);
	connect(ui->comboBoxSkyBoxRotation, QOverload<int>::of(&QComboBox::currentIndexChanged), _glWidget, &GLWidget::setSkyBoxZRotation);
	connect(ui->comboBoxSkyBoxMaps, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VisualizationEnvironmentPanel::onSkyBoxMapsChanged);
	connect(ui->pushButtonSkyBoxTex, &QPushButton::clicked, this, &VisualizationEnvironmentPanel::onSkyBoxTextureClicked);

	// ===== Shadow Controls =====
	connect(ui->checkBoxShadowMapping, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onShadowMappingStateChanged);
	connect(ui->checkBoxSelfShadows, &QCheckBox::toggled, this, &VisualizationEnvironmentPanel::onSelfShadowsChanged);
	connect(ui->comboBoxShadowQuality, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VisualizationEnvironmentPanel::onShadowQualityChanged);

	// ===== Floor Controls =====
	connect(ui->radioButtonGroundNone, &QRadioButton::toggled, this, &VisualizationEnvironmentPanel::onGroundModeChanged);
	connect(ui->radioButtonGroundFloor, &QRadioButton::toggled, this, &VisualizationEnvironmentPanel::onGroundModeChanged);
	connect(ui->radioButtonGroundGrid, &QRadioButton::toggled, this, &VisualizationEnvironmentPanel::onGroundModeChanged);
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
	bool floorEnabled = ui->radioButtonGroundFloor->isChecked();
	bool gridEnabled = ui->radioButtonGroundGrid->isChecked();
	bool groundEnabled = floorEnabled || gridEnabled;
	bool shadowsEnabled = ui->checkBoxShadowMapping->isChecked();
	bool hdrEnabled = ui->checkBoxHDRToneMapping->isChecked();
	bool gammaEnabled = ui->checkBoxGammaCorrection->isChecked();
	bool skyBoxHDRIEnabled = skyBoxEnabled && ui->checkBoxSkyBoxHDRI->isChecked();
	bool floorTextureEnabled = floorEnabled && ui->checkBoxFloorTexture->isChecked();

	// Skybox dependencies	
	ui->labelSkyBoxBlur->setEnabled(skyBoxEnabled);
	ui->sliderSkyBoxBlur->setEnabled(skyBoxEnabled);
	ui->labelSkyBoxBlurValue->setEnabled(skyBoxEnabled);
	ui->labelFOV->setEnabled(skyBoxEnabled);
	ui->doubleSpinBoxSkyBoxFOV->setEnabled(skyBoxEnabled);
	ui->labelSkyBoxRotation->setEnabled(skyBoxEnabled);
	ui->comboBoxSkyBoxRotation->setEnabled(skyBoxEnabled);
	
	// Floor dependencies
	ui->checkBoxReflections->setEnabled(floorEnabled);
	ui->checkBoxFloorTexture->setEnabled(floorEnabled);
	ui->labelFloorOffset->setEnabled(groundEnabled);
	ui->doubleSpinBoxFloorOffset->setEnabled(groundEnabled);
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

	QVector4D lightColor = _glWidget->getDefaultLightColor();
	QColor diffuseColor = QColor::fromRgbF(lightColor.x(), lightColor.y(), lightColor.z());
	QString diffuseStyle = QString("background-color: %1; color: %2; border: 1px solid gray;")
		.arg(diffuseColor.name(), diffuseColor.lightness() < 75 ? QColor(Qt::white).name() : QColor(Qt::black).name());
	ui->pushButtonLightColor->setStyleSheet(diffuseStyle);
}

// ==================== LIGHT COLOR BUTTONS ====================

void VisualizationEnvironmentPanel::onLightColorClicked()
{
	if (!_glWidget || !ui)
		return;

	QVector4D lightColor = _glWidget->getDefaultLightColor();
	QColor c = QColorDialog::getColor(QColor::fromRgbF(lightColor.x(), lightColor.y(), lightColor.z(), lightColor.w()), this, "Default Light Color");
	if (c.isValid())
	{
		_glWidget->setDefaultLightColor(QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
		updateButtonStyles();
		_glWidget->updateView();
	}
}

void VisualizationEnvironmentPanel::onDefaultLightsClicked()
{
	if (!_glWidget || !ui)
		return;

	_glWidget->setDefaultLightColor(QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

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
	ui->checkBoxIBL->blockSignals(true);
	ui->checkBoxShowLights->blockSignals(true);

	ui->checkBoxDefaultLights->setChecked(true);
	ui->checkBoxIBL->setChecked(true);
	ui->checkBoxShowLights->setChecked(false);

	ui->checkBoxDefaultLights->blockSignals(false);
	ui->checkBoxIBL->blockSignals(false);
	ui->checkBoxShowLights->blockSignals(false);

	// Manually trigger GLWidget calls
	_glWidget->useDefaultLights(true);
	_glWidget->usePunctualLights(true);  // kept true; tree checkboxes control per-light enable
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

// ---------------------------------------------------------------------------
// Punctual Lights tree
// ---------------------------------------------------------------------------

static const int kLightIndexRole    = Qt::UserRole;       // int  — light index in file (-1 = file item)
static const int kSourceFileRole    = Qt::UserRole + 1;   // QString — absolute source file path

QTreeWidgetItem* VisualizationEnvironmentPanel::makeLightFileItem(const QString& sourceFile) const
{
	QTreeWidgetItem* item = new QTreeWidgetItem();
	item->setText(0, QFileInfo(sourceFile).fileName());
	item->setData(0, kSourceFileRole, sourceFile);
	item->setData(0, kLightIndexRole, -1);
	// Tri-state: Qt will manage checked/partial/unchecked automatically
	// once children are added with individual check states.
	// Qt::ItemIsUserTristate is intentionally omitted.  With it, clicking a
	// Checked parent would first land on PartiallyChecked (a no-op for the
	// handler), requiring a second click to reach Unchecked.  Without it Qt
	// only toggles Checked ↔ Unchecked on user interaction; PartiallyChecked
	// is still set programmatically by the leaf handler when children diverge.
	item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
	QFont f = item->font(0);
	f.setBold(true);
	item->setFont(0, f);
	// Not selectable — clicking the row only toggles the checkbox
	item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
	return item;
}

QTreeWidgetItem* VisualizationEnvironmentPanel::makeLightLeafItem(const GltfLightEntry& entry,
                                                                   int lightIndex) const
{
	// Pick a type label for the tooltip
	const int t = entry.gpuLight.type;
	QString typeStr = (t == 0) ? QStringLiteral("Directional")
	               : (t == 1) ? QStringLiteral("Point")
	               :             QStringLiteral("Spot");

	QString displayName = entry.name.isEmpty()
	                      ? QString("%1 %2").arg(typeStr).arg(lightIndex + 1)
	                      : entry.name;

	QTreeWidgetItem* item = new QTreeWidgetItem();
	item->setText(0, displayName);
	item->setToolTip(0, typeStr);
	item->setData(0, kLightIndexRole, lightIndex);
	item->setCheckState(0, entry.enabled ? Qt::Checked : Qt::Unchecked);
	item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
	item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
	return item;
}

void VisualizationEnvironmentPanel::refreshPunctualLightsTree()
{
	if (!_sceneGraph || !ui)
		return;

	QTreeWidget* tree = ui->treePunctualLights;

	// Block itemChanged while rebuilding so we don't trigger onPunctualLightItemChanged
	tree->blockSignals(true);
	tree->clear();

	const QStringList files = _sceneGraph->filesWithLights();
	for (const QString& sourceFile : files)
	{
		const GltfLightData ld = _sceneGraph->lightDataForFile(sourceFile);
		if (ld.isEmpty())
			continue;

		QTreeWidgetItem* fileItem = makeLightFileItem(sourceFile);
		tree->addTopLevelItem(fileItem);

		for (int i = 0; i < ld.lights.size(); ++i)
		{
			QTreeWidgetItem* leaf = makeLightLeafItem(ld.lights[i], i);
			leaf->setData(0, kSourceFileRole, sourceFile);
			fileItem->addChild(leaf);
		}

		fileItem->setExpanded(true);

		// Set parent tri-state from children
		bool anyOn  = false, anyOff = false;
		for (int i = 0; i < ld.lights.size(); ++i)
		{
			(ld.lights[i].enabled ? anyOn : anyOff) = true;
		}
		fileItem->setCheckState(0, anyOn && anyOff ? Qt::PartiallyChecked
		                         : anyOn           ? Qt::Checked
		                                           : Qt::Unchecked);
	}

	tree->blockSignals(false);

	// Show the group box only when at least one file has lights
	ui->groupBoxPunctualLights->setVisible(!files.isEmpty());
}

void VisualizationEnvironmentPanel::onPunctualLightItemChanged(QTreeWidgetItem* item, int column)
{
	if (!_sceneGraph || !_glWidget || !item || column != 0)
		return;

	const int        lightIndex = item->data(0, kLightIndexRole).toInt();
	const QString    sourceFile = item->data(0, kSourceFileRole).toString();
	const Qt::CheckState state  = item->checkState(0);

	// Block lightDataChanged from firing while we're inside the itemChanged
	// handler.  setLightEnabled() would emit lightDataChanged() →
	// refreshPunctualLightsTree() → tree->clear(), which deletes the 'item'
	// pointer we're still using — instant crash on rapid toggling.
	// The tree UI is managed manually below; we trigger the GPU upload ourselves.
	QSignalBlocker sceneGraphBlocker(_sceneGraph);

	if (lightIndex == -1)
	{
		// File-level item toggled — apply same state to all children.
		ui->treePunctualLights->blockSignals(true);
		const bool enable = (state != Qt::Unchecked);
		for (int i = 0; i < item->childCount(); ++i)
		{
			item->child(i)->setCheckState(0, enable ? Qt::Checked : Qt::Unchecked);
			_sceneGraph->setLightEnabled(sourceFile, i, enable);
		}
		ui->treePunctualLights->blockSignals(false);
	}
	else
	{
		// Leaf item toggled — update SceneGraph for this one light.
		_sceneGraph->setLightEnabled(sourceFile, lightIndex, state == Qt::Checked);

		// Update parent tri-state without re-entering this slot.
		if (QTreeWidgetItem* parent = item->parent())
		{
			ui->treePunctualLights->blockSignals(true);
			bool anyOn = false, anyOff = false;
			for (int i = 0; i < parent->childCount(); ++i)
				(parent->child(i)->checkState(0) == Qt::Checked ? anyOn : anyOff) = true;
			parent->setCheckState(0, anyOn && anyOff ? Qt::PartiallyChecked
			                       : anyOn           ? Qt::Checked
			                                         : Qt::Unchecked);
			ui->treePunctualLights->blockSignals(false);
		}
	}

	// sceneGraphBlocker goes out of scope here — signals re-enabled on _sceneGraph.
	// Rebuild the GPU light list and upload directly without going through the
	// lightDataChanged → refreshPunctualLightsTree path.
	_glWidget->applyEnabledLightList(_sceneGraph->buildEnabledLightList());
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

void VisualizationEnvironmentPanel::updateSkyBoxRotationLabels(bool zUp)
{
	// Items 0/1 (X+/X-) are the same in both conventions.
	// Items 2/3 name the second horizontal axis: Y in Z-up, Z in Y-up.
	ui->comboBoxSkyBoxRotation->setItemText(2, zUp ? tr("Y+") : tr("Z+"));
	ui->comboBoxSkyBoxRotation->setItemText(3, zUp ? tr("Y-") : tr("Z-"));
}

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

	// Update preview widget with new environment state
	if (_previewWidget)
		_previewWidget->update();
}

void VisualizationEnvironmentPanel::onSkyBoxHDRIChanged(bool checked)
{
	if (!_glWidget)
		return;

	_glWidget->setSkyBoxTextureHDRI(checked);

	// Update preview widget when environment type changes
	if (_previewWidget)
		_previewWidget->update();
}

void VisualizationEnvironmentPanel::onSkyBoxBlurChanged(int value)
{
	if (ui)
		ui->labelSkyBoxBlurValue->setText(QString("%1%").arg(value));

	if (!_glWidget)
		return;

	_glWidget->setSkyBoxBlurPercent(value);
	_glWidget->updateView();

	// Update preview widget
	if (_previewWidget)
		_previewWidget->update();
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

	// Store current index based on HDRI/LDRI selection
	if (ui->checkBoxSkyBoxHDRI->isChecked())
		_skyBoxHDRIIndex = std::max(0, index);
	else
		_skyBoxLDRIIndex = std::max(0, index);

	QString selectedPath = ui->comboBoxSkyBoxMaps->itemData(index).toString();
	if (!selectedPath.isEmpty())
	{
		_glWidget->setSkyBoxTextureFolder(selectedPath);
		_glWidget->updateView();

		// Update preview widget with new environment map
		if (_previewWidget)
			_previewWidget->update();
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

void VisualizationEnvironmentPanel::onGroundModeChanged()
{
	if (!_glWidget || !ui)
		return;

	GroundMode mode = GroundMode::None;
	if (ui->radioButtonGroundFloor->isChecked())
		mode = GroundMode::Floor;
	else if (ui->radioButtonGroundGrid->isChecked())
		mode = GroundMode::Grid;

	_glWidget->setGroundMode(mode);
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
	if (_previewWidget) _previewWidget->update();
}

void VisualizationEnvironmentPanel::onHDRToneMappingModeChanged(int index)
{
	if (!_glWidget)
		return;

	// Each item carries its HDRToneMapMode enum value in UserRole (set in constructor).
	const int enumVal = ui->comboBoxHDRToneMappingMode->itemData(index).toInt();
	_glWidget->setHDRToneMappingMode(static_cast<HDRToneMapMode>(enumVal));
	_glWidget->updateView();
	if (_previewWidget) _previewWidget->update();
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
	if (_previewWidget) _previewWidget->update();
}

void VisualizationEnvironmentPanel::onScreenGammaChanged(double value)
{
	if (!_glWidget)
		return;

	_glWidget->setScreenGamma(value);
	_glWidget->updateView();
	if (_previewWidget) _previewWidget->update();
}

// ==================== DEFAULT VALUES BUTTON ====================

void VisualizationEnvironmentPanel::onDefaultEnvValuesClicked()
{
	if (!ui || !_glWidget)
		return;

	ui->doubleSpinBoxSkyBoxFOV->setValue(45.0);
	ui->sliderSkyBoxBlur->setValue(0);
	ui->comboBoxSkyBoxRotation->setCurrentIndex(0);
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
	ui->radioButtonGroundFloor->setChecked(realShaded);
	ui->radioButtonGroundNone->setChecked(!realShaded);
	ui->checkBoxSkyBoxHDRI->setChecked(ui->checkBoxSkyBoxHDRI->isChecked() || (realShaded && pbrLighting));

	bool skyBoxHDRIChecked = ui->checkBoxSkyBoxHDRI->isChecked();
	ui->checkBoxHDRToneMapping->setChecked(skyBoxHDRIChecked && pbrLighting);
	ui->checkBoxGammaCorrection->setChecked(skyBoxHDRIChecked && pbrLighting);

	blockSignals(false);

	updateControlDependencies();
	_glWidget->setSkyBoxTextureHDRI(skyBoxHDRIChecked);
}

// ==================== PBR LIGHTING MODE ====================

void VisualizationEnvironmentPanel::setPBRLightingMode(bool enable)
{
	if (!_glWidget || !ui)
		return;

	// If disabling PBR (switching to ADS), disable PBR-specific settings
	if (!enable)
	{
		// Disable environment mapping
		if (ui->checkBoxEnvMapping)
		{
			ui->checkBoxEnvMapping->blockSignals(true);
			ui->checkBoxEnvMapping->setChecked(false);
			ui->checkBoxEnvMapping->blockSignals(false);
		}
		_glWidget->showEnvironment(false);

		// Disable tone mapping and gamma correction (PBR-specific)
		ui->checkBoxHDRToneMapping->blockSignals(true);
		ui->checkBoxGammaCorrection->blockSignals(true);

		ui->checkBoxHDRToneMapping->setChecked(false);
		ui->checkBoxGammaCorrection->setChecked(false);

		ui->checkBoxHDRToneMapping->blockSignals(false);
		ui->checkBoxGammaCorrection->blockSignals(false);

		_glWidget->enableHDRToneMapping(false);
		_glWidget->enableGammaCorrection(false);
		return;
	}

	// Block signals during state changes
	ui->checkBoxSkyBoxHDRI->blockSignals(true);
	ui->checkBoxHDRToneMapping->blockSignals(true);
	ui->checkBoxGammaCorrection->blockSignals(true);

	ui->checkBoxSkyBoxHDRI->setChecked(true);
	ui->checkBoxHDRToneMapping->setChecked(enable);
	ui->checkBoxGammaCorrection->setChecked(enable);

	// Enable environment mapping when enabling PBR so preview uses bright HDRI
	if (ui->checkBoxEnvMapping)
	{
		ui->checkBoxEnvMapping->blockSignals(true);
		ui->checkBoxEnvMapping->setChecked(true);
		ui->checkBoxEnvMapping->blockSignals(false);
	}

	ui->checkBoxSkyBoxHDRI->blockSignals(false);
	ui->checkBoxHDRToneMapping->blockSignals(false);
	ui->checkBoxGammaCorrection->blockSignals(false);

	// Trigger updates
	onSkyBoxHDRIChanged(true);
	reloadSkyBoxPresets();
	_glWidget->enableHDRToneMapping(enable);
	_glWidget->enableGammaCorrection(enable);

	// Explicitly enable environment mapping
	_glWidget->showEnvironment(true);

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
	ui->toolButtonDetach->setToolTip(tr("Detach from panel"));
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

// ==================== LIGHT OFFSET RESTORE ====================

void VisualizationEnvironmentPanel::restoreDefaultLightOffset(const QVector3D& offset)
{
	if (!ui || !_glWidget)
		return;

	// Clamp to the current slider range so a stale saved value doesn't
	// produce a slider stuck against the wall after the range was re-scaled.
	const int x = qBound(ui->sliderLightPosX->minimum(),
	                      static_cast<int>(offset.x()),
	                      ui->sliderLightPosX->maximum());
	const int y = qBound(ui->sliderLightPosY->minimum(),
	                      static_cast<int>(offset.y()),
	                      ui->sliderLightPosY->maximum());
	const int z = qBound(ui->sliderLightPosZ->minimum(),
	                      static_cast<int>(offset.z()),
	                      ui->sliderLightPosZ->maximum());

	ui->sliderLightPosX->blockSignals(true);
	ui->sliderLightPosY->blockSignals(true);
	ui->sliderLightPosZ->blockSignals(true);
	ui->sliderLightPosX->setValue(x);
	ui->sliderLightPosY->setValue(y);
	ui->sliderLightPosZ->setValue(z);
	ui->sliderLightPosX->blockSignals(false);
	ui->sliderLightPosY->blockSignals(false);
	ui->sliderLightPosZ->blockSignals(false);

	_glWidget->setLightOffset(QVector3D(static_cast<float>(x),
	                                    static_cast<float>(y),
	                                    static_cast<float>(z)));
}
