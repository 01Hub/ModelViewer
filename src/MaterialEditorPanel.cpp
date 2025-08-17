#include "MaterialEditorPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QColorDialog>

MaterialEditorPanel::MaterialEditorPanel(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Top row: Tree + Preview
    QHBoxLayout *topLayout = new QHBoxLayout();
    treeWidget = new MaterialLibraryWidget(this);
    previewWidget = new MaterialPreviewWidget(this);
    previewWidget->setFixedSize(160, 160);

    topLayout->addWidget(treeWidget, 1);
    topLayout->addWidget(previewWidget, 0);
    mainLayout->addLayout(topLayout);

    // Property editors
    QFormLayout *formLayout = new QFormLayout();
    albedoButton = new QPushButton("Pick Color");
    formLayout->addRow("Albedo:", albedoButton);

    metalnessSpin = new QDoubleSpinBox();
    metalnessSpin->setRange(0.0, 1.0);
    formLayout->addRow("Metalness:", metalnessSpin);

    roughnessSpin = new QDoubleSpinBox();
    roughnessSpin->setRange(0.0, 1.0);
    formLayout->addRow("Roughness:", roughnessSpin);

    opacitySpin = new QDoubleSpinBox();
    opacitySpin->setRange(0.0, 1.0);
    formLayout->addRow("Opacity:", opacitySpin);

    shadingCombo = new QComboBox();
    shadingCombo->addItems({"Unlit", "Blinn-Phong", "PBR", "Toon"});
    formLayout->addRow("Shading Model:", shadingCombo);

    blendCombo = new QComboBox();
    blendCombo->addItems({"Opaque", "Masked", "Alpha", "Additive", "Multiply"});
    formLayout->addRow("Blend Mode:", blendCombo);

    twoSidedCheck = new QCheckBox();
    formLayout->addRow("Two-Sided:", twoSidedCheck);

    wireframeCheck = new QCheckBox();
    formLayout->addRow("Wireframe:", wireframeCheck);

	applyButton = new QPushButton("Apply");
	formLayout->addRow(applyButton);

    mainLayout->addLayout(formLayout);
    setLayout(mainLayout);

    // Connections
    connect(treeWidget, &MaterialLibraryWidget::materialSelected,
            this, &MaterialEditorPanel::onMaterialSelected);

    connect(albedoButton, &QPushButton::clicked, this, [=]() {
        QColor c = QColorDialog::getColor(Qt::white, this, "Select Albedo Color");
        if (c.isValid()) {
            currentMaterial.setAlbedoColor(QVector3D(c.redF(), c.greenF(), c.blueF()));
            previewWidget->setMaterial(currentMaterial);
            emit materialChanged(currentMaterial);
        }
    });

    connect(metalnessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double val) {
        currentMaterial.setMetalness(val);
        previewWidget->setMaterial(currentMaterial);
        emit materialChanged(currentMaterial);
    });

    connect(roughnessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double val) {
        currentMaterial.setRoughness(val);
        previewWidget->setMaterial(currentMaterial);
        emit materialChanged(currentMaterial);
    });

    connect(opacitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double val) {
        currentMaterial.setOpacity(val);
        previewWidget->setMaterial(currentMaterial);
        emit materialChanged(currentMaterial);
    });

    connect(shadingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int idx) {
        currentMaterial.setShadingModel(static_cast<GLMaterial::ShadingModel>(idx));
        previewWidget->setMaterial(currentMaterial);
        emit materialChanged(currentMaterial);
    });

    connect(blendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int idx) {
        currentMaterial.setBlendMode(static_cast<GLMaterial::BlendMode>(idx));
        previewWidget->setMaterial(currentMaterial);
        emit materialChanged(currentMaterial);
    });

    connect(twoSidedCheck, &QCheckBox::toggled, this, [=](bool checked) {
        currentMaterial.setTwoSided(checked);
        previewWidget->setMaterial(currentMaterial);
        emit materialChanged(currentMaterial);
    });

    connect(wireframeCheck, &QCheckBox::toggled, this, [=](bool checked) {
        currentMaterial.setWireframe(checked);
        previewWidget->setMaterial(currentMaterial);
        emit materialChanged(currentMaterial);
    });

    connect(applyButton, &QPushButton::clicked, this, [=]() {
        // Apply the current material settings to the preview widget
        previewWidget->setMaterial(currentMaterial);
        emit materialChanged(currentMaterial);
		});
}

void MaterialEditorPanel::onMaterialSelected(const GLMaterial &mat)
{
    currentMaterial = mat;
    previewWidget->setMaterial(mat);

    metalnessSpin->setValue(mat.metalness());
    roughnessSpin->setValue(mat.roughness());
    opacitySpin->setValue(mat.opacity());
    shadingCombo->setCurrentIndex(static_cast<int>(mat.shadingModel()));
    blendCombo->setCurrentIndex(static_cast<int>(mat.blendMode()));
    twoSidedCheck->setChecked(mat.twoSided());
    wireframeCheck->setChecked(mat.wireframe());

    emit materialChanged(currentMaterial);
}
