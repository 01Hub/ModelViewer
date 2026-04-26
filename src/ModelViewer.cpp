#include "ADSMaterialSettingsPanel.h"
#include "FloatingPanelDialog.h"
#include "AssImpModelLoader.h"
#include "ApplyPBRTexturesCommand.h"
#include "ApplyADSColorsCommand.h"
#include "ApplyADSTexturesCommand.h"
#include "DeleteMeshCommand.h"
#include "RenameMeshCommand.h"
#include "DuplicateCommand.h"
#include "GLWidget.h"
#include "LanguageManager.h"
#include "MainWindow.h"
#include "MaterialPreviewWidget.h"
#include "MeshProperties.h"
#include "ModelViewer.h"
#include "ModelViewerApplication.h"
#include "MvfDocument.h"
#include "MvfFormat.h"
#include "MvfSceneBuilder.h"
#include "SceneTreeWidget.h"
#include "ObjectTransformPanel.h"
#include "MaterialPropertiesPanel.h"
#include "MaterialLibraryWidget.h"
#include "PathUtils.h"
#include "SelectionCommand.h"
#include "TextureMappingPanel.h"
#include "TransformCommand.h"
#include "TriangleMesh.h"
#include "VisibilityCommand.h"
#include <assimp/Importer.hpp>
#include <QApplication>
#include <QColorDialog>
#include <QDataStream>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QThread>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QScrollArea>

QString ModelViewer::_lastOpenedDir;
QString ModelViewer::_lastSelectedFilter;

ModelViewer::ModelViewer(QWidget* parent) : QWidget(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);

	_documentSaved = false;
	_documentModified = false;
	_runningFirstTime = true;

	_textureDirOpenedFirstTime = true;

	setupUi(this);

	// Configure main splitter to constrain right panel to 450px on startup
	// Tree view gets the rest of the space
	QTimer::singleShot(0, this, [this]() {
		if (splitter && splitter->count() >= 2)
		{
			int totalWidth = splitter->width();
			int rightWidth = 480;  // Right panel fixed width
			int leftWidth = totalWidth - rightWidth;
			if (leftWidth < 300) leftWidth = 300;  // Minimum for left side
			splitter->setSizes({leftWidth, rightWidth});
		}
	});

	// Scene graph — owns the node hierarchy mirroring the loaded aiScene tree.
	_sceneGraph = new SceneGraph(this);

	// Initialize undo stack
	_undoStack = new QUndoStack(this);
	QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
	int maxUndo = settings.value("spinBoxUndoLimit", 50).toInt(); // Keep last 50 operations as default
	_undoStack->setUndoLimit(maxUndo);

	// Detect when undo becomes unavailable
	connect(_undoStack, &QUndoStack::canUndoChanged,
		this, [this](bool canUndo) {
			if (_lastCanUndo && !canUndo)  // Transition: true -> false
			{
				MainWindow::showStatusMessage("Nothing to undo", 2000);
			}
			_lastCanUndo = canUndo;
		});

	// Detect when redo becomes unavailable  
	connect(_undoStack, &QUndoStack::canRedoChanged,
		this, [this](bool canRedo) {
			if (_lastCanRedo && !canRedo)  // Transition: true -> false
			{
				MainWindow::showStatusMessage("Nothing to redo", 2000);
			}
			_lastCanRedo = canRedo;
		});

	setupUndoStackMonitoring();

	setAttribute(Qt::WA_DeleteOnClose);
		
	int values[] = { 0, 2, 4, 8, 16, 32 };
	int samples = values[settings.value("msaaComboBox", 4).toInt()];

	QSurfaceFormat format;
	format.setVersion(4, 5); // OpenGL version 4.5
	format.setProfile(QSurfaceFormat::CoreProfile);
	format.setDepthBufferSize(24);
	format.setStencilBufferSize(8);
	format.setSwapInterval(0);
	format.setStereo(true);
	format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
	format.setRenderableType(QSurfaceFormat::OpenGL);
	format.setSamples(samples); // Set MSAA samples
	_glWidget = new GLWidget(this, "glwidget");
	_glWidget->setAttribute(Qt::WA_DeleteOnClose);
	_glWidget->setFormat(format);
	_glWidget->setMouseTracking(true);
	// Put the GL widget inside the frame
	QVBoxLayout* flayout = new QVBoxLayout(glframe);
	flayout->setContentsMargins(0, 0, 0, 0);
	flayout->addWidget(_glWidget, 1);

	connect(checkBoxAutoFitView, &QCheckBox::toggled, _glWidget, &GLWidget::setAutoFitViewOnUpdate);
	connect(checkBoxSelectionHighlight, &QCheckBox::toggled, _glWidget, &GLWidget::setSelectionHighlighting);
	connect(_glWidget, &GLWidget::singleSelectionDone, this, &ModelViewer::setListRow);
	connect(_glWidget, &GLWidget::sweepSelectionDone, this, &ModelViewer::setListRows);
	connect(_glWidget, &GLWidget::zoomAndPanSet, this, [this]() {
		if (_treeRebuildPending)
			rebuildTreeFromCurrentState();
	});

	treeWidgetModel->setSceneGraph(_sceneGraph);
	treeWidgetModel->setGLWidget(_glWidget);
	treeWidgetModel->installEventFilter(this);
	treeWidgetModel->viewport()->installEventFilter(this);

	treeWidgetModel->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(treeWidgetModel, &SceneTreeWidget::customContextMenuRequested, this, &ModelViewer::showContextMenu);

	// Rename via tree widget's internal delegate handling
	connect(treeWidgetModel, &SceneTreeWidget::meshRenamed,
	        this, &ModelViewer::handleTreeWidgetMeshRenamed);

	QTimer* searchTimer = new QTimer(this);
	searchTimer->setSingleShot(true);
	searchTimer->setInterval(500);

	connect(searchBox, &QLineEdit::textEdited, this, [searchTimer](const QString&) {
		searchTimer->start();
		});

	connect(searchTimer, &QTimer::timeout, this, [this]() {
		treeWidgetModel->filterItems(searchBox->text());

		// Visual feedback if no match
		bool anySelected = treeWidgetModel->hasMeshSelection();
		searchBox->setStyleSheet((anySelected || searchBox->text().isEmpty()) ? "" : "QLineEdit { border: 2px solid red; }");
		});

	connect(treeWidgetModel, &SceneTreeWidget::selectionUpdated,
	        this, &ModelViewer::handleTreeWidgetSelectionChanged);

	connect(treeWidgetModel, &SceneTreeWidget::meshVisibilityChanged,
	        this, &ModelViewer::handleTreeWidgetVisibilityChanged);

	QShortcut* shortcut = new QShortcut(QKeySequence(Qt::Key_Delete), treeWidgetModel);
	connect(shortcut, &QShortcut::activated, this, &ModelViewer::deleteSelectedItems);

	shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I), this);
	connect(shortcut, &QShortcut::activated, this, &ModelViewer::onFileImport);

	shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E), this);
	connect(shortcut, &QShortcut::activated, this, &ModelViewer::onFileExport);


	connect(Ui_ModelViewer::objectTransformPanel, &ObjectTransformPanel::applyTransformationsRequested,
		this, &ModelViewer::setTransformation);

	connect(Ui_ModelViewer::objectTransformPanel, &ObjectTransformPanel::bakeTransformationsRequested,
		this, &ModelViewer::bakeTransformations);

	connect(Ui_ModelViewer::objectTransformPanel, &ObjectTransformPanel::resetTransformationsRequested,
		this, [this]() {
			resetTransformation();
			objectTransformPanel->resetAllValues();
		});

	connect(Ui_ModelViewer::objectTransformPanel, &ObjectTransformPanel::detachRequested,
		this, [this]() { detachTransformationsPanel(); });

	// Set default tab to Material Settings
	tabWidgetVizAttribs->setCurrentIndex(0);

	// Shortcut to toggle rendering mode (ADS <-> PBR)
	shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P), this);
	connect(shortcut, &QShortcut::activated, this, [this] {
		// Toggle between ADS and PBR based on current rendering mode
		RenderingMode currentMode = _glWidget->getRenderingMode();
		if (currentMode == RenderingMode::ADS_BLINN_PHONG)
		{
			onRenderingModeSelected("PBR");
		}
		else
		{
			onRenderingModeSelected("ADS");
		}
	});

	connect(Ui_ModelViewer::predefinedMaterialsPanel, &MaterialPropertiesPanel::materialApplied,
		this, &ModelViewer::onCustomMaterialApplied);

	connect(Ui_ModelViewer::predefinedMaterialsPanel, &MaterialPropertiesPanel::detachRequested,
		this, &ModelViewer::detachMaterialPanel);
	connect(Ui_ModelViewer::predefinedMaterialsPanel, &MaterialPropertiesPanel::textureSamplerChanged,
		this, &ModelViewer::setTextureSamplersToSelectedItems);
	connect(Ui_ModelViewer::predefinedMaterialsPanel, &MaterialPropertiesPanel::textureCacheClearRequested,
		this, &ModelViewer::onTextureCacheCleared);

	// Initialize material properties panel with MDI-scoped cache reference
	Ui_ModelViewer::predefinedMaterialsPanel->initialize(this, _glWidget);

	// Pass preview widget to environment panel so it updates on setting changes
	visualizationEnvironmentPanel->setPreviewWidget(Ui_ModelViewer::predefinedMaterialsPanel->getPreviewWidget());

	visualizationEnvironmentPanel->initialize(this, _glWidget);

	connect(_glWidget, QOverload<int>::of(&GLWidget::displayModeChanged),
		visualizationEnvironmentPanel, QOverload<int>::of(&VisualizationEnvironmentPanel::onDisplayModeChanged));

	// Update preview when rendering mode changes (ADS vs PBR)
	auto* previewWidget = Ui_ModelViewer::predefinedMaterialsPanel->getPreviewWidget();
	connect(_glWidget, QOverload<int>::of(&GLWidget::renderingModeChanged),
		this, [previewWidget](int){ previewWidget->update(); });

	// Connect ViewToolbar rendering mode selection
	connect(_glWidget->getViewToolbar(), &ViewToolbar::renderingModeSelected,
		this, &ModelViewer::onRenderingModeSelected);

	// Connect ViewToolbar navigation selection
	connect(_glWidget->getViewToolbar(), &ViewToolbar::rotateViewRequested,
		_glWidget, [this]() { _glWidget->setRotationActive(true); });

	connect(_glWidget->getViewToolbar(), &ViewToolbar::panViewRequested,
		_glWidget, [this]() { _glWidget->setPanningActive(true); });

	connect(_glWidget->getViewToolbar(), &ViewToolbar::zoomViewRequested,
		_glWidget, [this]() { _glWidget->setZoomingActive(true); });

	connect(Ui_ModelViewer::visualizationEnvironmentPanel, &VisualizationEnvironmentPanel::detachRequested,
		this, &ModelViewer::detachEnvironmentPanel);

	connect(Ui_ModelViewer::toolButtonDetach, &QToolButton::clicked,
		this, &ModelViewer::detachNavigationPanel);

	_hasPBRAlbedoTex = false;	
	_hasPBRMetallicTex = false;
	_hasPBRRoughnessTex = false;
	_hasPBRAOTex = false;
	_hasPBROpacTex = false;
	_hasPBRNormalTex = false;
	_hasPBRHeightTex = false;
	_heightPBRTexScale = 0.02f;

	_progressiveLoadingEnabled = QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName()).value("checkProgressiveLoading", true).toBool();

	updateControls();

	connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this, [this]() {
		retranslateUi(this);
		retranslateUI();  // if needed
		});
}

ModelViewer::~ModelViewer()
{
	// Close detached dialogs before destroying
	if (_detachedMaterialDialog)
	{
		_detachedMaterialDialog->close();
		_detachedMaterialDialog->deleteLater();
		_detachedMaterialDialog = nullptr;
	}

	if (_undoStack)
	{
		disconnect(_undoStack, nullptr, nullptr, nullptr);  // Prevent callbacks
		_undoStack->clear();
	}
	if (_glWidget)
	{
		delete _glWidget;
	}
}

void ModelViewer::retranslateUI()
{
	// Dynamically created	
}

void ModelViewer::deselectAll()
{
	treeWidgetModel->clearMeshSelection();
	resetTransformationValues();
	handleTreeWidgetSelectionChanged();
}

void ModelViewer::setListRow(int index)
{
	if (index == -1)
		return;

	std::vector<TriangleMesh*> meshes = _glWidget->getMeshStore();
	TriangleMesh* mesh = meshes.at(index);

	// Build new selection set
	QSet<int> newSelection;
	std::vector<int> currentIDs = getSelectedIDs();
	newSelection = QSet<int>(currentIDs.begin(), currentIDs.end());

	if (mesh->isSelected())
	{
		// Toggle off
		newSelection.remove(index);
	}
	else
	{		
		newSelection.insert(index);
	}

	// Apply selection with undo support
	setSelectionWithUndo(newSelection);

	// Update transformation panel if needed
	if (tabWidgetVizAttribs->currentIndex() == 1)
	{
		if (newSelection.size() == 1)
			updateTransformationValues();
		else
			resetTransformationValues();
	}
}

void ModelViewer::setListRows(QList<int> indices)
{
	if (indices.isEmpty())
		return;

	// Build selection set from indices
	QSet<int> newSelection;
	for (int index : indices)
		newSelection.insert(index);

	// Apply selection with undo support
	setSelectionWithUndo(newSelection);
}

void ModelViewer::setTransformation()
{
	if (!checkForActiveSelection())
		return;

	QApplication::setOverrideCursor(Qt::WaitCursor);

	// Get transformation values from panel
	QVector3D translate = objectTransformPanel->getTranslation();
	QVector3D rotate = objectTransformPanel->getRotation();
	QVector3D scale = objectTransformPanel->getScale();

	// Get UUIDs of selected meshes
	QVector<QUuid> uuids;
	std::vector<int> ids = getSelectedIDs();
	for (int id : ids)
	{
		QUuid uuid = _glWidget->getUuidByIndex(id);
		if (!uuid.isNull())
			uuids.append(uuid);
	}

	// Create and push transform command
	// redo() will be called automatically and will apply the transformation
	_undoStack->push(new TransformCommand(
		this, _glWidget, uuids, translate, rotate, scale
	));

	// Update UI (transformation already applied by command's redo())
	float range = _glWidget->getBoundingSphere().getRadius() * 4.0f;
	float offset = _glWidget->getFloorSize() * 1.25f;
	visualizationEnvironmentPanel->updateLightPositionRanges(range, offset);
	_glWidget->update();

	QApplication::restoreOverrideCursor();
}

void ModelViewer::bakeTransformations()
{
	if (!checkForActiveSelection())
		return;

	// Updated warning dialog
	QMessageBox::StandardButton reply = QMessageBox::question(
		this,
		tr("Bake Transformations"),
		tr("This operation will permanently apply transformations to mesh vertices.\n"
			"Transform undo/redo for these meshes will no longer work.\n"
			"Other undo operations (selection, visibility, etc.) will still work.\n\n"
			"Do you want to proceed?"),
		QMessageBox::Yes | QMessageBox::No
	);

	if (reply != QMessageBox::Yes)
		return;

	QApplication::setOverrideCursor(Qt::WaitCursor);

	// Get selected mesh IDs and UUIDs
	std::vector<int> ids = getSelectedIDs();
	QVector<QUuid> bakedUuids;
	for (int id : ids)
	{
		QUuid uuid = _glWidget->getUuidByIndex(id);
		if (!uuid.isNull())
			bakedUuids.append(uuid);
	}

	// Bake the transformations
	_glWidget->bakeTransformation(ids);

	// Mark all TransformCommands affecting these meshes as obsolete
	for (int i = 0; i < _undoStack->count(); ++i)
	{
		const TransformCommand* cmd =
			dynamic_cast<const TransformCommand*>(_undoStack->command(i));
		if (cmd && cmd->affectsAnyUuid(bakedUuids))
		{
			// Mark each baked mesh in the command
			for (const QUuid& uuid : bakedUuids)
			{
				cmd->markMeshBaked(uuid);
			}
		}
	}

	// Panel values already reset (you handled this!)

	QMessageBox::information(
		this,
		tr("Action Complete"),
		tr("Baked the applied transformations into the mesh vertices")
	);

	QApplication::restoreOverrideCursor();
	_glWidget->update();
}

void ModelViewer::resetTransformation()
{
	if (!checkForActiveSelection())
		return;

	QApplication::setOverrideCursor(Qt::WaitCursor);

	// Get UUIDs of selected meshes
	QVector<QUuid> uuids;
	std::vector<int> ids = getSelectedIDs();
	for (int id : ids)
	{
		QUuid uuid = _glWidget->getUuidByIndex(id);
		if (!uuid.isNull())
			uuids.append(uuid);
	}

	// Reset is transformation to identity values
	QVector3D identity_trans(0, 0, 0);
	QVector3D identity_rot(0, 0, 0);
	QVector3D identity_scale(1, 1, 1);

	// Create and push transform command with identity values
	_undoStack->push(new TransformCommand(
		this, _glWidget, uuids,
		identity_trans, identity_rot, identity_scale,
		tr("Reset Transform")  // Different text for reset
	));

	// Reset panel values
	objectTransformPanel->resetAllValues();

	// Update UI
	float range = _glWidget->getBoundingSphere().getRadius() * 4.0f;
	float offset = _glWidget->getFloorSize() * 1.25f;
	visualizationEnvironmentPanel->updateLightPositionRanges(range, offset);
	_glWidget->update();

	QApplication::restoreOverrideCursor();
}

void ModelViewer::updateTransformationValues()
{
	try
	{
		QList<QUuid> selected = treeWidgetModel->selectedMeshUuids();
		if (!selected.isEmpty())
		{
			TriangleMesh* mesh = _glWidget->getMeshByUuid(selected.at(0));
			if (mesh)
			{
				QVector3D trans = mesh->getTranslation();
				QVector3D rot = mesh->getRotation();
				QVector3D scale = mesh->getScaling();
				objectTransformPanel->setTranslationValues(trans);
				objectTransformPanel->setRotationValues(rot);
				objectTransformPanel->setScaleValues(scale);
			}
		}
	}
	catch (const std::exception& ex)
	{
		std::cout << "Exception raised in ModelViewer::on_tabWidgetVizAttribs_currentChanged\n" << ex.what() << std::endl;
	}
}

void ModelViewer::resetTransformationValues()
{
	objectTransformPanel->resetAllValues();
}

void ModelViewer::updateControls()
{
	visualizationEnvironmentPanel->updateButtonStyles();
	// ADS Lighting mode (computed from PBR properties)
}

QString ModelViewer::getSupportedQtImagesFilter()
{
	QList<QByteArray> supportedFormats = QImageReader::supportedImageFormats();
	QList<QString> filters;
	QString filter("All Supported Images (");
	for (const QByteArray& ba : supportedFormats)
	{
		filter += QString("*.%1 ").arg(QString(ba));
		filters.push_back(QString("*.%1").arg(QString(ba)));
	}
	filter += ")";
	for (const QString& fil : filters)
	{
		filter += ";;" + fil;
	}
	return filter;
}

void ModelViewer::detachMaterialPanel()
{
	if (!predefinedMaterialsPanel || !tabWidgetVizAttribs) return;

	if (_detachedMaterialDialog)
	{
		reattachMaterialPanel();
		return;
	}

	// STEP 1: Extract the entire previewFrame which contains all preview controls
	// This keeps the preview and all its tools in the main thread
	// The previewFrame is a QFrame that wraps:
	//   - MaterialPreviewWidget
	//   - Shape selector (comboShape)
	//   - Environment selector (comboEnv)
	//   - View Components label
	//   - Texture view selector (comboTexView)

	// Find the previewFrame in the panel's UI
	QFrame* previewFrame = predefinedMaterialsPanel->findChild<QFrame*>("previewFrame");
	if (!previewFrame)
	{
		qWarning() << "detachMaterialPanel: Failed to find previewFrame";
		return;
	}

	qDebug() << "detachMaterialPanel: Found previewFrame" << previewFrame;

	// Get the scroll area directly from the UI
	QScrollArea* scrollArea = findChild<QScrollArea*>("scrollAreaMaterial");
	if (!scrollArea)
	{
		qWarning() << "detachMaterialPanel: Failed to find scroll area";
		return;
	}

	_materialOriginalParent = scrollArea;

	// Create a container widget to hold the preview and its tools
	// This container will be added as a new tab and kept in the main window
	_materialPreviewContainer = new QWidget(this);
	_materialPreviewContainer->setWindowTitle(tr("Material Preview"));
	QVBoxLayout* previewContainerLayout = new QVBoxLayout(_materialPreviewContainer);
	previewContainerLayout->setContentsMargins(2, 2, 2, 2);
	previewContainerLayout->setSpacing(4);

	// STEP 2: Reparent the entire previewFrame to the container
	// This moves the complete preview section with all controls
	qDebug() << "Reparenting previewFrame to container";

	previewFrame->setParent(_materialPreviewContainer);
	previewContainerLayout->addWidget(previewFrame, 1);  // Stretch to fill
	previewFrame->show();
	previewContainerLayout->activate();  // Ensure layout is processed
	_materialPreviewContainer->adjustSize();  // Adjust container size

	qDebug() << "PreviewFrame reparented, container size:" << _materialPreviewContainer->size();

	// STEP 3: Create a new tab for the preview container
	// Insert it at the beginning (index 0) so it's the first tab
	_materialPreviewContainerTabIndex = 0;
	tabWidgetVizAttribs->insertTab(_materialPreviewContainerTabIndex, _materialPreviewContainer, tr("Preview"));

	qDebug() << "Preview tab inserted at index 0 (first position)";

	// STEP 4: Make sure the container is visible and the tab is current
	_materialPreviewContainer->show();
	_materialPreviewContainer->raise();  // Bring to front
	if (_materialPreviewContainerTabIndex >= 0)
	{
		tabWidgetVizAttribs->setCurrentIndex(_materialPreviewContainerTabIndex);
		qDebug() << "Set current tab to preview index";
	}

	// Ensure the preview frame is properly initialized in the new context
	previewFrame->update();  // Trigger a repaint
	previewFrame->repaint();  // Force immediate repaint

	// STEP 5: Hide the original material panel tab and prepare it for floating dialog
	_materialPageIndex = tabWidgetVizAttribs->indexOf(scrollArea->parentWidget());
	if (_materialPageIndex >= 0)
	{
		_materialPageLabel = tabWidgetVizAttribs->tabText(_materialPageIndex);
		tabWidgetVizAttribs->removeTab(_materialPageIndex);
		qDebug() << "Removed material panel tab at index:" << _materialPageIndex;
	}

	// STEP 6: The previewFrame has been moved, leaving the panel with just the library tree
	// The leftLayout (library tree) will expand to fill the available space

	// STEP 6: Create floating dialog with the (now preview-less) panel
	auto* floatingMatDlg = new FloatingPanelDialog(this, tr("Predefined Materials"));
	_detachedMaterialDialog = floatingMatDlg;

	// Remove panel from scroll area and add to floating dialog
	scrollArea->takeWidget();  // Remove current widget from scroll area
	floatingMatDlg->addContentWidget(predefinedMaterialsPanel);

	// Position and show the floating dialog...
	QScreen* screen = QGuiApplication::primaryScreen();
	QRect screenGeom = screen->availableGeometry();
	QRect myGeometry = this->frameGeometry();
	int x = myGeometry.right() + 20;
	int y = myGeometry.top();
	if (x + 420 > screenGeom.right()) x = screenGeom.right() - predefinedMaterialsPanel->width() - 40;  // Fit within screen;
	if (y + 700 > screenGeom.bottom()) y = screenGeom.bottom() - predefinedMaterialsPanel->height();
	if (x < screenGeom.left()) x = screenGeom.left();
	if (y < screenGeom.top()) y = screenGeom.top();

	_detachedMaterialDialog->move(x, y);
	_detachedMaterialDialog->resize(420, std::min(predefinedMaterialsPanel->height(), static_cast<int>(height() * 0.95)));
	_detachedMaterialDialog->show();
	predefinedMaterialsPanel->setDetached(true);

	connect(floatingMatDlg, &FloatingPanelDialog::reattachRequested,
		this, &ModelViewer::reattachMaterialPanel);

	qDebug() << "Material panel detached. Preview widget kept in main thread in new 'Preview' tab.";
}

void ModelViewer::reattachMaterialPanel()
{
	if (!_detachedMaterialDialog || !_materialOriginalParent) return;

	_detachedMaterialDialog->hide();
	_detachedMaterialDialog->disconnect();

	if (predefinedMaterialsPanel && _materialPageIndex >= 0)
	{
		// STEP 1: Move the previewFrame back into the panel
		if (_materialPreviewContainer)
		{
			qDebug() << "Reattaching: Moving previewFrame back to panel";

			// Find the previewFrame in the container
			QFrame* previewFrame = _materialPreviewContainer->findChild<QFrame*>("previewFrame");
			if (previewFrame)
			{
				// Remove from container
				QLayout* containerLayout = _materialPreviewContainer->layout();
				if (containerLayout)
				{
					containerLayout->removeWidget(previewFrame);
					qDebug() << "Removed previewFrame from container layout";
				}

				// Use the panel's helper method to restore the previewFrame to its original location
				predefinedMaterialsPanel->restorePreviewFrame(previewFrame);
				qDebug() << "Restored previewFrame to panel";
			}

			// Remove the preview tab from the tab widget
			if (_materialPreviewContainerTabIndex >= 0)
			{
				tabWidgetVizAttribs->removeTab(_materialPreviewContainerTabIndex);
				_materialPreviewContainerTabIndex = -1;
				qDebug() << "Removed preview tab from tab widget";
			}

			// Delete the temporary container
			_materialPreviewContainer->deleteLater();
			_materialPreviewContainer = nullptr;
			qDebug() << "Deleted preview container";
		}

		// STEP 2: Re-parent panel back to the scroll area
		predefinedMaterialsPanel->setParent(nullptr);  // Detach from floating dialog

		QScrollArea* scrollArea = qobject_cast<QScrollArea*>(_materialOriginalParent);
		if (scrollArea)
		{
			scrollArea->setWidget(predefinedMaterialsPanel);
			predefinedMaterialsPanel->show();

			// Preview widgets have been restored to their original layout
			predefinedMaterialsPanel->setDetached(false);
		}

		// STEP 3: Restore the panel tab
		tabWidgetVizAttribs->insertTab(_materialPageIndex, scrollArea->parentWidget(), _materialPageLabel);
		tabWidgetVizAttribs->setCurrentIndex(_materialPageIndex);
	}

	_detachedMaterialDialog->deleteLater();
	_detachedMaterialDialog = nullptr;
	_materialOriginalParent = nullptr;

	qDebug() << "Material panel reattached. Preview widget moved back into panel.";
}

void ModelViewer::detachTransformationsPanel()
{
	if (!tabWidgetVizAttribs) return;

	if(_detachedTransformationsDialog)
	{
		reattachTransformationsPanel();
		return;
	}

	// Get the scroll area directly from the UI
	QScrollArea* scrollArea = findChild<QScrollArea*>("scrollAreaTransform");
	if (!scrollArea) return;

	_transformationsOriginalParent = scrollArea;

	// Hide the tab
	_transformationsPageIndex = tabWidgetVizAttribs->indexOf(scrollArea->parentWidget());
	if (_transformationsPageIndex >= 0)
	{
		_transformationsPageLabel = tabWidgetVizAttribs->tabText(_transformationsPageIndex);
		tabWidgetVizAttribs->removeTab(_transformationsPageIndex);
	}

	// Create floating dialog
	auto* floatingTransDlg = new FloatingPanelDialog(this, tr("Object Transformations"));
	_detachedTransformationsDialog = floatingTransDlg;

	// Remove panel from scroll area and add to floating dialog
	scrollArea->takeWidget();  // Remove current widget from scroll area
	floatingTransDlg->addContentWidget(objectTransformPanel);
	// Position and show...
	QScreen* screen = QGuiApplication::primaryScreen();
	QRect screenGeom = screen->availableGeometry();
	QRect myGeometry = this->frameGeometry();
	int x = myGeometry.right() + 20;
	int y = myGeometry.top();
	if (x + 420 > screenGeom.right()) x = screenGeom.right() - objectTransformPanel->width() - 40;  // Fit within screen;
	if (y + 700 > screenGeom.bottom()) y = screenGeom.bottom() - objectTransformPanel->height();
	if (x < screenGeom.left()) x = screenGeom.left();
	if (y < screenGeom.top()) y = screenGeom.top();
	_detachedTransformationsDialog->move(x, y);
	_detachedTransformationsDialog->resize(420, std::min(objectTransformPanel->height(), static_cast<int>(height() * 0.95)));
	_detachedTransformationsDialog->show();
	objectTransformPanel->setDetached(true);
	connect(floatingTransDlg, &FloatingPanelDialog::reattachRequested,
		this, &ModelViewer::reattachTransformationsPanel);
}

void ModelViewer::reattachTransformationsPanel()
{
	if (!_detachedTransformationsDialog || !_transformationsOriginalParent) return;

	_detachedTransformationsDialog->hide();
	_detachedTransformationsDialog->disconnect();

	if (objectTransformPanel && _transformationsPageIndex >= 0)
	{
		objectTransformPanel->setParent(nullptr);  // Detach from floating dialog

		// Re-parent panel back to the scroll area
		QScrollArea* scrollArea = qobject_cast<QScrollArea*>(_transformationsOriginalParent);
		if (scrollArea)
		{
			scrollArea->setWidget(objectTransformPanel);
			objectTransformPanel->show();
			objectTransformPanel->setDetached(false);
		}

		// Restore the tab
		tabWidgetVizAttribs->insertTab(_transformationsPageIndex, scrollArea->parentWidget(), _transformationsPageLabel);
		tabWidgetVizAttribs->setCurrentIndex(_transformationsPageIndex);
	}

	_detachedTransformationsDialog->deleteLater();
	_detachedTransformationsDialog = nullptr;
	_transformationsOriginalParent = nullptr;
}

void ModelViewer::detachEnvironmentPanel()
{
	if (!visualizationEnvironmentPanel || !tabWidgetVizAttribs) return;

	if (_detachedEnvironmentDialog)
	{
		reattachEnvironmentPanel();
		return;
	}

	// Get the scroll area directly from the UI
	QScrollArea* scrollArea = findChild<QScrollArea*>("scrollAreaEnv");
	if (!scrollArea) return;

	_environmentOriginalParent = scrollArea;

	// Hide the tab
	_environmentPageIndex = tabWidgetVizAttribs->indexOf(scrollArea->parentWidget());
	if (_environmentPageIndex >= 0)
	{
		_environmentPageLabel = tabWidgetVizAttribs->tabText(_environmentPageIndex);
		tabWidgetVizAttribs->removeTab(_environmentPageIndex);
	}

	// Create floating dialog
	auto* floatingEnvDlg = new FloatingPanelDialog(this, tr("Visualization Environment Settings"));
	_detachedEnvironmentDialog = floatingEnvDlg;

	// Remove panel from scroll area and add to floating dialog
	scrollArea->takeWidget();  // Remove current widget from scroll area
	floatingEnvDlg->addContentWidget(visualizationEnvironmentPanel);

	// Position and show...
	QScreen* screen = QGuiApplication::primaryScreen();
	QRect screenGeom = screen->availableGeometry();
	QRect myGeometry = this->frameGeometry();
	int x = myGeometry.right() + 20;
	int y = myGeometry.top();
	if (x + 420 > screenGeom.right()) x = screenGeom.right() - visualizationEnvironmentPanel->width() - 40;  // Fit within screen;
	if (y + 700 > screenGeom.bottom()) y = screenGeom.bottom() - visualizationEnvironmentPanel->height();
	if (x < screenGeom.left()) x = screenGeom.left();
	if (y < screenGeom.top()) y = screenGeom.top();

	_detachedEnvironmentDialog->move(x, y);
	_detachedEnvironmentDialog->resize(420, std::min(visualizationEnvironmentPanel->height(), static_cast<int>(height() * 0.95)));
	_detachedEnvironmentDialog->show();
	visualizationEnvironmentPanel->setDetached(true);

	connect(floatingEnvDlg, &FloatingPanelDialog::reattachRequested,
		this, &ModelViewer::reattachEnvironmentPanel);
}

void ModelViewer::reattachEnvironmentPanel()
{
	if (!_detachedEnvironmentDialog || !_environmentOriginalParent) return;

	_detachedEnvironmentDialog->hide();
	_detachedEnvironmentDialog->disconnect();

	if (visualizationEnvironmentPanel && _environmentPageIndex >= 0)
	{
		visualizationEnvironmentPanel->setParent(nullptr);  // Detach from floating dialog

		// Re-parent panel back to the scroll area
		QScrollArea* scrollArea = qobject_cast<QScrollArea*>(_environmentOriginalParent);
		if (scrollArea)
		{
			scrollArea->setWidget(visualizationEnvironmentPanel);
			visualizationEnvironmentPanel->show();
			visualizationEnvironmentPanel->setDetached(false);
		}

		// Restore the tab
		tabWidgetVizAttribs->insertTab(_environmentPageIndex, scrollArea->parentWidget(), _environmentPageLabel);
		tabWidgetVizAttribs->setCurrentIndex(_environmentPageIndex);
	}

	_detachedEnvironmentDialog->deleteLater();
	_detachedEnvironmentDialog = nullptr;
	_environmentOriginalParent = nullptr;
}

void ModelViewer::detachNavigationPanel()
{
	if (!modelNavigationWidget || !splitter_2) return;

	// Toggle back into the splitter when already detached as an overlay.
	if (_detachedNavigationOverlay)
	{
		reattachNavigationPanel();
		return;
	}

	// Save splitter proportions so we can restore them on reattach.
	_navigationSplitterSizes = splitter_2->sizes();

	const int overlayWidth = 420;
	_detachedNavigationOverlay = _glWidget->attachOverlayPanel(
		modelNavigationWidget,
		QRect(10, 36, overlayWidth, std::max(120, _glWidget->height() - 36 - 96)),
		Qt::AlignTop | Qt::AlignLeft,
		"navigationOverlayPanel");

	if (_detachedNavigationOverlay)
	{
		treeWidgetModel->setDetachedOverlayMode(true);
		updateNavigationOverlayGeometry();
		_detachedNavigationOverlay->show();
	}

	toolButtonDetach->setIcon(QIcon(":/icons/res/reattach.png"));
	toolButtonDetach->setToolTip(tr("Reattach to panel"));
	modelNavigationWidget->show();
}

void ModelViewer::updateNavigationOverlayGeometry()
{
	if (!_detachedNavigationOverlay || !_glWidget)
		return;

	const int overlayTop = 36;
	const int overlayLeft = 10;
	const int overlayWidth = 420;
	const int overlayBottomMargin = 96;
	_detachedNavigationOverlay->setGeometry(
		overlayLeft,
		overlayTop,
		overlayWidth,
		std::max(120, _glWidget->height() - overlayTop - overlayBottomMargin));
}

void ModelViewer::reattachNavigationPanel()
{
	if (!_detachedNavigationOverlay || !splitter_2) return;

	treeWidgetModel->setDetachedOverlayMode(false);
	_glWidget->takeOverlayPanel(modelNavigationWidget);
	_detachedNavigationOverlay = nullptr;

	// Re-insert at index 0 (its original slot) — insertWidget handles the re-parent.
	splitter_2->insertWidget(0, modelNavigationWidget);

	if (!_navigationSplitterSizes.isEmpty())
		splitter_2->setSizes(_navigationSplitterSizes);

	toolButtonDetach->setIcon(QIcon(":/icons/res/detach.png"));
	toolButtonDetach->setToolTip(tr("Detach from panel"));
	modelNavigationWidget->show();
}

void ModelViewer::setupUndoStackMonitoring()
{
	// Connect to stack changes
	connect(_undoStack, &QUndoStack::indexChanged,
		this, &ModelViewer::onUndoStackChanged);

	// Initialize cache
	_lastStackCount = 0;
	_cachedReferencedUuids.clear();
}

void ModelViewer::onUndoStackChanged()
{
	if (!_undoStack || !_glWidget)
		return;

	int currentCount = _undoStack->count();

	// Only cleanup when stack size changes (commands added/purged)
	// Not on every undo/redo (which just changes index)
	if (currentCount != _lastStackCount)
	{
		// Check if commands were purged (count decreased)
		// or if this is the first operation (count increased from 0)
		bool shouldCleanup = (currentCount < _lastStackCount) ||
			(_lastStackCount == 0 && currentCount > 0);

		if (shouldCleanup)
		{
			cleanupOrphanedMeshes();
		}
		else
		{
			// Count increased - command was added
			// Update cache incrementally instead of full scan

			// Get the newly added command (at current index - 1)
			int newCmdIndex = _undoStack->index() - 1;
			if (newCmdIndex >= 0 && newCmdIndex < _undoStack->count())
			{
				const QUndoCommand* cmd = _undoStack->command(newCmdIndex);

				// If it's a DeleteCommand, add its UUIDs to cache
				if (const auto* delCmd = dynamic_cast<const DeleteMeshCommand*>(cmd))
				{
					_cachedReferencedUuids.unite(delCmd->getReferencedUuids());
				}
			}
		}

		_lastStackCount = currentCount;
	}
}

void ModelViewer::cleanupOrphanedMeshes()
{
	// Get current set of referenced UUIDs by scanning stack
	QSet<QUuid> currentlyReferenced = scanStackForReferencedUuids();

	// Find UUIDs that were in cache but no longer referenced
	QSet<QUuid> orphaned = _cachedReferencedUuids - currentlyReferenced;

	if (!orphaned.isEmpty())
	{
		// Permanently delete orphaned meshes from recycle bin
		for (const QUuid& uuid : orphaned)
		{
			_glWidget->permanentlyDeleteFromBin(uuid);
		}

		qDebug() << "Cleaned up" << orphaned.size()
			<< "orphaned mesh(es) from recycle bin";
	}

	// Update cache
	_cachedReferencedUuids = currentlyReferenced;
}

bool ModelViewer::saveMaterialsBeforeClose()
{
	// Get the material panel
	MaterialPropertiesPanel* materialPanel = Ui_ModelViewer::predefinedMaterialsPanel;
	if (!materialPanel)
	{
		qWarning() << "Material panel not available for saving unsaved materials";
		return false;
	}

	// Get all unsaved material keys
	QSet<QString> unsavedKeys = materialPanel->getUnsavedMaterialKeys();
	if (unsavedKeys.isEmpty())
	{
		return true;  // Nothing to save
	}

	int savedCount = 0;
	int failedCount = 0;

	// Block signals during batch save to prevent "select a mesh" dialogs during tree refresh
	materialPanel->beginSaveUnsavedMaterials();

	// Save each unsaved material to the library
	for (const QString& key : unsavedKeys)
	{
		// Get cached material with metadata
		auto cachedIt = _materialCache.find(key);
		if (cachedIt == _materialCache.end())
		{
			qWarning() << "Material key not found in cache:" << key;
			failedCount++;
			continue;
		}

		const CachedMaterial& cached = cachedIt.value();
		QString groupLabel = cached.group;
		QString materialName = cached.name;
		const GLMaterial& material = cached.material;

		// Save to library using existing infrastructure
		// Pass nullptr as parent to suppress "Overwrite?" dialog during closeEvent
		// User already chose "Save All", so we auto-confirm overwrites
		QString errorMsg;
		bool success = MaterialLibraryWidget::saveUserMaterialToUserLocation(
			groupLabel,
			key,
			materialName,
			material,
			nullptr,  // No parent = no confirmation dialog
			&errorMsg
		);

		if (success)
		{
			// Remove from unsaved set
			materialPanel->removeMaterialFromUnsaved(key);
			_ownedUnsavedMaterials.remove(key);
			savedCount++;
			qDebug() << "Successfully saved material:" << materialName << "(" << key << ")";
		}
		else
		{
			failedCount++;
			qWarning() << "FAILED to save material:" << materialName;
			qWarning() << "  Key:" << key;
			qWarning() << "  Group:" << groupLabel;
			qWarning() << "  Error:" << errorMsg;
		}
	}

	// Unblock signals and refresh tree BEFORE showing dialogs
	// This prevents the tree refresh from triggering material selection signals
	// which would cause "Please select a mesh" warnings
	materialPanel->endSaveUnsavedMaterials();

	// Show result to user
	if (failedCount > 0)
	{
		QString msg = QString(tr("Saved %1 of %2 material(s). %3 failed to save."))
			.arg(savedCount)
			.arg(savedCount + failedCount)
			.arg(failedCount);
		QMessageBox::warning(this, tr("Save Materials - Partial Success"), msg);
		return false;  // Some materials failed to save
	}
	else if (savedCount > 0)
	{
		QString msg = QString(tr("Successfully saved %1 material(s) to library.")).arg(savedCount);
		QMessageBox::information(this, tr("Materials Saved"), msg);
		return true;  // All saved successfully
	}

	return true;  // Nothing needed saving
}

void ModelViewer::cleanupUnsavedMaterialsFromLibrary()
{
	// Remove ONLY unsaved materials CREATED BY THIS MDI from shared library when it closes
	// This allows other MDIs' unsaved materials to remain visible

	if (_ownedUnsavedMaterials.isEmpty())
	{
		qDebug() << "No unsaved materials owned by this MDI to clean up";
		return;
	}

	// Remove from shared material map - only owned materials
	auto& sharedMap = const_cast<QMap<QString, std::function<GLMaterial()>>&>(
		MaterialLibraryWidget::sharedMaterialMap());

	for (const QString& key : _ownedUnsavedMaterials)
	{
		if (sharedMap.remove(key) > 0)
		{
			qDebug() << "Removed owned unsaved material from shared map:" << key;
		}
	}

	// Remove from shared groups - only owned materials
	auto& mutableGroups = const_cast<QVector<QPair<QString, QVector<QPair<QString, QString>>>>&>(
		MaterialLibraryWidget::sharedGroups());

	for (auto& groupPair : mutableGroups)
	{
		// Remove only unsaved materials owned by this MDI
		auto& materials = groupPair.second;
		materials.erase(
			std::remove_if(materials.begin(), materials.end(),
				[this](const QPair<QString, QString>& item) {
					return _ownedUnsavedMaterials.contains(item.second);
				}),
			materials.end()
		);
	}

	qDebug() << "Cleaned up" << _ownedUnsavedMaterials.size() << "unsaved material(s) owned by this MDI";
}

QSet<QUuid> ModelViewer::scanStackForReferencedUuids()
{
	QSet<QUuid> referenced;

	// Scan all commands in the undo stack
	int count = _undoStack->count();
	for (int i = 0; i < count; ++i)
	{
		const QUndoCommand* cmd = _undoStack->command(i);

		// Check if it's a DeleteCommand
		if (const auto* delCmd = dynamic_cast<const DeleteMeshCommand*>(cmd))
		{
			referenced.unite(delCmd->getReferencedUuids());
		}

		// Optional: Could also check other command types if needed
		// For example, MaterialCommand might want to prevent deletion
		// of meshes it references, but this is probably not necessary
	}

	return referenced;
}

void ModelViewer::updateDisplayList()
{
	_glWidget->setTransmissionEnabled(false);
	for (TriangleMesh* mesh : _glWidget->getMeshStore())
	{
		if (mesh->getMaterial().hasTransmission())
		{
			_glWidget->setTransmissionEnabled(true);
			break;
		}
	}

	const bool shouldAutoFit = checkBoxAutoFitView->isChecked();
	_glWidget->setAutoFitViewOnUpdate(false);

	_visibleMeshUuids = collectVisibleUuidsFromDisplayList();
	applyVisibleMeshState(false);

	++_treeRebuildGeneration;
	_treeRebuildPending = false;
	rebuildTreeFromCurrentState();
	_glWidget->setAutoFitViewOnUpdate(shouldAutoFit);

	// Start the fit animation immediately using the bounding sphere already
	// computed by setDisplayList() above — no need to wait for the tree
	// rebuild.  Skip when the mesh store is empty (e.g. the initial show
	// before any file is loaded) to avoid fitting an empty / sentinel sphere.
	if (shouldAutoFit && !_glWidget->getMeshStore().empty())
		_glWidget->fitAll();

	
}

void ModelViewer::updateSelectionStatusMessage()
{
	int count = static_cast<int>(treeWidgetModel->selectedMeshUuids().count());
	if (count)
	{
		QString noun = count > 1 ? tr("objects") : tr("object");
		MainWindow::showStatusMessage(QString(tr("Selected %1 %2")).arg(count).arg(noun));
	}
	else
		MainWindow::showStatusMessage(tr("No selection"), 2000);
}

void ModelViewer::showEvent(QShowEvent*)
{
	//showMaximized();
	if (_runningFirstTime)
	{
		updateDisplayList();
		_runningFirstTime = false;
	}
}

bool ModelViewer::eventFilter(QObject* watched, QEvent* event)
{
	if (_treeRebuildPending &&
		(watched == treeWidgetModel || watched == treeWidgetModel->viewport()))
	{
		switch (event->type())
		{
		case QEvent::Enter:
		case QEvent::FocusIn:
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonDblClick:
		case QEvent::ContextMenu:
		case QEvent::KeyPress:
			rebuildTreeFromCurrentState();
			break;
		default:
			break;
		}
	}

	if (_treeVisibilityDirty &&
		(watched == treeWidgetModel || watched == treeWidgetModel->viewport()))
	{
		switch (event->type())
		{
		case QEvent::Enter:
		case QEvent::FocusIn:
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonDblClick:
		case QEvent::ContextMenu:
		case QEvent::KeyPress:
			syncTreeVisibilityFromModel();
			break;
		default:
			break;
		}
	}

	return QWidget::eventFilter(watched, event);
}

void ModelViewer::keyPressEvent(QKeyEvent* event)
{
	if (event->modifiers() == Qt::ControlModifier)
	{
		if (event->key() == Qt::Key_A)
		{
			selectAll();
		}
	}
	else if (event->modifiers() == Qt::AltModifier)
	{
		if (event->key() == Qt::Key_A)
			hideAllItems();
		if (event->key() == Qt::Key_C)
			centerScreen();
	}
	else if (event->modifiers() == Qt::ShiftModifier)
	{
		if (event->key() == Qt::Key_A)
			showAllItems();
	}
	else
	{
	}

	QWidget::keyPressEvent(event);
}

void ModelViewer::selectAll()
{
	if (treeWidgetModel->meshCount() > 0)
	{
		QSet<QUuid> toSelect;
		if (_glWidget->isVisibleSwapped())
		{
			// Visible-swapped mode: select the "hidden" meshes (unchecked)
			const QSet<QUuid> visible = getVisibleUuids();
			const auto& store = _glWidget->getMeshStore();
			for (size_t i = 0; i < store.size(); ++i)
			{
				QUuid uuid = _glWidget->getUuidByIndex(static_cast<int>(i));
				if (!visible.contains(uuid)) toSelect.insert(uuid);
			}
		}
		else
		{
			// Normal mode: select all visible (checked) meshes
			toSelect = getVisibleUuids();
		}
		treeWidgetModel->setSelectionByUuids(toSelect);
		handleTreeWidgetSelectionChanged();
	}
}

void ModelViewer::dragEnterEvent(QDragEnterEvent* event)
{
	if (event->mimeData()->hasUrls())
	{
		event->acceptProposedAction();
	}
}

void ModelViewer::dropEvent(QDropEvent* event)
{
	QStringList supportedExtensions = ModelViewerApplication::supportedImportExtensions();
	QApplication::setOverrideCursor(Qt::WaitCursor);
	foreach(const QUrl & url, event->mimeData()->urls())
	{
		QString fileName = url.toLocalFile();
		_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time
		QFileInfo fi(fileName);
		QString extn = fi.suffix();
		if (!supportedExtensions[0].contains(extn, Qt::CaseInsensitive)
			&& extn != "mvf")
		{
			QMessageBox::critical(this, tr("Error"), url.toString() + tr("\nUnsupported file format: ") + extn);
		}
		else
		{
			if (extn == "mvf")
				loadFromFile(fileName);
			else
			{
				UVMethod method;
				QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
				bool remember = settings.value("RememberUVMethod", false).toBool();
				if (remember)
				{
					int value = settings.value("UVMethod", static_cast<int>(UVMethod::None)).toInt();
					method = static_cast<UVMethod>(value);
				}
				else
					method = askUserForUVMethod(this).method;
				QString errMsg;
				_progressiveLoadingEnabled = settings.value("checkProgressiveLoading", true).toBool();
				bool success = _glWidget->loadAssImpModel(fileName, method, errMsg, _progressiveLoadingEnabled);
				if (!success)
				{
					if (errMsg == "Model loading cancelled by user.")
					{
						continue;
					}
					QMessageBox::critical(this, tr("Error"), tr("Failed to load model: ") + fileName + "\n" + errMsg);
					continue;
				}
			}

			updateDisplayList();
		}
	}
	QApplication::restoreOverrideCursor();
}

void ModelViewer::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	updateNavigationOverlayGeometry();
}

void ModelViewer::mouseMoveEvent(QMouseEvent* event)
{
	QWidget::mouseMoveEvent(event);
}


void ModelViewer::closeEvent(QCloseEvent* event)
{
	// Check for unsaved materials first
	MaterialPropertiesPanel* materialPanel = Ui_ModelViewer::predefinedMaterialsPanel;
	QSet<QString> unsavedKeys = materialPanel ? materialPanel->getUnsavedMaterialKeys() : QSet<QString>();

	if (!unsavedKeys.isEmpty())
	{
		int count = unsavedKeys.size();
		QString msg = QString(tr("You have %1 unsaved material(s). Do you want to save them?")).arg(count);

		QMessageBox::StandardButton reply = QMessageBox::question(
			this,
			tr("Unsaved Materials"),
			msg,
			QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

		if (reply == QMessageBox::Cancel)
		{
			event->ignore();
			return;
		}
		else if (reply == QMessageBox::Yes)
		{
			// Save all unsaved materials to library
			if (!saveMaterialsBeforeClose())
			{
				// Ask user if they want to close anyway
				int closeAnyway = QMessageBox::question(this,
					tr("Save Failed"),
					tr("Failed to save some materials. Close anyway?"),
					QMessageBox::Yes | QMessageBox::No,
					QMessageBox::No);

				if (closeAnyway != QMessageBox::Yes)
				{
					event->ignore();
					return;
				}
			}
		}
		// If No, just proceed with closing (materials will be discarded)
	}

	// Check for unsaved document changes
	if (_documentModified)
	{
		auto ret = QMessageBox::question(this, tr("Unsaved Changes"),
			tr("The document has unsaved changes. Do you want to save before closing?"),
			QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
			QMessageBox::Yes);
		if (ret == QMessageBox::Yes)
		{
			if (!save())
			{
				event->ignore();
				return;
			}
		}
		else if (ret == QMessageBox::Cancel)
		{
			event->ignore();
			return;
		}
	}

	// Clean up unsaved materials from shared library
	// Unsaved materials are MDI-scoped and should not appear in other MDIs
	cleanupUnsavedMaterialsFromLibrary();

	event->accept();
}

void ModelViewer::setCurrentFile(const QString& fileName)
{
	_currentFile = fileName;
	_documentSaved = true;
	_documentModified = false;
	setWindowTitle(tr("%1").arg(QFileInfo(_currentFile).fileName()));
}

QString ModelViewer::currentFile() const
{
	return _currentFile;
}

void ModelViewer::importModel()
{
	onFileImport();
}

void ModelViewer::exportModel()
{
	onFileExport();
}

void ModelViewer::setDocumentModified(bool modified)
{
	_documentModified = modified;
	if (modified)
	{
		setWindowTitle(tr("%1*").arg(QFileInfo(_currentFile).fileName()));
	}
	else
	{
		setWindowTitle(tr("%1").arg(QFileInfo(_currentFile).fileName()));
	}
}

bool ModelViewer::save()
{
	// If current file's extension is not .mvf, call saveAs
	// this way, user cannot accidentally overwrite non-mvf files
	QString ext = QFileInfo(_currentFile).suffix();
	if (ext.toLower() != "mvf")
	{
		return saveAs();
	}

	if (_currentFile.isEmpty())
	{
		return saveAs();
	}

	if (saveToFile(_currentFile))
	{
		_documentSaved = true;
		_documentModified = false;
		MainWindow::showStatusMessage(tr("File saved"), 2000);
		setWindowTitle(tr("%1").arg(QFileInfo(_currentFile).fileName()));
		return true;
	}
	else
	{
		QMessageBox::critical(this, tr("Error"), tr("Failed to save file: %1").arg(_currentFile));
		return false;
	}
}

bool ModelViewer::saveAs()
{
	// Set the filter for .mvf files
	QString filter = tr("Model Viewer Files (*.mvf)");
	QString fileName = QFileDialog::getSaveFileName(this, tr("Save Model"), currentFile(), filter);

	if (fileName.isEmpty())
		return false;

	// Ensure the file has the .mvf extension
	if (!fileName.endsWith(".mvf", Qt::CaseInsensitive))
		fileName += ".mvf";

	_currentFile = fileName;
	return save();
}

void ModelViewer::setDocumentSaved(bool saved)
{
	_documentSaved = saved;
	if (saved)
	{
		setWindowTitle(tr("%1").arg(QFileInfo(_currentFile).fileName()));
	}
	else
	{
		setWindowTitle(tr("%1*").arg(QFileInfo(_currentFile).fileName()));
	}
}

QString ModelViewer::getLastOpenedDir()
{
	return _lastOpenedDir;
}

void ModelViewer::setLastOpenedDir(const QString& lastOpenedDir)
{
	_lastOpenedDir = lastOpenedDir;
}

QString ModelViewer::getLastSelectedFilter()
{
	return _lastSelectedFilter;
}

void ModelViewer::setLastSelectedFilter(const QString& lastSelectedFilter)
{
	_lastSelectedFilter = lastSelectedFilter;
}

void ModelViewer::showContextMenu(const QPoint& pos)
{
	setFocus();

	// Determine what was right-clicked
	const bool clickedAssembly = treeWidgetModel->isAssemblyAt(pos);

	// When right-clicking an assembly item Qt may or may not have selected it
	// depending on the existing selection.  Force-select it so the mesh ops
	// below operate on the correct set of leaves.
	if (clickedAssembly)
		treeWidgetModel->ensureAssemblySelectionAt(pos);

	const bool hasMeshes = treeWidgetModel->hasMeshSelection();

	if (!hasMeshes && !clickedAssembly) return;

	QMenu myMenu;

	// ---- Expand / Collapse section (assembly items only) -------------------
	if (clickedAssembly && treeWidgetModel->hasChildrenAt(pos))
	{
		// Expand to first level: expand node, collapse direct children
		myMenu.addAction(QIcon(QPixmap(":/icons/res/expand.png")),
		    tr("Expand/Collapse to 1st Level"), treeWidgetModel,
		    [this, pos]() { treeWidgetModel->expandOneLevelAt(pos); });

		// Recursively expand the whole subtree
		myMenu.addAction(QIcon(QPixmap(":/icons/res/expandall.png")),
		    tr("Expand All Children"), treeWidgetModel,
		    [this, pos]() { treeWidgetModel->expandSubtreeAt(pos); });

		myMenu.addSeparator();

		// Recursively collapse the whole subtree
		myMenu.addAction(QIcon(QPixmap(":/icons/res/collapse.png")),
		    tr("Collapse All Children"), treeWidgetModel,
		    [this, pos]() { treeWidgetModel->collapseAllBelowAt(pos); });

		if (hasMeshes) myMenu.addSeparator();
	}

	// ---- Mesh operations (shown when leaves are selected) ------------------
	if (hasMeshes)
	{
		myMenu.addAction(tr("Center Screen"),          this, &ModelViewer::centerScreen);
		myMenu.addAction(tr("Visualization Settings"), this, &ModelViewer::showVisualizationModelPage);
		myMenu.addAction(tr("Transformations"),        this, &ModelViewer::showTransformationsPage);
		myMenu.addSeparator();
		myMenu.addAction(tr("Hide"),      this, &ModelViewer::hideSelectedItems);
		myMenu.addAction(tr("Show"),      this, &ModelViewer::showSelectedItems);
		myMenu.addAction(tr("Show Only"), this, &ModelViewer::showOnlySelectedItems);
		myMenu.addSeparator();
		myMenu.addAction(tr("Duplicate"), this, &ModelViewer::duplicateSelectedItems);
		myMenu.addAction(tr("Delete"),    this, &ModelViewer::deleteSelectedItems);
		myMenu.addSeparator();
		myMenu.addAction(tr("Mesh Info"), this, &ModelViewer::displaySelectedMeshInfo);
	}

	myMenu.exec(treeWidgetModel->mapMenuToGlobal(pos));
}

void ModelViewer::centerScreen()
{
	std::vector<int> selectedIDs = getSelectedIDs();
	_glWidget->centerScreen(selectedIDs);
}

void ModelViewer::duplicateSelectedItems()
{
	if (!treeWidgetModel->hasMeshSelection())
		return;

	QApplication::setOverrideCursor(Qt::WaitCursor);

	std::vector<int> ids = getSelectedIDs();

	// CAPTURE ORIGINAL SELECTION FIRST (before updateDisplayList)
	QSet<QUuid> originalSelection = getSelectedUuids();

	QVector<QUuid> duplicatedUuids = _glWidget->duplicateObjects(ids);

	updateDisplayList();  // May clear selection, but we already saved it

	// PASS original selection to command
	_undoStack->push(new DuplicateCommand(
		this, _glWidget, duplicatedUuids, originalSelection
	));

	QApplication::restoreOverrideCursor();
}

void ModelViewer::deleteSelectedItems()
{
	if (!checkForActiveSelection())
		return;

	QMessageBox::StandardButton reply = QMessageBox::question(
		this,
		tr("Delete"),
		tr("Delete selected item(s)?"),
		QMessageBox::Yes | QMessageBox::No
	);

	if (reply != QMessageBox::Yes)
		return;

	// Get UUIDs of selected meshes
	std::vector<int> indices = getSelectedIDs();
	QVector<QUuid> uuidsToDelete;

	for (int index : indices)
	{
		QUuid uuid = _glWidget->getUuidByIndex(index);
		if (!uuid.isNull())
			uuidsToDelete.append(uuid);
	}

	if (uuidsToDelete.isEmpty())
		return;

	// Push delete command (will move to recycle bin)
	_undoStack->push(new DeleteMeshCommand(this, _glWidget, uuidsToDelete));

	// Update UI
	updateControls();
}

#include "UVGenerationDialog.h"
void ModelViewer::generateUVsForSelectedItems()
{
	std::vector<int> selected = getSelectedIDs();
	if (selected.size() != 0)
	{
		QString error;
		UVGenerationDialog dialog(this);
		if (dialog.exec() == QDialog::Accepted)
		{
			// User clicked OK - get the selected method and config
			UVMethod method = dialog.getSelectedMethod();
			UVConfig config = dialog.getUVConfig();

			bool success = _glWidget->generateUVsForMeshes(selected, method, config, error);
			if (success)
			{
				MainWindow::showStatusMessage(QString("UVs generated using %1 method")
					.arg(dialog.getMethodName(method)));
			}
			else
			{
				QMessageBox::critical(this, "Error", "Failed to generate UVs.\n" + error);
			}
		}
	}
}

void ModelViewer::hideAllItems()
{
	// Hide all meshes (empty visibility set)
	QSet<QUuid> newVisible;  // Empty set

	// Apply visibility with undo support
	setVisibilityWithUndo(newVisible, tr("Hide All"));

	// Turn off swap visible if it was on
	if (_glWidget->isVisibleSwapped())
		_glWidget->swapVisible(false);
}

void ModelViewer::hideSelectedItems()
{
	if (!checkForActiveSelection())
		return;

	// Get current visibility
	QSet<QUuid> currentlyVisible = getVisibleUuids();

	// Get UUIDs of selected items to hide
	std::vector<int> selectedIds = getSelectedIDs();
	QSet<QUuid> toHide;
	for (int id : selectedIds)
	{
		QUuid uuid = _glWidget->getUuidByIndex(id);
		if (!uuid.isNull())
			toHide.insert(uuid);
	}

	// Calculate new visibility (remove selected from visible set)
	QSet<QUuid> newVisible = currentlyVisible - toHide;

	// Apply visibility with undo support
	setVisibilityWithUndo(newVisible, tr("Hide"));

	// Clear selection (preserve existing behavior)
	deselectAll();
}

void ModelViewer::showOnlySelectedItems()
{
	if (!checkForActiveSelection())
		return;

	// Get UUIDs of selected items - these will be the ONLY visible ones
	std::vector<int> selectedIds = getSelectedIDs();
	QSet<QUuid> newVisible;
	for (int id : selectedIds)
	{
		QUuid uuid = _glWidget->getUuidByIndex(id);
		if (!uuid.isNull())
			newVisible.insert(uuid);
	}

	// Apply visibility with undo support
	setVisibilityWithUndo(newVisible, tr("Show Only"));

	// Turn off swap visible if it was on
	if (_glWidget->isVisibleSwapped())
		_glWidget->swapVisible(false);
}

void ModelViewer::showAllItems()
{
	// Get all mesh UUIDs
	QSet<QUuid> newVisible;
	int meshCount = static_cast<int>(_glWidget->getMeshStore().size());

	for (int i = 0; i < meshCount; ++i)
	{
		QUuid uuid = _glWidget->getUuidByIndex(i);
		if (!uuid.isNull())
			newVisible.insert(uuid);
	}

	// Apply visibility with undo support
	setVisibilityWithUndo(newVisible, tr("Show All"));

	// Turn off swap visible if it was on
	if (_glWidget->isVisibleSwapped())
		_glWidget->swapVisible(false);
}

void ModelViewer::showSelectedItems()
{
	if (!checkForActiveSelection())
		return;

	// Get current visibility
	QSet<QUuid> currentlyVisible = getVisibleUuids();

	// Get UUIDs of selected items to show
	std::vector<int> selectedIds = getSelectedIDs();
	QSet<QUuid> toShow;
	for (int id : selectedIds)
	{
		QUuid uuid = _glWidget->getUuidByIndex(id);
		if (!uuid.isNull())
			toShow.insert(uuid);
	}

	// Calculate new visibility (add selected to visible set)
	QSet<QUuid> newVisible = currentlyVisible | toShow;

	// Apply visibility with undo support
	setVisibilityWithUndo(newVisible, tr("Show"));

	// Clear selection (preserve existing behavior)
	deselectAll();
}

bool ModelViewer::checkForActiveSelection()
{
	if (!hasSelection())
	{
		QMessageBox::information(this, tr("Selection Required"), tr("Please select an object first"));
		return false;
	}
	return true;
}

bool ModelViewer::hasSelection() const
{
	return treeWidgetModel->hasMeshSelection();
}

std::vector<int> ModelViewer::getSelectedIDs() const
{
	return treeWidgetModel->getSelectedIndices();
}

QSet<QUuid> ModelViewer::getSelectedUuids() const
{
	std::vector<int> selectedIds = getSelectedIDs();
	QSet<QUuid> selectedUuids;

	for (int id : selectedIds)
	{
		QUuid uuid = _glWidget->getUuidByIndex(id);
		if (!uuid.isNull())
			selectedUuids.insert(uuid);
	}

	return selectedUuids;
}

void ModelViewer::displaySelectedMeshInfo()
{
	std::vector<int> selected = getSelectedIDs();
	if (selected.size() != 0)
	{
		std::vector<TriangleMesh*> meshes = _glWidget->getMeshStore();
		QString name;
		size_t points = 0, triangles = 0;
		unsigned long long rawmem = 0;
		float surfArea = 0, volume = 0;
		QVector3D centerOfMass;
		float weight = 0, density = 0;
		TriangleMesh* mesh = nullptr;
		BoundingBox bbox;
		size_t selectionCount = selected.size();
		if (selectionCount > 1)
			name = QString("%1 Meshes\n").arg(selectionCount);
		else
			name = meshes.at(selected[0])->getName() + "\n";
		int meshCount = 0;
		for (int id : selected)
		{
			mesh = meshes.at(id);
			points += mesh->getPoints().size() / 3;
			triangles += mesh->getIndices().size() / 3;
			rawmem += mesh->memorySize();
			try
			{
				MeshProperties props(mesh);
				surfArea += props.surfaceArea();
				volume += props.volume();
				centerOfMass += props.centerOfMass() * props.weight();
				weight += props.weight();
				density = props.density();
				if (meshCount == 0)
					bbox = props.boundingBox();
				else
					bbox.addBox(props.boundingBox());
			}
			catch (const std::exception& ex)
			{
				std::cout << "Exception raised in ModelViewer::displaySelectedMeshInfo, Meshproperties" << ex.what() << std::endl;
			}
			meshCount++;
		}
		centerOfMass /= weight;

		QString strpoints = QString(tr("Points: %1\n")).arg(points);
		QString strtriangles = QString(tr("Triangles: %1\n")).arg(triangles);
		unsigned long long mem = 0;
		QString units;
		if (rawmem < 1024)
		{
			mem = rawmem;
			units = "bytes";
		}
		else if (rawmem < (1024 * 1024))
		{
			mem = rawmem / 1024;
			units = "kb";
		}
		else if (rawmem < (1024 * 1024 * 1024))
		{
			mem = rawmem / (1024 * 1024);
			units = "mb";
		}
		else
		{
			mem = rawmem / (1024 * 1024 * 1024);
			units = "gb";
		}
		QString meshSize = QString(tr("Memory: %1 ")).arg(mem) + units + "\n";
		QString meshProps;

		meshProps = QString(tr("Mesh Volume: %1mm^3\nSurface Area: %2mm^2\nDensity: %3kg/m^3\nWeight: %4kg\n")).arg(volume).arg(surfArea)
			.arg(density).arg(weight);

		meshProps += QString(tr("Mesh Center of Mass: X%1, Y%2, Z%3\n")).arg(centerOfMass.x()).arg(centerOfMass.y()).arg(centerOfMass.z());

		meshProps += QString(tr("Bounding Limits:\n\tXMin %1  XMax %2\n\tYMin %3  YMax %4\n\tZMin %5  ZMax %6\n"))
			.arg(bbox.xMin()).arg(bbox.xMax()).arg(bbox.yMin()).arg(bbox.yMax()).arg(bbox.zMin()).arg(bbox.zMax());

		meshProps += QString(tr("Bounding Size:\n\tX %1\n\tY %2\n\tZ %3"))
			.arg(fabs(bbox.xMax() - bbox.xMin())).arg(fabs(bbox.yMax() - bbox.yMin())).arg(fabs(bbox.zMax() - bbox.zMin()));

		QString info = name + strpoints + strtriangles + meshSize + meshProps;
		QMessageBox::information(this, tr("Mesh Info"), info);
	}
}

void ModelViewer::showVisualizationModelPage()
{
	// Material Settings tab now handles both ADS and PBR modes
	tabWidgetVizAttribs->setCurrentIndex(0);
}

void ModelViewer::showPredefinedMaterialsPage()
{
	tabWidgetVizAttribs->setCurrentIndex(0);
}

void ModelViewer::showTransformationsPage()
{
	tabWidgetVizAttribs->setCurrentIndex(1);
	updateTransformationValues();
}

void ModelViewer::showEnvironmentPage()
{
	tabWidgetVizAttribs->setCurrentIndex(2);
}

void ModelViewer::handleTreeWidgetVisibilityChanged()
{
	_visibleMeshUuids = treeWidgetModel->getVisibleUuids();
	applyVisibleMeshState(false);
}

void ModelViewer::handleTreeWidgetSelectionChanged()
{
	// Deselect everything in GLWidget first
	const auto& store = _glWidget->getMeshStore();
	for (size_t i = 0; i < store.size(); ++i)
		_glWidget->deselect(static_cast<int>(i));

	// Select the mesh-store indices of selected leaf items
	for (int idx : treeWidgetModel->getSelectedIndices())
		_glWidget->select(idx);

	_glWidget->update();
	updateSelectionStatusMessage();
}

void ModelViewer::handleTreeWidgetMeshRenamed(const QUuid& uuid, const QString& newName)
{
	TriangleMesh* mesh = _glWidget->getMeshByUuid(uuid);
	if (!mesh) return;

	// Capture old name before any mutation so the command can restore it.
	const QString oldName   = mesh->getName();
	const QString finalName = computeUniqueName(mesh, newName);

	// Nothing to do if the resolved name matches the current one.
	if (finalName == oldName) return;

	_undoStack->push(new RenameMeshCommand(
	    this, _glWidget, treeWidgetModel,
	    uuid, oldName, finalName,
	    tr("Rename \"%1\" to \"%2\"").arg(oldName, finalName)));
}

void ModelViewer::checkAndRenameModel(TriangleMesh* mesh, const QString& name)
{
	bool duplicate = false;
	QString finalName = name;
	int dupCnt = 1;
	std::vector<TriangleMesh*> meshes = _glWidget->getMeshStore();
	do
	{
		for (TriangleMesh* msh : meshes)
		{
			if (msh->getName() == finalName)
			{
				duplicate = true;
				finalName = QString("%1_%2").arg(name).arg(dupCnt);
				dupCnt++;
				break;
			}
			else
				duplicate = false;
		}
	} while (duplicate);
	mesh->setName(finalName);
	updateDisplayList();
}

QString ModelViewer::computeUniqueName(TriangleMesh* exclude, const QString& name) const
{
	// Return a version of 'name' that does not collide with any existing mesh
	// name, skipping 'exclude' (the mesh being renamed) so it doesn't conflict
	// with itself.  Appends _1, _2, … until a free slot is found.
	bool    duplicate = false;
	QString finalName = name;
	int     dupCnt    = 1;
	const std::vector<TriangleMesh*> meshes = _glWidget->getMeshStore();
	do
	{
		duplicate = false;
		for (TriangleMesh* msh : meshes)
		{
			if (msh == exclude) continue;
			if (msh->getName() == finalName)
			{
				duplicate = true;
				finalName = QString("%1_%2").arg(name).arg(dupCnt++);
				break;
			}
		}
	} while (duplicate);
	return finalName;
}

void ModelViewer::onFileImport()
{
	QFileDialog fileDialog(this, tr("Import Model File"), _lastOpenedDir);
	fileDialog.setFileMode(QFileDialog::ExistingFiles);
	QStringList supportedExtensions = ModelViewerApplication::supportedImportExtensions();
	fileDialog.setNameFilters(supportedExtensions);

	if (supportedExtensions.contains(_lastSelectedFilter))
	{
		fileDialog.selectNameFilter(_lastSelectedFilter);
	}

	// Run dialog
	QStringList fileNames;
	if (fileDialog.exec())
	{
		fileNames = fileDialog.selectedFiles();
		_lastSelectedFilter = fileDialog.selectedNameFilter();
		_lastOpenedDir = QFileInfo(fileNames.first()).absolutePath();
	}

	importFiles(fileNames);
}

void ModelViewer::importFiles(QStringList& fileNames)
{
	// Load selected files
	if (!fileNames.isEmpty())
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);
		for (const QString& fileName : std::as_const(fileNames))
		{
			loadFile(fileName);
		}
		_documentModified = true;
		_documentSaved = false;

		QApplication::restoreOverrideCursor();
		MainWindow::mainWindow()->activateWindow();
		QApplication::alert(MainWindow::mainWindow());
	}
}


#include "AssImpMeshExporter.h"
#include <AssImpMesh.h>
#include "SceneUtils.h"
void ModelViewer::onFileExport()
{
	Assimp::Exporter exporter;
	QStringList filters;
	QStringList allExtensions;
	QMap<QString, QString> filterToExtension; // Map filter -> extension

	// Build filters and track extensions
	for (unsigned int i = 0; i < exporter.GetExportFormatCount(); ++i)
	{
		const aiExportFormatDesc* desc = exporter.GetExportFormatDescription(i);
		QString ext = QString::fromUtf8(desc->fileExtension);
		QString descStr = QString::fromUtf8(desc->description);
		QString filter = QString("%1 (*.%2)").arg(descStr).arg(ext);

		filters.append(filter);
		allExtensions.append("*." + ext);
		filterToExtension[filter] = ext;
	}

	// All Supported Files filter
	QString allSupportedFilter = QString("All Supported Files (%1)").arg(allExtensions.join(' '));
	filters.prepend(allSupportedFilter);

	// Map the "All Supported Files" to empty extension (no default append)
	filterToExtension[allSupportedFilter] = "";

	QString selectedFilter;
	QString fileName = QFileDialog::getSaveFileName(this, tr("Export Model"), _lastOpenedDir, filters.join(";;"), &selectedFilter);

	if (!fileName.isEmpty())
	{
		QString extToAppend = filterToExtension[selectedFilter];

		// Append extension only if not present already
		if (!extToAppend.isEmpty() && !fileName.endsWith("." + extToAppend, Qt::CaseInsensitive))
		{
			fileName += "." + extToAppend;
		}

		// Export
		AssImpMeshExporter exporter(this);
		std::vector<TriangleMesh*> triMeshes = _glWidget->getMeshStore();
		std::vector<AssImpMesh*> assImpMeshes;
		for (TriangleMesh* triMesh : triMeshes)
			assImpMeshes.push_back(dynamic_cast<AssImpMesh*>(triMesh));

		// Check the user settings
		QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
		bool exportScene = settings.value("radioButtonExportScene", true).toBool();
				
		// Export a copy of the the original aiScene
		aiScene* copyScene = SceneUtils::deepCopyScene(_glWidget->getAssImpScene());

		// Apply inverse global transform to meshes before export, since we have applied 
		// auto scaling and rotation based on the up vector of the model and user settings.
		glm::mat4 transform = _glWidget->getGlobalSceneTransform();
		// Invert the transform
		glm::mat4 inverseTransform = glm::inverse(transform);
		// Apply to the root node of the scene, which will affect all meshes
		aiNode* node = copyScene->mRootNode;
		aiMatrix4x4 aiTransform = SceneUtils::glmToAiMatrix(inverseTransform);
		node->mTransformation = aiTransform * node->mTransformation;

		// Apply inverse transforms to the punctual lights as well
		std::vector<GPULight> lights = _glWidget->getParsedLights();
		for (GPULight& light : lights)
		{
			// Transform position (with translation)
			glm::vec4 transformedPos = inverseTransform * glm::vec4(light.position, 1.0f);
			light.position = glm::vec3(transformedPos);

			// Transform direction (no translation)
			glm::vec4 transformedDir = inverseTransform * glm::vec4(light.direction, 0.0f);
			light.direction = glm::normalize(glm::vec3(transformedDir));

			// Extract scale from transform matrix
			glm::vec3 scale(
				glm::length(glm::vec3(inverseTransform[0])),
				glm::length(glm::vec3(inverseTransform[1])),
				glm::length(glm::vec3(inverseTransform[2]))
			);
			float avgScale = (scale.x + scale.y + scale.z) / 3.0f;
			light.range *= avgScale;
		}

		// Export the meshes loaded in the scene
		AssImpMeshExporter::ExportSettings expSettings;
		expSettings.outputDirectory = QFileInfo(fileName).absolutePath();
		expSettings.copyTextures = true;
		expSettings.useRelativePaths = true;
		expSettings.deduplicateTextures = true;
		expSettings.verbose = true;		
		expSettings.lights = lights;

		aiReturn res = aiReturn_FAILURE;
		if (exportScene)
		{			
			res = exporter.exportScene(copyScene, triMeshes, fileName.toStdString(), expSettings);
			qDebug() << "Exporting scene result:" << res;
			delete copyScene;
		}
		else
		{			
			res = exporter.exportMeshes(copyScene, triMeshes, fileName, expSettings);
			qDebug() << "Exporting meshes result:" << res;
		}

		if (res == aiReturn_SUCCESS)
			QMessageBox::information(this, tr("Information"), tr("Exported %1").arg(QFileInfo(fileName).fileName()));
		else
			QMessageBox::critical(this, tr("Error"), tr("Export failed!"));
	}
}


bool ModelViewer::loadFile(const QString& fileName)
{
	_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time

	QString errMsg;
	bool success = false;
	const QString suffix = QFileInfo(fileName).suffix().toLower();
	const bool isNativeSession = (suffix == "mvf");
	if (isNativeSession)
	{
		// Load native ModelViewer session file
		success = loadFromFile(fileName);
	}
	else
	{
		UVMethod method;
		QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
		bool remember = settings.value("RememberUVMethod", false).toBool();
		if (remember)
		{
			int value = settings.value("UVMethod", static_cast<int>(UVMethod::None)).toInt();
			method = static_cast<UVMethod>(value);
		}
		else
			method = askUserForUVMethod(this).method;

		_progressiveLoadingEnabled = settings.value("checkProgressiveLoading", true).toBool();
		success = _glWidget->loadAssImpModel(fileName, method, errMsg, _progressiveLoadingEnabled);
	}

	if (success && !_glWidget->getMeshStore().empty())
	{
		if (!isNativeSession)
		{
			updateDisplayList();
			_documentModified = true;
			_documentSaved = false;
		}

		treeWidgetModel->scrollToTop();

		MainWindow::showStatusMessage(tr("File loaded"), 2000);

		return success;
	}
	else
	{
		if (errMsg == "Model loading cancelled by user.")
		{
			return false;
		}

		QApplication::restoreOverrideCursor();
		QMessageBox::critical(this, tr("Error"), QString(tr("Failed to load model %1")).arg(fileName) + "\n" + errMsg);
		QApplication::setOverrideCursor(Qt::WaitCursor);

		return false;
	}


	return false;
}

bool ModelViewer::loadFromFile(const QString& fileName)
{
	// -- Show progress bar (no cancel button for MVF) ------------------
	QString displayFileName = QFileInfo(fileName).fileName();
	MainWindow::showStatusMessage(tr("Reading file: ") + displayFileName);
	MainWindow::showProgressBar(false);
	MainWindow::setProgressValue(0);

	// Ensure the GL context / shader is ready before the worker starts.
	_glWidget->makeCurrent();
	if (!_glWidget->getShader())
	{
		_glWidget->update();
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		_glWidget->makeCurrent();
	}
	if (!_glWidget->getShader())
	{
		MainWindow::hideProgressBar();
		return false;
	}

	// ---------------------------------------------------------------
	// Single worker thread mirrors the AssImp pattern:
	//   • File I/O, JSON parse, vertex assembly run on the worker.
	//   • Each mesh's GL upload is dispatched to the main thread via
	//     BlockingQueuedConnection, so the main event loop stays
	//     fully alive between uploads — exactly like
	//     AssImpModelLoader::meshBatchReady → onMeshBatchReady.
	// ---------------------------------------------------------------
	struct LoadResult
	{
		Mvf::Document document;
		bool          ok       = false;
		bool          badMagic = false;
	};

	LoadResult result;
	QEventLoop waitLoop;
	QThread    workerThread;

	auto* worker = new QObject();
	worker->moveToThread(&workerThread);

	connect(&workerThread, &QThread::started, worker,
		[&result, &fileName, &waitLoop, this, &displayFileName, progressiveMode = _progressiveLoadingEnabled]()
	{
		// --- Phase 1: File I/O ----------------------------------------
		QFile file(fileName);
		if (!file.open(QIODevice::ReadOnly))
		{
			waitLoop.quit();
			return;
		}
		QDataStream in(&file);

		quint32 magic = 0;
		in >> magic;
		if (in.status() != QDataStream::Ok)       { waitLoop.quit(); return; }
		if (magic != Mvf::Magic)                   { result.badMagic = true; waitLoop.quit(); return; }

		file.seek(0);
		in.device()->seek(0);

		Mvf::Header header;
		if (!Mvf::readHeader(in, header) || !Mvf::isSupportedHeader(header))
		{
			waitLoop.quit();
			return;
		}

		QByteArray jsonPayload, geomChunk, imgChunk;
		while (in.status() == QDataStream::Ok && !in.atEnd())
		{
			Mvf::ChunkHeader chunkHeader;
			if (!Mvf::readChunkHeader(in, chunkHeader))
				break;
			const QByteArray chunkPayload = Mvf::readChunkPayload(in, chunkHeader);
			switch (chunkHeader.type)
			{
			case Mvf::ChunkType::Json:     jsonPayload = chunkPayload; break;
			case Mvf::ChunkType::Geometry: geomChunk   = chunkPayload; break;
			case Mvf::ChunkType::Images:   imgChunk    = chunkPayload; break;
			default: break;
			}
		}
		if (jsonPayload.isEmpty()) { waitLoop.quit(); return; }

		// --- Phase 2: JSON parse + vertex/material assembly (CPU) ------
		QMetaObject::invokeMethod(_glWidget, [&displayFileName]() {
			MainWindow::showStatusMessage(
				QObject::tr("Preparing meshes: ") + displayFileName);
			MainWindow::setProgressValue(10);
		}, Qt::BlockingQueuedConnection);

		result.document = Mvf::fromJsonBytes(jsonPayload);
		QVector<GLWidget::PreparedMvfMesh> prepared =
			GLWidget::prepareMvfMeshes(result.document, geomChunk, imgChunk);

		// Extract mesh UUIDs and visibility
		QList<QUuid> allMeshUuids;
		for (const auto& pm : prepared)
			allMeshUuids.append(pm.uuid);

		QSet<QUuid> visibleUuids;
		const QJsonArray visArr = result.document.mvfSession[QStringLiteral("visibleMeshUuids")].toArray();
		for (const QJsonValue& v : visArr)
			visibleUuids.insert(QUuid::fromString(v.toString()));

		// --- Phase 3: GL upload — dispatched one mesh at a time --------
		//     BlockingQueuedConnection blocks the worker while the main
		//     thread uploads, then returns control to waitLoop.exec()
		//     which processes ALL events (paint, timers, user input)
		//     before the next mesh is dispatched.

		// Clear old meshes and set visibility before uploading
		QMetaObject::invokeMethod(_glWidget, [this, &visibleUuids]() {
			_glWidget->clearMeshStore();
			_visibleMeshUuids = visibleUuids;
		}, Qt::BlockingQueuedConnection);

		const int totalMeshes = prepared.size();
		for (int i = 0; i < totalMeshes; ++i)
		{
			QMetaObject::invokeMethod(_glWidget,
				[this, &prepared, i, totalMeshes, &displayFileName, progressiveMode]()
			{
				const GLWidget::PreparedMvfMesh& pm = prepared[i];
				_glWidget->uploadOneMvfMesh(pm);

				const int pct = 15 + (i + 1) * 75 / totalMeshes;
				MainWindow::setProgressValue(pct);
				MainWindow::showStatusMessage(
					tr("Loading mesh %1 / %2").arg(i + 1).arg(totalMeshes));

				// In progressive mode, update display every 20 meshes (matching AssImp's
				// batchSize) so user sees meshes appearing as they load. In non-progressive
				// mode, defer until Phase 3.5 so all meshes appear together after loading completes.
				if (progressiveMode && ((i + 1) % 20 == 0 || (i + 1) == totalMeshes))
					updateDisplayList();
			}, Qt::BlockingQueuedConnection);
		}

		// --- Phase 3.5: Finalize session (still in event loop) ---
		//     Update display to show all pending meshes (either progressively
		//     during Phase 3, or all at once if non-progressive).
		QMetaObject::invokeMethod(this,
			[this, &result, &visibleUuids, &fileName]()
		{
			// Ensure all mesh UUIDs in _pendingSceneUuids are marked visible
			// In progressive mode, this was already called during Phase 3.
			// In non-progressive mode, this is the first call, so all meshes appear together.
			updateDisplayList();

			// Apply visibility
			_visibleMeshUuids = visibleUuids;
			const bool shouldAutoFit = checkBoxAutoFitView->isChecked();
			_glWidget->setAutoFitViewOnUpdate(false);
			_glWidget->setDisplayList(visibleIndicesFromState());
			_glWidget->setAutoFitViewOnUpdate(shouldAutoFit);
			updateVisibilityUiFromState();

			// Restore selection
			const QJsonArray selArr = result.document.mvfSession[QStringLiteral("selectedMeshUuids")].toArray();
			QSet<QUuid> selectedUuids;
			for (const QJsonValue& v : selArr)
				selectedUuids.insert(QUuid::fromString(v.toString()));
			setSelectionWithoutUndo(selectedUuids);

			// Clear undo/redo history
			if (_undoStack)
				_undoStack->clear();
			_cachedReferencedUuids.clear();

			_currentFile = fileName;
			_documentSaved = true;
			_documentModified = false;
			setWindowTitle(QFileInfo(fileName).fileName());

			MainWindow::setProgressValue(100);
		}, Qt::BlockingQueuedConnection);

		result.ok = true;
		waitLoop.quit();
	});

	workerThread.start();
	waitLoop.exec();          // main event loop fully alive the ENTIRE time

	workerThread.quit();
	workerThread.wait();
	delete worker;

	if (result.badMagic)
	{
		MainWindow::hideProgressBar();
		QMessageBox::critical(this, tr("Error"),
			tr("Unrecognized file format: %1").arg(fileName));
		return false;
	}
	if (!result.ok)
	{
		MainWindow::hideProgressBar();
		return false;
	}

	_glWidget->updateView();

	// --- Phase 4: Build tree structure (after event loop exits) ---
	//     All meshes are loaded and visible. Build the tree now.
	//     The logger can still output asynchronously in the background.
	//     For MVF files, reconstruct the original hierarchy from the saved node structure.
	const int sceneIndex = result.document.scene;
	const QJsonArray sceneRootNodes =
		result.document.scenes[sceneIndex][QStringLiteral("nodes")].toArray();
	_sceneGraph->rebuildFromMvf(result.document.nodes, sceneRootNodes);

	MainWindow::hideProgressBar();
	return true;
}

Mvf::MVFPackage ModelViewer::buildMVFPackage() const
{
	QSet<QUuid> selectedSet;
	for (const QUuid& uuid : treeWidgetModel->selectedMeshUuids())
		selectedSet.insert(uuid);

	return Mvf::buildMVFPackage(*_sceneGraph,
	                                _glWidget->getMeshStore(),
	                                _visibleMeshUuids,
	                                selectedSet);
}

bool ModelViewer::saveToFile(const QString& fileName)
{
	const Mvf::MVFPackage package = buildMVFPackage();
	const QByteArray jsonPayload = Mvf::toJsonBytes(package.document);
	const QByteArray& geometryPayload = package.geometryChunk;
	const QByteArray& imagePayload = package.imageChunk;

	Mvf::Header header;
	header.fileLength = static_cast<quint32>(
		sizeof(quint32) * 4 +
		sizeof(quint32) * 2 + jsonPayload.size() +
		sizeof(quint32) * 2 + geometryPayload.size() +
		sizeof(quint32) * 2 + imagePayload.size());

	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly))
		return false;

	QDataStream out(&file);
	if (!Mvf::writeHeader(out, header))
		return false;
	if (!Mvf::writeChunk(out, Mvf::ChunkType::Json, jsonPayload))
		return false;
	if (!Mvf::writeChunk(out, Mvf::ChunkType::Geometry, geometryPayload))
		return false;
	if (!Mvf::writeChunk(out, Mvf::ChunkType::Images, imagePayload))
		return false;

	return out.status() == QDataStream::Ok;
}

void ModelViewer::setMaterialToSelectedItems(const GLMaterial& mat)
{
	_material = mat;
	std::vector<int> ids = getSelectedIDs();
	_glWidget->setMaterialToObjects(ids, mat);
	_glWidget->updateView();
	updateControls();
}

void ModelViewer::setTexturesToSelectedItems(const GLMaterial& mat)
{
	if (checkForActiveSelection())
	{
		_material = mat;
		std::vector<int> ids = getSelectedIDs();
		_glWidget->setTexturesToObjects(ids, mat);
		_glWidget->updateView();
	}
}

void ModelViewer::setTextureSamplersToSelectedItems(const GLMaterial* material, GLMaterial::TextureType type)
{
	if (!_glWidget) return;
	_glWidget->synchronizeTextureCache(material, type);
}

void ModelViewer::on_checkBoxSelectAll_stateChanged(int arg1)
{
	if (arg1 != Qt::PartiallyChecked)
	{
		if (treeWidgetModel->meshCount() > 0)
		{
			Qt::CheckState cs = checkBoxSelectAll->checkState();
			QSet<QUuid> visibleUuids;
			if (cs == Qt::Checked)
			{
				// Make all meshes visible
				const auto& store = _glWidget->getMeshStore();
				for (size_t i = 0; i < store.size(); ++i)
				{
					QUuid uuid = _glWidget->getUuidByIndex(static_cast<int>(i));
					if (!uuid.isNull()) visibleUuids.insert(uuid);
				}
			}
			// If Unchecked, visibleUuids stays empty (hide all)
			_visibleMeshUuids = visibleUuids;
			applyVisibleMeshState(true, true);
		}
	}
	else
	{
		checkBoxSelectAll->setCheckState(Qt::Checked);
	}
}

void ModelViewer::on_tabWidgetVizAttribs_currentChanged(int index)
{
	if (index == 1) // Transformations tab
	{
		updateTransformationValues();
	}
}

void ModelViewer::switchToRealisticRendering()
{
	if (_glWidget->getDisplayMode() == DisplayMode::REALSHADED)
		return;
	_glWidget->setDisplayMode(DisplayMode::REALSHADED);
}

void ModelViewer::onDisplayModeChanged(int mode)
{
	visualizationEnvironmentPanel->onDisplayModeChanged(mode);
}

void ModelViewer::onRenderingModeSelected(const QString& mode)
{
	if (mode == "ADS")
	{
		_glWidget->setRenderingMode(RenderingMode::ADS_BLINN_PHONG);
		visualizationEnvironmentPanel->setPBRLightingMode(false);
	}
	else if (mode == "PBR")
	{
		_glWidget->setRenderingMode(RenderingMode::PHYSICALLY_BASED_RENDERING);
		visualizationEnvironmentPanel->setPBRLightingMode(true);
		_glWidget->setSkyBoxTextureHDRI(true);
		switchToRealisticRendering();
	}
	// Update toolbar button to reflect the new rendering mode
	_glWidget->getViewToolbar()->updateRenderingModeButton(mode);
	updateControls();
	_glWidget->update();
}

void ModelViewer::onTextureCacheCleared()
{
	if (_glWidget)
	{
		_glWidget->clearTextureCache();
	}
}

void ModelViewer::on_toolButtonClearOpacityTex_clicked()
{
	if (checkForActiveSelection())
	{
		std::vector<int> ids = getSelectedIDs();
		QApplication::setOverrideCursor(Qt::WaitCursor);
		_glWidget->clearADSOpacityTexMap(ids);
		_glWidget->updateView();
		QApplication::restoreOverrideCursor();
	}
}

void ModelViewer::onPredefinedMaterialSelected(const GLMaterial& mat)
{
	// This handler is triggered when MaterialLibraryWidget emits materialSelected,
	// but material application now goes through MaterialEditorPanel::onMaterialSelected
	// which processes textures before emitting materialApplied.
	// This prevents duplicate ApplyMaterialCommand creation and ensures proper texture handling.
	// So we do nothing here - let MaterialEditorPanel handle the processing and emission.
	Q_UNUSED(mat);
}

void ModelViewer::onCustomMaterialApplied(const GLMaterial& mat)
{
	if (!checkForActiveSelection())
		return;

	QApplication::setOverrideCursor(Qt::WaitCursor);

	QVector<QUuid> uuids;
	std::vector<int> ids = getSelectedIDs();
	for (int id : ids)
	{
		QUuid uuid = _glWidget->getUuidByIndex(id);
		if (!uuid.isNull())
			uuids.append(uuid);
	}

	QString materialName = "Custom Material";

	_undoStack->push(new ApplyMaterialCommand(
		this, _glWidget, uuids, mat, materialName
	));

	QApplication::restoreOverrideCursor();
}

void ModelViewer::onTexturesApplied(const GLMaterial* mat)
{
	if (!mat || !checkForActiveSelection())
		return;

	QApplication::setOverrideCursor(Qt::WaitCursor);

	QVector<QUuid> uuids;
	std::vector<int> ids = getSelectedIDs();
	for (int id : ids)
	{
		QUuid uuid = _glWidget->getUuidByIndex(id);
		if (!uuid.isNull())
			uuids.append(uuid);
	}

	_undoStack->push(new ApplyPBRTexturesCommand(
		this, _glWidget, uuids, *mat  // Dereference pointer
	));

	QApplication::restoreOverrideCursor();
}

UVDialogResult ModelViewer::askUserForUVMethod(QWidget* parent)
{
	UVDialogResult result;

	UVPromptDialog dialog(parent);

	if (dialog.exec() == QDialog::Accepted)
	{
		UVPromptDialog::Choice choice = dialog.selectedChoice();
		if (choice == UVPromptDialog::Choice::Planar)
		{
			result.method = UVMethod::Planar;
		}
		else if (choice == UVPromptDialog::Choice::Cylindrical)
		{
			result.method = UVMethod::Cylindrical;
		}
		else if (choice == UVPromptDialog::Choice::Spherical)
		{
			result.method = UVMethod::Spherical;
		}
		else if (choice == UVPromptDialog::Choice::Angular)
		{
			result.method = UVMethod::AngleBased;
		}
		else if (choice == UVPromptDialog::Choice::Hybrid)
		{
			result.method = UVMethod::Hybrid;
		}
		else if (choice == UVPromptDialog::Choice::Smart)
		{
			result.method = UVMethod::AngleBasedSmartUV;
		}
		else
		{
			result.method = UVMethod::None; // Skip UV generation			
		}
	}
	else
	{
		result.method = UVMethod::None; // User cancelled	
	}

	if (dialog.rememberChoiceChecked())
	{
		QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
		settings.setValue("RememberUVMethod", true);
		settings.setValue("UVMethod", static_cast<int>(result.method));
	}

	return result;
}

bool ModelViewer::hasUndo() const
{
	return _undoStack && _undoStack->canUndo();
}

bool ModelViewer::hasRedo() const
{
	return _undoStack && _undoStack->canRedo();
}

void ModelViewer::undo()
{
	if (_undoStack)
		_undoStack->undo();
}

void ModelViewer::redo()
{
	if (_undoStack)
		_undoStack->redo();
}

void ModelViewer::setSelectionWithUndo(const QSet<int>& newSelection)
{
	// Create and push the undo command
	// Note: push() automatically calls redo() on the command
	_undoStack->push(new SelectionCommand(this, _glWidget, newSelection));
}

void ModelViewer::setSelectionWithoutUndo(const QSet<int>& selection)
{
	treeWidgetModel->setSelectionByIndices(selection);
	handleTreeWidgetSelectionChanged();
}

void ModelViewer::setSelectionWithoutUndo(const QSet<QUuid>& uuids)
{
	treeWidgetModel->setSelectionByUuids(uuids);
	handleTreeWidgetSelectionChanged();
}

QSet<QUuid> ModelViewer::getVisibleUuids() const
{
	return _visibleMeshUuids;
}

void ModelViewer::setVisibilityWithUndo(const QSet<QUuid>& newVisibleUuids,
	const QString& commandText)
{
	// Create and push the undo command
	// Note: push() automatically calls redo() on the command
	_undoStack->push(new VisibilityCommand(this, _glWidget,
		newVisibleUuids, commandText));
}

void ModelViewer::setVisibilityWithoutUndo(const QSet<QUuid>& visibleUuids)
{
	QSet<QUuid> changedUuids = _visibleMeshUuids - visibleUuids;
	changedUuids.unite(visibleUuids - _visibleMeshUuids);
	_visibleMeshUuids = visibleUuids;
	applyVisibleMeshState(true, true, changedUuids);
}

QSet<QUuid> ModelViewer::collectVisibleUuidsFromDisplayList() const
{
	QSet<QUuid> visibleUuids;
	for (int id : _glWidget->getDisplayedObjectsIds())
	{
		QUuid uuid = _glWidget->getUuidByIndex(id);
		if (!uuid.isNull())
			visibleUuids.insert(uuid);
	}

	if (visibleUuids.isEmpty())
	{
		const auto& store = _glWidget->getMeshStore();
		for (size_t i = 0; i < store.size(); ++i)
		{
			QUuid uuid = _glWidget->getUuidByIndex(static_cast<int>(i));
			if (!uuid.isNull())
				visibleUuids.insert(uuid);
		}
	}

	return visibleUuids;
}

std::vector<int> ModelViewer::visibleIndicesFromState() const
{
	std::vector<int> ids;
	const auto& store = _glWidget->getMeshStore();
	ids.reserve(store.size());

	for (size_t i = 0; i < store.size(); ++i)
	{
		QUuid uuid = _glWidget->getUuidByIndex(static_cast<int>(i));
		if (_visibleMeshUuids.contains(uuid))
			ids.push_back(static_cast<int>(i));
	}

	return ids;
}

void ModelViewer::updateVisibilityUiFromState()
{
	checkBoxSelectAll->blockSignals(true);
	const int total = static_cast<int>(_glWidget->getMeshStore().size());
	const int visible = _visibleMeshUuids.size();
	if (visible == 0)
		checkBoxSelectAll->setCheckState(Qt::Unchecked);
	else if (visible == total)
		checkBoxSelectAll->setCheckState(Qt::Checked);
	else
		checkBoxSelectAll->setCheckState(Qt::PartiallyChecked);
	checkBoxSelectAll->blockSignals(false);

	float range = _glWidget->getBoundingSphere().getRadius() * 4.0f;
	float offset = _glWidget->getFloorSize() * 1.25f;
	visualizationEnvironmentPanel->updateLightPositionRanges(range, offset);
}

void ModelViewer::applyVisibleMeshState(bool syncTree,
                                        bool deferTreeSync,
                                        const QSet<QUuid>& changedUuids)
{
	if (!_glWidget)
		return;

	_glWidget->setDisplayList(visibleIndicesFromState());
	updateVisibilityUiFromState();

	if (syncTree)
	{
		constexpr int kTargetedTreeSyncThreshold = 128;
		const bool useTargetedSync =
			!changedUuids.isEmpty() &&
			changedUuids.size() <= kTargetedTreeSyncThreshold;

		if (useTargetedSync)
		{
			++_treeVisibilitySyncGeneration; // invalidate any pending full sync
			_treeVisibilityDirty = false;
			treeWidgetModel->setVisibilityDelta(changedUuids, _visibleMeshUuids);
		}
		else if (deferTreeSync)
		{
			_treeVisibilityDirty = true;
			scheduleTreeVisibilitySync();
		}
		else
		{
			_treeVisibilityDirty = false;
			syncTreeVisibilityFromModel();
		}
	}
}

void ModelViewer::scheduleTreeRebuild(int delayMs)
{
	const int generation = ++_treeRebuildGeneration;
	_treeRebuildPending = true;
	QPointer<ModelViewer> self(this);
	QTimer::singleShot(delayMs, this, [self, generation]() {
		if (!self)
			return;
		if (self->_treeRebuildGeneration != generation)
			return;
		self->rebuildTreeFromCurrentState();
	});
}

void ModelViewer::rebuildTreeFromCurrentState()
{
	_treeRebuildPending = false;
	treeWidgetModel->rebuild();
	syncTreeVisibilityFromModel();
}

void ModelViewer::scheduleTreeVisibilitySync(int delayMs)
{
	const int generation = ++_treeVisibilitySyncGeneration;
	const QSet<QUuid> snapshot = _visibleMeshUuids;
	QPointer<ModelViewer> self(this);
	QTimer::singleShot(delayMs, this, [self, generation, snapshot]() {
		if (!self)
			return;
		if (self->_treeVisibilitySyncGeneration != generation)
			return;
		if (self->_visibleMeshUuids != snapshot)
			return;
		self->syncTreeVisibilityFromModel();
		});
}

void ModelViewer::syncTreeVisibilityFromModel()
{
	_treeVisibilityDirty = false;
	treeWidgetModel->setVisibilityByUuids(_visibleMeshUuids);
}
