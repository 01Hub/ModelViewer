#ifndef __ADSMATERIALSETTINGSPANEL_H__
#define __ADSMATERIALSETTINGSPANEL_H__

#include "ui_ADSMaterialSettingsPanel.h"
#include <QWidget>
#include <QVector3D>
#include <QVector4D>

// Forward declarations
class ModelViewer;
class GLWidget;
class GLMaterial;

class ADSMaterialSettingsPanel : public QWidget, public Ui::ADSMaterialSettingsPanel
{
    Q_OBJECT

public:
    explicit ADSMaterialSettingsPanel(QWidget* parent = nullptr);
    ~ADSMaterialSettingsPanel();

    /// Initialize the panel with references to parent classes
    void initialize(ModelViewer* modelViewer, GLWidget* glWidget, GLMaterial* material);

    /// Update material-related controls based on current material state
    void updateMaterialButtonStyles();

    /// Update slider states and labels based on material properties
    void updateMaterialPropertySliders();

    /// Update selection context - enable/disable material property controls
    void setSelectionState(bool hasSelection);

    /// Update texture thumbnail previews
    void updateTexturePreviews();

	bool isDetached() const { return _detached; }
    void setDetached(bool detached);

	GLMaterial* getMaterial() const { return _material; }

    // Color getters
    QVector3D getAmbientColor() const;
    QVector3D getDiffuseColor() const;
    QVector3D getSpecularColor() const;
    QVector3D getEmissiveColor() const;
    float getOpacity() const;
    int getShininess() const;

    // Texture path getters
    QString getDiffuseTexturePath() const;
    QString getSpecularTexturePath() const;
    QString getNormalTexturePath() const;
    QString getEmissiveTexturePath() const;
    QString getHeightTexturePath() const;
    QString getOpacityTexturePath() const;
    bool isOpacityTextureInverted() const;

signals:
    // Material color signals
    void materialAmbientChanged(const QVector3D& color);
    void materialDiffuseChanged(const QVector3D& color);
    void materialSpecularChanged(const QVector3D& color);
    void materialEmissiveChanged(const QVector3D& color);

    // Material property signals
    void opacityChanged(float opacity);
    void shininessChanged(int shine);
    void opacitySliderReleased();
    void shininessSliderReleased();

    // Texture signals
    void diffuseTextureChanged(const QString& path);
    void specularTextureChanged(const QString& path);
    void normalTextureChanged(const QString& path);
    void emissiveTextureChanged(const QString& path);
    void heightTextureChanged(const QString& path);
    void opacityTextureChanged(const QString& path);
    void opacityTextureInverted(bool inverted);
    
    // Batch texture operations
    void applyTexturesRequested();
    void clearTexturesRequested();

    // Material reset signal
    void defaultMaterialsRequested();

    // Material application signal
    void applyColorToSelectionRequested();

    // UI signals
    void detachRequested();

private slots:
    // Material color button slots
    void onMaterialAmbientClicked();
    void onMaterialDiffuseClicked();
    void onMaterialSpecularClicked();
    void onMaterialEmissiveClicked();

    // Material property slider slots
    void onTransparencyChanged(int value);
    void onShineChanged(int value);

    // Texture checkbox slots
    void onDiffuseTexToggled(bool checked);
    void onSpecularTexToggled(bool checked);
    void onNormalTexToggled(bool checked);
    void onEmissiveTexToggled(bool checked);
    void onHeightTexToggled(bool checked);
    void onOpacityTexToggled(bool checked);
    void onOpacityInvertToggled(bool checked);

    // Texture selection button slots
    void onSelectDiffuseTexture();
    void onSelectSpecularTexture();
    void onSelectNormalTexture();
    void onSelectEmissiveTexture();
    void onSelectHeightTexture();
    void onSelectOpacityTexture();

    // Texture clear button slots
    void onClearDiffuseTexture();
    void onClearSpecularTexture();
    void onClearNormalTexture();
    void onClearEmissiveTexture();
    void onClearHeightTexture();
    void onClearOpacityTexture();

    // Action button slots
    void onApplyColorsClicked();
    void onDefaultMaterialsClicked();
    void onApplyTexturesClicked();
    void onClearTexturesClicked();

private:
    // Helper methods
    void connectSignalsAndSlots();
    void updateButtonStyleSheet(QPushButton* button, const QVector3D& color);
    QString selectTextureFile();
    void loadTexturePreview(QLabel* previewLabel, const QString& texturePath);
    void updateTextureControlState(QCheckBox* checkBox, QPushButton* selectButton, 
                                   QLabel* preview, QToolButton* clearButton, bool enabled);

    // Member variables
    ModelViewer* _modelViewer;
    GLWidget* _glWidget;
    GLMaterial* _material;

    // Texture file paths (cached)
    QString _diffuseTexPath;
    QString _specularTexPath;
    QString _normalTexPath;
    QString _emissiveTexPath;
    QString _heightTexPath;
    QString _opacityTexPath;

    // State flags
    bool _isInitialized;
    bool _hasActiveSelection;
    bool _updateInProgress;

    bool _detached = false;
};

#endif // __ADSMATERIALSETTINGSPANEL_H__
