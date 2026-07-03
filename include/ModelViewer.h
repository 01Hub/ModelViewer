#pragma once


#include "ui_ModelViewer.h"

#include "ViewportWidget.h"
#include "Material.h"
#include "SceneGraph.h"
#include "SceneTreeWidget.h"
#include "UVPromptDialog.h"
#include "AssImpModelLoader.h"
#include "ApplyMaterialCommand.h"
#include "RenameMeshCommand.h"
#include "AnimationsPanel.h"
#include "CamerasPanel.h"
#include "ExplodedViewPanel.h"
#include "MaterialPropertiesPanel.h"
#include "SceneClipboard.h"
#include "CutCommand.h"
#include "MaterialVariantsPanel.h"
#include "TextureDebugPanel.h"

#include <QUndoStack>

class QTabWidget;

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

	ViewportWidget*    getViewportWidget()    const { return _viewportWidget; }
	SceneGraph*  sceneGraph()   const { return _sceneGraph; }
	QMap<QString, CachedMaterial>* getMaterialCache() { return &_materialCache; }
	void registerOwnedUnsavedMaterial(const QString& materialKey) { _ownedUnsavedMaterials.insert(materialKey); }

	void setMaterialToSelectedItems(const Material& mat);
	void setTexturesToSelectedItems(const Material& mat);
	void setTextureSamplersToSelectedItems(const Material* material, Material::TextureType type);

	void setTransformation();
	void resetTransformation();
	void syncLightPositionUiToScene();

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
	void markNonUndoDocumentModified();

	bool save();
	bool saveAs();

	void closeEvent(QCloseEvent* event);

	void setDocumentSaved(bool saved = true);
	bool isDocumentSaved() const { return _documentSaved; }

	void selectAll();
	void deselectAll();
	void deselectAllWithUndo();

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

	// Opens (or raises) the Texture Debug Panel for the current selection.
	// Called by MainWindow when Tools → Texture Debugger is triggered.
	void showTextureDebugPanel();

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

signals:
	void documentModifiedChanged(bool modified);

public:
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
	void updateNavigationOverlayGeometry();

	// Apply a named variant to all meshes from the given source file.
	// variantIndex = -1 resets to the file's default material assignments.
	void applyVariant(const QString& sourceFile, int variantIndex);

public slots:
	void updateDisplayList();
	void updateSelectionStatusMessage();
	void showAllItems();
	void showSelectedItems();
	void showOnlySelectedItems();
	void hideAllItems();
	void hideSelectedItems();
	void centerScreen();
	void copySelectedItems();
	void cutSelectedItems();
	void pasteIntoSelectedNode(const SceneNode* targetNode);
	void duplicateSelectedItems();

	// Called by CutCommand and PasteCommand to manage cut-mark state.
	void clearCutMarks();
	void reapplyCutMarks(const QList<ClipboardEntry>& entries,
	                     const QSet<QUuid>& meshUuids,
	                     const QSet<QUuid>& nodeUuids);
	void deleteSelectedItems();
	void generateUVsForSelectedItems();
	void displaySelectedMeshInfo();
	void editMeshMaterial();
	void showVisualizationModelPage();
	void showEnvironmentPage();
	void showPredefinedMaterialsPage();
	void showTransformationsPage();
	void onDisplayModeChanged(int mode);
	void onTextureCacheCleared();
	void onRenderingModeSelected(const QString& mode);

private slots:
	void setListRow(int index);
	void setListRows(QList<int> indices);
	void showContextMenu(const QPoint& pos);

	void onFileImport();

	// Validates the cut clipboard whenever the scene structure changes.
	// Invalidates (clears) the clipboard if any cut source is no longer present.
	void validateCutClipboard();
	void importFiles(QStringList& fileNames);
	void onFileExport();

	void on_checkBoxSelectAll_stateChanged(int arg1);

	void handleTreeWidgetVisibilityChanged();
	void handleTreeWidgetSelectionChanged();
	void handleTreeWidgetMeshRenamed(const QUuid& uuid, const QString& newName);

	void on_tabWidgetVizAttribs_currentChanged(int index);

	void onPredefinedMaterialSelected(const Material& mat);
	void onCustomMaterialApplied(const Material& mat);

	void onTexturesApplied(const Material* mat = nullptr);


protected:
	void showEvent(QShowEvent* event);
	bool eventFilter(QObject* watched, QEvent* event) override;
	void keyPressEvent(QKeyEvent* event);
	void dragEnterEvent(QDragEnterEvent* event);
	void dropEvent(QDropEvent* event);
	void resizeEvent(QResizeEvent* event);
	void mouseMoveEvent(QMouseEvent* event);

private:
	void ensureDockedNavigationHeader();
	void placeNavigationContentInHost(QWidget* navigationContent, QWidget* hostParent, QLayout* hostLayout);

	// Rebuild the inner navigation sub-tab widget based on what the currently
	// loaded model(s) support: Variants, Animations, and/or Cameras.
	// The inner tab widget is created on demand and destroyed when no optional
	// panels are needed, leaving the layout exactly as setupUi() made it.
	void refreshNavigationSubTabs();

	// Called when the inner sub-tab selection changes.
	void onInnerNavTabChanged(int index);

	void checkAndRenameModel(SceneMesh* mesh, const QString& name);
	QString computeUniqueName(SceneMesh* exclude, const QString& name) const;
	bool checkForActiveSelection();
	Mvf::MVFPackage buildMVFPackage() const;
			
	void updateControls();
	QString getSupportedQtImagesFilter();

	// Cleanup methods
	void setupUndoStackMonitoring();
	void onUndoStackChanged();
	bool undoCommandAffectsDocument(const QUndoCommand* command) const;
	bool hasUnsavedUndoDocumentChanges() const;
	void cleanupOrphanedMeshes();
	void validateVariantData();   // removes variant data for files with no remaining meshes
	void validateAnimationData(); // removes animation data for files with no remaining meshes
	void validateCameraData();    // removes camera data for files with no remaining meshes
	void validateLightData();     // removes punctual-light data for files with no remaining meshes
	bool saveMaterialsBeforeClose();  // Save all unsaved materials to library before closing
	void cleanupUnsavedMaterialsFromLibrary();
	QSet<QUuid> scanStackForReferencedUuids();
	QSet<QUuid> collectVisibleUuidsFromDisplayList() const;
	std::vector<int> visibleIndicesFromState() const;
	void updateVisibilityUiFromState();
	void invalidateCutClipboard();  // clears cut clipboard + tree marks
	void scheduleTreeRebuild(int delayMs = 1200);
	void rebuildTreeFromCurrentState();
	void applyVisibleMeshState(bool syncTree,
	                           bool deferTreeSync = false,
	                           const QSet<QUuid>& changedUuids = {});
	void scheduleTreeVisibilitySync(int delayMs = 900);
	void syncTreeVisibilityFromModel();

private:
	ViewportWidget*   _viewportWidget;
	SceneGraph* _sceneGraph;

	Material _material;

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
	bool _animateProgressiveFitEnabled = true;

	static QString _lastOpenedDir;
	static QString _lastSelectedFilter;

	int _skyBoxLDRIIndex = 0;
	int _skyBoxHDRIIndex = 0;

	QPointer<QDialog> _detachedMaterialDialog; // Stores the floating dialog
	QWidget* _materialOriginalParent = nullptr; // Stores the scroll area
	int _materialPageIndex = -1;
	QString _materialPageLabel;

	// Preview widget container (keeps preview in main thread when panel is detached)
	QWidget* _materialPreviewContainer = nullptr; // Container holding preview widget while panel is detached
	QLayout* _materialPreviewLayout = nullptr; // Original layout of preview tools
	int _materialPreviewContainerTabIndex = -1; // Track where we inserted the preview tab

	QPointer<QDialog> _detachedTransformationsDialog; // Stores the floating dialog
	QWidget* _transformationsOriginalParent = nullptr; // Stores the scroll area
	int _transformationsPageIndex = -1;
	QString _transformationsPageLabel;

	QPointer<QDialog> _detachedEnvironmentDialog; // Stores the floating dialog
	QWidget* _environmentOriginalParent = nullptr; // Stores the scroll area
	int _environmentPageIndex = -1;
	QString _environmentPageLabel;

	QPointer<QWidget> _detachedNavigationOverlay;
	QPointer<QWidget> _dockedNavigationHeader;
	int _navigationPageIndex = -1;
	QString _navigationPageLabel;

	// Optional navigation sub-tabs (Variants, Animations, Cameras).
	// _innerTabWidget is null at startup. When at least one optional panel is
	// needed it is created on the fly: modelNavigationWidget is reparented into
	// it as tab 0 and the relevant panels follow in order. When all optional
	// panels are removed the process is reversed and the tab widget is destroyed,
	// leaving the layout exactly as it was originally.
	QTabWidget*            _innerTabWidget  = nullptr;
	MaterialVariantsPanel* _variantsPanel   = nullptr;
	AnimationsPanel*       _animationsPanel    = nullptr;
	CamerasPanel*          _camerasPanel       = nullptr;
	TextureDebugPanel*     _textureDebugPanel  = nullptr;

	QUndoStack* _undoStack;
	bool _lastCanUndo = false;
	bool _lastCanRedo = false;
	int _lastUndoIndex = 0;
	int _savedUndoIndex = 0;
	bool _nonUndoDocumentDirty = false;
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

	QUuid _currentEditingMeshUuid;  // UUID of mesh being edited (null if not editing)

	QList<ClipboardEntry> _clipboard;  // copy-paste clipboard (non-undoable)
};
