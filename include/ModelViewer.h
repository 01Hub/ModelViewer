#ifndef __MODELVIEWER_H__
#define __MODELVIEWER_H__

#include "ui_ModelViewer.h"

#include "GLWidget.h"
#include "GLMaterial.h"
#include "UVPromptDialog.h"
#include "AssImpModelLoader.h"
#include "SetMaterialCommand.h"

#include <QUndoStack>

struct UVDialogResult
{
	UVMethod method = UVMethod::None;
};

class ModelViewer : public QWidget, public Ui::ModelViewer
{
	Q_OBJECT
public:
	ModelViewer(QWidget* parent = 0);
	~ModelViewer();

	void retranslateUI();

	GLWidget* getGLView() const { return _glWidget; }

	void setMaterialToSelectedItems(const GLMaterial& mat);
	void setTexturesToSelectedItems(const GLMaterial& mat);
	void setTextureSamplersToSelectedItems(const GLMaterial* material, GLMaterial::TextureType type);

	void setTransformation();
	void bakeTransformations();
	void resetTransformation();

	QListWidget* getListModel() { return listWidgetModel; }

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
	QUndoStack* getUndoStack() const { return m_undoStack; }

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

	void applyADSColors();

	void onFileImport();
	void importFiles(QStringList& fileNames);
	void onFileExport();

	void on_checkBoxSelectAll_stateChanged(int arg1);

	void on_listWidgetModel_itemChanged(QListWidgetItem*);
	void on_listWidgetModel_itemSelectionChanged();
	void itemEdited(QWidget* widget, QAbstractItemDelegate::EndEditHint hint);

	void on_toolBox_currentChanged(int index);

	void on_toolButtonClearOpacityTex_clicked();

	void applyADSTextures();
	void clearADSTextures();

	void onPredefinedMaterialSelected(const GLMaterial& mat);
	void onCustomMaterialApplied(const GLMaterial& mat);

protected:
	void showEvent(QShowEvent* event);
	void keyPressEvent(QKeyEvent* event);
	void dragEnterEvent(QDragEnterEvent* event);
	void dropEvent(QDropEvent* event);
	void resizeEvent(QResizeEvent* event);
	void mouseMoveEvent(QMouseEvent* event);
	void closeEvent(QCloseEvent* event);

private:
	void checkAndRenameModel(TriangleMesh* mesh, const QString& name);
	bool checkForActiveSelection();
	
	void updateControls();
	QString getSupportedQtImagesFilter();

	void detachADSMaterialPanel();
	void reattachADSMaterialPanel();
	void detachTexturePanel();
	void reattachTexturePanel();
	void detachMaterialPanel();
	void reattachMaterialPanel();
	void detachTransformationsPanel();
	void reattachTransformationsPanel();
	void detachEnvironmentPanel();
	void reattachEnvironmentPanel();

	// Cleanup methods
	void setupUndoStackMonitoring();
	void onUndoStackChanged();
	void cleanupOrphanedMeshes();
	QSet<QUuid> scanStackForReferencedUuids();

private:
	GLWidget* _glWidget;

	GLMaterial _material;

	bool _bHasTexture;

	QString _diffuseADSTexture;
	QString _specularADSTexture;
	QString _emissiveADSTexture;
	QString _normalADSTexture;
	QString _heightADSTexture;
	QString _opacityADSTexture;
	bool    _hasADSDiffuseTex;
	bool    _hasADSSpecularTex;
	bool    _hasADSEmissiveTex;
	bool    _hasADSNormalTex;
	bool    _hasADSHeightTex;
	bool    _hasADSOpacityTex;

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

	QPointer<QDialog> _detachedADSMaterialDialog;// Stores the floating dialog
	QWidget* _adsMaterialOriginalParent = nullptr; // Stores where panel came from
	int _adsMaterialPageIndex = -1;
	QString _adsMaterialPageLabel;

	QPointer<QDialog> _detachedTextureDialog;  // Stores the floating dialog
	QWidget* _textureOriginalParent = nullptr; // Stores where panel came from
	int _texturePageIndex = -1;
	QString _texturePageLabel;

	QPointer<QDialog> _detachedMaterialDialog; // Stores the floating dialog
	QWidget* _materialOriginalParent = nullptr; // Stores where panel came from		
	int _materialPageIndex = -1;
	QString _materialPageLabel;

	QPointer<QDialog> _detachedTransformationsDialog; // Stores the floating dialog
	QWidget* _transformationsOriginalParent = nullptr; // Stores where panel came from
	int _transformationsPageIndex = -1;
	QString _transformationsPageLabel;

	QPointer<QDialog> _detachedEnvironmentDialog; // Stores the floating dialog
	QWidget* _environmentOriginalParent = nullptr; // Stores where panel came from
	int _environmentPageIndex = -1;
	QString _environmentPageLabel;

	QUndoStack* m_undoStack;
	// Cleanup optimization
	int m_lastStackCount = 0;
	QSet<QUuid> m_cachedReferencedUuids;  // Meshes referenced in undo stack
};

#endif
