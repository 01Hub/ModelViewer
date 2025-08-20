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


class GLMaterial;
class MaterialPreviewWidget;

namespace Ui { class TextureMappingPanel; }

class TextureMappingPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TextureMappingPanel(QWidget* parent = nullptr);
    ~TextureMappingPanel() override;

    // Provide the material we’re editing
    void bindMaterial(GLMaterial* material);

signals:
    void materialChanged();

private:
    struct MapSlot
    {
        QPushButton* button = nullptr;   // square thumbnail button
        QLabel* label = nullptr;   // text under the button
        QToolButton* gear = nullptr;   // optional (null if not packable)
        QString      key;                // e.g., "albedo", "roughness"
    };

    // --- helpers (methods have NO underscore prefix) ---
    void registerMaps();
    void connectSignals();
    void applyButtonEmptyIcon(MapSlot& m);
    void applyButtonImageIcon(MapSlot& m, const QString& file);
    QIcon makeIconFromFile(const QString& file, int edge = 90) const;
    static QIcon makeCheckerIcon(int w = 90, int h = 90, int cell = 8);

    // GLMaterial sync (rename to your exact API in the .cpp body)
    void setMapPath(const QString& key, const QString& file);
    void clearMap(const QString& key);
    QString mapPath(const QString& key) const;

    // Channel packing hook
    void openPackingDialogFor(const QString& key);

    // UV + preview
    void onUVChanged();
    void updatePreview();

private:
    Ui::TextureMappingPanel* _ui = nullptr;
    GLMaterial* _material = nullptr;
    MaterialPreviewWidget* _preview = nullptr;

    QHash<QString, MapSlot> _maps;   // by key
    QIcon _checkerIcon;
};
