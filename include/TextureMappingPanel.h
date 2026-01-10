#pragma once
#include <QWidget>
#include <QIcon>
#include <QPointer>
#include <QHash>
#include <QPushButton>
#include <QLabel>
#include <QToolButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QMenu>
#include <QImage>
#include <QPainter>
#include <QVector3D>
#include <QDataStream>
#include <QFileDialog>
#include <QColorDialog>

#include "GLMaterial.h"

class MaterialPreviewWidget;

namespace Ui { class TextureMappingPanel; }

class TextureMappingPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TextureMappingPanel(QWidget* parent = nullptr);
    ~TextureMappingPanel() override;

    // Provide the material we're editing
    void bindMaterial(GLMaterial* material);

    GLMaterial* material() const { return _material; }

    void onTintParamsChanged();

signals:
    void materialChanged(const GLMaterial* material);
    void applyTexturesTriggered(const GLMaterial& material);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    struct MapSlot
    {
        QPushButton* button = nullptr;           // square thumbnail button
        QLabel* label = nullptr;                 // text under the button
        QToolButton* gear = nullptr;             // optional channel packing gear
        QToolButton* transformButton = nullptr;  // transform/sampler button
        QDoubleSpinBox* factorSpinBox = nullptr; // numeric factor spin box
        QPushButton* colorPickerButton = nullptr; // color picker for color factors
        QString key;                             // e.g., "albedo", "roughness"
        GLMaterial::TextureType type;            // enum type
    };

    // --- helpers ---
    void registerMaps();
    void connectSignals();
    void applyButtonEmptyIcon(MapSlot& m);
    void applyButtonImageIcon(MapSlot& m, const QString& file);
    QIcon makeIconFromFile(const QString& file, int edge = 90) const;
    static QIcon makeCheckerIcon(int w = 90, int h = 90, int cell = 8);

    // Helper: map string key to TextureType enum
    GLMaterial::TextureType keyToTextureType(const QString& key) const;

    // GLMaterial sync
    void setMapPath(const QString& key, const QString& file);
    void clearMap(const QString& key);
    void clearAllMaps();
    QString mapPath(const QString& key) const;

    // Load factor values from material into UI spin boxes
    void loadFactorValuesFromMaterial();

    // Channel packing hook
    void openPackingDialogFor(const QString& key);

    // Slot handlers for new functionality
    void onColorPickerClicked(GLMaterial::TextureType type);
    void onFactorChanged(GLMaterial::TextureType type);
    void onTransformButtonClicked(GLMaterial::TextureType type);

    void updatePreview();

    void applyMaterialPreset(const QString& presetName);

private:
    Ui::TextureMappingPanel* _ui = nullptr;
    GLMaterial* _material = nullptr;
    MaterialPreviewWidget* _preview = nullptr;

    QHash<QString, MapSlot> _maps;   // by key
    QIcon _checkerIcon;

    QString _lastUsedFolder;
};
