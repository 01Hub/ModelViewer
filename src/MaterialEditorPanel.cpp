#include "MaterialEditorPanel.h"
#include "Utils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QColorDialog>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <functional>

MaterialEditorPanel::MaterialEditorPanel(QWidget* parent)
	: QWidget(parent)
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);

	// Top section with search, tree view, and preview
	QLineEdit* searchEdit = new QLineEdit(this);
	searchEdit->setPlaceholderText("Search materials...");
	searchEdit->setClearButtonEnabled(true);
	searchEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	treeWidget = new MaterialLibraryWidget(this);
	treeWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	treeWidget->setMinimumWidth(150);

	QVBoxLayout* leftLayout = new QVBoxLayout();
	leftLayout->setContentsMargins(0, 0, 0, 0);
	leftLayout->setSpacing(6);
	leftLayout->addWidget(searchEdit, 0);
	leftLayout->addWidget(treeWidget, 1);

	// Create a container widget for the left layout to ensure proper resizing
	QWidget* leftWidget = new QWidget(this);
	leftWidget->setLayout(leftLayout);
	leftWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	// Preview section
	QVBoxLayout* previewLayout = new QVBoxLayout();
	previewLayout->setContentsMargins(0, 0, 0, 0);
	previewLayout->setSpacing(6);

	// Model selection combo box
	modelCombo = new QComboBox();
	modelCombo->addItems({ "Sphere", "Cube", "Cylinder", "Plane", "Teapot" });
	connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int index) {
		previewWidget->setPreviewShape(static_cast<PreviewShape>(index));
		});

	// Material preview widget
	previewWidget = new MaterialPreviewWidget(this);
	previewWidget->setPreviewProfile(PreviewProfile::MaterialShowcase);
	previewWidget->setMinimumSize(160, 160);
	previewWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	previewWidget->setMaximumHeight(160);

	previewLayout->addWidget(previewWidget, 0, Qt::AlignTop);
	previewLayout->addWidget(modelCombo, 0, Qt::AlignTop);

	QHBoxLayout* topLayout = new QHBoxLayout();
	topLayout->setContentsMargins(0, 0, 0, 0);
	topLayout->setSpacing(8);

	topLayout->addWidget(leftWidget, /*stretch=*/3);
	topLayout->addLayout(previewLayout, /*stretch=*/1);

	mainLayout->addLayout(topLayout);

	// Property editors
	QFormLayout* formLayout = new QFormLayout();
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
	shadingCombo->addItems({ "Unlit", "Blinn-Phong", "PBR", "Toon" });
	formLayout->addRow("Shading Model:", shadingCombo);

	blendCombo = new QComboBox();
	blendCombo->addItems({ "Opaque", "Masked", "Alpha", "Additive", "Multiply" });
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
		if (c.isValid())
		{
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
		if (c.isValid())
		{
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

	// -------------------------
	// Simple search/filter for the tree
	// -------------------------
	// The lambda below will recursively check items and show/hide them based on the search text.
	connect(searchEdit, &QLineEdit::textChanged, this, [this, searchEdit](const QString& text) {
		QTreeWidget* tw = qobject_cast<QTreeWidget*>(treeWidget);
		if (!tw) return;

		const QString needle = text.trimmed().toLower();
		const bool hasFilter = !needle.isEmpty();

		QStringList tokens;
		if (hasFilter) tokens = needle.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

		int matchCount = 0;

		// Recursively walk and update each item.
		std::function<bool(QTreeWidgetItem*)> matchAndShow = [&](QTreeWidgetItem* item) -> bool {
			const QString label = item->text(0).toLower();

			// Only consider self-match when a filter is active.
			bool selfMatched = false;
			if (hasFilter)
			{
				selfMatched = true;
				for (const QString& t : tokens)
				{
					if (!label.contains(t)) { selfMatched = false; break; }
				}
			}

			// Always recurse children so we can reset their fonts/foreground when clearing.
			bool anyChildMatched = false;
			for (int i = 0; i < item->childCount(); ++i)
			{
				if (matchAndShow(item->child(i))) anyChildMatched = true;
			}

			// If no filter, everything should be visible (and not highlighted).
			const bool matched = hasFilter ? (selfMatched || anyChildMatched) : true;
			item->setHidden(hasFilter ? !matched : false);

			// Expand only when filtering and the item matched.
			if (hasFilter && matched) item->setExpanded(true);

			// --- persistent highlight: bold ONLY if this item's own label matched AND a filter is active ---
			QFont f = item->font(0);         // preserve family/size
			//f.setBold(hasFilter && selfMatched);
			item->setFont(0, f);

			// Foreground color: use subtle blue when self matched; reset otherwise (including when cleared)
			if (hasFilter && selfMatched)
			{
				item->setForeground(0, QBrush(QColor(0x1e88e5)));
			}
			else
			{
				item->setForeground(0, QBrush()); // reset to default
			}

			if (matched) ++matchCount;
			return matched;
			};

		for (int i = 0; i < tw->topLevelItemCount(); ++i)
			matchAndShow(tw->topLevelItem(i));

		// Red border when no matches and query non-empty; reset otherwise.
		if (!hasFilter)
		{
			searchEdit->setStyleSheet("");
			searchEdit->setToolTip("");
		}
		else if (matchCount == 0)
		{
			searchEdit->setStyleSheet("QLineEdit { border: 1px solid #e53935; }");
			searchEdit->setToolTip("No matches");
		}
		else
		{
			searchEdit->setStyleSheet("");
			searchEdit->setToolTip("");
		}
		});


}

void MaterialEditorPanel::onMaterialSelected(const GLMaterial& mat)
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
