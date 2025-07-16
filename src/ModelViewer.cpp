#include <QApplication>
#include <QColorDialog>
#include <QFileDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QToolTip>
#include <assimp/Importer.hpp>
#include "AssImpModelLoader.h"
#include "ModelViewerApplication.h"
#include "MainWindow.h"
#include "ModelViewer.h"
#include "GLWidget.h"
#include "TriangleMesh.h"
#include "MeshProperties.h"

#include "config.h"

#include "LanguageManager.h"

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

	setAttribute(Qt::WA_DeleteOnClose);

	QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
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
	flayout->addWidget(_glWidget, 1);
			
	connect(checkBoxAutoFitView, SIGNAL(toggled(bool)), _glWidget, SLOT(setAutoFitViewOnUpdate(bool)));
	connect(_glWidget, SIGNAL(singleSelectionDone(int)), this, SLOT(setListRow(int)));
	connect(_glWidget, SIGNAL(sweepSelectionDone(QList<int>)), this, SLOT(setListRows(QList<int>)));
	connect(_glWidget, SIGNAL(floorShown(bool)), checkBoxFloor, SLOT(setChecked(bool)));
	
	listWidgetModel->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(listWidgetModel, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));

	// For item editing
	connect(listWidgetModel->itemDelegate(), SIGNAL(closeEditor(QWidget*, QAbstractItemDelegate::EndEditHint)),
		this, SLOT(itemEdited(QWidget*, QAbstractItemDelegate::EndEditHint)));


	connect(searchBox, &QLineEdit::textChanged, listWidgetModel, [&](const QString& text) {
		listWidgetModel->filterItems(text);

		// Optional: give visual feedback if no match
		bool anySelected = false;
		for (int i = 0; i < listWidgetModel->count(); ++i) {
			if (listWidgetModel->item(i)->isSelected()) {
				anySelected = true;
				break;
			}
		}

		searchBox->setStyleSheet(anySelected ? "" : "QLineEdit { border: 2px solid red; }");
		});

	connect(clearSearchBtn, &QPushButton::clicked, [&]() {
		// Clear the search box and reset the filter
		searchBox->clear();
		listWidgetModel->filterItems("");
		// Reset the style to default
		searchBox->setStyleSheet("");
		// Optionally, you can also clear the selection
		listWidgetModel->clearSelection();
		searchBox->clear();
		deselectAll();
		});

	connect(listWidgetModel, &ModelObjectList::selectionUpdated, this, &ModelViewer::on_listWidgetModel_itemSelectionChanged);


	QShortcut* shortcut = new QShortcut(QKeySequence(Qt::Key_Delete), listWidgetModel);
	connect(shortcut, SIGNAL(activated()), this, SLOT(deleteSelectedItems()));

	shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I), this);
	connect(shortcut, SIGNAL(activated()), this, SLOT(onFileImport()));

	shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E), this);
	connect(shortcut, SIGNAL(activated()), this, SLOT(onFileExport()));

	connect(checkBoxLockLightCamera, SIGNAL(toggled(bool)), _glWidget, SLOT(lockLightAndCamera(bool)));
	connect(doubleSpinBoxRepeatS, SIGNAL(valueChanged(double)), _glWidget, SLOT(setFloorTexRepeatS(double)));
	connect(doubleSpinBoxRepeatT, SIGNAL(valueChanged(double)), _glWidget, SLOT(setFloorTexRepeatT(double)));
	connect(doubleSpinBoxSkyBoxFOV, SIGNAL(valueChanged(double)), _glWidget, SLOT(setSkyBoxFOV(double)));
	connect(doubleSpinBoxFloorOffset, SIGNAL(valueChanged(double)), _glWidget, SLOT(setFloorOffsetPercent(double)));
	connect(checkBoxSkyBoxHDRI, SIGNAL(toggled(bool)), _glWidget, SLOT(setSkyBoxTextureHDRI(bool)));
	connect(checkBoxShowLights, SIGNAL(toggled(bool)), _glWidget, SLOT(showLights(bool)));

	connect(checkBoxHDRToneMapping, SIGNAL(toggled(bool)), _glWidget, SLOT(enableHDRToneMapping(bool)));
	connect(checkBoxGammaCorrection, SIGNAL(toggled(bool)), _glWidget, SLOT(enableGammaCorrection(bool)));
	connect(doubleSpinBoxScreenGamma, SIGNAL(valueChanged(double)), _glWidget, SLOT(setScreenGamma(double)));

	connect(buttonGroupLighting, SIGNAL(buttonToggled(QAbstractButton*, bool)), this, SLOT(lightingType_toggled(QAbstractButton*, bool)));
	toolBox->setItemEnabled(0, true);
	toolBox->setItemEnabled(1, false);
	toolBox->setItemEnabled(2, false);
	toolBox->setCurrentIndex(0);

	connect(sliderTransparencyPBR, SIGNAL(valueChanged(int)), this, SLOT(on_sliderTransparency_valueChanged(int)));
	connect(pushButtonDefaultMatlsPBR, SIGNAL(clicked()), this, SLOT(on_pushButtonDefaultMatls_clicked()));

	_hasADSDiffuseTex = false;
	_hasADSSpecularTex = false;
	_hasADSEmissiveTex = false;
	_hasADSNormalTex = false;
	_hasADSHeightTex = false;
	_hasADSOpacityTex = false;
	_hasPBRAlbedoTex = false;
	_hasPBRMetallicTex = false;
	_hasPBRRoughnessTex = false;
	_hasPBRAOTex = false;
	_hasPBROpacTex = false;
	_hasPBRNormalTex = false;
	_hasPBRHeightTex = false;
	_heightPBRTexScale = 0.05f;

	_progressiveLoadingEnabled = QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName()).value("checkProgressiveLoading", true).toBool();

	updateControls();

	connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this, [this]() {
		retranslateUi(this);
		retranslateUI();  // if needed
		});

}

ModelViewer::~ModelViewer()
{
	if (_glWidget)
	{
		delete _glWidget;
	}
}

void ModelViewer::retranslateUI()
{
	// Dynamically created QActions
	cameraModeOrbit->setText(tr("Orbit"));
	cameraModeFly->setText(tr("Fly"));
	cameraModeFirstPerson->setText(tr("First Person"));
	isometricView->setText(tr("Isometric"));
	dimetricView->setText(tr("Dimetric"));
	trimetricView->setText(tr("Trimetric"));
	displayShaded->setText(tr("Shaded"));
	displayWireframe->setText(tr("Wireframe"));
	displayWireShaded->setText(tr("Wire Shaded"));
	displayRealShaded->setText(tr("Realistic"));
}

void ModelViewer::deselectAll()
{
	bool oldState = listWidgetModel->blockSignals(true);
	QList<QListWidgetItem*> items = listWidgetModel->selectedItems();
	for (QListWidgetItem* item : items)
	{
		item->setSelected(false);
	}
	resetTransformationValues();
	listWidgetModel->blockSignals(oldState);
	on_listWidgetModel_itemSelectionChanged();
}

void ModelViewer::setListRow(int index)
{
	if (index != -1)
	{
		bool oldState = listWidgetModel->blockSignals(true);
		std::vector<TriangleMesh*> meshes = _glWidget->getMeshStore();
		TriangleMesh* mesh = meshes.at(index);
		QListWidgetItem* item = listWidgetModel->item(index);
		if (mesh->isSelected())
		{
			item->setSelected(false);
		}
		else
		{
			item->setSelected(true);
			listWidgetModel->setCurrentItem(item);
		}
		if (toolBox->currentIndex() == 4)
		{
			if (listWidgetModel->selectedItems().count() == 1)
				updateTransformationValues();
			else
				resetTransformationValues();
		}
		listWidgetModel->blockSignals(oldState);
		on_listWidgetModel_itemSelectionChanged();
	}
}

void ModelViewer::setListRows(QList<int> indices)
{
	if (indices.count())
	{
		bool oldState = listWidgetModel->blockSignals(true);
		for (int index : indices)
		{
			QListWidgetItem* item = listWidgetModel->item(index);
			item->setSelected(true);
		}
		listWidgetModel->blockSignals(oldState);
		on_listWidgetModel_itemSelectionChanged();
	}
}

void ModelViewer::setTransformation()
{
	if (checkForActiveSelection())
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);
		std::vector<int> ids = getSelectedIDs();
		QVector3D translate(doubleSpinBoxDX->value(), doubleSpinBoxDY->value(), doubleSpinBoxDZ->value());
		QVector3D rotate(doubleSpinBoxRX->value(), doubleSpinBoxRY->value(), doubleSpinBoxRZ->value());
		QVector3D scale(doubleSpinBoxSX->value(), doubleSpinBoxSY->value(), doubleSpinBoxSZ->value());
		_glWidget->setTransformation(ids, translate, rotate, scale);
		float range = _glWidget->getBoundingSphere().getRadius() * 4.0f;
		sliderLightPosX->setRange(-range, range);
		sliderLightPosY->setRange(-range, range);
		sliderLightPosZ->setRange(-range / 3, range / 2);
		sliderLightPosZ->setValue((-range / 3 + range / 2) / 2);
		QApplication::restoreOverrideCursor();
	}
}

void ModelViewer::resetTransformation()
{
	if (checkForActiveSelection())
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);
		std::vector<int> ids = getSelectedIDs();
		doubleSpinBoxDX->setValue(0.0f);
		doubleSpinBoxDY->setValue(0.0f);
		doubleSpinBoxDZ->setValue(0.0f);
		doubleSpinBoxRX->setValue(0.0f);
		doubleSpinBoxRY->setValue(0.0f);
		doubleSpinBoxRZ->setValue(0.0f);
		doubleSpinBoxSX->setValue(1.0f);
		doubleSpinBoxSY->setValue(1.0f);
		doubleSpinBoxSZ->setValue(1.0f);
		_glWidget->resetTransformation(ids);
		float range = _glWidget->getBoundingSphere().getRadius() * 4.0f;
		sliderLightPosX->setRange(-range, range);
		sliderLightPosY->setRange(-range, range);
		sliderLightPosZ->setRange(-range / 3, range / 2);
		sliderLightPosZ->setValue((-range / 3 + range / 2) / 2);
		QApplication::restoreOverrideCursor();
	}
}

void ModelViewer::updateTransformationValues()
{
	try
	{
		QList<QListWidgetItem*> selected = listWidgetModel->selectedItems();
		if (selected.count() > 0)
		{
			QListWidgetItem* item = selected.at(0);
			std::vector<TriangleMesh*> meshStore = _glWidget->getMeshStore();
			TriangleMesh* mesh = meshStore.at(listWidgetModel->row(item));
			if (mesh)
			{
				QVector3D trans = mesh->getTranslation();
				doubleSpinBoxDX->setValue(trans.x());
				doubleSpinBoxDY->setValue(trans.y());
				doubleSpinBoxDZ->setValue(trans.z());

				QVector3D rot = mesh->getRotation();
				doubleSpinBoxRX->setValue(rot.x());
				doubleSpinBoxRY->setValue(rot.y());
				doubleSpinBoxRZ->setValue(rot.z());

				QVector3D scale = mesh->getScaling();
				doubleSpinBoxSX->setValue(scale.x());
				doubleSpinBoxSY->setValue(scale.y());
				doubleSpinBoxSZ->setValue(scale.z());
			}
		}
	}
	catch (const std::exception& ex)
	{
		std::cout << "Exception raised in ModelViewer::on_toolBox_currentChanged\n" << ex.what() << std::endl;
	}
}

void ModelViewer::resetTransformationValues()
{
	doubleSpinBoxDX->setValue(0.0f);
	doubleSpinBoxDY->setValue(0.0f);
	doubleSpinBoxDZ->setValue(0.0f);

	doubleSpinBoxRX->setValue(0.0f);
	doubleSpinBoxRY->setValue(0.0f);
	doubleSpinBoxRZ->setValue(0.0f);

	doubleSpinBoxSX->setValue(1.0f);
	doubleSpinBoxSY->setValue(1.0f);
	doubleSpinBoxSZ->setValue(1.0f);
}

void ModelViewer::updateControls()
{
	sliderShine->blockSignals(true);
	sliderTransparency->blockSignals(true);
	sliderMetallic->blockSignals(true);
	sliderRoughness->blockSignals(true);
	sliderTransparencyPBR->blockSignals(true);

	QColor col;
	QString qss;
	QVector4D ambientLight = _glWidget->getAmbientLight();
	col.setRgbF(ambientLight.x(), ambientLight.y(), ambientLight.z());
	qss = QString("background-color: %1;color: %2").arg(col.name(), col.lightness() < 75 ? QColor(Qt::white).name() : QColor(Qt::black).name());
	pushButtonLightAmbient->setStyleSheet(qss);

	QVector4D diffuseLight = _glWidget->getDiffuseLight();
	col.setRgbF(diffuseLight.x(), diffuseLight.y(), diffuseLight.z());
	qss = QString("background-color: %1;color: %2").arg(col.name(), col.lightness() < 75 ? QColor(Qt::white).name() : QColor(Qt::black).name());
	pushButtonLightDiffuse->setStyleSheet(qss);

	QVector4D specularLight = _glWidget->getSpecularLight();
	col.setRgbF(specularLight.x(), specularLight.y(), specularLight.z());
	qss = QString("background-color: %1;color: %2").arg(col.name(), col.lightness() < 75 ? QColor(Qt::white).name() : QColor(Qt::black).name());
	pushButtonLightSpecular->setStyleSheet(qss);
	// ADS Lighting
	if (radioButtonADSL->isChecked())
	{
		sliderShine->setValue((int)_material.shininess());
		sliderTransparency->setValue((int)(1000 * _material.opacity()));

		col.setRgbF(_material.ambient().x(), _material.ambient().y(), _material.ambient().z());
		qss = QString("background-color: %1;color: %2").arg(col.name(), col.lightness() < 75 ? QColor(Qt::white).name() : QColor(Qt::black).name());
		pushButtonMaterialAmbient->setStyleSheet(qss);

		col.setRgbF(_material.diffuse().x(), _material.diffuse().y(), _material.diffuse().z());
		qss = QString("background-color: %1;color: %2").arg(col.name(), col.lightness() < 75 ? QColor(Qt::white).name() : QColor(Qt::black).name());
		pushButtonMaterialDiffuse->setStyleSheet(qss);

		col.setRgbF(_material.specular().x(), _material.specular().y(), _material.specular().z());
		qss = QString("background-color: %1;color: %2").arg(col.name(), col.lightness() < 75 ? QColor(Qt::white).name() : QColor(Qt::black).name());
		pushButtonMaterialSpecular->setStyleSheet(qss);

		col.setRgbF(_material.emissive().x(), _material.emissive().y(), _material.emissive().z());
		qss = QString("background-color: %1;color: %2").arg(col.name(), col.lightness() < 75 ? QColor(Qt::white).name() : QColor(Qt::black).name());
		pushButtonMaterialEmissive->setStyleSheet(qss);
	}
	// PBR Direct Lighting
	if (radioButtonDLPBR->isChecked())
	{
		col.setRgbF(_material.albedoColor().x(), _material.albedoColor().y(), _material.albedoColor().z());
		qss = QString("background-color: %1;color: %2").arg(col.name(), col.lightness() < 75 ? QColor(Qt::white).name() : QColor(Qt::black).name());
		pushButtonAlbedoColor->setStyleSheet(qss);
		sliderMetallic->setValue((int)(_material.metalness() * 1000));
		sliderRoughness->setValue((int)(_material.roughness() * 1000));
		sliderTransparencyPBR->setValue((int)(_material.opacity() * 1000));
	}
	sliderShine->blockSignals(false);
	sliderTransparency->blockSignals(false);
	sliderMetallic->blockSignals(false);
	sliderRoughness->blockSignals(false);
	sliderTransparencyPBR->blockSignals(false);
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

void ModelViewer::updateDisplayList()
{
	if(_glWidget->getMeshStore().empty())
	{
		listWidgetModel->clear();
		return;
	}	
	QApplication::setOverrideCursor(Qt::WaitCursor);
	listWidgetModel->clear();
	std::vector<TriangleMesh*> store = _glWidget->getMeshStore();
	std::vector<int> ids = _glWidget->getDisplayedObjectsIds();
	int id = 0;
	QListWidgetItem* item = nullptr;
	bool oldState = listWidgetModel->blockSignals(true);
	for (TriangleMesh* mesh : store)
	{
		item = new QListWidgetItem(mesh->getName());
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable); // set checkable flag
		// AND initialize check state
		if (std::count(ids.begin(), ids.end(), id))
			item->setCheckState(Qt::Checked);
		else
			item->setCheckState(Qt::Unchecked);
		listWidgetModel->addItem(item);
		id++;
	}

	QApplication::restoreOverrideCursor();

	listWidgetModel->blockSignals(oldState);
	on_listWidgetModel_itemChanged(item);
}

void ModelViewer::updateSelectionStatusMessage()
{
	int count = listWidgetModel->selectedItems().count();
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
	if (listWidgetModel->count())
	{
		bool oldState = listWidgetModel->blockSignals(true);
		for (int i = 0; i < listWidgetModel->count(); i++)
		{
			QListWidgetItem* item = listWidgetModel->item(i);
			if (item->checkState() == (_glWidget->isVisibleSwapped() ? Qt::Unchecked : Qt::Checked))
			{
				item->setSelected(true);
			}
		}
		listWidgetModel->blockSignals(oldState);
		on_listWidgetModel_itemSelectionChanged();
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
			if(extn == "mvf")
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
				if(!success)
				{
					QMessageBox::critical(this, tr("Error"), tr("Failed to load model: ") + fileName + "\n" + errMsg);
					continue;
				}
			}

			updateDisplayList();

			listWidgetModel->setCurrentRow(listWidgetModel->count() - 1);
			listWidgetModel->currentItem()->setCheckState(Qt::Checked);

			updateDisplayList();
		}
	}
	QApplication::restoreOverrideCursor();
}

void ModelViewer::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
}

void ModelViewer::mouseMoveEvent(QMouseEvent* event)
{
	QWidget::mouseMoveEvent(event);
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

bool ModelViewer::hasUndo()
{
	return false;
}

bool ModelViewer::hasRedo()
{
	return false;
}


void ModelViewer::setDocumentModified(bool modified)
{
	_documentModified = modified;
	if (modified) {
		setWindowTitle(tr("%1*").arg(QFileInfo(_currentFile).fileName()));
	} else {
		setWindowTitle(tr("%1").arg(QFileInfo(_currentFile).fileName()));
	}
}

bool ModelViewer::save()
{
	if (_currentFile.isEmpty()) {
		return saveAs();
	}

	if (saveToFile(_currentFile)) {
		_documentSaved = true;
		_documentModified = false;
		MainWindow::showStatusMessage(tr("File saved"), 2000);
		setWindowTitle(tr("%1").arg(QFileInfo(_currentFile).fileName()));
		return true;
	}
	else {
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
	if (saved) {
		setWindowTitle(tr("%1").arg(QFileInfo(_currentFile).fileName()));
	} else {
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
	if (listWidgetModel->selectedItems().count() != 0)
	{
		// Create menu and insert some actions
		QMenu myMenu;

		myMenu.addAction(tr("Center Screen"), this, SLOT(centerScreen()));
		myMenu.addAction(tr("Visualization Settings"), this, SLOT(showVisualizationModelPage()));
		myMenu.addAction(tr("Transformations"), this, SLOT(showTransformationsPage()));
		myMenu.addAction(tr("Hide"), this, SLOT(hideSelectedItems()));
		myMenu.addAction(tr("Show"), this, SLOT(showSelectedItems()));
		myMenu.addAction(tr("Show Only"), this, SLOT(showOnlySelectedItems()));
		myMenu.addAction(tr("Duplicate"), this, SLOT(duplicateSelectedItems()));
		myMenu.addAction(tr("Delete"), this, SLOT(deleteSelectedItems()));
		myMenu.addAction(tr("Mesh Info"), this, SLOT(displaySelectedMeshInfo()));

		// Show context menu at handling position
		myMenu.exec(listWidgetModel->mapToGlobal(pos));
	}
}

void ModelViewer::centerScreen()
{
	std::vector<int> selectedIDs = getSelectedIDs();
	_glWidget->centerScreen(selectedIDs);
}

void ModelViewer::duplicateSelectedItems()
{
	QList<QListWidgetItem*> selectedItems = listWidgetModel->selectedItems();
	if (!selectedItems.isEmpty())
	{
		if (QMessageBox::question(this, tr("Confirmation"), tr("Duplicate selection?")) == QMessageBox::Yes)
		{
			QApplication::setOverrideCursor(Qt::WaitCursor);
			std::vector<int> ids = getSelectedIDs();
			_glWidget->duplicateObjects(ids);
			updateDisplayList();

			listWidgetModel->setCurrentRow(listWidgetModel->count() - 1);
			listWidgetModel->currentItem()->setCheckState(Qt::Checked);

			updateDisplayList();
			QApplication::restoreOverrideCursor();
		}
	}
}

void ModelViewer::deleteSelectedItems()
{
	QList<QListWidgetItem*> selectedItems = listWidgetModel->selectedItems();
	if (!selectedItems.isEmpty())
	{
		if (QMessageBox::question(this, tr("Confirmation"), tr("Delete selection?")) == QMessageBox::Yes)
		{
			QApplication::setOverrideCursor(Qt::WaitCursor);
			hideSelectedItems();
			bool oldState = listWidgetModel->blockSignals(true);
			int rowId = 0;
			for (QListWidgetItem* item : selectedItems)
			{
				rowId = listWidgetModel->row(item);

				// Remove the displayed object
				_glWidget->removeFromDisplay(rowId);

				// Get curent item on selected row
				QListWidgetItem* curItem = listWidgetModel->takeItem(rowId);
				// And remove it
				delete curItem;
			}
			listWidgetModel->blockSignals(oldState);
			if (listWidgetModel->count())
			{
				listWidgetModel->setCurrentRow(rowId, QItemSelectionModel::Clear);
				QListWidgetItem* item = listWidgetModel->item(rowId);
				on_listWidgetModel_itemChanged(item);
			}
			_glWidget->update();
			QApplication::restoreOverrideCursor();
		}
	}
}

void ModelViewer::hideAllItems()
{
	bool oldState = listWidgetModel->blockSignals(true);
	for (int i = 0; i < listWidgetModel->count(); i++)
	{
		QListWidgetItem* item = listWidgetModel->item(i);
		item->setCheckState(Qt::Unchecked);
	}
	listWidgetModel->blockSignals(oldState);
	on_listWidgetModel_itemChanged(nullptr);
	if (_glWidget->isVisibleSwapped())
		_glWidget->swapVisible(false);
}

void ModelViewer::hideSelectedItems()
{
	bool oldState = listWidgetModel->blockSignals(true);
	QList<QListWidgetItem*> selectedItems = listWidgetModel->selectedItems();
	for (QListWidgetItem* item : selectedItems)
	{
		item->setCheckState(Qt::Unchecked);
		item->setSelected(false);
	}
	listWidgetModel->blockSignals(oldState);
	on_listWidgetModel_itemChanged(nullptr);
	deselectAll();
}

void ModelViewer::showOnlySelectedItems()
{
	bool oldState = listWidgetModel->blockSignals(true);
	for (int i = 0; i < listWidgetModel->count(); i++)
	{
		QListWidgetItem* item = listWidgetModel->item(i);
		if (item->isSelected())
		{
			item->setCheckState(Qt::Checked);
		}
		else
		{
			item->setCheckState(Qt::Unchecked);
		}
	}
	listWidgetModel->blockSignals(oldState);
	on_listWidgetModel_itemChanged(nullptr);

	if (_glWidget->isVisibleSwapped())
		_glWidget->swapVisible(false);
}

void ModelViewer::showAllItems()
{
	bool oldState = listWidgetModel->blockSignals(true);
	for (int i = 0; i < listWidgetModel->count(); i++)
	{
		QListWidgetItem* item = listWidgetModel->item(i);
		item->setCheckState(Qt::Checked);
	}
	listWidgetModel->blockSignals(oldState);
	on_listWidgetModel_itemChanged(nullptr);
	if (_glWidget->isVisibleSwapped())
		_glWidget->swapVisible(false);
}

void ModelViewer::showSelectedItems()
{
	bool oldState = listWidgetModel->blockSignals(true);
	QList<QListWidgetItem*> selectedItems = listWidgetModel->selectedItems();
	for (QListWidgetItem* item : selectedItems)
	{
		item->setCheckState(Qt::Checked);
		item->setSelected(false);
	}
	if (_glWidget->isVisibleSwapped())
		_glWidget->swapVisible(false);
	listWidgetModel->blockSignals(oldState);
	on_listWidgetModel_itemChanged(nullptr);
	deselectAll();
}

bool ModelViewer::checkForActiveSelection()
{
	if (listWidgetModel->selectedItems().isEmpty())
	{
		QMessageBox::information(this, tr("Selection Required"), tr("Please select an object first"));
		return false;
	}
	return true;
}

std::vector<int> ModelViewer::getSelectedIDs() const
{
	std::vector<int> ids;
	QList<QListWidgetItem*> items = listWidgetModel->selectedItems();
	for (QListWidgetItem* i : items)
	{
		int rowId = listWidgetModel->row(i);
		ids.push_back(rowId);
	}
	return ids;
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
	if (radioButtonADSL->isChecked())
	{
		toolBox->setCurrentIndex(0);
	}
	if (radioButtonDLPBR->isChecked())
	{
		toolBox->setCurrentIndex(1);
	}
	if (radioButtonTXPBR->isChecked())
	{
		toolBox->setCurrentIndex(2);
	}
}

void ModelViewer::showPredefinedMaterialsPage()
{
	toolBox->setCurrentIndex(3);
}

void ModelViewer::showTransformationsPage()
{
	toolBox->setCurrentIndex(4);
	updateTransformationValues();
}

void ModelViewer::showEnvironmentPage()
{
	toolBox->setCurrentIndex(5);
}


void ModelViewer::on_checkTexture_toggled(bool checked)
{
	_bHasTexture = checked;
	if (checkForActiveSelection())
	{
		std::vector<TriangleMesh*> meshes = _glWidget->getMeshStore();
		QList<QListWidgetItem*> items = listWidgetModel->selectedItems();
		for (QListWidgetItem* i : items)
		{
			int rowId = listWidgetModel->row(i);
			TriangleMesh* mesh = meshes.at(rowId);
			if (mesh)
			{
				mesh->enableTexture(_bHasTexture);
			}
		}
		_glWidget->updateView();
	}
	else
	{
		checkTexture->blockSignals(true);
		checkTexture->setChecked(!checked);
		pushButtonTexture->setEnabled(!checked);
		checkTexture->blockSignals(false);
	}
}

void ModelViewer::on_pushButtonTexture_clicked()
{
	if (checkForActiveSelection())
	{
		QImage buf;
		QString filter = getSupportedQtImagesFilter();
		QString fileName = QFileDialog::getOpenFileName(
			this,
			"Choose an image for texture",
			_lastOpenedDir,
			filter);
		_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time
		if (fileName != "")
		{
			if (!buf.load(fileName))
			{ // Load first image from file
				qWarning("ModelViewer::on_pushButtonTexture_clicked - Could not read image file, using single-color instead.");
				QImage dummy(128, 128, QImage::Format_ARGB32);
				dummy.fill(1);
				buf = dummy;
			}

			std::vector<int> ids = getSelectedIDs();
			_glWidget->setTexture(ids, buf);
			_glWidget->updateView();
		}
	}
}

void ModelViewer::on_pushButtonDefaultLights_clicked()
{
	_glWidget->setAmbientLight({ 0.0f, 0.0f, 0.0f, 1.0f });
	_glWidget->setDiffuseLight({ 1.0f, 1.0f, 1.0f, 1.0f });
	_glWidget->setSpecularLight({ 0.5f, 0.5f, 0.5f, 1.0f });

	sliderLightPosX->setValue(0);
	sliderLightPosY->setValue(0);

	float range = _glWidget->getBoundingSphere().getRadius() * 4.0f;	
	sliderLightPosZ->setValue((-range / 3 + range / 2) / 2);

	_glWidget->updateView();
	updateControls();
}

void ModelViewer::on_pushButtonApplyADSColors_clicked()
{
	setMaterialToSelectedItems(_material);
	_glWidget->updateView();
	updateControls();
}

void ModelViewer::on_pushButtonDefaultMatls_clicked()
{
	if (checkForActiveSelection())
	{
		_material.setOpacity(1.0f);
		setMaterialToSelectedItems(GLMaterial::DEFAULT_MAT());
		_glWidget->updateView();
		updateControls();
	}
}

void ModelViewer::on_pushButtonApplyTransformations_clicked()
{
	setTransformation();
	_glWidget->update();
}

void ModelViewer::on_pushButtonResetTransformations_clicked()
{
	resetTransformation();
	_glWidget->update();
}

void ModelViewer::on_pushButtonLightAmbient_clicked()
{
	QVector4D ambientLight = _glWidget->getAmbientLight();
	QColor c = QColorDialog::getColor(QColor::fromRgbF(ambientLight.x(), ambientLight.y(), ambientLight.z()), this, "Ambient Light Color");
	if (c.isValid())
	{
		_glWidget->setAmbientLight({ static_cast<float>(c.redF()),
									 static_cast<float>(c.greenF()),
									 static_cast<float>(c.blueF()),
									 static_cast<float>(c.alphaF()) });
		updateControls();
		_glWidget->updateView();
	}
}

void ModelViewer::on_pushButtonLightDiffuse_clicked()
{
	QVector4D diffuseLight = _glWidget->getDiffuseLight();
	QColor c = QColorDialog::getColor(QColor::fromRgbF(diffuseLight.x(), diffuseLight.y(), diffuseLight.z()), this, "Diffuse Light Color");
	if (c.isValid())
	{
		_glWidget->setDiffuseLight({ static_cast<float>(c.redF()),
									 static_cast<float>(c.greenF()),
									 static_cast<float>(c.blueF()),
									 static_cast<float>(c.alphaF()) });
		updateControls();
		_glWidget->updateView();
	}
}

void ModelViewer::on_pushButtonLightSpecular_clicked()
{
	QVector4D specularLight = _glWidget->getSpecularLight();
	QColor c = QColorDialog::getColor(QColor::fromRgbF(specularLight.x(), specularLight.y(), specularLight.z()), this, "Specular Light Color");
	if (c.isValid())
	{
		_glWidget->setSpecularLight({ static_cast<float>(c.redF()),
									  static_cast<float>(c.greenF()),
									  static_cast<float>(c.blueF()),
									  static_cast<float>(c.alphaF()) });
		updateControls();
		_glWidget->updateView();
	}
}

void ModelViewer::on_pushButtonMaterialAmbient_clicked()
{
	if (checkForActiveSelection())
	{
		QColor c = QColorDialog::getColor(QColor::fromRgbF(_material.ambient().x(), _material.ambient().y(), _material.ambient().z()), this, "Ambient Material Color");
		if (c.isValid())
		{
			_material.setAmbient(QVector3D(
				static_cast<float>(c.redF()),
				static_cast<float>(c.greenF()),
				static_cast<float>(c.blueF()))
			);
			setMaterialToSelectedItems(_material);

			updateControls();
			_glWidget->updateView();
		}
	}
}

void ModelViewer::on_pushButtonMaterialDiffuse_clicked()
{
	if (checkForActiveSelection())
	{
		QColor c = QColorDialog::getColor(QColor::fromRgbF(_material.diffuse().x(), _material.diffuse().y(), _material.diffuse().z()), this, "Diffuse Material Color");
		if (c.isValid())
		{
			_material.setDiffuse(QVector3D(
				static_cast<float>(c.redF()),
				static_cast<float>(c.greenF()),
				static_cast<float>(c.blueF()))
			);
			setMaterialToSelectedItems(_material);

			updateControls();
			_glWidget->updateView();
		}
	}
}

void ModelViewer::on_pushButtonMaterialSpecular_clicked()
{
	if (checkForActiveSelection())
	{
		QColor c = QColorDialog::getColor(QColor::fromRgbF(_material.specular().x(), _material.specular().y(), _material.specular().z()), this, "Specular Material Color");
		if (c.isValid())
		{
			_material.setSpecular(QVector3D(
				static_cast<float>(c.redF()),
				static_cast<float>(c.greenF()),
				static_cast<float>(c.blueF()))
			);
			setMaterialToSelectedItems(_material);

			updateControls();
			_glWidget->updateView();
		}
	}
}

void ModelViewer::on_pushButtonMaterialEmissive_clicked()
{
	if (checkForActiveSelection())
	{
		QColor c = QColorDialog::getColor(QColor::fromRgbF(_material.emissive().x(), _material.emissive().y(), _material.emissive().z()), this, "Emissive Material Color");
		if (c.isValid())
		{
			_material.setEmissive(QVector3D(
				static_cast<float>(c.redF()),
				static_cast<float>(c.greenF()),
				static_cast<float>(c.blueF()))
			);
			setMaterialToSelectedItems(_material);

			updateControls();
			_glWidget->updateView();
		}
	}
}

void ModelViewer::on_sliderLightPosX_valueChanged(int)
{
	_glWidget->setLightOffset(QVector3D(static_cast<float>(sliderLightPosX->value()),
		static_cast<float>(sliderLightPosY->value()),
		static_cast<float>(sliderLightPosZ->value())));
	_glWidget->updateView();
}

void ModelViewer::on_sliderLightPosY_valueChanged(int)
{
	_glWidget->setLightOffset(QVector3D(static_cast<float>(sliderLightPosX->value()),
		static_cast<float>(sliderLightPosY->value()),
		static_cast<float>(sliderLightPosZ->value())));
	_glWidget->updateView();
}

void ModelViewer::on_sliderLightPosZ_valueChanged(int)
{
	_glWidget->setLightOffset(QVector3D(static_cast<float>(sliderLightPosX->value()),
		static_cast<float>(sliderLightPosY->value()),
		static_cast<float>(sliderLightPosZ->value())));
	_glWidget->updateView();
}

void ModelViewer::on_sliderTransparency_valueChanged(int value)
{
	if (checkForActiveSelection())
	{
		_material.setOpacity((float)value / 1000.0f);
		for (int id : getSelectedIDs())
		{
			TriangleMesh* mesh = _glWidget->getMeshStore().at(id);
			if (mesh)
			{
				mesh->setOpacity((float)value / 1000.0f);
			}
		}

		_glWidget->updateView();
	}
}

void ModelViewer::on_sliderShine_valueChanged(int value)
{
	if (checkForActiveSelection())
	{
		_material.setShininess(value);
		for (int id : getSelectedIDs())
		{
			TriangleMesh* mesh = _glWidget->getMeshStore().at(id);
			if (mesh)
			{
				mesh->setShininess(value);
			}
		}

		_glWidget->updateView();
	}
}

void ModelViewer::on_pushButtonBrass_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::BRASS());
	}
}

void ModelViewer::on_pushButtonBronze_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::BRONZE());
	}
}

void ModelViewer::on_pushButtonCopper_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::COPPER());
	}
}

void ModelViewer::on_pushButtonGold_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::GOLD());
	}
}

void ModelViewer::on_pushButtonSilver_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::SILVER());
	}
}

void ModelViewer::on_pushButtonChrome_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::CHROME());
	}
}

void ModelViewer::on_pushButtonRuby_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::RUBY());
	}
}

void ModelViewer::on_pushButtonEmerald_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::EMERALD());
	}
}

void ModelViewer::on_pushButtonTurquoise_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::TURQUOISE());
	}
}

void ModelViewer::on_pushButtonJade_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::JADE());
	}
}

void ModelViewer::on_pushButtonObsidian_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::OBSIDIAN());
	}
}

void ModelViewer::on_pushButtonPearl_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::PEARL());
	}
}

void ModelViewer::on_pushButtonBlackPlastic_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::BLACK_PLASTIC());
	}
}

void ModelViewer::on_pushButtonCyanPlastic_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::CYAN_PLASTIC());
	}
}

void ModelViewer::on_pushButtonGreenPlastic_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::GREEN_PLASTIC());
	}
}

void ModelViewer::on_pushButtonRedPlastic_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::RED_PLASTIC());
	}
}

void ModelViewer::on_pushButtonWhitePlastic_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::WHITE_PLASTIC());
	}
}

void ModelViewer::on_pushButtonYellowPlastic_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::YELLOW_PLASTIC());
	}
}

void ModelViewer::on_pushButtonBlackRubber_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::BLACK_RUBBER());
	}
}

void ModelViewer::on_pushButtonCyanRubber_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::CYAN_RUBBER());
	}
}

void ModelViewer::on_pushButtonGreenRubber_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::GREEN_RUBBER());
	}
}

void ModelViewer::on_pushButtonRedRubber_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::RED_RUBBER());
	}
}

void ModelViewer::on_pushButtonWhiteRubber_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::WHITE_RUBBER());
	}
}

void ModelViewer::on_pushButtonYellowRubber_clicked()
{
	if (checkForActiveSelection())
	{
		setMaterialToSelectedItems(GLMaterial::YELLOW_RUBBER());
	}
}

void ModelViewer::on_listWidgetModel_itemChanged(QListWidgetItem* item)
{
	if (listWidgetModel->count())
	{
		std::vector<int> ids;
		for (int i = 0; i < listWidgetModel->count(); i++)
		{
			QListWidgetItem* item = listWidgetModel->item(i);
			if (item->checkState() == Qt::Checked)
			{
				int rowId = listWidgetModel->row(item);
				ids.push_back(rowId);
			}
		}

		listWidgetModel->scrollToItem(item, QAbstractItemView::PositionAtCenter);

		// Update the tristate checkbox
		checkBoxSelectAll->blockSignals(true);
		if (ids.size() == 0)
			checkBoxSelectAll->setCheckState(Qt::Unchecked);
		else if (ids.size() == static_cast<size_t>(listWidgetModel->count()))
			checkBoxSelectAll->setCheckState(Qt::Checked);
		else
			checkBoxSelectAll->setCheckState(Qt::PartiallyChecked);
		checkBoxSelectAll->blockSignals(false);

		_glWidget->setDisplayList(ids);
		float range = _glWidget->getBoundingSphere().getRadius() * 4.0f;
		sliderLightPosX->setRange(-range, range);
		sliderLightPosY->setRange(-range, range);
		sliderLightPosZ->setRange(-range/3, range/2);
		sliderLightPosZ->setValue((-range / 3 + range / 2)/2);
	}
}

void ModelViewer::on_listWidgetModel_itemSelectionChanged()
{
	for (int i = 0; i < listWidgetModel->count(); i++)
	{
		QListWidgetItem* item = listWidgetModel->item(i);
		int rowId = listWidgetModel->row(item);
		if (item->isSelected())
			_glWidget->select(rowId);
		else
			_glWidget->deselect(rowId);
	}
	_glWidget->update();
	updateSelectionStatusMessage();
}

void ModelViewer::itemEdited(QWidget* widget, QAbstractItemDelegate::EndEditHint /*hint*/)
{
	const QString path = reinterpret_cast<QLineEdit*>(widget)->text();
	int rowId = listWidgetModel->currentRow();
	std::vector<TriangleMesh*> meshes = _glWidget->getMeshStore();
	TriangleMesh* mesh = meshes.at(rowId);
	if (mesh->getName() != path)
		checkAndRenameModel(mesh, path);
}

void ModelViewer::checkAndRenameModel(TriangleMesh* mesh, const QString& name)
{
	bool duplicate = false;
	QString finalName = name;
	int dupCnt = 1;
	std::vector<TriangleMesh*> meshes = _glWidget->getMeshStore();
	do {
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

void ModelViewer::onFileImport()
{
	QFileDialog fileDialog(this, tr("Import Model File"), _lastOpenedDir);
	fileDialog.setFileMode(QFileDialog::ExistingFiles);
	QStringList supportedExtensions = ModelViewerApplication::supportedImportExtensions();
	fileDialog.setNameFilters(supportedExtensions);

	if (supportedExtensions.contains(_lastSelectedFilter)) {
		fileDialog.selectNameFilter(_lastSelectedFilter);
	}

	// Run dialog
	QStringList fileNames;
	if (fileDialog.exec()) {
		fileNames = fileDialog.selectedFiles();
		_lastSelectedFilter = fileDialog.selectedNameFilter();
		_lastOpenedDir = QFileInfo(fileNames.first()).absolutePath();
	}

	// Load selected files
	if (!fileNames.isEmpty()) {
		QApplication::setOverrideCursor(Qt::WaitCursor);
		for (const QString& fileName : std::as_const(fileNames)) {
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
void ModelViewer::onFileExport()
{
	Assimp::Exporter exporter;
	QStringList filters;
	QStringList allExtensions;
	QMap<QString, QString> filterToExtension; // Map filter -> extension

	// Build filters and track extensions
	for (unsigned int i = 0; i < exporter.GetExportFormatCount(); ++i) {
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

	if (!fileName.isEmpty()) {
		QString extToAppend = filterToExtension[selectedFilter];

		// Append extension only if not present already
		if (!extToAppend.isEmpty() && !fileName.endsWith("." + extToAppend, Qt::CaseInsensitive)) {
			fileName += "." + extToAppend;
		}

		// Export
		AssImpMeshExporter exporter;
		std::vector<TriangleMesh*> triMeshes = _glWidget->getMeshStore();
		std::vector<AssImpMesh*> assImpMeshes;
		for (TriangleMesh* triMesh : triMeshes)
			assImpMeshes.push_back(dynamic_cast<AssImpMesh*>(triMesh));

		aiReturn res = exporter.exportMeshes(assImpMeshes, fileName.toStdString());
		if (res == aiReturn_SUCCESS)
			QMessageBox::information(this, tr("Information"), tr("Exported"));
		else
			QMessageBox::critical(this, tr("Error"), tr("Export failed!"));
	}
}


bool ModelViewer::loadFile(const QString& fileName)
{
	_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time

	QString errMsg;
	bool success = false;
	if (QFileInfo(fileName).suffix().toLower() == "mvf")
	{
		// Load MVF file
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
		updateDisplayList();

		listWidgetModel->setCurrentRow(listWidgetModel->count() - 1);
		listWidgetModel->currentItem()->setCheckState(Qt::Checked);

		updateDisplayList();

		_documentModified = true;

		MainWindow::showStatusMessage(tr("File loaded"), 2000);

		return success;
	}
	else
	{
		QApplication::restoreOverrideCursor();
		QMessageBox::critical(this, tr("Error"), QString(tr("Failed to load model %1")).arg(fileName) + "\n" + errMsg);
		QApplication::setOverrideCursor(Qt::WaitCursor);

		return false;
	}
	

	return false;
}

#include <QFile>
#include <QDataStream>

bool ModelViewer::saveToFile(const QString& fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly))
		return false;
	QDataStream out(&file);
	_glWidget->serializeScene(out);
	return true;
}

bool ModelViewer::loadFromFile(const QString& fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly))
		return false;
	QDataStream in(&file);
	_glWidget->deserializeScene(in);
	setWindowTitle(QFileInfo(fileName).fileName());
	return true;
}

void ModelViewer::setMaterialToSelectedItems(const GLMaterial& mat)
{
	_material = mat;
	std::vector<int> ids = getSelectedIDs();
	_glWidget->setMaterialToObjects(ids, mat);
	_glWidget->updateView();
	updateControls();
}

void ModelViewer::on_checkBoxSelectAll_stateChanged(int arg1)
{
	if (arg1 != Qt::PartiallyChecked)
	{
		if (listWidgetModel->count())
		{
			bool oldState = listWidgetModel->blockSignals(true);
			for (int i = 0; i < listWidgetModel->count(); i++)
			{
				QListWidgetItem* item = listWidgetModel->item(i);
				item->setCheckState(checkBoxSelectAll->checkState());
			}
			listWidgetModel->blockSignals(oldState);
			on_listWidgetModel_itemChanged(nullptr);
		}
	}
	else
	{
		checkBoxSelectAll->setCheckState(Qt::Checked);
	}
}

void ModelViewer::on_checkBoxShadowMapping_toggled(bool checked)
{
	_glWidget->showShadows(checked);
	_glWidget->update();
}

void ModelViewer::on_checkBoxSelfShadows_toggled(bool checked)
{
	_glWidget->showSelfShadows(checked);
	_glWidget->update();
}

void ModelViewer::on_checkBoxEnvMapping_toggled(bool checked)
{
	_glWidget->showEnvironment(checked);
	_glWidget->update();
}

void ModelViewer::on_checkBoxSkyBox_toggled(bool checked)
{
	_glWidget->showSkyBox(checked);
	_glWidget->update();
}

void ModelViewer::on_checkBoxReflections_toggled(bool checked)
{
	_glWidget->showReflections(checked);
	_glWidget->update();
}

void ModelViewer::on_checkBoxFloor_toggled(bool checked)
{
	_glWidget->showFloor(checked);
	_glWidget->update();
}

void ModelViewer::on_checkBoxFloorTexture_toggled(bool checked)
{
	_glWidget->showFloorTexture(checked);
	_glWidget->update();
}

void ModelViewer::on_pushButtonFloorTexture_clicked()
{
	QString appPath = MODELVIEWER_DATA_DIR;
	QImage buf;
	QString filter = getSupportedQtImagesFilter();
	QString fileName = QFileDialog::getOpenFileName(
		this,
		"Choose an image for texture",
		appPath + "/textures/envmap/floor",
		filter);
	_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time
	if (fileName != "")
	{
		if (!buf.load(fileName))
		{ // Load first image from file
			qWarning("ModelViewer::on_pushButtonFloorTexture_clicked - Could not read image file, using single-color instead.");
			QImage dummy(128, 128, QImage::Format_ARGB32);
			dummy.fill(1);
			buf = dummy;
		}
		_glWidget->setFloorTexture(buf);
		_glWidget->update();
	}
}

void ModelViewer::on_toolBox_currentChanged(int index)
{
	if (index == 3) // Transformations page
	{
		updateTransformationValues();
	}
}


void ModelViewer::on_pushButtonSkyBoxTex_clicked()
{
	QString texpath = checkBoxSkyBoxHDRI->isChecked() ? "/textures/envmap/skyboxes/HDRI" : "/textures/envmap/skyboxes";
	QString appPath = MODELVIEWER_DATA_DIR;
	QString dir = QFileDialog::getExistingDirectory(this, tr("Select Skybox Texture Folder"),
		appPath + texpath,
		QFileDialog::ShowDirsOnly
		| QFileDialog::DontResolveSymlinks);
	if (dir != "")
	{
		_lastOpenedDir = dir;
		_glWidget->setSkyBoxTextureFolder(_lastOpenedDir);
	}
}

void ModelViewer::switchToRealisticRendering()
{
	QToolTip::showText(groupBoxVisModel->mapToGlobal(groupBoxVisModel->pos()), "Switching to Realistic Display Mode", this);
	_glWidget->setDisplayMode(DisplayMode::REALSHADED);	
}

void ModelViewer::lightingType_toggled(QAbstractButton*, bool)
{
	if (radioButtonADSL->isChecked())
	{
		toolBox->setItemEnabled(0, true);
		toolBox->setItemEnabled(1, false);
		toolBox->setItemEnabled(2, false);
		toolBox->setCurrentIndex(0);
		_glWidget->setRenderingMode(RenderingMode::ADS_PHONG);
	}
	if (radioButtonDLPBR->isChecked())
	{
		toolBox->setItemEnabled(0, false);
		toolBox->setItemEnabled(1, true);
		toolBox->setItemEnabled(2, false);
		toolBox->setCurrentIndex(1);
		_glWidget->setRenderingMode(RenderingMode::PBR_DIRECT_LIGHTING);
		switchToRealisticRendering();
	}
	if (radioButtonTXPBR->isChecked())
	{
		toolBox->setItemEnabled(0, false);
		toolBox->setItemEnabled(1, false);
		toolBox->setItemEnabled(2, true);
		toolBox->setCurrentIndex(2);
		_glWidget->setRenderingMode(RenderingMode::PBR_TEXTURED_LIGHTING);
		switchToRealisticRendering();
	}
	updateControls();
	_glWidget->update();
}

void ModelViewer::onDisplayModeChanged(int mode)
{
	bool checked = (mode == static_cast<int>(DisplayMode::REALSHADED));
	checkBoxEnvMapping->setChecked(checked);
	checkBoxShadowMapping->setChecked(checked);
	checkBoxSelfShadows->setChecked(checked);
	checkBoxReflections->setChecked(checked);
	checkBoxFloor->setChecked(checked);
}

void ModelViewer::on_pushButtonAlbedoColor_clicked()
{
	if (checkForActiveSelection())
	{
		QColor c = QColorDialog::getColor(QColor::fromRgbF(_material.albedoColor().x(), _material.albedoColor().y(), _material.albedoColor().z()), this, "Albedo Color");
		if (c.isValid())
		{
			QApplication::setOverrideCursor(Qt::WaitCursor);
			_material.setAlbedoColor(QVector3D(c.red() / 255.0f, c.green() / 255.0f, c.blue() / 255.0f));

			std::vector<int> ids = getSelectedIDs();
			_glWidget->setMaterialToObjects(ids, _material);
			_glWidget->updateView();
			updateControls();
			QApplication::restoreOverrideCursor();
		}
	}
}

void ModelViewer::on_sliderMetallic_valueChanged(int value)
{
	_material.setMetalness(value / 1000.0f);
	if (listWidgetModel->count())
	{
		std::vector<int> ids = getSelectedIDs();
		_glWidget->setPBRMetallic(ids, _material.metalness());
		_glWidget->updateView();
	}
}

void ModelViewer::on_sliderRoughness_valueChanged(int value)
{
	_material.setRoughness(value / 1000.0f);
	if (listWidgetModel->count())
	{
		std::vector<int> ids = getSelectedIDs();
		_glWidget->setPBRRoughness(ids, _material.roughness());
		_glWidget->updateView();
	}
}

void ModelViewer::on_checkBoxAlbedoMap_toggled(bool checked)
{
	if (checkForActiveSelection())
	{
		_hasPBRAlbedoTex = checked;
		std::vector<int> ids = getSelectedIDs();
		_glWidget->enablePBRAlbedoTexMap(ids, checked);
		_glWidget->updateView();
	}
	else
	{
		checkBoxAlbedoMap->blockSignals(true);
		checkBoxAlbedoMap->setChecked(!checked);
		pushButtonAlbedoMap->setEnabled(!checked);
		labelAlbedoMap->setEnabled(!checked);
		toolButtonClearAlbedo->setEnabled(!checked);
		checkBoxAlbedoMap->blockSignals(false);
	}
}

void ModelViewer::on_pushButtonAlbedoMap_clicked()
{
	if (checkForActiveSelection())
	{
		QString appPath = MODELVIEWER_DATA_DIR;
		QString dirPath = appPath + "/textures/materials";
		QString filter = getSupportedQtImagesFilter();
		QString fileName = QFileDialog::getOpenFileName(
			this,
			"Choose an image for Albedo map texture",
			_textureDirOpenedFirstTime ? dirPath : _lastOpenedDir,
			filter);
		_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time
		if (fileName != "")
		{
			_textureDirOpenedFirstTime = false;
			QPixmap img; img.load(fileName);
			if (!img.isNull())
			{
				QApplication::setOverrideCursor(Qt::WaitCursor);
				_albedoPBRTexture = fileName;
				labelAlbedoMap->setPixmap(img);
				std::vector<int> ids = getSelectedIDs();
				_glWidget->enablePBRAlbedoTexMap(ids, _hasPBRAlbedoTex);
				_glWidget->setPBRAlbedoTexMap(ids, fileName);
				_glWidget->updateView();
				QApplication::restoreOverrideCursor();
			}
		}
	}
}

void ModelViewer::on_checkBoxMetallicMap_toggled(bool checked)
{
	if (checkForActiveSelection())
	{
		_hasPBRMetallicTex = checked;
		std::vector<int> ids = getSelectedIDs();
		_glWidget->enablePBRMetallicTexMap(ids, checked);
		_glWidget->updateView();
	}
	else
	{
		checkBoxMetallicMap->blockSignals(true);
		checkBoxMetallicMap->setChecked(!checked);
		pushButtonMetallicMap->setEnabled(!checked);
		labelMetallicMap->setEnabled(!checked);
		toolButtonClearMetallic->setEnabled(!checked);
		checkBoxMetallicMap->blockSignals(false);
	}
}

void ModelViewer::on_pushButtonMetallicMap_clicked()
{
	if (checkForActiveSelection())
	{
		QString appPath = MODELVIEWER_DATA_DIR;
		QString dirPath = appPath + "/textures/materials";
		QString filter = getSupportedQtImagesFilter();
		QString fileName = QFileDialog::getOpenFileName(
			this,
			"Choose an image for Metallic map texture",
			_textureDirOpenedFirstTime ? dirPath : _lastOpenedDir,
			filter);
		_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time
		if (fileName != "")
		{
			_metallicPBRTexture = fileName;
			_textureDirOpenedFirstTime = false;
			QPixmap img; img.load(fileName);
			if (!img.isNull())
			{
				QApplication::setOverrideCursor(Qt::WaitCursor);
				labelMetallicMap->setPixmap(img);
				std::vector<int> ids = getSelectedIDs();
				_glWidget->enablePBRMetallicTexMap(ids, _hasPBRMetallicTex);
				_glWidget->setPBRMetallicTexMap(ids, fileName);
				_glWidget->updateView();
				QApplication::restoreOverrideCursor();
			}
		}
	}
}

void ModelViewer::on_checkBoxRoughnessMap_toggled(bool checked)
{
	if (checkForActiveSelection())
	{
		_hasPBRRoughnessTex = checked;
		std::vector<int> ids = getSelectedIDs();
		_glWidget->enablePBRRoughnessTexMap(ids, checked);
		_glWidget->updateView();
	}
	else
	{
		checkBoxRoughnessMap->blockSignals(true);
		checkBoxRoughnessMap->setChecked(!checked);
		pushButtonRoughnessMap->setEnabled(!checked);
		labelRoughnessMap->setEnabled(!checked);
		toolButtonClearRoughness->setEnabled(!checked);
		checkBoxRoughnessMap->blockSignals(false);
	}
}

void ModelViewer::on_pushButtonRoughnessMap_clicked()
{
	if (checkForActiveSelection())
	{
		QString appPath = MODELVIEWER_DATA_DIR;
		QString dirPath = appPath + "/textures/materials";
		QString filter = getSupportedQtImagesFilter();
		QString fileName = QFileDialog::getOpenFileName(
			this,
			"Choose an image for Roughness map texture",
			_textureDirOpenedFirstTime ? dirPath : _lastOpenedDir,
			filter);
		_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time
		if (fileName != "")
		{
			_roughnessPBRTexture = fileName;
			_textureDirOpenedFirstTime = false;
			QPixmap img; img.load(fileName);
			if (!img.isNull())
			{
				QApplication::setOverrideCursor(Qt::WaitCursor);
				labelRoughnessMap->setPixmap(img);
				std::vector<int> ids = getSelectedIDs();
				_glWidget->enablePBRRoughnessTexMap(ids, _hasPBRRoughnessTex);
				_glWidget->setPBRRoughnessTexMap(ids, fileName);
				_glWidget->updateView();
				QApplication::restoreOverrideCursor();
			}
		}
	}
}

void ModelViewer::on_checkBoxNormalMap_toggled(bool checked)
{
	if (checkForActiveSelection())
	{
		_hasPBRNormalTex = checked;
		std::vector<int> ids = getSelectedIDs();
		_glWidget->enablePBRNormalTexMap(ids, checked);
		_glWidget->updateView();
	}
	else
	{
		checkBoxNormalMap->blockSignals(true);
		checkBoxNormalMap->setChecked(!checked);
		pushButtonNormalMap->setEnabled(!checked);
		labelNormalMap->setEnabled(!checked);
		toolButtonClearNormal->setEnabled(!checked);
		checkBoxNormalMap->blockSignals(false);
	}
}

void ModelViewer::on_pushButtonNormalMap_clicked()
{
	if (checkForActiveSelection())
	{
		QString appPath = MODELVIEWER_DATA_DIR;
		QString dirPath = appPath + "/textures/materials";
		QString filter = getSupportedQtImagesFilter();
		QString fileName = QFileDialog::getOpenFileName(
			this,
			"Choose an image for Normal map texture",
			_textureDirOpenedFirstTime ? dirPath : _lastOpenedDir,
			filter);
		_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time
		if (fileName != "")
		{
			_normalPBRTexture = fileName;
			_textureDirOpenedFirstTime = false;
			QPixmap img; img.load(fileName);
			if (!img.isNull())
			{
				QApplication::setOverrideCursor(Qt::WaitCursor);
				labelNormalMap->setPixmap(img);
				std::vector<int> ids = getSelectedIDs();
				_glWidget->enablePBRNormalTexMap(ids, _hasPBRNormalTex);
				_glWidget->setPBRNormalTexMap(ids, fileName);
				_glWidget->updateView();
				QApplication::restoreOverrideCursor();
			}
		}
	}
}

void ModelViewer::on_checkBoxAOMap_toggled(bool checked)
{
	if (checkForActiveSelection())
	{
		_hasPBRAOTex = checked;
		std::vector<int> ids = getSelectedIDs();
		_glWidget->enablePBRAOTexMap(ids, checked);
		_glWidget->updateView();
	}
	else
	{
		checkBoxAOMap->blockSignals(true);
		checkBoxAOMap->setChecked(!checked);
		pushButtonAOMap->setEnabled(!checked);
		labelAOMap->setEnabled(!checked);
		toolButtonClearAO->setEnabled(!checked);
		checkBoxAOMap->blockSignals(false);
	}
}

void ModelViewer::on_pushButtonAOMap_clicked()
{
	if (checkForActiveSelection())
	{
		QString appPath = MODELVIEWER_DATA_DIR;
		QString dirPath = appPath + "/textures/materials";
		QString filter = getSupportedQtImagesFilter();
		QString fileName = QFileDialog::getOpenFileName(
			this,
			"Choose an image for AO map texture",
			_textureDirOpenedFirstTime ? dirPath : _lastOpenedDir,
			filter);
		_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time
		if (fileName != "")
		{
			_aoPBRTexture = fileName;
			_textureDirOpenedFirstTime = false;
			QPixmap img; img.load(fileName);
			if (!img.isNull())
			{
				QApplication::setOverrideCursor(Qt::WaitCursor);
				labelAOMap->setPixmap(img);
				std::vector<int> ids = getSelectedIDs();
				_glWidget->enablePBRAOTexMap(ids, _hasPBRAOTex);
				_glWidget->setPBRAOTexMap(ids, fileName);
				_glWidget->updateView();
				QApplication::restoreOverrideCursor();
			}
		}
	}
}

void ModelViewer::on_checkBoxOpacityMap_toggled(bool checked)
{
	if (checkForActiveSelection())
	{
		_hasPBROpacTex = checked;
		std::vector<int> ids = getSelectedIDs();
		_glWidget->enablePBROpacityTexMap(ids, checked);
		_glWidget->updateView();
	}
	else
	{
		checkBoxOpacityMap->blockSignals(true);
		checkBoxOpacityMap->setChecked(!checked);
		checkBoxOpacMapInvert->setEnabled(!checked);
		pushButtonOpacityMap->setEnabled(!checked);
		labelOpacityMap->setEnabled(!checked);
		toolButtonClearOpacityMap->setEnabled(!checked);
		checkBoxOpacityMap->blockSignals(false);
	}
}

void ModelViewer::on_checkBoxOpacMapInvert_toggled(bool inverted)
{
	if (checkForActiveSelection())
	{
		std::vector<int> ids = getSelectedIDs();
		_glWidget->invertPBROpacityTexMap(ids, inverted);
		_glWidget->updateView();
	}
}

void ModelViewer::on_pushButtonOpacityMap_clicked()
{
	if (checkForActiveSelection())
	{
		QString appPath = MODELVIEWER_DATA_DIR;
		QString dirPath = appPath + "/textures/materials";
		QString filter = getSupportedQtImagesFilter();
		QString fileName = QFileDialog::getOpenFileName(
			this,
			"Choose an image for PBR Opacity texture",
			_textureDirOpenedFirstTime ? dirPath : _lastOpenedDir,
			filter);
		_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time
		if (fileName != "")
		{
			_opacityPBRTexture = fileName;
			_textureDirOpenedFirstTime = false;
			QPixmap img; img.load(fileName);
			if (!img.isNull())
			{
				QApplication::setOverrideCursor(Qt::WaitCursor);
				labelOpacityMap->setPixmap(img);
				std::vector<int> ids = getSelectedIDs();
				_glWidget->enablePBROpacityTexMap(ids, _hasPBROpacTex);
				_glWidget->setPBROpacityTexMap(ids, fileName);
				_glWidget->updateView();
				QApplication::restoreOverrideCursor();
			}
		}
	}
}

void ModelViewer::on_toolButtonClearOpacityMap_clicked()
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

void ModelViewer::on_checkBoxHeightMap_toggled(bool checked)
{
	if (checkForActiveSelection())
	{
		_hasPBRHeightTex = checked;
		std::vector<int> ids = getSelectedIDs();
		_glWidget->enablePBRHeightTexMap(ids, checked);
		_glWidget->updateView();
	}

	else
	{
		checkBoxHeightMap->blockSignals(true);
		checkBoxHeightMap->setChecked(!checked);
		pushButtonHeightMap->setEnabled(!checked);
		labelHeightMap->setEnabled(!checked);
		toolButtonClearHeight->setEnabled(!checked);
		labelHeightScale->setEnabled(!checked);
		doubleSpinBoxHeightScale->setEnabled(!checked);
		checkBoxHeightMap->blockSignals(false);
	}
}

void ModelViewer::on_pushButtonHeightMap_clicked()
{
	if (checkForActiveSelection())
	{
		QString appPath = MODELVIEWER_DATA_DIR;
		QString dirPath = appPath + "/textures/materials";
		QString filter = getSupportedQtImagesFilter();
		QString fileName = QFileDialog::getOpenFileName(
			this,
			"Choose an image for Height map texture",
			_textureDirOpenedFirstTime ? dirPath : _lastOpenedDir,
			filter);
		_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time
		if (fileName != "")
		{
			_heightPBRTexture = fileName;
			_textureDirOpenedFirstTime = false;
			QPixmap img; img.load(fileName);
			if (!img.isNull())
			{
				QApplication::setOverrideCursor(Qt::WaitCursor);
				labelHeightMap->setPixmap(img);
				std::vector<int> ids = getSelectedIDs();
				_glWidget->enablePBRHeightTexMap(ids, _hasPBRHeightTex);
				_glWidget->setPBRHeightTexMap(ids, fileName);
				_glWidget->updateView();
				QApplication::restoreOverrideCursor();
			}
		}
	}
}

void ModelViewer::on_doubleSpinBoxHeightScale_valueChanged(double val)
{
	if (checkForActiveSelection())
	{
		_heightPBRTexScale = val;
		std::vector<int> ids = getSelectedIDs();
		_glWidget->setPBRHeightScale(ids, static_cast<float>(val));
		_glWidget->updateView();
	}
}

void ModelViewer::on_pushButtonApplyPBRTexture_clicked()
{
	bool allOK = true;
	if (!_hasPBRAlbedoTex || (_hasPBRAlbedoTex && _albedoPBRTexture == ""))
	{
		QMessageBox::critical(this, tr("PBR Texture Missing"), tr("Albedo map texture not set"));
		allOK = false;
	}
	else if (!_hasPBRMetallicTex || (_hasPBRMetallicTex && _metallicPBRTexture == ""))
	{
		QMessageBox::critical(this, tr("PBR Texture Missing"), tr("Metallic map texture not set"));
		allOK = false;
	}
	else if (!_hasPBRRoughnessTex || (_hasPBRRoughnessTex && _roughnessPBRTexture == ""))
	{
		QMessageBox::critical(this, tr("PBR Texture Missing"), tr("Roughness map texture not set"));
		allOK = false;
	}
	else if (_hasPBRNormalTex && _normalPBRTexture == "")
	{
		QMessageBox::critical(this, tr("PBR Texture Missing"), tr("Normal map texture not set"));
		allOK = false;
	}
	else if (_hasPBRAOTex && _aoPBRTexture == "")
	{
		QMessageBox::critical(this, tr("PBR Texture Missing"), tr("AO map texture not set"));
		allOK = false;
	}
	else if (_hasPBRHeightTex && _heightPBRTexture == "")
	{
		QMessageBox::critical(this, tr("PBR Texture Missing"), tr("Height map texture not set"));
		allOK = false;
	}
	if (allOK)
	{
		if (checkForActiveSelection())
		{
			QApplication::setOverrideCursor(Qt::WaitCursor);

			std::vector<int> ids = getSelectedIDs();
			_glWidget->enablePBRAlbedoTexMap(ids, _hasPBRAlbedoTex);
			if (_hasPBRAlbedoTex)
			{
				_glWidget->setPBRAlbedoTexMap(ids, _albedoPBRTexture);
			}
			_glWidget->enablePBRMetallicTexMap(ids, _hasPBRMetallicTex);
			if (_hasPBRMetallicTex)
			{
				_glWidget->setPBRMetallicTexMap(ids, _metallicPBRTexture);
			}
			_glWidget->enablePBRRoughnessTexMap(ids, _hasPBRRoughnessTex);
			if (_hasPBRRoughnessTex)
			{
				_glWidget->setPBRRoughnessTexMap(ids, _roughnessPBRTexture);
			}
			_glWidget->enablePBRNormalTexMap(ids, _hasPBRNormalTex);
			if (_hasPBRNormalTex)
			{
				_glWidget->setPBRNormalTexMap(ids, _normalPBRTexture);
			}
			_glWidget->enablePBRAOTexMap(ids, _hasPBRAOTex);
			if (_hasPBRAOTex)
			{
				_glWidget->setPBRAOTexMap(ids, _aoPBRTexture);
			}
			_glWidget->enablePBROpacityTexMap(ids, _hasPBROpacTex);
			if (_hasPBROpacTex)
			{
				_glWidget->setPBROpacityTexMap(ids, _opacityPBRTexture);
			}
			_glWidget->enablePBRHeightTexMap(ids, _hasPBRHeightTex);
			if (_hasPBRHeightTex)
			{
				_glWidget->setPBRHeightTexMap(ids, _heightPBRTexture);
				_glWidget->setPBRHeightScale(ids, static_cast<float>(_heightPBRTexScale));
			}
			_glWidget->updateView();
			QApplication::restoreOverrideCursor();
		}
	}
}

void ModelViewer::on_pushButtonClearPBRTextures_clicked()
{
	if (checkForActiveSelection())
	{
		std::vector<int> ids = getSelectedIDs();
		QApplication::setOverrideCursor(Qt::WaitCursor);
		_glWidget->clearPBRTexMaps(ids);
		_glWidget->updateView();
		QApplication::restoreOverrideCursor();
	}
}

void ModelViewer::on_toolButtonClearAlbedo_clicked()
{
	if (checkForActiveSelection())
	{
		std::vector<int> ids = getSelectedIDs();
		QApplication::setOverrideCursor(Qt::WaitCursor);
		_glWidget->clearPBRAlbedoTexMap(ids);
		_glWidget->updateView();
		QApplication::restoreOverrideCursor();
	}
}

void ModelViewer::on_toolButtonClearMetallic_clicked()
{
	if (checkForActiveSelection())
	{
		std::vector<int> ids = getSelectedIDs();
		QApplication::setOverrideCursor(Qt::WaitCursor);
		_glWidget->clearPBRMetallicTexMap(ids);
		_glWidget->updateView();
		QApplication::restoreOverrideCursor();
	}
}

void ModelViewer::on_toolButtonClearRoughness_clicked()
{
	if (checkForActiveSelection())
	{
		std::vector<int> ids = getSelectedIDs();
		QApplication::setOverrideCursor(Qt::WaitCursor);
		_glWidget->clearPBRRoughnessTexMap(ids);
		_glWidget->updateView();
		QApplication::restoreOverrideCursor();
	}
}

void ModelViewer::on_toolButtonClearNormal_clicked()
{
	if (checkForActiveSelection())
	{
		std::vector<int> ids = getSelectedIDs();
		QApplication::setOverrideCursor(Qt::WaitCursor);
		_glWidget->clearPBRNormalTexMap(ids);
		_glWidget->updateView();
		QApplication::restoreOverrideCursor();
	}
}

void ModelViewer::on_toolButtonClearAO_clicked()
{
	if (checkForActiveSelection())
	{
		std::vector<int> ids = getSelectedIDs();
		QApplication::setOverrideCursor(Qt::WaitCursor);
		_glWidget->clearPBRAOTexMap(ids);
		_glWidget->updateView();
		QApplication::restoreOverrideCursor();
	}
}

void ModelViewer::on_toolButtonClearHeight_clicked()
{
	if (checkForActiveSelection())
	{
		std::vector<int> ids = getSelectedIDs();
		QApplication::setOverrideCursor(Qt::WaitCursor);
		_glWidget->clearPBRHeightTexMap(ids);
		_glWidget->updateView();
		QApplication::restoreOverrideCursor();
	}
}

void ModelViewer::on_checkBoxDiffuseTex_toggled(bool checked)
{
	if (checkForActiveSelection())
	{
		_hasADSDiffuseTex = checked;
		std::vector<int> ids = getSelectedIDs();
		_glWidget->enableADSDiffuseTexMap(ids, checked);
		_glWidget->updateView();
	}
	else
	{
		checkBoxDiffuseTex->blockSignals(true);
		checkBoxDiffuseTex->setChecked(!checked);
		pushButtonDiffuseTexture->setEnabled(!checked);
		labelDiffuseTexture->setEnabled(!checked);
		toolButtonClearDiffuseTex->setEnabled(!checked);
		checkBoxDiffuseTex->blockSignals(false);
	}
}

void ModelViewer::on_pushButtonDiffuseTexture_clicked()
{
	if (checkForActiveSelection())
	{
		QString appPath = MODELVIEWER_DATA_DIR;
		QString dirPath = appPath + "/textures/lightmaps";
		QString filter = getSupportedQtImagesFilter();
		QString fileName = QFileDialog::getOpenFileName(
			this,
			"Choose an image for ADS Diffuse texture",
			_textureDirOpenedFirstTime ? dirPath : _lastOpenedDir,
			filter);
		_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time
		if (fileName != "")
		{
			_diffuseADSTexture = fileName;
			_textureDirOpenedFirstTime = false;
			QPixmap img; img.load(fileName);
			if (!img.isNull())
			{
				QApplication::setOverrideCursor(Qt::WaitCursor);
				labelDiffuseTexture->setPixmap(img);
				std::vector<int> ids = getSelectedIDs();
				_glWidget->enableADSDiffuseTexMap(ids, _hasADSDiffuseTex);
				_glWidget->setADSDiffuseTexMap(ids, fileName);
				_glWidget->updateView();
				QApplication::restoreOverrideCursor();
			}
		}
	}
}

void ModelViewer::on_toolButtonClearDiffuseTex_clicked()
{
	if (checkForActiveSelection())
	{
		std::vector<int> ids = getSelectedIDs();
		QApplication::setOverrideCursor(Qt::WaitCursor);
		_glWidget->clearADSDiffuseTexMap(ids);
		_glWidget->updateView();
		QApplication::restoreOverrideCursor();
	}
}

void ModelViewer::on_checkBoxSpecularTex_toggled(bool checked)
{
	if (checkForActiveSelection())
	{
		_hasADSSpecularTex = checked;
		std::vector<int> ids = getSelectedIDs();
		_glWidget->enableADSSpecularTexMap(ids, checked);
		_glWidget->updateView();
	}
	else
	{
		checkBoxSpecularTex->blockSignals(true);
		checkBoxSpecularTex->setChecked(!checked);
		pushButtonSpecularTexture->setEnabled(!checked);
		labelSpecularTexture->setEnabled(!checked);
		toolButtonClearSpecularTex->setEnabled(!checked);
		checkBoxSpecularTex->blockSignals(false);
	}
}

void ModelViewer::on_pushButtonSpecularTexture_clicked()
{
	if (checkForActiveSelection())
	{
		QString appPath = MODELVIEWER_DATA_DIR;
		QString dirPath = appPath + "/textures/lightmaps";
		QString filter = getSupportedQtImagesFilter();
		QString fileName = QFileDialog::getOpenFileName(
			this,
			"Choose an image for ADS Specular texture",
			_textureDirOpenedFirstTime ? dirPath : _lastOpenedDir,
			filter);
		_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time
		if (fileName != "")
		{
			_specularADSTexture = fileName;
			_textureDirOpenedFirstTime = false;
			QPixmap img; img.load(fileName);
			if (!img.isNull())
			{
				QApplication::setOverrideCursor(Qt::WaitCursor);
				labelSpecularTexture->setPixmap(img);
				std::vector<int> ids = getSelectedIDs();
				_glWidget->enableADSSpecularTexMap(ids, _hasADSSpecularTex);
				_glWidget->setADSSpecularTexMap(ids, fileName);
				_glWidget->updateView();
				QApplication::restoreOverrideCursor();
			}
		}
	}
}

void ModelViewer::on_toolButtonClearSpecularTex_clicked()
{
	if (checkForActiveSelection())
	{
		std::vector<int> ids = getSelectedIDs();
		QApplication::setOverrideCursor(Qt::WaitCursor);
		_glWidget->clearADSSpecularTexMap(ids);
		_glWidget->updateView();
		QApplication::restoreOverrideCursor();
	}
}

void ModelViewer::on_checkBoxEmissiveTex_toggled(bool checked)
{
	if (checkForActiveSelection())
	{
		_hasADSEmissiveTex = checked;
		std::vector<int> ids = getSelectedIDs();
		_glWidget->enableADSEmissiveTexMap(ids, checked);
		_glWidget->updateView();
	}
	else
	{
		checkBoxEmissiveTex->blockSignals(true);
		checkBoxEmissiveTex->setChecked(!checked);
		pushButtonEmissiveTexture->setEnabled(!checked);
		labelEmissiveTexture->setEnabled(!checked);
		toolButtonClearEmissiveTex->setEnabled(!checked);
		checkBoxEmissiveTex->blockSignals(false);
	}
}

void ModelViewer::on_pushButtonEmissiveTexture_clicked()
{
	if (checkForActiveSelection())
	{
		QString appPath = MODELVIEWER_DATA_DIR;
		QString dirPath = appPath + "/textures/lightmaps";
		QString filter = getSupportedQtImagesFilter();
		QString fileName = QFileDialog::getOpenFileName(
			this,
			"Choose an image for ADS Emissive texture",
			_textureDirOpenedFirstTime ? dirPath : _lastOpenedDir,
			filter);
		_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time
		if (fileName != "")
		{
			_emissiveADSTexture = fileName;
			_textureDirOpenedFirstTime = false;
			QPixmap img; img.load(fileName);
			if (!img.isNull())
			{
				QApplication::setOverrideCursor(Qt::WaitCursor);
				labelEmissiveTexture->setPixmap(img);
				std::vector<int> ids = getSelectedIDs();
				_glWidget->enableADSEmissiveTexMap(ids, _hasADSEmissiveTex);
				_glWidget->setADSEmissiveTexMap(ids, fileName);
				_glWidget->updateView();
				QApplication::restoreOverrideCursor();
			}
		}
	}
}

void ModelViewer::on_toolButtonClearEmissiveTex_clicked()
{
	if (checkForActiveSelection())
	{
		std::vector<int> ids = getSelectedIDs();
		QApplication::setOverrideCursor(Qt::WaitCursor);
		_glWidget->clearADSEmissiveTexMap(ids);
		_glWidget->updateView();
		QApplication::restoreOverrideCursor();
	}
}

void ModelViewer::on_checkBoxNormalTex_toggled(bool checked)
{
	if (checkForActiveSelection())
	{
		_hasADSNormalTex = checked;
		std::vector<int> ids = getSelectedIDs();
		_glWidget->enableADSNormalTexMap(ids, checked);
		_glWidget->updateView();
	}
	else
	{
		checkBoxNormalTex->blockSignals(true);
		checkBoxNormalTex->setChecked(!checked);
		pushButtonNormalTexture->setEnabled(!checked);
		labelNormalTexture->setEnabled(!checked);
		toolButtonClearNormalTex->setEnabled(!checked);
		checkBoxNormalTex->blockSignals(false);
	}
}

void ModelViewer::on_pushButtonNormalTexture_clicked()
{
	if (checkForActiveSelection())
	{
		QString appPath = MODELVIEWER_DATA_DIR;
		QString dirPath = appPath + "/textures/lightmaps";
		QString filter = getSupportedQtImagesFilter();
		QString fileName = QFileDialog::getOpenFileName(
			this,
			"Choose an image for ADS Normal texture",
			_textureDirOpenedFirstTime ? dirPath : _lastOpenedDir,
			filter);
		_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time
		if (fileName != "")
		{
			_normalADSTexture = fileName;
			_textureDirOpenedFirstTime = false;
			QPixmap img; img.load(fileName);
			if (!img.isNull())
			{
				QApplication::setOverrideCursor(Qt::WaitCursor);
				labelNormalTexture->setPixmap(img);
				std::vector<int> ids = getSelectedIDs();
				_glWidget->enableADSNormalTexMap(ids, _hasADSNormalTex);
				_glWidget->setADSNormalTexMap(ids, fileName);
				_glWidget->updateView();
				QApplication::restoreOverrideCursor();
			}
		}
	}
}

void ModelViewer::on_toolButtonClearNormalTex_clicked()
{
	if (checkForActiveSelection())
	{
		std::vector<int> ids = getSelectedIDs();
		QApplication::setOverrideCursor(Qt::WaitCursor);
		_glWidget->clearADSNormalTexMap(ids);
		_glWidget->updateView();
		QApplication::restoreOverrideCursor();
	}
}

void ModelViewer::on_checkBoxHeightTex_toggled(bool checked)
{
	if (checkForActiveSelection())
	{
		_hasADSHeightTex = checked;
		std::vector<int> ids = getSelectedIDs();
		_glWidget->enableADSHeightTexMap(ids, checked);
		_glWidget->updateView();
	}
	else
	{
		checkBoxHeightTex->blockSignals(true);
		checkBoxHeightTex->setChecked(!checked);
		pushButtonHeightTexture->setEnabled(!checked);
		labelHeightTexture->setEnabled(!checked);
		toolButtonClearHeightTex->setEnabled(!checked);
		checkBoxHeightTex->blockSignals(false);
	}
}

void ModelViewer::on_pushButtonHeightTexture_clicked()
{
	if (checkForActiveSelection())
	{
		QString appPath = MODELVIEWER_DATA_DIR;
		QString dirPath = appPath + "/textures/lightmaps";
		QString filter = getSupportedQtImagesFilter();
		QString fileName = QFileDialog::getOpenFileName(
			this,
			"Choose an image for ADS Height texture",
			_textureDirOpenedFirstTime ? dirPath : _lastOpenedDir,
			filter);
		_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time
		if (fileName != "")
		{
			_heightADSTexture = fileName;
			_textureDirOpenedFirstTime = false;
			QPixmap img; img.load(fileName);
			if (!img.isNull())
			{
				QApplication::setOverrideCursor(Qt::WaitCursor);
				labelHeightTexture->setPixmap(img);
				std::vector<int> ids = getSelectedIDs();
				_glWidget->enableADSHeightTexMap(ids, _hasADSHeightTex);
				_glWidget->setADSHeightTexMap(ids, fileName);
				_glWidget->updateView();
				QApplication::restoreOverrideCursor();
			}
		}
	}
}

void ModelViewer::on_toolButtonClearHeightTex_clicked()
{
	if (checkForActiveSelection())
	{
		std::vector<int> ids = getSelectedIDs();
		QApplication::setOverrideCursor(Qt::WaitCursor);
		_glWidget->clearADSHeightTexMap(ids);
		_glWidget->updateView();
		QApplication::restoreOverrideCursor();
	}
}

void ModelViewer::on_checkBoxOpacityTex_toggled(bool checked)
{
	if (checkForActiveSelection())
	{
		_hasADSOpacityTex = checked;
		std::vector<int> ids = getSelectedIDs();
		_glWidget->enableADSOpacityTexMap(ids, checked);
		_glWidget->updateView();
	}
	else
	{
		checkBoxOpacityTex->blockSignals(true);
		checkBoxOpacityTex->setChecked(!checked);
		checkBoxOpacInvert->setEnabled(!checked);
		pushButtonOpacityTexture->setEnabled(!checked);
		labelOpacityTexture->setEnabled(!checked);
		toolButtonClearOpacityTex->setEnabled(!checked);
		checkBoxOpacityTex->blockSignals(false);
	}
}

void ModelViewer::on_checkBoxOpacInvert_toggled(bool inverted)
{
	if (checkForActiveSelection())
	{
		//_hasADSOpacityTex = checked;
		std::vector<int> ids = getSelectedIDs();
		_glWidget->invertADSOpacityTexMap(ids, inverted);
		_glWidget->updateView();
	}
}

void ModelViewer::on_pushButtonOpacityTexture_clicked()
{
	if (checkForActiveSelection())
	{
		QString appPath = MODELVIEWER_DATA_DIR;
		QString dirPath = appPath + "/textures/lightmaps";
		QString filter = getSupportedQtImagesFilter();
		QString fileName = QFileDialog::getOpenFileName(
			this,
			"Choose an image for ADS Opacity texture",
			_textureDirOpenedFirstTime ? dirPath : _lastOpenedDir,
			filter);
		_lastOpenedDir = QFileInfo(fileName).path(); // store path for next time
		if (fileName != "")
		{
			_opacityADSTexture = fileName;
			_textureDirOpenedFirstTime = false;
			QPixmap img; img.load(fileName);
			if (!img.isNull())
			{
				QApplication::setOverrideCursor(Qt::WaitCursor);
				labelOpacityTexture->setPixmap(img);
				std::vector<int> ids = getSelectedIDs();
				_glWidget->enableADSOpacityTexMap(ids, _hasADSHeightTex);
				_glWidget->setADSOpacityTexMap(ids, fileName);
				_glWidget->updateView();
				QApplication::restoreOverrideCursor();
			}
		}
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

void ModelViewer::on_pushButtonApplyADSTexture_clicked()
{
	bool allOK = true;
	if (!_hasADSDiffuseTex || (_hasADSDiffuseTex && _diffuseADSTexture == ""))
	{
		QMessageBox::critical(this, tr("ADS Texture Missing"), tr("Diffuse map texture not set"));
		allOK = false;
	}
	else if (_hasADSSpecularTex && _specularADSTexture == "")
	{
		QMessageBox::critical(this, tr("ADS Texture Missing"), tr("Specular map texture not set"));
		allOK = false;
	}
	else if (_hasADSNormalTex && _normalADSTexture == "")
	{
		QMessageBox::critical(this, tr("ADS Texture Missing"), tr("Normal map texture not set"));
		allOK = false;
	}
	else if (_hasADSHeightTex && _heightADSTexture == "")
	{
		QMessageBox::critical(this, tr("ADS Texture Missing"), tr("Height map texture not set"));
		allOK = false;
	}

	if (allOK)
	{
		if (checkForActiveSelection())
		{
			QApplication::setOverrideCursor(Qt::WaitCursor);

			std::vector<int> ids = getSelectedIDs();
			_glWidget->enableADSDiffuseTexMap(ids, _hasADSDiffuseTex);
			if (_hasADSDiffuseTex)
			{
				_glWidget->setADSDiffuseTexMap(ids, _diffuseADSTexture);
			}
			_glWidget->enableADSSpecularTexMap(ids, _hasADSSpecularTex);
			if (_hasADSSpecularTex)
			{
				_glWidget->setADSSpecularTexMap(ids, _specularADSTexture);
			}
			_glWidget->enableADSNormalTexMap(ids, _hasADSNormalTex);
			if (_hasADSNormalTex)
			{
				_glWidget->setADSNormalTexMap(ids, _normalADSTexture);
			}
			_glWidget->enableADSHeightTexMap(ids, _hasADSHeightTex);
			if (_hasADSHeightTex)
			{
				_glWidget->setADSHeightTexMap(ids, _heightADSTexture);
			}
			_glWidget->enableADSOpacityTexMap(ids, _hasADSOpacityTex);
			if (_hasADSOpacityTex)
			{
				_glWidget->setADSOpacityTexMap(ids, _opacityADSTexture);
			}
			_glWidget->updateView();
			QApplication::restoreOverrideCursor();
		}
	}
}

void ModelViewer::on_pushButtonClearADSTextures_clicked()
{
	if (checkForActiveSelection())
	{
		std::vector<int> ids = getSelectedIDs();
		QApplication::setOverrideCursor(Qt::WaitCursor);
		_glWidget->clearADSTexMaps(ids);
		_glWidget->updateView();
		QApplication::restoreOverrideCursor();
	}
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
