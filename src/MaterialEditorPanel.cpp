#include "MaterialEditorPanel.h"
#include "MaterialRegistry.h" 
#include "MaterialLibraryWidget.h"
#include "Utils.h"

#include <QInputDialog>
#include <QMessageBox>
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
	saveButton = new QPushButton("Save");
	deleteButton = new QPushButton("Delete");

	QHBoxLayout* buttonRowLayout = new QHBoxLayout;
	buttonRowLayout->addWidget(applyButton);
	buttonRowLayout->addWidget(saveButton);
	buttonRowLayout->addWidget(deleteButton);

	formLayout->addRow(buttonRowLayout);

	mainLayout->addLayout(formLayout);
	setLayout(mainLayout);

	// Connections
	connect(treeWidget, &MaterialLibraryWidget::materialSelected,
		this, &MaterialEditorPanel::onMaterialSelected);

	connect(albedoButton, &QPushButton::clicked, this, [=]() {
		QColor albedoColor(
			int(_currentMaterial.albedoColor().x() * 255),
			int(_currentMaterial.albedoColor().y() * 255),
			int(_currentMaterial.albedoColor().z() * 255)
		);
		QColor c = QColorDialog::getColor(albedoColor, this, "Select Albedo Color");
		if (c.isValid())
		{
			_currentMaterial.setAlbedoColor(QVector3D(c.redF(), c.greenF(), c.blueF()));
			_currentMaterial.convertToBlinnPhong();
			previewWidget->setMaterial(_currentMaterial);
			albedoButton->setStyleSheet(makeButtonStyleSheet(c));
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
		QColor sheenColor(
			int(_currentMaterial.sheenColor().x() * 255),
			int(_currentMaterial.sheenColor().y() * 255),
			int(_currentMaterial.sheenColor().z() * 255)
		);
		QColor c = QColorDialog::getColor(sheenColor, this, "Select Sheen Color");
		if (c.isValid())
		{
			_currentMaterial.setSheenColor(QVector3D(c.redF(), c.greenF(), c.blueF()));
			previewWidget->setMaterial(_currentMaterial);
			sheenColorButton->setStyleSheet(makeButtonStyleSheet(c));
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

	connect(saveButton, &QPushButton::clicked, this, [=]() {
		onSaveButtonClicked();
		emit materialChanged(_currentMaterial);
		});

	connect(deleteButton, &QPushButton::clicked, this, [=]() {
		onDeleteButtonClicked();
		emit materialChanged(_currentMaterial);
		});

	// -------------------------
	// Simple search/filter for the tree
	// -------------------------
	// The lambda below will recursively check items and show/hide them based on the search text.
	// Modified to show children when parent matches, even if children don't match the search term.
	connect(searchEdit, &QLineEdit::textChanged, this, [this, searchEdit](const QString& text) {
		QTreeWidget* tw = qobject_cast<QTreeWidget*>(treeWidget);
		if (!tw) return;
		const QString needle = text.trimmed().toLower();
		const bool hasFilter = !needle.isEmpty();
		QStringList tokens;
		if (hasFilter) tokens = needle.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
		int matchCount = 0;

		// Recursively walk and update each item.
		std::function<bool(QTreeWidgetItem*, bool)> matchAndShow = [&](QTreeWidgetItem* item, bool parentMatched) -> bool {
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
			// Pass down whether this item or any ancestor matched
			bool anyChildMatched = false;
			bool shouldShowChildren = parentMatched || selfMatched;
			for (int i = 0; i < item->childCount(); ++i)
			{
				if (matchAndShow(item->child(i), shouldShowChildren)) anyChildMatched = true;
			}

			// Item should be shown if:
			// 1. No filter is active (show all), OR
			// 2. This item matched the search, OR  
			// 3. Any child matched the search, OR
			// 4. A parent/ancestor matched the search
			const bool matched = hasFilter ? (selfMatched || anyChildMatched || parentMatched) : true;
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

		// Start recursion from top-level items with parentMatched = false
		for (int i = 0; i < tw->topLevelItemCount(); ++i)
			matchAndShow(tw->topLevelItem(i), false);

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

void MaterialEditorPanel::onSaveButtonClicked()
{
	// current material instance from editor
	GLMaterial mat = _currentMaterial;

	// Derive defaults from current selection in the material tree (if any)
	QString key;
	QString name;
	QString groupLabel;

	if (treeWidget)
	{
		QList<QTreeWidgetItem*> sel = treeWidget->selectedItems();
		if (!sel.isEmpty())
		{
			QTreeWidgetItem* item = sel.first();
			// If a group header is selected, prefer its first child if available
			if (item->childCount() > 0)
			{
				QTreeWidgetItem* child = item->child(0);
				if (child)
				{
					key = child->data(0, Qt::UserRole).toString();
					name = child->text(0);
					groupLabel = item->text(0);
				}
			}
			else
			{
				// leaf node
				key = item->data(0, Qt::UserRole).toString();
				name = item->text(0);
				if (item->parent()) groupLabel = item->parent()->text(0);
			}
		}
	}

	// Ensure we have a group label (ask user if not)
	if (groupLabel.isEmpty())
	{
		QStringList groups;
		const auto& sharedGroups = MaterialLibraryWidget::sharedGroups();
		for (const auto& g : sharedGroups) groups << g.first;
		if (groups.isEmpty()) groups << QStringLiteral("User Materials");

		bool ok = false;
		QString picked = QInputDialog::getItem(this,
			tr("Choose Group"),
			tr("Select a group to save into:"),
			groups,
			0,
			true,
			&ok);
		if (!ok) return;
		groupLabel = picked.trimmed();
		if (groupLabel.isEmpty()) groupLabel = QStringLiteral("User Materials");
	}

	// Determine whether the current key exists and whether it is user or factory
	const auto& sharedMap = MaterialLibraryWidget::sharedMaterialMap();
	bool keyExists = !key.isEmpty() && sharedMap.contains(key);
	bool isUserKey = MaterialLibraryWidget::s_userMaterialKeys.contains(key);

	// If a factory material is selected (exists && not a user key), we must force "Save As" into user library
	if (keyExists && !isUserKey)
	{
		// Inform user that they are creating a user copy rather than modifying factory
		QMessageBox::information(this,
			tr("Save As New User Material"),
			tr("You are saving changes while a factory material is selected. "
				"A new user material will be created instead of modifying or deleting the factory material."));

		// Suggest a safe new key and name
		QString suggestedName = name.isEmpty() ? QStringLiteral("User Material") : QStringLiteral("User %1").arg(name);
		QString suggestedKey = suggestedName.toUpper().simplified().replace(' ', '_');

		// Ensure suggested key doesn't collide; if it does append suffix numbers
		int suffix = 1;
		QString trialKey = suggestedKey;
		while (sharedMap.contains(trialKey))
		{
			trialKey = QString("%1_%2").arg(suggestedKey).arg(suffix++);
		}

		// Ask user for display name
		bool okName = false;
		QString enteredName = QInputDialog::getText(this,
			tr("Material Name"),
			tr("Display name for material:"),
			QLineEdit::Normal,
			suggestedName,
			&okName);
		if (!okName || enteredName.trimmed().isEmpty()) return;
		name = enteredName.trimmed();

		// Ask user for key and validate uniqueness
		bool okKey = false;
		QString enteredKey;
		QString suggestedFinalKey = trialKey;
		// Loop until user provides a unique key or cancels
		for (;;)
		{
			enteredKey = QInputDialog::getText(this,
				tr("Material Key"),
				tr("Enter a unique material key (no spaces):"),
				QLineEdit::Normal,
				suggestedFinalKey,
				&okKey);
			if (!okKey) return; // cancel
			enteredKey = enteredKey.trimmed();
			enteredKey = enteredKey.simplified().replace(' ', '_');
			if (enteredKey.isEmpty()) continue;

			if (MaterialLibraryWidget::sharedMaterialMap().contains(enteredKey))
			{
				// If this key exists and is a factory key (not user), forbid reuse
				if (!MaterialLibraryWidget::s_userMaterialKeys.contains(enteredKey))
				{
					QMessageBox::warning(this,
						tr("Key Not Allowed"),
						tr("That key already exists as a factory material. Please choose a different key."));
					continue; // ask again
				}
				else
				{
					// This is a user key — confirm overwrite (save helper will also ask). Ask user if want to overwrite:
					QMessageBox::StandardButton overwriteReply =
						QMessageBox::question(this,
							tr("Overwrite User Material?"),
							tr("A user material with this key already exists. Overwrite it?"),
							QMessageBox::Yes | QMessageBox::No,
							QMessageBox::No);
					if (overwriteReply == QMessageBox::Yes)
					{
						// accept this key (will overwrite)
						key = enteredKey;
						break;
					}
					else
					{
						// ask for another key
						continue;
					}
				}
			}
			else
			{
				// key not present anywhere — accept
				key = enteredKey;
				break;
			}
		}
	}
	else
	{
		// Not saving over a factory material.
		// If there is no key yet (new material) ask user for name & key.
		if (key.isEmpty())
		{
			// ask for name if missing
			if (name.isEmpty())
			{
				bool ok = false;
				QString suggestedName = QStringLiteral("New Material");
				QString enteredName = QInputDialog::getText(this,
					tr("Material Name"),
					tr("Display name for material:"),
					QLineEdit::Normal,
					suggestedName,
					&ok);
				if (!ok || enteredName.trimmed().isEmpty()) return;
				name = enteredName.trimmed();
			}

			// ask for key
			bool okKey = false;
			QString suggestedKey = name.toUpper().simplified().replace(' ', '_');
			QString enteredKey = QInputDialog::getText(this,
				tr("Material Key"),
				tr("Unique material key (no spaces):"),
				QLineEdit::Normal,
				suggestedKey,
				&okKey);
			if (!okKey || enteredKey.trimmed().isEmpty()) return;
			key = enteredKey.trimmed().simplified().replace(' ', '_');

			// If the key collides with a factory key, disallow (ask user to choose another)
			if (MaterialLibraryWidget::sharedMaterialMap().contains(key) &&
				!MaterialLibraryWidget::s_userMaterialKeys.contains(key))
			{
				QMessageBox::warning(this,
					tr("Key Not Allowed"),
					tr("That key collides with a shipped factory material. Please choose a different key."));
				return;
			}
		}
		else
		{
			// We have a key already (user material or user selected factory handled above).
			// If it's a factory key (shouldn't happen here), treat as Save As (but we handled that above).
			if (MaterialLibraryWidget::sharedMaterialMap().contains(key) &&
				!MaterialLibraryWidget::s_userMaterialKeys.contains(key))
			{
				// defensive: treat as Save As flow (same as above)
				QMessageBox::information(this, tr("Save As New User Material"),
					tr("A factory material is selected. A new user material will be created instead."));
				// redirect to same logic as factory-case: ask for new key/name -- for brevity, we just abort here.
				// (Alternatively, you could loop into the Save As flow above.)
				return;
			}
		}
	}

	// Final sanity: key/name/groupLabel must be set
	if (key.isEmpty() || name.isEmpty() || groupLabel.isEmpty())
	{
		QMessageBox::warning(this, tr("Save Failed"), tr("Invalid material name/key/group."));
		return;
	}

	// Save via helper (will prompt for overwrite if key exists and is user)
	QString err;
	bool saved = MaterialLibraryWidget::saveUserMaterialToUserLocation(groupLabel, key, name, mat, this, &err);
	if (!saved)
	{
		// if user cancelled overwrite, err may be "User cancelled overwrite" — only show error if real error
		if (!err.isEmpty() && err != QStringLiteral("User cancelled overwrite"))
		{
			QMessageBox::warning(this, tr("Save Material Failed"), err);
		}
		return;
	}

	// Mark key as user key (save helper should also have done this, but ensure it)
	MaterialLibraryWidget::s_userMaterialKeys.insert(key);

	// Notify registry/widgets
	Q_EMIT MaterialRegistry::instance().materialsChanged();

	// Make the new/updated material the current selection
	auto* lib = qobject_cast<MaterialLibraryWidget*>(treeWidget);
	if (lib)
	{
		lib->selectMaterialByKey(key);
	}

	QMessageBox::information(this, tr("Material Saved"), tr("Material '%1' saved to your library as '%2'.").arg(name, key));
}


void MaterialEditorPanel::onDeleteButtonClicked()
{
	treeWidget->deleteSelectedMaterial();
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
