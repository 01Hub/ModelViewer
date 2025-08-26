#include "TextureMappingPanel.h"

// If your generated header name differs, adjust this include:
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

// Your project headers
#include "GLMaterial.h"
#include "MaterialPreviewWidget.h"
#include "ChannelPackingEditorDialog.h"

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

    // default checker on all buttons
    for (auto it = _maps.begin(); it != _maps.end(); ++it)
        applyButtonEmptyIcon(it.value());

    // pleasant stepping for UV
    _ui->spinTU->setSingleStep(0.1);
    _ui->spinTV->setSingleStep(0.1);
    _ui->spinOU->setSingleStep(0.1);
    _ui->spinOV->setSingleStep(0.1);

	// Initialize the material to default values and bind it    
    bindMaterial(new GLMaterial());

    connect(this , &TextureMappingPanel::materialChanged, this, [this]() {
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

	_ui->spinTU->setValue(_material->uvTilingU());
	_ui->spinTV->setValue(_material->uvTilingV());
	_ui->spinOU->setValue(_material->uvOffsetU());
	_ui->spinOV->setValue(_material->uvOffsetV());
        
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

            const QString file = QFileDialog::getOpenFileName(
                this, tr("Select %1 Map").arg(k.at(0).toUpper() + k.mid(1)),
                QString(), tr("Images (*.png *.jpg *.jpeg *.tga *.bmp *.hdr *.exr)"));
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

    // UV
    auto uv = [this] { onUVChanged(); };
    connect(_ui->spinTU, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [uv](double) { uv(); });
    connect(_ui->spinTV, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [uv](double) { uv(); });
    connect(_ui->spinOU, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [uv](double) { uv(); });
    connect(_ui->spinOV, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [uv](double) { uv(); });
    connect(_ui->btnUVReset, &QToolButton::clicked, this, [this] {
        _ui->spinTU->setValue(1.0); _ui->spinTV->setValue(1.0);
        _ui->spinOU->setValue(0.0); _ui->spinOV->setValue(0.0);
        onUVChanged();
        });

    // Preview controls
    connect(_ui->comboShape, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { updatePreview(); });
    connect(_ui->comboEnv, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { updatePreview(); });
    connect(_ui->sliderExposure, &QSlider::valueChanged, this, [this](int) { updatePreview(); });
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
    else if (key == "opacity")   _material->setOpacityMap(file);
	else if (key == "height")    _material->setHeightMap(file);
    else if (key == "transmission") _material->setTransmissionMap(file);
    else if (key == "ior")          _material->setIORMap(file);
    else if (key == "sheen_color")  _material->setSheenColorMap(file);
    else if (key == "sheen_rough")  _material->setSheenRoughnessMap(file);
    else if (key == "cc_color")     _material->setClearcoatColorMap(file);
    else if (key == "cc_rough")     _material->setClearcoatRoughnessMap(file);
    else if (key == "cc_normal")    _material->setClearcoatNormalMap(file);
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
    else if (key == "opacity")   _material->clearOpacityMap();
	else if (key == "height")    _material->clearHeightMap();
    else if (key == "transmission") _material->clearTransmissionMap();
    else if (key == "ior")          _material->clearIORMap();
    else if (key == "sheen_color")  _material->clearSheenColorMap();
    else if (key == "sheen_rough")  _material->clearSheenRoughnessMap();
    else if (key == "cc_color")     _material->clearClearcoatColorMap();
    else if (key == "cc_rough")     _material->clearClearcoatRoughnessMap();
    else if (key == "cc_normal")    _material->clearClearcoatNormalMap();
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


// ---------------- UV + preview ----------------
void TextureMappingPanel::onUVChanged()
{
    if (_material)
    {
        // Adapt to your GLMaterial API:
        _material->setUVTiling(_ui->spinTU->value(), _ui->spinTV->value());
        _material->setUVOffset(_ui->spinOU->value(), _ui->spinOV->value());
    }
    updatePreview();
    emit materialChanged(_material);
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
