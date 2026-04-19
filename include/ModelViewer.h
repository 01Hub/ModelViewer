#ifndef __MODELVIEWER_H__
#define __MODELVIEWER_H__

#include "ui_ModelViewer.h"

#include "GLWidget.h"
#include "GLMaterial.h"
#include "SceneGraph.h"
#include "SceneTreeWidget.h"
#include "UVPromptDialog.h"
#include "AssImpModelLoader.h"
#include "ApplyMaterialCommand.h"
#include "RenameMeshCommand.h"
#include "MaterialPropertiesPanel.h"

#include <QUndoStack>

struct UVDialogResult
{
	UVMethod method = UVMethod::None;
};

namespace Mvf
{
struct Document;
struct MVFPackage;
}

class ModelViewer : public QWidget, public Ui::ModelViewer
{
	Q_OBJECT
public:
	ModelViewer(QWidget* parent = 0);
	~ModelViewer();

	void retranslateUI();

	GLWidget*    getGLView()    const { return _glWidget; }
	SceneGraph*  sceneGraph()   const { return _sceneGraph; }
	QMap<QString, CachedMaterial>* getMaterialCache() { return &_materialCache; }
	void registerOwnedUnsavedMaterial(const QString& materialKey) { _ownedUnsavedMaterials.insert(materialKey); }

	void setMaterialToSelectedItems(const GLMaterial& mat);
	void setTexturesToSelectedItems(const GLMaterial& mat);
	void setTextureSamplersToSelectedItems(const GLMaterial* material, GLMaterial::TextureType type);

	void setTransformation();
	void bakeTransformations();
	void resetTransformation();

	SceneTreeWidget* getTreeModel() { return treeWidgetModel; }

	void updateTransformationValues();
	void resetTransformationValues();

	void switchToRealisticRendering();

	static QString getLastOpenedDir();
	static void setLastOpenedDir(const QString& lastOpenedDir);

	static QString getLastSelectedFilter();
	static void setLastSelectedFilter(const QString& lastSelectedFilter);

	void setCurrentFile(const QString& fileName);
	QString currentFile() const;

	bool loadFile(const QString& fileName);

	void importModel();
	void exportModel();

	bool saveToFile(const QString& fileName);
	bool loadFromFile(const QString& fileName);

	bool documentModified() const { return _documentModified; }
	void setDocumentModified(bool modified = true);

	bool save();
	bool saveAs();

	void closeEvent(QCloseEvent* event);

	void setDocumentSaved(bool saved = true);
	bool isDocumentSaved() const { return _documentSaved; }

	void selectAll();
	void deselectAll();

	// For UV generation dialog user selection
	static UVDialogResult askUserForUVMethod(QWidget* parent);

	// Skybox index accessors for VisualizationEnvironmentPanel
	int getSkyBoxLDRIIndex() const { return _skyBoxLDRIIndex; }
	int getSkyBoxHDRIIndex() const { return _skyBoxHDRIIndex; }
	void setSkyBoxLDRIIndex(int index) { _skyBoxLDRIIndex = index; }
	void setSkyBoxHDRIIndex(int index) { _skyBoxHDRIIndex = index; }

	// Undo/Redo interface (called by MainWindow)
	bool hasUndo() const;
	bool hasRedo() const;
	void undo();
	void redo();

	// Undo stack access
	QUndoStack* getUndoStack() const { return _undoStack; }

	// Selection helpers (used by SelectionCommand)
	void setSelectionWithUndo(const QSet<int>& newSelection);
	void setSelectionWithoutUndo(const QSet<int>& selection);
	// Selection helpers (for DuplicateCommand)
	void setSelectionWithoutUndo(const QSet<QUuid>& uuids);

	// Visibility helpers (used by VisibilityCommand)
	QSet<QUuid> getVisibleUuids() const;
	void setVisibilityWithUndo(const QSet<QUuid>& newVisibleUuids,
		const QString& commandText);
	void setVisibilityWithoutUndo(const QSet<QUuid>& visibleUuids);
	
	bool hasSelection() const;
	std::vector<int> getSelectedIDs() const;

	// Selection helpers (for DuplicateCommand)
	QSet<QUuid> getSelectedUuids() const;

	void detachMaterialPanel();
	void reattachMaterialPanel();
	void detachTransformationsPanel();
	void reattachTransformationsPanel();
	void detachEnvironmentPanel();
	void reattachEnvironmentPanel();
	void detachNavigationPanel();
	void reattachNavigationPanel();

public slots:
	void updateDisplayList();
	void updateSelectionStatusMessage();
	void showAllItems();
	void showSelectedItems();
	void showOnlySelectedItems();
	void hideAllItems();
	void hideSelectedItems();
	void centerScreen();
	void duplicateSelectedItems();
	void deleteSelectedItems();
	void generateUVsForSelectedItems();
	void displaySelectedMeshInfo();
	void showVisualizationModelPage();
	void showEnvironmentPage();
	void showPredefinedMaterialsPage();
	void showTransformationsPage();
	void onDisplayModeChanged(int mode);
	void onTextureCacheCleared();

private slots:
	void setListRow(int index);
	void setListRows(QList<int> indices);
	void showContextMenu(const QPoint& pos);
	void lightingType_toggled(QAbstractButton*, bool);

	void onFileImport();
	void importFiles(QStringList& fileNames);
	void onFileExport();

	void on_checkBoxSelectAll_stateChanged(int arg1);

	void handleTreeWidgetVisibilityChanged();
	void handleTreeWidgetSelectionChanged();
	void handleTreeWidgetMeshRenamed(const QUuid& uuid, const QString& newName);

	void on_tabWidgetVizAttribs_currentChanged(int index);

	void on_toolButtonClearOpacityTex_clicked();

	void onPredefinedMaterialSelected(const GLMaterial& mat);
	void onCustomMaterialApplied(const GLMaterial& mat);

	void onTexturesApplied(const GLMaterial* mat = nullptr);


protected:
	void showEvent(QShowEvent* event);
	bool eventFilter(QObject* watched, QEvent* event) override;
	void keyPressEvent(QKeyEvent* event);
	void dragEnterEvent(QDragEnterEvent* event);
	void dropEvent(QDropEvent* event);
	void resizeEvent(QResizeEvent* event);
	void mouseMoveEvent(QMouseEvent* event);

private:
	void updateNavigationOverlayGeometry();

	void checkAndRenameModel(TriangleMesh* mesh, const QString& name);
	QString computeUniqueName(TriangleMesh* exclude, const QString& name) const;
	bool checkForActiveSelection();
	Mvf::MVFPackage buildMVFPackage() const;
			
	void updateControls();
	QString getSupportedQtImagesFilter();

	// Cleanup methods
	void setupUndoStackMonitoring();
	void onUndoStackChanged();
	void cleanupOrphanedMeshes();
	void cleanupUnsavedMaterialsFromLibrary();
	QSet<QUuid> scanStackForReferencedUuids();
	QSet<QUuid> collectVisibleUuidsFromDisplayList() const;
	std::vector<int> visibleIndicesFromState() const;
	void updateVisibilityUiFromState();
	void scheduleTreeRebuild(int delayMs = 1200);
	void rebuildTreeFromCurrentState();
	void applyVisibleMeshState(bool syncTree,
	                           bool deferTreeSync = false,
	                           const QSet<QUuid>& changedUuids = {});
	void scheduleTreeVisibilitySync(int delayMs = 900);
	void syncTreeVisibilityFromModel();

private:
	GLWidget*   _glWidget;
	SceneGraph* _sceneGraph;

	GLMaterial _material;

	bool _bHasTexture;

	QString _albedoPBRTexture;
	QString _metallicPBRTexture;
	QString _roughnessPBRTexture;
	QString _normalPBRTexture;
	QString _aoPBRTexture;
	QString _opacityPBRTexture;
	QString _heightPBRTexture;
	bool    _hasPBRAlbedoTex;
	bool    _hasPBRMetallicTex;
	bool    _hasPBRRoughnessTex;
	bool    _hasPBRNormalTex;
	bool    _hasPBRAOTex;
	bool    _hasPBROpacTex;
	bool    _hasPBRHeightTex;
	float   _heightPBRTexScale;

	bool _runningFirstTime;

	QString _currentFile;
	bool _textureDirOpenedFirstTime;
	bool _documentSaved;
	bool _documentModified;

	bool _progressiveLoadingEnabled = false;

	static QString _lastOpenedDir;
	static QString _lastSelectedFilter;

	int _skyBoxLDRIIndex = 0;
	int _skyBoxHDRIIndex = 0;

	QPointer<QDialog> _detachedMaterialDialog; // Stores the floating dialog
	QWidget* _materialOriginalParent = nullptr; // Stores the scroll area
	int _materialPageIndex = -1;
	QString _materialPageLabel;

	QPointer<QDialog> _detachedTransformationsDialog; // Stores the floating dialog
	QWidget* _transformationsOriginalParent = nullptr; // Stores the scroll area
	int _transformationsPageIndex = -1;
	QString _transformationsPageLabel;

	QPointer<QDialog> _detachedEnvironmentDialog; // Stores the floating dialog
	QWidget* _environmentOriginalParent = nullptr; // Stores the scroll area
	int _environmentPageIndex = -1;
	QString _environmentPageLabel;

	QPointer<QWidget> _detachedNavigationOverlay; // Stores the GLWidget overlay wrapper
	QList<int> _navigationSplitterSizes;         // Saved splitter proportions for restore

	QUndoStack* _undoStack;
	bool _lastCanUndo = false;
	bool _lastCanRedo = false;
	// Cleanup optimization
	int _lastStackCount = 0;
	QSet<QUuid> _cachedReferencedUuids;  // Meshes referenced in undo stack
	QSet<QUuid> _visibleMeshUuids;       // Authoritative visible mesh state
	int _treeRebuildGeneration = 0;
	bool _treeRebuildPending = false;
	int _treeVisibilitySyncGeneration = 0;
	bool _treeVisibilityDirty = false;

	// Material cache - MDI-scoped, auto-destroyed when MDI closes
	QMap<QString, CachedMaterial> _materialCache;  // Maps material keys to cached materials with metadata
	QSet<QString> _ownedUnsavedMaterials;  // Tracks unsaved materials created by this MDI (for cleanup)
};

#endif
