#include "TextureMappingPanel.h"
#include "ui_TextureMappingPanel.h"

#include <QFileDialog>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QSlider>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

#include "config.h"
#include "GLMaterial.h"
#include "MaterialPreviewWidget.h"
#include "ChannelPackingEditorDialog.h"
#include "MaterialTextureLibrary.h"

// ---------------- ctor / dtor ----------------
TextureMappingPanel::TextureMappingPanel(QWidget* parent)
    : QWidget(parent)
    , _ui(new Ui::TextureMappingPanel)
{
    _ui->setupUi(this);
    _preview = qobject_cast<MaterialPreviewWidget*>(_ui->previewWidget);
    _preview->setPreviewProfile(PreviewProfile::TextureAuthoring);

    _checkerIcon = makeCheckerIcon();
    registerMaps();
    connectSignals();

    const MaterialsMap& mats = MaterialTextureLibrary::instance().materials();
    MaterialScanner::populateWithMaterials(_ui->treeWidgetPresetTextures, mats);

    QTimer::singleShot(0, this, [this] {
        // Find first material item (not category)
        for (int i = 0; i < _ui->treeWidgetPresetTextures->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem* category = _ui->treeWidgetPresetTextures->topLevelItem(i);
            if (category->childCount() > 0)
            {
                QTreeWidgetItem* firstMaterial = category->child(0);
                _ui->treeWidgetPresetTextures->setCurrentItem(firstMaterial);
                QString materialName = firstMaterial->data(0, Qt::UserRole).toString();
                applyMaterialPreset(materialName);
                break;
            }
        }
        });

    // default checker on all buttons
    for (auto it = _maps.begin(); it != _maps.end(); ++it)
        applyButtonEmptyIcon(it.value());

    // enable drag/drop on the texture buttons
    for (auto it = _maps.begin(); it != _maps.end(); ++it)
    {
        if (it.value().button)
        {
            it.value().button->setAcceptDrops(true);
            it.value().button->installEventFilter(this);
        }
    }

    // Initialize the material to default values and bind it    
    bindMaterial(new GLMaterial());
}

TextureMappingPanel::~TextureMappingPanel()
{
    delete _ui;
}

// ---------------- public ----------------
void TextureMappingPanel::bindMaterial(GLMaterial* material)
{
    _material = material;

    // reflect current textures -> button icons
    for (auto it = _maps.begin(); it != _maps.end(); ++it)
    {
        const QString path = mapPath(it.key());
        if (path.isEmpty()) applyButtonEmptyIcon(it.value());
        else                applyButtonImageIcon(it.value(), path);
    }

    _preview->setMaterial(*_material);
    updatePreview();
}

void TextureMappingPanel::onTintParamsChanged()
{
    GLMaterial* m = _material;
    if (!m) return;

    m->albedoTint.mode = static_cast<GLMaterial::TintMode>(_ui->tintModeCombo->currentIndex());
    m->albedoTint.strength = float(_ui->tintStrengthSpin->value());
    m->albedoTint.grayEps = float(_ui->grayEpsSpin->value());
    m->albedoTint.useVertexColor = _ui->useVtxColorCheck->isChecked();
    m->albedoTint.maskChannel = _ui->maskChannelCombo->currentIndex();

    // Enable/disable mask channel control
    _ui->maskChannelCombo->setEnabled(m->albedoTint.mode == GLMaterial::TintMode::LerpMask);

    emit materialChanged(m);
}


// ---------------- registry / wiring ----------------
void TextureMappingPanel::registerMaps()
{
    // Main
    _maps.insert("albedo", { _ui->btnAlbedo,      _ui->lblAlbedo,      nullptr,           "albedo" });
    _maps.insert("normal", { _ui->btnNormal,      _ui->lblNormal,      nullptr,           "normal" });
    _maps.insert("emissive", { _ui->btnEmissive,    _ui->lblEmissive,    nullptr,           "emissive" });
    _maps.insert("metallic", { _ui->btnMetallic,    _ui->lblMetallic,    _ui->gearMetallic, "metallic" });
    _maps.insert("roughness", { _ui->btnRoughness,   _ui->lblRoughness,   _ui->gearRoughness,"roughness" });
    _maps.insert("ao", { _ui->btnAO,          _ui->lblAO,          _ui->gearAO,       "ao" });
    _maps.insert("opacity", { _ui->btnOpacity,     _ui->lblOpacity,     _ui->gearOpacity,  "opacity" });
    _maps.insert("height", { _ui->btnHeight,      _ui->lblHeight,      nullptr,           "height" });
    _maps.insert("transmission", { _ui->btnTransmission,_ui->lblTransmission,nullptr,           "transmission" });
    _maps.insert("ior", { _ui->btnIOR,         _ui->lblIOR,         nullptr,           "ior" });

    // Sheen
    _maps.insert("sheen_color", { _ui->btnSheenColor,  _ui->lblSheenColor,  nullptr,           "sheen_color" });
    _maps.insert("sheen_rough", { _ui->btnSheenRough,  _ui->lblSheenRough,  nullptr,           "sheen_rough" });

    // Clearcoat
    _maps.insert("cc_color", { _ui->btnCCColor,     _ui->lblCCColor,     nullptr,           "cc_color" });
    _maps.insert("cc_rough", { _ui->btnCCRough,     _ui->lblCCRough,     nullptr,           "cc_rough" });
    _maps.insert("cc_normal", { _ui->btnCCNormal,    _ui->lblCCNormal,    nullptr,           "cc_normal" });

    // Iridescence
    _maps.insert("iridescence", { _ui->btnIridColor,    _ui->lblIridColor,    nullptr,           "iridescence" });
    _maps.insert("iridescence_thickness", { _ui->btnIridRough,    _ui->lblIridRough,    nullptr,           "iridescence_thickness" });

    // Specular
    _maps.insert("specular_factor", { _ui->btnSpecFactorColor,    _ui->lblSpecFactorColor,    nullptr,           "specular_factor" });
    _maps.insert("specular_color", { _ui->btnSpecColorColor,     _ui->lblSpecColorColor,     nullptr,           "specular_color" });

    // Anisotropy
    _maps.insert("anisotropy", { _ui->btnAnisotropyColor,    _ui->lblAnisotropyColor,    nullptr,           "anisotropy" });

    // Diffuse Transmission
    _maps.insert("diffuse_transmission", { _ui->btnDiffuseTrans,      _ui->lblDiffuseTrans,      nullptr,           "diffuse_transmission" });
    _maps.insert("diffuse_transmission_color", { _ui->btnDiffuseTransColor, _ui->lblDiffuseTransColor, nullptr,           "diffuse_transmission_color" });

    // Volume
    _maps.insert("thickness", { _ui->btnThicknessColor,   _ui->lblThicknessColor,   nullptr,           "thickness" });
}

void TextureMappingPanel::connectSignals()
{
    auto wire = [this](const QString& key) {
        auto& m = _maps[key];

        // Left-click = Replace
        connect(m.button, &QPushButton::clicked, this, [this, btn = m.button] {
            // find key by pointer
            QString k;
            for (auto it = _maps.begin(); it != _maps.end(); ++it)
                if (it.value().button == btn) { k = it.key(); break; }
            if (k.isEmpty()) return;

            QString texFolder = _lastUsedFolder;
            if (texFolder.isEmpty())
            {
                const QString path = QString(MODELVIEWER_DATA_DIR) + "/";
                texFolder = path + "textures/materials/";
            }

            const QString file = QFileDialog::getOpenFileName(
                this, tr("Select %1 Map").arg(k.at(0).toUpper() + k.mid(1)),
                texFolder,
                tr("Images (*.png *.jpg *.jpeg *.tga *.bmp *.hdr *.exr)"));
            if (file.isEmpty()) return;

            applyButtonImageIcon(_maps[k], file);
            setMapPath(k, file);
            updatePreview();
            emit materialChanged(_material);
            });

        // Right-click context menu
        m.button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m.button, &QWidget::customContextMenuRequested, this, [this, key, btn = m.button](const QPoint& pos) {
            QMenu menu(btn);
            if (_maps[key].gear)
            {
                menu.addAction(tr("Channel Packing..."), this, [this, key] { openPackingDialogFor(key); });
                menu.addSeparator();
            }
            menu.addAction(tr("Replace..."), this, [this, btn] { btn->click(); });
            menu.addAction(tr("Clear"), this, [this, key] {
                applyButtonEmptyIcon(_maps[key]);
                clearMap(key);
                updatePreview();
                emit materialChanged(_material);
                });
            menu.exec(btn->mapToGlobal(pos));
            });

        // Gear (if present)
        if (m.gear)
            connect(m.gear, &QToolButton::clicked, this, [this, key] { openPackingDialogFor(key); });
        };

    for (auto it = _maps.begin(); it != _maps.end(); ++it)
        wire(it.key());

    // Preview controls
    connect(_ui->comboShape, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { updatePreview(); });
    connect(_ui->comboEnv, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { updatePreview(); });
    connect(_ui->sliderExposure, &QSlider::valueChanged, this, [this](int) { updatePreview(); });

    connect(this, &TextureMappingPanel::materialChanged, this, [this]() {
        // Update the preview when the material changes
        _preview->setMaterial(*_material);
        });

    connect(_ui->comboBoxTexMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int idx) {
            _preview->setTextureViewMode(static_cast<TexViewMode>(idx));
        });

    connect(_ui->pushButtonApply, &QPushButton::clicked, this, [this]() {
        emit applyTexturesTriggered(*_material);
        });

    connect(_ui->tintModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &TextureMappingPanel::onTintParamsChanged);
    connect(_ui->tintStrengthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &TextureMappingPanel::onTintParamsChanged);
    connect(_ui->grayEpsSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &TextureMappingPanel::onTintParamsChanged);
    connect(_ui->useVtxColorCheck, &QCheckBox::toggled,
        this, &TextureMappingPanel::onTintParamsChanged);
    connect(_ui->maskChannelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &TextureMappingPanel::onTintParamsChanged);

    connect(_ui->treeWidgetPresetTextures, &QTreeWidget::itemDoubleClicked,
        this, [this](QTreeWidgetItem* item, int column) {
            Q_UNUSED(column);
            if (!item) return;

            // Only apply if it's a material item (has parent = category)
            if (!item->parent()) return;

            QString materialName = item->data(0, Qt::UserRole).toString();
            if (materialName.isEmpty()) return;

            applyMaterialPreset(materialName);
        });

    connect(_ui->lineEditSearchPreset, &QLineEdit::textChanged,
        this, [this](const QString& text) {
            bool searchEmpty = text.trimmed().isEmpty();

            // Iterate through all categories
            for (int i = 0; i < _ui->treeWidgetPresetTextures->topLevelItemCount(); ++i)
            {
                QTreeWidgetItem* category = _ui->treeWidgetPresetTextures->topLevelItem(i);
                bool categoryHasVisibleChildren = false;

                // Check each material in the category
                for (int j = 0; j < category->childCount(); ++j)
                {
                    QTreeWidgetItem* material = category->child(j);
                    QString materialName = material->data(0, Qt::UserRole).toString();

                    // Show/hide based on search match
                    bool matches = searchEmpty || materialName.contains(text, Qt::CaseInsensitive);
                    material->setHidden(!matches);

                    if (matches)
                        categoryHasVisibleChildren = true;
                }

                // Hide category if no children match (optional)
                // category->setHidden(!categoryHasVisibleChildren && !searchEmpty);

                // Or always show categories but expand/collapse based on matches
                if (categoryHasVisibleChildren && !searchEmpty)
                {
                    category->setExpanded(true);
                }
                else if (searchEmpty)
                {
                    // Reset to default expansion state
                    category->setExpanded(category->childCount() <= 10);
                }
            }
        });

    connect(_ui->pushButtonClearAllMaps, &QPushButton::clicked, this, &TextureMappingPanel::clearAllMaps);
}

// ---------------- icons / thumbs ----------------
void TextureMappingPanel::applyButtonEmptyIcon(MapSlot& m)
{
    m.button->setIcon(_checkerIcon);
    m.button->setIconSize(QSize(90, 90));
}

void TextureMappingPanel::applyButtonImageIcon(MapSlot& m, const QString& file)
{
    m.button->setIcon(makeIconFromFile(file));
    m.button->setIconSize(QSize(90, 90));
}

QIcon TextureMappingPanel::makeIconFromFile(const QString& file, int edge) const
{
    QImage img(file);
    if (img.isNull()) return _checkerIcon;

    // square canvas, keep aspect
    QImage canvas(edge, edge, QImage::Format_RGBA8888);
    canvas.fill(QColor(40, 40, 40, 255));
    QPainter p(&canvas);
    const QSize scaled = img.size().scaled(edge - 2, edge - 2, Qt::KeepAspectRatio);
    const QPoint pos((edge - scaled.width()) / 2, (edge - scaled.height()) / 2);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(QRect(pos, scaled), img);
    p.end();
    return QIcon(QPixmap::fromImage(canvas));
}

QIcon TextureMappingPanel::makeCheckerIcon(int w, int h, int cell)
{
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    QColor a(58, 58, 58), b(78, 78, 78);
    for (int y = 0; y < h; y += cell)
        for (int x = 0; x < w; x += cell)
            p.fillRect(QRect(x, y, cell, cell), ((x / cell + y / cell) & 1) ? a : b);
    p.end();
    return QIcon(QPixmap::fromImage(img));
}

// ---------------- GLMaterial sync ----------------
// NOTE: Rename these to your real API if different.
void TextureMappingPanel::setMapPath(const QString& key, const QString& file)
{
    if (!_material) return;

    if (key == "albedo")         _material->setAlbedoMap(file);
    else if (key == "normal")    _material->setNormalMap(file);
    else if (key == "emissive")  _material->setEmissiveMap(file);
    else if (key == "metallic")  _material->setMetallicMap(file);
    else if (key == "roughness") _material->setRoughnessMap(file);
    else if (key == "ao")        _material->setAOMap(file);
    else if (key == "opacity")
    {
        _material->setOpacityMap(file);
        _material->setBlendMode(GLMaterial::BlendMode::Alpha);
    }
    else if (key == "height")    _material->setHeightMap(file);
    else if (key == "transmission") _material->setTransmissionMap(file);
    else if (key == "ior")          _material->setIORMap(file);
    else if (key == "sheen_color")  _material->setSheenColorMap(file);
    else if (key == "sheen_rough")  _material->setSheenRoughnessMap(file);
    else if (key == "cc_color")     _material->setClearcoatColorMap(file);
    else if (key == "cc_rough")     _material->setClearcoatRoughnessMap(file);
    else if (key == "cc_normal")    _material->setClearcoatNormalMap(file);
    else if (key == "iridescence")           _material->setIridescenceMap(file);
    else if (key == "iridescence_thickness") _material->setIridescenceThicknessMap(file);
    else if (key == "specular_factor")       _material->setSpecularFactorMap(file);
    else if (key == "specular_color")        _material->setSpecularColorMap(file);
    else if (key == "anisotropy")            _material->setAnisotropyMap(file);
    else if (key == "diffuse_transmission")  _material->setDiffuseTransmissionMap(file);
    else if (key == "diffuse_transmission_color") _material->setDiffuseTransmissionColorMap(file);
    else if (key == "thickness")             _material->setThicknessMap(file);
}

void TextureMappingPanel::clearMap(const QString& key)
{
    if (!_material) return;

    if (key == "albedo")         _material->clearAlbedoMap();
    else if (key == "normal")    _material->clearNormalMap();
    else if (key == "emissive")  _material->clearEmissiveMap();
    else if (key == "metallic")  _material->clearMetallicMap();
    else if (key == "roughness") _material->clearRoughnessMap();
    else if (key == "ao")        _material->clearAOMap();
    else if (key == "opacity")
    {
        _material->clearOpacityMap();
        _material->setBlendMode(GLMaterial::BlendMode::Opaque);
    }
    else if (key == "height")    _material->clearHeightMap();
    else if (key == "transmission") _material->clearTransmissionMap();
    else if (key == "ior")          _material->clearIORMap();
    else if (key == "sheen_color")  _material->clearSheenColorMap();
    else if (key == "sheen_rough")  _material->clearSheenRoughnessMap();
    else if (key == "cc_color")     _material->clearClearcoatColorMap();
    else if (key == "cc_rough")     _material->clearClearcoatRoughnessMap();
    else if (key == "cc_normal")    _material->clearClearcoatNormalMap();
    else if (key == "iridescence")           _material->clearIridescenceMap();
    else if (key == "iridescence_thickness") _material->clearIridescenceThicknessMap();
    else if (key == "specular_factor")       _material->clearSpecularFactorMap();
    else if (key == "specular_color")        _material->clearSpecularColorMap();
    else if (key == "anisotropy")            _material->clearAnisotropyMap();
    else if (key == "diffuse_transmission")  _material->clearDiffuseTransmissionMap();
    else if (key == "diffuse_transmission_color") _material->clearDiffuseTransmissionColorMap();
    else if (key == "thickness")             _material->clearThicknessMap();
}

void TextureMappingPanel::clearAllMaps()
{
    if (!_material) return;

    // Iterate over registered maps and clear both model and UI
    for (auto it = _maps.constBegin(); it != _maps.constEnd(); ++it)
    {
        const QString key = it.key();
        const MapSlot& slot = it.value(); // your MapEntry type used in _maps

        // Clear the map on the GLMaterial
        clearMap(key);

        // Clear the UI icon on corresponding button (if any)
        if (slot.button)
        {
            applyButtonEmptyIcon(const_cast<MapSlot&>(slot));
        }
    }

    // After clearing, refresh preview and emit changed so UI/host can react
    updatePreview();
    emit materialChanged(_material);
}

QString TextureMappingPanel::mapPath(const QString& key) const
{
    if (!_material) return {};

    if (key == "albedo")         return _material->albedoMapPath();
    if (key == "normal")         return _material->normalMapPath();
    if (key == "emissive")       return _material->emissiveMapPath();
    if (key == "metallic")       return _material->metallicMapPath();
    if (key == "roughness")      return _material->roughnessMapPath();
    if (key == "ao")             return _material->aoMapPath();
    if (key == "opacity")        return _material->opacityMapPath();
    if (key == "height")         return _material->heightMapPath();
    if (key == "transmission")   return _material->transmissionMapPath();
    if (key == "ior")            return _material->iorMapPath();
    if (key == "sheen_color")    return _material->sheenColorMapPath();
    if (key == "sheen_rough")    return _material->sheenRoughnessMapPath();
    if (key == "cc_color")       return _material->clearcoatColorMapPath();
    if (key == "cc_rough")       return _material->clearcoatRoughnessMapPath();
    if (key == "cc_normal")      return _material->clearcoatNormalMapPath();
    if (key == "iridescence")           return _material->iridescenceMap();
    if (key == "iridescence_thickness") return _material->iridescenceThicknessMap();
    if (key == "specular_factor")       return _material->specularFactorMap();
    if (key == "specular_color")        return _material->specularColorMap();
    if (key == "anisotropy")            return _material->anisotropyMap();
    if (key == "diffuse_transmission")  return _material->diffuseTransmissionMap();
    if (key == "diffuse_transmission_color") return _material->diffuseTransmissionColorMap();
    if (key == "thickness")             return _material->thicknessMap();
    return {};
}

// ---------------- Channel packing ----------------
void TextureMappingPanel::openPackingDialogFor(const QString& key)
{
    // Pretty name for the window title
    auto pretty = [&](const QString& k)->QString {
        if (k == "metallic") return tr("Metallic");
        if (k == "roughness")return tr("Roughness");
        if (k == "ao")       return tr("Ambient Occlusion");
        if (k == "opacity")  return tr("Opacity");
        return k;
        };

    ChannelPackingEditorDialog dlg(this);

    // Show dialog with current (or default) packing
    GLMaterial::ChannelPacking cur{};
    if (_material) cur = _material->packingFor(key);
    dlg.setCurrentPacking(cur, pretty(key));

    if (dlg.exec() == QDialog::Accepted)
    {
        if (_material)
        {
            _material->setPackingFor(key, dlg.packing());
            updatePreview();
            emit materialChanged(_material);
        }
    }
}


void TextureMappingPanel::updatePreview()
{
    if (!_preview) return;
    // Adapt to your preview widget API:
    _preview->setPreviewShape(static_cast<PreviewShape>(_ui->comboShape->currentIndex()));  // 0=sphere,1=cube,2=plane
    _preview->setEnvironment(static_cast<EnvMode>(_ui->comboEnv->currentIndex()));     // pick HDRI
    _preview->setExposureEV(_ui->sliderExposure->value() / 10.0f);
    _preview->update();
}

void TextureMappingPanel::applyMaterialPreset(const QString& presetName)
{
    if (!_material) return;

    clearAllMaps();

    const MaterialsMap& mats = MaterialTextureLibrary::instance().materials();
    if (!mats.contains(presetName)) return;

    const TextureMap texs = mats.value(presetName);

    // Get the path of the first tex (for initial folder)    
    for (auto it = texs.constBegin(); it != texs.constEnd(); ++it)
    {
        const QString& path = it.value();
        if (!path.isEmpty())
        {
            _lastUsedFolder = QFileInfo(path).absolutePath();
            break;
        }
    }

    // --- 1) If packed AORM meta is present, apply it to AO/Roughness/Metallic only ---
    bool appliedPackedAORM = false;
    if (texs.contains("packed:aorm"))
    {
        QString packedVal = texs.value("packed:aorm"); // expected "<path>|<chanOrder>"
        QString packedPath = packedVal.split('|').value(0).trimmed();
        if (!packedPath.isEmpty())
        {
            // assign packed file to the three logical maps
            setMapPath("ao", packedPath);
            setMapPath("roughness", packedPath);
            setMapPath("metallic", packedPath);

            // update UI icons for the three map slots exactly like the original loop would
            const QStringList keys = { "ao", "roughness", "metallic" };
            for (const QString& k : keys)
            {
                if (_maps.contains(k))
                {
                    auto entry = _maps.value(k); // keep same usage as your original code
                    if (entry.button && !packedPath.isEmpty())
                    {
                        applyButtonImageIcon(entry, packedPath);
                    }
                }
            }

            appliedPackedAORM = true;
        }
    }

    // --- 2) Apply remaining maps from the texture map (skip meta keys, and skip ao/roughness/metallic if already applied) ---
    for (auto it = texs.constBegin(); it != texs.constEnd(); ++it)
    {
        const QString& key = it.key();
        const QString& path = it.value();

        // skip meta entries
        if (key.startsWith("packed:")) continue;

        // if packed AORM was applied, skip separate ao/roughness/metallic entries (we already applied them)
        if (appliedPackedAORM && (key == "ao" || key == "roughness" || key == "metallic")) continue;

        // normal, albedo, etc. will be applied as usual
        setMapPath(key, path);

        if (_maps.contains(key))
        {
            auto entry = _maps.value(key);
            if (entry.button && !path.isEmpty())
            {
                applyButtonImageIcon(entry, path);
            }
        }
    }

    updatePreview();
    emit materialChanged(_material);
}




// small helper: acceptable file extensions (same as file dialog)
static bool isImageFileExtension(const QString& path)
{
    const QStringList exts = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".hdr", ".exr" };
    const QString lower = path.toLower();
    for (const QString& e : exts)
        if (lower.endsWith(e)) return true;
    return false;
}

bool TextureMappingPanel::eventFilter(QObject* obj, QEvent* ev)
{
    // Only care about drag enter / drop events on the buttons
    if (!obj || !_ui) return QWidget::eventFilter(obj, ev);

    // We expect obj to be one of the QPushButton* in _maps values.
    if (ev->type() == QEvent::DragEnter)
    {
        QDragEnterEvent* den = static_cast<QDragEnterEvent*>(ev);
        const QMimeData* md = den->mimeData();
        if (md && md->hasUrls())
        {
            // check first url points to local file with supported extension
            const QList<QUrl> urls = md->urls();
            if (!urls.isEmpty())
            {
                const QString local = urls.first().toLocalFile();
                if (!local.isEmpty() && isImageFileExtension(local))
                {
                    den->acceptProposedAction();
                    return true;
                }
            }
        }
        return QWidget::eventFilter(obj, ev);
    }
    else if (ev->type() == QEvent::Drop)
    {
        QDropEvent* de = static_cast<QDropEvent*>(ev);
        const QMimeData* md = de->mimeData();
        if (md && md->hasUrls())
        {
            const QList<QUrl> urls = md->urls();
            if (!urls.isEmpty())
            {
                const QString local = urls.first().toLocalFile();
                if (!local.isEmpty() && isImageFileExtension(local))
                {
                    // Find which map key corresponds to obj (the button)
                    QString key;
                    for (auto it = _maps.begin(); it != _maps.end(); ++it)
                    {
                        if (it.value().button == obj)
                        {
                            key = it.key();
                            break;
                        }
                    }
                    if (!key.isEmpty())
                    {
                        // Apply the image just like the file dialog path
                        applyButtonImageIcon(_maps[key], local);
                        setMapPath(key, local);
                        updatePreview();
                        emit materialChanged(_material);

                        de->acceptProposedAction();
                        return true;
                    }
                }
            }
        }
        return QWidget::eventFilter(obj, ev);
    }

    return QWidget::eventFilter(obj, ev);
}