#ifndef __MODELVIEWER_H__
#define __MODELVIEWER_H__

#include "ui_ModelViewer.h"

#include "GLWidget.h"
#include "GLMaterial.h"
#include "UVPromptDialog.h"
#include "AssImpModelLoader.h"

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

	bool hasUndo();
	bool hasRedo();
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
	void lightingType_toggled(QAbstractButton *, bool);
		
	void on_pushButtonDefaultLights_clicked();
	void on_pushButtonApplyADSColors_clicked();	
	void on_pushButtonApplyTransformations_clicked();
	void on_pushButtonBakeTransformations_clicked();
	void on_pushButtonResetTransformations_clicked();	
	void on_pushButtonLightAmbient_clicked();
	void on_pushButtonLightDiffuse_clicked();
	void on_pushButtonLightSpecular_clicked();	
	void on_sliderLightPosX_valueChanged(int);
	void on_sliderLightPosY_valueChanged(int);
	void on_sliderLightPosZ_valueChanged(int);	
	void on_sliderTransparency_valueChanged(int value);
	void on_sliderShine_valueChanged(int value);

	void onFileImport();
	void importFiles(QStringList& fileNames);
	void onFileExport();
	
	void on_checkBoxSelectAll_stateChanged(int arg1);

	void on_listWidgetModel_itemChanged(QListWidgetItem*);
	void on_listWidgetModel_itemSelectionChanged();
	void itemEdited(QWidget* widget, QAbstractItemDelegate::EndEditHint hint);

	void on_checkBoxShadowMapping_toggled(bool checked);
	void on_checkBoxSelfShadows_toggled(bool checked);
	void on_checkBoxEnvMapping_toggled(bool checked);
	void on_checkBoxSkyBox_toggled(bool checked);
	void on_checkBoxReflections_toggled(bool checked);
	void on_checkBoxFloor_toggled(bool checked);
	void on_checkBoxFloorTexture_toggled(bool checked);
	void on_pushButtonFloorTexture_clicked();
	void on_toolBox_currentChanged(int index);

	void on_pushButtonSkyBoxTex_clicked();
	
	void on_toolButtonClearOpacityTex_clicked();

	void on_pushButtonApplyADSTexture_clicked();
	void on_pushButtonClearADSTextures_clicked();
		
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
	void loadSkyBoxPresetMaps();

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

private:
	bool checkForActiveSelection();
	bool hasSelection() const;
	std::vector<int> getSelectedIDs() const;
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
};

#endif
