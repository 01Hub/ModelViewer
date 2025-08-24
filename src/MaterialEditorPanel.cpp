#include "MaterialEditorPanel.h"
#include "Utils.h"
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
    previewWidget->setPreviewProfile(PreviewProfile::MaterialShowcase);
	previewWidget->setMinimumSize(160, 160);
	previewWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
	previewWidget->setMaximumHeight(160);

    topLayout->addWidget(treeWidget, 1);
	QVBoxLayout* previewLayout = new QVBoxLayout();
	modelCombo = new QComboBox();
	modelCombo->addItems({ "Sphere", "Cube", "Cylinder", "Plane", "Teapot" });
    connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int index) {
        previewWidget->setPreviewShape(static_cast<PreviewShape>(index));
    });
	previewLayout->addWidget(previewWidget);
	previewLayout->addWidget(modelCombo);
    topLayout->addLayout(previewLayout, 1);
    mainLayout->addLayout(topLayout);

    // Property editors
    QFormLayout *formLayout = new QFormLayout();
    albedoButton = new QPushButton("Pick Albedo Color");
    formLayout->addRow("Albedo:", albedoButton);

    metalnessSpin = new QDoubleSpinBox();
    metalnessSpin->setRange(0.0, 1.0);
	metalnessSpin->setSingleStep(0.01);
    formLayout->addRow("Metalness:", metalnessSpin);

    roughnessSpin = new QDoubleSpinBox();
    roughnessSpin->setRange(0.0, 1.0);
	roughnessSpin->setSingleStep(0.01);
    formLayout->addRow("Roughness:", roughnessSpin);

    opacitySpin = new QDoubleSpinBox();
    opacitySpin->setRange(0.0, 1.0);
	opacitySpin->setSingleStep(0.01);
    formLayout->addRow("Opacity:", opacitySpin);

	iorSpin = new QDoubleSpinBox();
	iorSpin->setRange(1.0, 3.0);
	iorSpin->setSingleStep(0.01);
	iorSpin->setValue(1.5); // Default IOR
	formLayout->addRow("IOR:", iorSpin);

	clearcoatSpin = new QDoubleSpinBox();
	clearcoatSpin->setRange(0.0, 1.0);
	clearcoatSpin->setSingleStep(0.01);
	clearcoatSpin->setValue(0.0); // Default clearcoat
	formLayout->addRow("Clearcoat:", clearcoatSpin);

	clearcoatRoughnessSpin = new QDoubleSpinBox();
	clearcoatRoughnessSpin->setRange(0.0, 1.0);
	clearcoatRoughnessSpin->setSingleStep(0.01);
	clearcoatRoughnessSpin->setValue(0.0); // Default clearcoat roughness
	formLayout->addRow("Clearcoat Roughness:", clearcoatRoughnessSpin);

	sheenColorButton = new QPushButton("Pick Sheen Color");
	formLayout->addRow("Sheen Color:", sheenColorButton);
	sheenRoughnessSpin = new QDoubleSpinBox();
	sheenRoughnessSpin->setRange(0.0, 1.0);
	sheenRoughnessSpin->setSingleStep(0.01);
	sheenRoughnessSpin->setValue(0.0); // Default sheen roughness
	formLayout->addRow("Sheen Roughness:", sheenRoughnessSpin);

	transmissionSpin = new QDoubleSpinBox();
	transmissionSpin->setRange(0.0, 1.0);
	transmissionSpin->setSingleStep(0.01);
	transmissionSpin->setValue(0.0); // Default transmission
	formLayout->addRow("Transmission:", transmissionSpin);

	alphaThresholdSpin = new QDoubleSpinBox();
	alphaThresholdSpin->setRange(0.0, 1.0);
	alphaThresholdSpin->setSingleStep(0.01);
	alphaThresholdSpin->setValue(0.5); // Default alpha threshold
	formLayout->addRow("Alpha Threshold:", alphaThresholdSpin);



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
            _currentMaterial.setAlbedoColor(QVector3D(c.redF(), c.greenF(), c.blueF()));
			_currentMaterial.convertToBlinnPhong();
            previewWidget->setMaterial(_currentMaterial);
            emit materialChanged(_currentMaterial);
        }
    });

    connect(metalnessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double val) {
        _currentMaterial.setMetalness(val);
        _currentMaterial.convertToBlinnPhong();
        previewWidget->setMaterial(_currentMaterial);
        emit materialChanged(_currentMaterial);
    });

    connect(roughnessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double val) {
        _currentMaterial.setRoughness(val);
        previewWidget->setMaterial(_currentMaterial);
        emit materialChanged(_currentMaterial);
    });

    connect(opacitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double val) {
        _currentMaterial.setOpacity(val);
        previewWidget->setMaterial(_currentMaterial);
        emit materialChanged(_currentMaterial);
    });

	connect(iorSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [=](double val) {
        _currentMaterial.setIOR(val);
        previewWidget->setMaterial(_currentMaterial);
        emit materialChanged(_currentMaterial);
		});

	connect(clearcoatSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [=](double val) {
        _currentMaterial.setClearcoat(val);
        previewWidget->setMaterial(_currentMaterial);
		emit materialChanged(_currentMaterial);
		});

    connect(clearcoatRoughnessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [=](double val) {
        _currentMaterial.setClearcoatRoughness(val);
		previewWidget->setMaterial(_currentMaterial);
		emit materialChanged(_currentMaterial);
		});

    connect(sheenColorButton, &QPushButton::clicked, this, [=]() {
        QColor c = QColorDialog::getColor(Qt::white, this, "Select Sheen Color");
        if (c.isValid()) {
            _currentMaterial.setSheenColor(QVector3D(c.redF(), c.greenF(), c.blueF()));
            previewWidget->setMaterial(_currentMaterial);
            emit materialChanged(_currentMaterial);
        }
		});

    connect(sheenRoughnessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double val) {
        _currentMaterial.setSheenRoughness(val);
        previewWidget->setMaterial(_currentMaterial);
        emit materialChanged(_currentMaterial);
		});

    connect(transmissionSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double val) {
        _currentMaterial.setTransmission(val);
        previewWidget->setMaterial(_currentMaterial);
		emit materialChanged(_currentMaterial);
		});

    connect(alphaThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double val) {
        _currentMaterial.setAlphaThreshold(val);
        previewWidget->setMaterial(_currentMaterial);
        emit materialChanged(_currentMaterial);
		});


    connect(shadingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int idx) {
        _currentMaterial.setShadingModel(static_cast<GLMaterial::ShadingModel>(idx));
        previewWidget->setMaterial(_currentMaterial);
        emit materialChanged(_currentMaterial);
    });

    connect(blendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int idx) {
        _currentMaterial.setBlendMode(static_cast<GLMaterial::BlendMode>(idx));
        previewWidget->setMaterial(_currentMaterial);
        emit materialChanged(_currentMaterial);
    });

    connect(twoSidedCheck, &QCheckBox::toggled, this, [=](bool checked) {
        _currentMaterial.setTwoSided(checked);
        previewWidget->setMaterial(_currentMaterial);
        emit materialChanged(_currentMaterial);
    });

    connect(wireframeCheck, &QCheckBox::toggled, this, [=](bool checked) {
        _currentMaterial.setWireframe(checked);
        previewWidget->setMaterial(_currentMaterial);
        emit materialChanged(_currentMaterial);
    });

    connect(applyButton, &QPushButton::clicked, this, [=]() {
        // Apply the current material settings to the preview widget
        previewWidget->setMaterial(_currentMaterial);
        emit materialChanged(_currentMaterial);
		});
}

void MaterialEditorPanel::onMaterialSelected(const GLMaterial &mat)
{
    _currentMaterial = mat;
    previewWidget->setMaterial(mat);

    QColor albedoColor(
        int(mat.albedoColor().x() * 255),
        int(mat.albedoColor().y() * 255),
        int(mat.albedoColor().z() * 255)
    );
    albedoButton->setStyleSheet(makeButtonStyleSheet(albedoColor));

    metalnessSpin->setValue(mat.metalness());
    roughnessSpin->setValue(mat.roughness());
    opacitySpin->setValue(mat.opacity());
	iorSpin->setValue(mat.ior());
	clearcoatSpin->setValue(mat.clearcoat());
	clearcoatRoughnessSpin->setValue(mat.clearcoatRoughness());
	sheenRoughnessSpin->setValue(mat.sheenRoughness());

    QColor sheenColor(
        int(mat.sheenColor().x() * 255),
        int(mat.sheenColor().y() * 255),
        int(mat.sheenColor().z() * 255)
    );
    sheenColorButton->setStyleSheet(makeButtonStyleSheet(sheenColor));

	transmissionSpin->setValue(mat.transmission());
	alphaThresholdSpin->setValue(mat.alphaThreshold());
    
    shadingCombo->setCurrentIndex(static_cast<int>(mat.shadingModel()));
    blendCombo->setCurrentIndex(static_cast<int>(mat.blendMode()));
    twoSidedCheck->setChecked(mat.twoSided());
    wireframeCheck->setChecked(mat.wireframe());

    emit materialChanged(_currentMaterial);
}
