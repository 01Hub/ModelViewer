#include "ADSMaterialSettingsPanel.h"
#include "GLWidget.h"
#include "GLMaterial.h"
#include "ModelViewer.h"
#include <QColorDialog>
#include <QFileDialog>
#include <QPixmap>
#include <QImageReader>
#include <QMessageBox>

ADSMaterialSettingsPanel::ADSMaterialSettingsPanel(QWidget* parent)
    : QWidget(parent),
    _modelViewer(nullptr),
    _glWidget(nullptr),
    _material(nullptr),
    _isInitialized(false),
    _hasActiveSelection(false),
    _updateInProgress(false)
{
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
}

ADSMaterialSettingsPanel::~ADSMaterialSettingsPanel()
{
}

void ADSMaterialSettingsPanel::initialize(ModelViewer* modelViewer, GLWidget* glWidget, GLMaterial* material)
{
    if (_isInitialized)
        return;

    _modelViewer = modelViewer;
    _glWidget = glWidget;
    _material = material;

    sliderTransparency->setEnabled(false);
    sliderShine->setEnabled(false);
    pushButtonMaterialAmbient->setEnabled(false);
    pushButtonMaterialDiffuse->setEnabled(false);
    pushButtonMaterialSpecular->setEnabled(false);
    pushButtonMaterialEmissive->setEnabled(false);
    pushButtonApplyADSColors->setEnabled(false);
    pushButtonApplyADSTexture->setEnabled(false);

    connectSignalsAndSlots();
    updateMaterialButtonStyles();
    updateMaterialPropertySliders();

    _isInitialized = true;
}

void ADSMaterialSettingsPanel::connectSignalsAndSlots()
{
    // Material color buttons
    connect(pushButtonMaterialAmbient, &QPushButton::clicked, this, &ADSMaterialSettingsPanel::onMaterialAmbientClicked);
    connect(pushButtonMaterialDiffuse, &QPushButton::clicked, this, &ADSMaterialSettingsPanel::onMaterialDiffuseClicked);
    connect(pushButtonMaterialSpecular, &QPushButton::clicked, this, &ADSMaterialSettingsPanel::onMaterialSpecularClicked);
    connect(pushButtonMaterialEmissive, &QPushButton::clicked, this, &ADSMaterialSettingsPanel::onMaterialEmissiveClicked);

    // Material property sliders
    connect(sliderTransparency, &QSlider::valueChanged, this, &ADSMaterialSettingsPanel::onTransparencyChanged);
    connect(sliderShine, &QSlider::valueChanged, this, &ADSMaterialSettingsPanel::onShineChanged);

    // Texture checkboxes
    connect(checkBoxDiffuseTex, &QCheckBox::toggled, this, &ADSMaterialSettingsPanel::onDiffuseTexToggled);
    connect(checkBoxSpecularTex, &QCheckBox::toggled, this, &ADSMaterialSettingsPanel::onSpecularTexToggled);
    connect(checkBoxNormalTex, &QCheckBox::toggled, this, &ADSMaterialSettingsPanel::onNormalTexToggled);
    connect(checkBoxEmissiveTex, &QCheckBox::toggled, this, &ADSMaterialSettingsPanel::onEmissiveTexToggled);
    connect(checkBoxHeightTex, &QCheckBox::toggled, this, &ADSMaterialSettingsPanel::onHeightTexToggled);
    connect(checkBoxOpacityTex, &QCheckBox::toggled, this, &ADSMaterialSettingsPanel::onOpacityTexToggled);
    connect(checkBoxOpacInvert, &QCheckBox::toggled, this, &ADSMaterialSettingsPanel::onOpacityInvertToggled);

    // Texture selection buttons
    connect(pushButtonDiffuseTexture, &QPushButton::clicked, this, &ADSMaterialSettingsPanel::onSelectDiffuseTexture);
    connect(pushButtonSpecularTexture, &QPushButton::clicked, this, &ADSMaterialSettingsPanel::onSelectSpecularTexture);
    connect(pushButtonNormalTexture, &QPushButton::clicked, this, &ADSMaterialSettingsPanel::onSelectNormalTexture);
    connect(pushButtonEmissiveTexture, &QPushButton::clicked, this, &ADSMaterialSettingsPanel::onSelectEmissiveTexture);
    connect(pushButtonHeightTexture, &QPushButton::clicked, this, &ADSMaterialSettingsPanel::onSelectHeightTexture);
    connect(pushButtonOpacityTexture, &QPushButton::clicked, this, &ADSMaterialSettingsPanel::onSelectOpacityTexture);

    // Texture clear buttons
    connect(toolButtonClearDiffuseTex, &QToolButton::clicked, this, &ADSMaterialSettingsPanel::onClearDiffuseTexture);
    connect(toolButtonClearSpecularTex, &QToolButton::clicked, this, &ADSMaterialSettingsPanel::onClearSpecularTexture);
    connect(toolButtonClearNormalTex, &QToolButton::clicked, this, &ADSMaterialSettingsPanel::onClearNormalTexture);
    connect(toolButtonClearEmissiveTex, &QToolButton::clicked, this, &ADSMaterialSettingsPanel::onClearEmissiveTexture);
    connect(toolButtonClearHeightTex, &QToolButton::clicked, this, &ADSMaterialSettingsPanel::onClearHeightTexture);
    connect(toolButtonClearOpacityTex, &QToolButton::clicked, this, &ADSMaterialSettingsPanel::onClearOpacityTexture);

    // Action buttons
    connect(pushButtonApplyADSColors, &QPushButton::clicked, this, &ADSMaterialSettingsPanel::onApplyColorsClicked);
    connect(pushButtonDefaultMatls, &QPushButton::clicked, this, &ADSMaterialSettingsPanel::onDefaultMaterialsClicked);
    connect(pushButtonApplyADSTexture, &QPushButton::clicked, this, &ADSMaterialSettingsPanel::onApplyTexturesClicked);
    connect(pushButtonClearADSTextures, &QPushButton::clicked, this, &ADSMaterialSettingsPanel::onClearTexturesClicked);

	connect(toolButtonDetach, &QToolButton::clicked, this, &ADSMaterialSettingsPanel::detachRequested);
}

void ADSMaterialSettingsPanel::updateMaterialButtonStyles()
{
    if (!_material)
        return;

    _updateInProgress = true;

    // Update material color buttons
    updateButtonStyleSheet(pushButtonMaterialAmbient, _material->ambient());
    updateButtonStyleSheet(pushButtonMaterialDiffuse, _material->diffuse());
    updateButtonStyleSheet(pushButtonMaterialSpecular, _material->specular());
    updateButtonStyleSheet(pushButtonMaterialEmissive, _material->emissive());

    _updateInProgress = false;
}

void ADSMaterialSettingsPanel::updateMaterialPropertySliders()
{
    if (!_material)
        return;

    _updateInProgress = true;

    // Update transparency slider and label
    int opacityValue = static_cast<int>(_material->opacity() * 1000.0f);
    sliderTransparency->blockSignals(true);
    sliderTransparency->setValue(opacityValue);
    sliderTransparency->blockSignals(false);
    valueOpacity->setText(QString::number(_material->opacity(), 'f', 2));

    // Update shine slider and label
    int shineValue = static_cast<int>(_material->shininess());
    sliderShine->blockSignals(true);
    sliderShine->setValue(shineValue);
    sliderShine->blockSignals(false);
    valueShine->setText(QString::number(shineValue));

    _updateInProgress = false;
}

void ADSMaterialSettingsPanel::setSelectionState(bool hasSelection)
{
    _hasActiveSelection = hasSelection;

    // Enable/disable material property sliders based on selection
    sliderTransparency->setEnabled(hasSelection);
    sliderShine->setEnabled(hasSelection);

    if (!hasSelection)
    {
        // Disable material color buttons when no selection
        pushButtonMaterialAmbient->setEnabled(false);
        pushButtonMaterialDiffuse->setEnabled(false);
        pushButtonMaterialSpecular->setEnabled(false);
        pushButtonMaterialEmissive->setEnabled(false);
        pushButtonApplyADSColors->setEnabled(false);
        pushButtonApplyADSTexture->setEnabled(false);
    }
    else
    {
        // Re-enable material color buttons
        pushButtonMaterialAmbient->setEnabled(true);
        pushButtonMaterialDiffuse->setEnabled(true);
        pushButtonMaterialSpecular->setEnabled(true);
        pushButtonMaterialEmissive->setEnabled(true);
        pushButtonApplyADSColors->setEnabled(true);
        pushButtonApplyADSTexture->setEnabled(true);
    }
}

void ADSMaterialSettingsPanel::updateTexturePreviews()
{
    // This would load and display texture previews
    // For now, placeholder implementation
}

void ADSMaterialSettingsPanel::setDetached(bool detached)
{
    _detached = detached;
    toolButtonDetach->setVisible(!_detached);
    lineSeparator->setVisible(!_detached);
}

// Color getters
QVector3D ADSMaterialSettingsPanel::getAmbientColor() const
{
    return _material ? _material->ambient() : QVector3D(0.2f, 0.2f, 0.2f);
}

QVector3D ADSMaterialSettingsPanel::getDiffuseColor() const
{
    return _material ? _material->diffuse() : QVector3D(0.8f, 0.8f, 0.8f);
}

QVector3D ADSMaterialSettingsPanel::getSpecularColor() const
{
    return _material ? _material->specular() : QVector3D(1.0f, 1.0f, 1.0f);
}

QVector3D ADSMaterialSettingsPanel::getEmissiveColor() const
{
    return _material ? _material->emissive() : QVector3D(0.0f, 0.0f, 0.0f);
}

float ADSMaterialSettingsPanel::getOpacity() const
{
    return sliderTransparency->value();
}

int ADSMaterialSettingsPanel::getShininess() const
{    
    return sliderShine->value();
}

// Texture path getters
QString ADSMaterialSettingsPanel::getDiffuseTexturePath() const
{
    return _diffuseTexPath;
}

QString ADSMaterialSettingsPanel::getSpecularTexturePath() const
{
    return _specularTexPath;
}

QString ADSMaterialSettingsPanel::getNormalTexturePath() const
{
    return _normalTexPath;
}

QString ADSMaterialSettingsPanel::getEmissiveTexturePath() const
{
    return _emissiveTexPath;
}

QString ADSMaterialSettingsPanel::getHeightTexturePath() const
{
    return _heightTexPath;
}

QString ADSMaterialSettingsPanel::getOpacityTexturePath() const
{
    return _opacityTexPath;
}

bool ADSMaterialSettingsPanel::isOpacityTextureInverted() const
{    
    return checkBoxOpacInvert->isChecked();
}

// ============================================================================
// Material Color Button Slots
// ============================================================================

void ADSMaterialSettingsPanel::onMaterialAmbientClicked()
{
    if (!_material || !_modelViewer)
        return;

    QVector3D ambient = _material->ambient();
    QColor c = QColorDialog::getColor(
        QColor::fromRgbF(ambient.x(), ambient.y(), ambient.z()),
        this, "Ambient Material Color"
    );

    if (c.isValid())
    {
        _material->setAmbient(QVector3D(
            static_cast<float>(c.redF()),
            static_cast<float>(c.greenF()),
            static_cast<float>(c.blueF())
        ));
        updateButtonStyleSheet(pushButtonMaterialAmbient, _material->ambient());
        emit materialAmbientChanged(_material->ambient());
    }
}

void ADSMaterialSettingsPanel::onMaterialDiffuseClicked()
{
    if (!_material || !_modelViewer)
        return;

    QVector3D diffuse = _material->diffuse();
    QColor c = QColorDialog::getColor(
        QColor::fromRgbF(diffuse.x(), diffuse.y(), diffuse.z()),
        this, "Diffuse Material Color"
    );

    if (c.isValid())
    {
        _material->setDiffuse(QVector3D(
            static_cast<float>(c.redF()),
            static_cast<float>(c.greenF()),
            static_cast<float>(c.blueF())
        ));
        updateButtonStyleSheet(pushButtonMaterialDiffuse, _material->diffuse());
        emit materialDiffuseChanged(_material->diffuse());
    }
}

void ADSMaterialSettingsPanel::onMaterialSpecularClicked()
{
    if (!_material || !_modelViewer)
        return;

    QVector3D specular = _material->specular();
    QColor c = QColorDialog::getColor(
        QColor::fromRgbF(specular.x(), specular.y(), specular.z()),
        this, "Specular Material Color"
    );

    if (c.isValid())
    {
        _material->setSpecular(QVector3D(
            static_cast<float>(c.redF()),
            static_cast<float>(c.greenF()),
            static_cast<float>(c.blueF())
        ));
        updateButtonStyleSheet(pushButtonMaterialSpecular, _material->specular());
        emit materialSpecularChanged(_material->specular());
    }
}

void ADSMaterialSettingsPanel::onMaterialEmissiveClicked()
{
    if (!_material || !_modelViewer)
        return;

    QVector3D emissive = _material->emissive();
    QColor c = QColorDialog::getColor(
        QColor::fromRgbF(emissive.x(), emissive.y(), emissive.z()),
        this, "Emissive Material Color"
    );

    if (c.isValid())
    {
        _material->setEmissive(QVector3D(
            static_cast<float>(c.redF()),
            static_cast<float>(c.greenF()),
            static_cast<float>(c.blueF())
        ));
        updateButtonStyleSheet(pushButtonMaterialEmissive, _material->emissive());
        emit materialEmissiveChanged(_material->emissive());
    }
}

// ============================================================================
// Material Property Slider Slots
// ============================================================================

void ADSMaterialSettingsPanel::onTransparencyChanged(int value)
{
    if (_updateInProgress || !_material || !_hasActiveSelection)
        return;

    float opacity = static_cast<float>(value) / 1000.0f;
    _material->setOpacity(opacity);
    valueOpacity->setText(QString::number(opacity, 'f', 2));

    emit opacityChanged(opacity);
}

void ADSMaterialSettingsPanel::onShineChanged(int value)
{
    if (_updateInProgress || !_material || !_hasActiveSelection)
        return;

    _material->setShininess(value);
    valueShine->setText(QString::number(value));

    emit shininessChanged(value);
}

// ============================================================================
// Texture Checkbox Slots
// ============================================================================

void ADSMaterialSettingsPanel::onDiffuseTexToggled(bool checked)
{
    updateTextureControlState(checkBoxDiffuseTex, pushButtonDiffuseTexture, 
                             labelDiffuseTexture, toolButtonClearDiffuseTex, checked);
}

void ADSMaterialSettingsPanel::onSpecularTexToggled(bool checked)
{
    updateTextureControlState(checkBoxSpecularTex, pushButtonSpecularTexture,
                             labelSpecularTexture, toolButtonClearSpecularTex, checked);
}

void ADSMaterialSettingsPanel::onNormalTexToggled(bool checked)
{
    updateTextureControlState(checkBoxNormalTex, pushButtonNormalTexture,
                             labelNormalTexture, toolButtonClearNormalTex, checked);
}

void ADSMaterialSettingsPanel::onEmissiveTexToggled(bool checked)
{
    updateTextureControlState(checkBoxEmissiveTex, pushButtonEmissiveTexture,
                             labelEmissiveTexture, toolButtonClearEmissiveTex, checked);
}

void ADSMaterialSettingsPanel::onHeightTexToggled(bool checked)
{
    updateTextureControlState(checkBoxHeightTex, pushButtonHeightTexture,
                             labelHeightTexture, toolButtonClearHeightTex, checked);
}

void ADSMaterialSettingsPanel::onOpacityTexToggled(bool checked)
{
    updateTextureControlState(checkBoxOpacityTex, pushButtonOpacityTexture,
                             labelOpacityTexture, toolButtonClearOpacityTex, checked);
    checkBoxOpacInvert->setEnabled(checked);
}

void ADSMaterialSettingsPanel::onOpacityInvertToggled(bool checked)
{
    emit opacityTextureInverted(checked);
}

// ============================================================================
// Texture Selection Button Slots
// ============================================================================

void ADSMaterialSettingsPanel::onSelectDiffuseTexture()
{
    QString path = selectTextureFile();
    if (!path.isEmpty())
    {
        _diffuseTexPath = path;
        loadTexturePreview(labelDiffuseTexture, path);
        emit diffuseTextureChanged(path);
    }
}

void ADSMaterialSettingsPanel::onSelectSpecularTexture()
{
    QString path = selectTextureFile();
    if (!path.isEmpty())
    {
        _specularTexPath = path;
        loadTexturePreview(labelSpecularTexture, path);
        emit specularTextureChanged(path);
    }
}

void ADSMaterialSettingsPanel::onSelectNormalTexture()
{
    QString path = selectTextureFile();
    if (!path.isEmpty())
    {
        _normalTexPath = path;
        loadTexturePreview(labelNormalTexture, path);
        emit normalTextureChanged(path);
    }
}

void ADSMaterialSettingsPanel::onSelectEmissiveTexture()
{
    QString path = selectTextureFile();
    if (!path.isEmpty())
    {
        _emissiveTexPath = path;
        loadTexturePreview(labelEmissiveTexture, path);
        emit emissiveTextureChanged(path);
    }
}

void ADSMaterialSettingsPanel::onSelectHeightTexture()
{
    QString path = selectTextureFile();
    if (!path.isEmpty())
    {
        _heightTexPath = path;
        loadTexturePreview(labelHeightTexture, path);
        emit heightTextureChanged(path);
    }
}

void ADSMaterialSettingsPanel::onSelectOpacityTexture()
{
    QString path = selectTextureFile();
    if (!path.isEmpty())
    {
        _opacityTexPath = path;
        loadTexturePreview(labelOpacityTexture, path);
        emit opacityTextureChanged(path);
    }
}

// ============================================================================
// Texture Clear Button Slots
// ============================================================================

void ADSMaterialSettingsPanel::onClearDiffuseTexture()
{
    _diffuseTexPath.clear();
    labelDiffuseTexture->clear();
    emit diffuseTextureChanged(QString());
}

void ADSMaterialSettingsPanel::onClearSpecularTexture()
{
    _specularTexPath.clear();
    labelSpecularTexture->clear();
    emit specularTextureChanged(QString());
}

void ADSMaterialSettingsPanel::onClearNormalTexture()
{
    _normalTexPath.clear();
    labelNormalTexture->clear();
    emit normalTextureChanged(QString());
}

void ADSMaterialSettingsPanel::onClearEmissiveTexture()
{
    _emissiveTexPath.clear();
    labelEmissiveTexture->clear();
    emit emissiveTextureChanged(QString());
}

void ADSMaterialSettingsPanel::onClearHeightTexture()
{
    _heightTexPath.clear();
    labelHeightTexture->clear();
    emit heightTextureChanged(QString());
}

void ADSMaterialSettingsPanel::onClearOpacityTexture()
{
    _opacityTexPath.clear();
    labelOpacityTexture->clear();
    emit opacityTextureChanged(QString());
}

// ============================================================================
// Action Button Slots
// ============================================================================

void ADSMaterialSettingsPanel::onApplyColorsClicked()
{
    if (!_modelViewer || !_hasActiveSelection)
        return;

    emit applyColorToSelectionRequested();
}

void ADSMaterialSettingsPanel::onDefaultMaterialsClicked()
{
    if (!_material)
        return;

    // Reset material to default values
    _material->setAmbient(QVector3D(0.2f, 0.2f, 0.2f));
    _material->setDiffuse(QVector3D(0.8f, 0.8f, 0.8f));
    _material->setSpecular(QVector3D(0.5f, 0.5f, 0.5f));
    _material->setEmissive(QVector3D(0.0f, 0.0f, 0.0f));
    _material->setOpacity(1.0f);
    _material->setShininess(32);

    updateMaterialButtonStyles();
    updateMaterialPropertySliders();

    emit defaultMaterialsRequested();
}

void ADSMaterialSettingsPanel::onApplyTexturesClicked()
{
    if (!_modelViewer || !_hasActiveSelection)
        return;

    emit applyTexturesRequested();
}

void ADSMaterialSettingsPanel::onClearTexturesClicked()
{
    if (!_modelViewer)
        return;

    // Clear all texture paths
    _diffuseTexPath.clear();
    _specularTexPath.clear();
    _normalTexPath.clear();
    _emissiveTexPath.clear();
    _heightTexPath.clear();
    _opacityTexPath.clear();

    // Clear all texture display
    labelDiffuseTexture->clear();
    labelSpecularTexture->clear();
    labelNormalTexture->clear();
    labelEmissiveTexture->clear();
    labelHeightTexture->clear();
    labelOpacityTexture->clear();

    // Uncheck all texture checkboxes
    checkBoxDiffuseTex->setChecked(false);
    checkBoxSpecularTex->setChecked(false);
    checkBoxNormalTex->setChecked(false);
    checkBoxEmissiveTex->setChecked(false);
    checkBoxHeightTex->setChecked(false);
    checkBoxOpacityTex->setChecked(false);
    checkBoxOpacInvert->setChecked(false);

    emit clearTexturesRequested();
}

// ============================================================================
// Helper Methods
// ============================================================================

void ADSMaterialSettingsPanel::updateButtonStyleSheet(QPushButton* button, const QVector3D& color)
{
    if (!button)
        return;

    QColor qcolor;
    qcolor.setRgbF(color.x(), color.y(), color.z());

    QString qss = QString("background-color: %1; color: %2")
        .arg(qcolor.name(),
             qcolor.lightness() < 75 ? QColor(Qt::white).name() : QColor(Qt::black).name());

    button->setStyleSheet(qss);
}

QString ADSMaterialSettingsPanel::selectTextureFile()
{
    QList<QByteArray> supportedFormats = QImageReader::supportedImageFormats();
    QString filter("All Supported Images (");

    for (const QByteArray& ba : supportedFormats)
    {
        filter += QString("*.%1 ").arg(QString(ba));
    }
    filter += ")";

    for (const QByteArray& ba : supportedFormats)
    {
        filter += QString(";;*.%1").arg(QString(ba));
    }

    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Select Texture File",
        QString(),
        filter
    );

    return fileName;
}

void ADSMaterialSettingsPanel::loadTexturePreview(QLabel* previewLabel, const QString& texturePath)
{
    if (!previewLabel || texturePath.isEmpty())
        return;

    QPixmap pixmap(texturePath);
    if (!pixmap.isNull())
    {
        previewLabel->setPixmap(pixmap.scaledToWidth(42, Qt::SmoothTransformation));
    }
    else
    {
        previewLabel->setText("?");
    }
}

void ADSMaterialSettingsPanel::updateTextureControlState(QCheckBox* checkBox, QPushButton* selectButton,
                                                        QLabel* preview, QToolButton* clearButton, bool enabled)
{
    if (selectButton)
        selectButton->setEnabled(enabled);
    if (preview)
        preview->setEnabled(enabled);
    if (clearButton)
        clearButton->setEnabled(enabled);
}
