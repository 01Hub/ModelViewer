#include "MaterialEditorPanel.h"
#include "MaterialRegistry.h"
#include "MaterialLibraryWidget.h"
#include "LanguageManager.h"
#include "Utils.h"
#include "PathUtils.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QColorDialog>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QScrollArea>
#include <functional>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

#include "ui_MaterialEditorPanel.h"

MaterialEditorPanel::MaterialEditorPanel(QWidget* parent)
	: QWidget(parent), ui(std::make_unique<Ui::MaterialEditorPanel>())
{
	// Setup UI from .ui file
	ui->setupUi(this);

	connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this, [this]() {
		ui->retranslateUi(this);
		});

	// Set initial preview shape and settings
	ui->previewWidget->setPreviewShape(PreviewShape::Sphere);  // Default to Sphere
	ui->previewWidget->setPreviewProfile(PreviewProfile::MaterialShowcase);
	ui->previewWidget->setMaterial(_currentMaterial);

	// Connect modelCombo to preview - MUST come AFTER initialization
	connect(ui->modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, [this](int index) {
			ui->previewWidget->setPreviewShape(static_cast<PreviewShape>(index));
		});

	// Connections
	connect(ui->detachButton, &QToolButton::clicked,
		this, &MaterialEditorPanel::onDetachButtonClicked);

	connect(ui->treeWidget, &MaterialLibraryWidget::materialPreview,
		this, &MaterialEditorPanel::onMaterialPreview);
	connect(ui->treeWidget, &MaterialLibraryWidget::materialSelected,
		this, &MaterialEditorPanel::onMaterialSelected);

	connect(ui->albedoButton, &QPushButton::clicked, this, [=]() {
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
			ui->previewWidget->setMaterial(_currentMaterial);
			ui->albedoButton->setStyleSheet(makeButtonStyleSheet(c));
			emit materialChanged(_currentMaterial);
		}
		});

	connect(ui->metalnessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [=](double val) {
			_currentMaterial.setMetalness(val);
			_currentMaterial.convertToBlinnPhong();
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});

	connect(ui->roughnessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [=](double val) {
			_currentMaterial.setRoughness(val);
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});

	connect(ui->opacitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [=](double val) {
			_currentMaterial.setOpacity(val);
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});

	connect(ui->emissiveSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [=](double val) {
			_currentMaterial.setEmissiveStrength(val);
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});

	// Iridescence connections
	connect(ui->iridescenceFactorSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [=](double val) {
			_currentMaterial.setIridescenceFactor(val);
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});
	connect(ui->iridescenceIorSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [=](double val) {
			_currentMaterial.setIridescenceIor(val);
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});
	connect(ui->iridescenceThicknessMinSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [=](double val) {
			_currentMaterial.setIridescenceThicknessMin(val);
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});
	connect(ui->iridescenceThicknessMaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [=](double val) {
			_currentMaterial.setIridescenceThicknessMax(val);
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});

	connect(ui->clearcoatSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [=](double val) {
			_currentMaterial.setClearcoat(val);
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});

	connect(ui->clearcoatRoughnessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [=](double val) {
			_currentMaterial.setClearcoatRoughness(val);
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});

	connect(ui->sheenColorButton, &QPushButton::clicked, this, [=]() {
		QColor sheenColor(
			int(_currentMaterial.sheenColor().x() * 255),
			int(_currentMaterial.sheenColor().y() * 255),
			int(_currentMaterial.sheenColor().z() * 255)
		);
		QColor c = QColorDialog::getColor(sheenColor, this, "Select Sheen Color");
		if (c.isValid())
		{
			_currentMaterial.setSheenColor(QVector3D(c.redF(), c.greenF(), c.blueF()));
			ui->previewWidget->setMaterial(_currentMaterial);
			ui->sheenColorButton->setStyleSheet(makeButtonStyleSheet(c));
			emit materialChanged(_currentMaterial);
		}
		});

	connect(ui->sheenRoughnessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [=](double val) {
			_currentMaterial.setSheenRoughness(val);
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});

	connect(ui->iorSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [=](double val) {
			_currentMaterial.setIOR(val);
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});

	connect(ui->transmissionSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [=](double val) {
			_currentMaterial.setTransmission(val);
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});

	connect(ui->thicknessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [=](double val) {
			_currentMaterial.setThicknessFactor(val);
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});

	connect(ui->attenuationDistanceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [=](double val) {
			_currentMaterial.setAttenuationDistance(val);
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});

	connect(ui->attenuationColorButton, &QPushButton::clicked, this, [=]() {
		QColor attenuationColor(
			int(_currentMaterial.attenuationColor().x() * 255),
			int(_currentMaterial.attenuationColor().y() * 255),
			int(_currentMaterial.attenuationColor().z() * 255)
		);
		QColor c = QColorDialog::getColor(attenuationColor, this, "Select Attenuation Color");
		if (c.isValid())
		{
			_currentMaterial.setAttenuationColor(QVector3D(c.redF(), c.greenF(), c.blueF()));
			ui->previewWidget->setMaterial(_currentMaterial);
			ui->attenuationColorButton->setStyleSheet(makeButtonStyleSheet(c));
			emit materialChanged(_currentMaterial);
		}
		});

	connect(ui->dispersionSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [=](double val) {
			_currentMaterial.setDispersion(val);
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});

	connect(ui->alphaThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [=](double val) {
			_currentMaterial.setAlphaThreshold(val);
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});


	connect(ui->shadingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, [=](int idx) {
			_currentMaterial.setShadingModel(static_cast<GLMaterial::ShadingModel>(idx));
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});

	connect(ui->blendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, [=](int idx) {
			_currentMaterial.setBlendMode(static_cast<GLMaterial::BlendMode>(idx));
			ui->previewWidget->setMaterial(_currentMaterial);
			emit materialChanged(_currentMaterial);
		});

	connect(ui->twoSidedCheck, &QCheckBox::toggled, this, [=](bool checked) {
		_currentMaterial.setTwoSided(checked);
		ui->previewWidget->setMaterial(_currentMaterial);
		emit materialChanged(_currentMaterial);
		});

	connect(ui->wireframeCheck, &QCheckBox::toggled, this, [=](bool checked) {
		_currentMaterial.setWireframe(checked);
		ui->previewWidget->setMaterial(_currentMaterial);
		emit materialChanged(_currentMaterial);
		});

	connect(ui->applyButton, &QPushButton::clicked, this, [=]() {
		// Apply the current material settings to the preview widget
		ui->previewWidget->setMaterial(_currentMaterial);
		emit materialApplied(_currentMaterial);
		});

	connect(ui->saveButton, &QPushButton::clicked, this, [=]() {
		onSaveButtonClicked();
		emit materialChanged(_currentMaterial);
		});

	connect(ui->deleteButton, &QPushButton::clicked, this, [=]() {
		onDeleteButtonClicked();
		emit materialChanged(_currentMaterial);
		});

	// -------------------------
	// Simple search/filter for the tree
	// -------------------------
	// The lambda below will recursively check items and show/hide them based on the search text.
	// Modified to show children when parent matches, even if children don't match the search term.
	connect(ui->searchEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
		QTreeWidget* tw = qobject_cast<QTreeWidget*>(ui->treeWidget);
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
			ui->searchEdit->setStyleSheet("");
			ui->searchEdit->setToolTip("");
		}
		else if (matchCount == 0)
		{
			ui->searchEdit->setStyleSheet("QLineEdit { border: 1px solid #e53935; }");
			ui->searchEdit->setToolTip("No matches");
		}
		else
		{
			ui->searchEdit->setStyleSheet("");
			ui->searchEdit->setToolTip("");
		}
		});	
}

MaterialEditorPanel::~MaterialEditorPanel() = default;

void MaterialEditorPanel::onSaveButtonClicked()
{
	// current material instance from editor
	GLMaterial mat = _currentMaterial;

	// Derive defaults from current selection in the material tree (if any)
	QString key;
	QString name;
	QString groupLabel;

	if (ui->treeWidget)
	{
		QList<QTreeWidgetItem*> sel = ui->treeWidget->selectedItems();
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
					// This is a user key - confirm overwrite (save helper will also ask). Ask user if want to overwrite:
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
				// key not present anywhere - accept
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
				// (Alternatively, loop into the Save As flow above.)
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
		// if user cancelled overwrite, err may be "User cancelled overwrite" - only show error if real error
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
	auto* lib = qobject_cast<MaterialLibraryWidget*>(ui->treeWidget);
	if (lib)
	{
		lib->selectMaterialByKey(key);
	}

	QMessageBox::information(this, tr("Material Saved"), tr("Material '%1' saved to your library as '%2'.").arg(name, key));
}


void MaterialEditorPanel::onDeleteButtonClicked()
{
	ui->treeWidget->deleteSelectedMaterial();
}

void MaterialEditorPanel::setDetached(bool detached)
{
	_detached = detached;
	ui->detachButton->setVisible(!_detached);
	ui->detachButton->setToolTip(tr("Detach from panel"));
	ui->separator->setVisible(!_detached);
}

void MaterialEditorPanel::onDetachButtonClicked()
{
	emit detachRequested();
}

void MaterialEditorPanel::onMaterialPreview(const GLMaterial& mat)
{
	_currentMaterial = mat;

	// Load textures from materials.json for this material
	QList<QTreeWidgetItem*> selected = ui->treeWidget->selectedItems();
	if (!selected.isEmpty())
	{
		QString matKey = selected.first()->data(0, Qt::UserRole).toString();
		if (!matKey.isEmpty())
		{
			loadTexturesIntoMaterial(_currentMaterial, matKey);
		}
	}

	ui->previewWidget->setMaterial(_currentMaterial);

	updateUI(mat);
}

void MaterialEditorPanel::onMaterialSelected(const GLMaterial& mat)
{
	qDebug() << "=== onMaterialSelected CALLED ===";
	_currentMaterial = mat;

	// Load textures from materials.json for this material
	QList<QTreeWidgetItem*> selected = ui->treeWidget->selectedItems();
	qDebug() << "Selected items count:" << selected.size();

	if (!selected.isEmpty())
	{
		QString matKey = selected.first()->data(0, Qt::UserRole).toString();
		qDebug() << "Material key from tree:" << matKey;

		if (!matKey.isEmpty())
		{
			qDebug() << "Calling loadTexturesIntoMaterial for key:" << matKey;
			loadTexturesIntoMaterial(_currentMaterial, matKey);
		}
		else
		{
			qDebug() << "WARNING: matKey is empty!";
		}
	}
	else
	{
		qDebug() << "WARNING: No items selected in tree! Textures will NOT be loaded.";
	}

	ui->previewWidget->setMaterial(_currentMaterial);

	updateUI(mat);

	// DEBUG: Log what paths are in the material before emission
	qDebug() << "=== onMaterialSelected - BEFORE materialApplied emission ===";
	qDebug() << "  _currentMaterial.albedoMapPath():" << _currentMaterial.albedoMapPath();
	qDebug() << "  _currentMaterial.normalMapPath():" << _currentMaterial.normalMapPath();
	qDebug() << "  _currentMaterial.metallicMapPath():" << _currentMaterial.metallicMapPath();
	qDebug() << "  _currentMaterial.roughnessMapPath():" << _currentMaterial.roughnessMapPath();

	// Apply to main viewer mesh (with warning if no selection)
	qDebug() << "Emitting materialApplied signal...";
	emit materialApplied(_currentMaterial);
	qDebug() << "=== onMaterialSelected - AFTER materialApplied emission ===";
}

void MaterialEditorPanel::updateUI(const GLMaterial& mat)
{
	QColor albedoColor(
		int(mat.albedoColor().x() * 255),
		int(mat.albedoColor().y() * 255),
		int(mat.albedoColor().z() * 255)
	);
	ui->albedoButton->setStyleSheet(makeButtonStyleSheet(albedoColor));

	ui->metalnessSpin->setValue(mat.metalness());
	ui->roughnessSpin->setValue(mat.roughness());
	ui->opacitySpin->setValue(mat.opacity());
	ui->emissiveSpin->setValue(mat.emissiveStrength());
	ui->iridescenceFactorSpin->setValue(mat.iridescenceFactor());
	ui->iridescenceIorSpin->setValue(mat.iridescenceIor());
	ui->iridescenceThicknessMinSpin->setValue(mat.iridescenceThicknessMin());
	ui->iridescenceThicknessMaxSpin->setValue(mat.iridescenceThicknessMax());
	ui->clearcoatSpin->setValue(mat.clearcoat());
	ui->clearcoatRoughnessSpin->setValue(mat.clearcoatRoughness());
	ui->sheenRoughnessSpin->setValue(mat.sheenRoughness());

	QColor sheenColor(
		int(mat.sheenColor().x() * 255),
		int(mat.sheenColor().y() * 255),
		int(mat.sheenColor().z() * 255)
	);
	ui->sheenColorButton->setStyleSheet(makeButtonStyleSheet(sheenColor));

	ui->iorSpin->setValue(mat.ior());
	ui->transmissionSpin->setValue(mat.transmission());
	ui->thicknessSpin->setValue(mat.thicknessFactor());
	ui->attenuationDistanceSpin->setValue(mat.attenuationDistance());
	QColor attenuationColor(
		int(mat.attenuationColor().x() * 255),
		int(mat.attenuationColor().y() * 255),
		int(mat.attenuationColor().z() * 255)
	);
	ui->attenuationColorButton->setStyleSheet(makeButtonStyleSheet(attenuationColor));
	ui->dispersionSpin->setValue(mat.dispersion());
	ui->alphaThresholdSpin->setValue(mat.alphaThreshold());

	ui->shadingCombo->setCurrentIndex(static_cast<int>(mat.shadingModel()));
	ui->blendCombo->setCurrentIndex(static_cast<int>(mat.blendMode()));
	ui->twoSidedCheck->setChecked(mat.twoSided());
	ui->wireframeCheck->setChecked(mat.wireframe());
}

void MaterialEditorPanel::loadTexturesIntoMaterial(GLMaterial& material, const QString& materialKey)
{
	qDebug() << "=== loadTexturesIntoMaterial START ===" << materialKey;

	// Load unified materials.json
	QString jsonPath = PathUtils::getDataDirectory() + "/data/catalogs/materials.json";
	qDebug() << "JSON path:" << jsonPath;
	QFile file(jsonPath);

	if (!file.open(QIODevice::ReadOnly))
	{
		qDebug() << "ERROR: Cannot open materials.json";
		return;
	}

	QJsonObject catalog = QJsonDocument::fromJson(file.readAll()).object();
	file.close();

	// Find material in groups
	QJsonObject materialObj;
	for (const QJsonValue& groupVal : catalog["groups"].toArray())
	{
		for (const QJsonValue& matVal : groupVal.toObject()["items"].toArray())
		{
			if (matVal.toObject()["key"].toString() == materialKey)
			{
				materialObj = matVal.toObject();
				break;
			}
		}
		if (!materialObj.isEmpty())
			break;
	}

	if (materialObj.isEmpty())
	{
		qDebug() << "ERROR: Material not found in JSON:" << materialKey;
		return;
	}

	qDebug() << "Material found:" << materialKey;
	qDebug() << "Available keys in material object:" << materialObj.keys();

	QString baseDir = PathUtils::getDataDirectory() + "/";

	// Load texture paths from JSON and set them on the material
	if (materialObj.contains("albedoMapPath"))
	{
		QString relativePath = materialObj["albedoMapPath"].toString();
		qDebug() << "albedoMapPath found - relative:" << relativePath;
		if (!relativePath.isEmpty())
		{
			QString fullPath = baseDir + relativePath;
			bool fileExists = QFile::exists(fullPath);
			qDebug() << "albedoMapPath full:" << fullPath;
			qDebug() << "albedoMapPath exists:" << fileExists;
			if (fileExists)
			{
				material.setAlbedoMap(fullPath);

				// Also populate unified storage (syncs with setTexture)
				auto tex = material.texture(GLMaterial::TextureType::Albedo);
				tex.path = fullPath.toStdString();
				material.setTexture(GLMaterial::TextureType::Albedo, tex);

				qDebug() << "ALBEDO MAP LOADED";
			}
			else
			{
				qDebug() << "WARNING: Albedo map file not found at" << fullPath;
			}
		}
		else
		{
			qDebug() << "WARNING: albedoMapPath is empty";
		}
	}
	else
	{
		qDebug() << "WARNING: 'albedoMapPath' key NOT found in material object";
	}

	if (materialObj.contains("metallicMapPath"))
	{
		QString relativePath = materialObj["metallicMapPath"].toString();
		if (!relativePath.isEmpty())
		{
			QString fullPath = baseDir + relativePath;
			if (QFile::exists(fullPath))
			{
				material.setMetallicMap(fullPath);
				auto tex = material.texture(GLMaterial::TextureType::Metallic);
				tex.path = fullPath.toStdString();
				material.setTexture(GLMaterial::TextureType::Metallic, tex);
			}
		}
	}

	if (materialObj.contains("roughnessMapPath"))
	{
		QString relativePath = materialObj["roughnessMapPath"].toString();
		if (!relativePath.isEmpty())
		{
			QString fullPath = baseDir + relativePath;
			if (QFile::exists(fullPath))
			{
				material.setRoughnessMap(fullPath);
				auto tex = material.texture(GLMaterial::TextureType::Roughness);
				tex.path = fullPath.toStdString();
				material.setTexture(GLMaterial::TextureType::Roughness, tex);
			}
		}
	}

	if (materialObj.contains("normalMapPath"))
	{
		QString relativePath = materialObj["normalMapPath"].toString();
		qDebug() << "normalMapPath found - relative:" << relativePath;
		if (!relativePath.isEmpty())
		{
			QString fullPath = baseDir + relativePath;
			bool fileExists = QFile::exists(fullPath);
			qDebug() << "normalMapPath full:" << fullPath;
			qDebug() << "normalMapPath exists:" << fileExists;
			if (fileExists)
			{
				material.setNormalMap(fullPath);

				// Also populate unified storage (syncs with setTexture)
				auto tex = material.texture(GLMaterial::TextureType::Normal);
				tex.path = fullPath.toStdString();
				material.setTexture(GLMaterial::TextureType::Normal, tex);

				qDebug() << "NORMAL MAP LOADED";
			}
			else
			{
				qDebug() << "WARNING: Normal map file not found at" << fullPath;
			}
		}
		else
		{
			qDebug() << "WARNING: normalMapPath is empty";
		}
	}
	else
	{
		qDebug() << "WARNING: 'normalMapPath' key NOT found in material object";
	}

	if (materialObj.contains("aoMapPath"))
	{
		QString relativePath = materialObj["aoMapPath"].toString();
		if (!relativePath.isEmpty())
		{
			QString fullPath = baseDir + relativePath;
			if (QFile::exists(fullPath))
			{
				material.setAOMap(fullPath);
				auto tex = material.texture(GLMaterial::TextureType::AmbientOcclusion);
				tex.path = fullPath.toStdString();
				material.setTexture(GLMaterial::TextureType::AmbientOcclusion, tex);
			}
		}
	}

	if (materialObj.contains("heightMapPath"))
	{
		QString relativePath = materialObj["heightMapPath"].toString();
		if (!relativePath.isEmpty())
		{
			QString fullPath = baseDir + relativePath;
			if (QFile::exists(fullPath))
			{
				material.setHeightMap(fullPath);
				auto tex = material.texture(GLMaterial::TextureType::Height);
				tex.path = fullPath.toStdString();
				material.setTexture(GLMaterial::TextureType::Height, tex);
			}
		}
	}

	if (materialObj.contains("emissiveMapPath"))
	{
		QString relativePath = materialObj["emissiveMapPath"].toString();
		if (!relativePath.isEmpty())
		{
			QString fullPath = baseDir + relativePath;
			if (QFile::exists(fullPath))
			{
				material.setEmissiveMap(fullPath);
				auto tex = material.texture(GLMaterial::TextureType::Emissive);
				tex.path = fullPath.toStdString();
				material.setTexture(GLMaterial::TextureType::Emissive, tex);
			}
		}
	}

	if (materialObj.contains("opacityMapPath"))
	{
		QString relativePath = materialObj["opacityMapPath"].toString();
		if (!relativePath.isEmpty())
		{
			QString fullPath = baseDir + relativePath;
			if (QFile::exists(fullPath))
			{
				material.setOpacityMap(fullPath);
				auto tex = material.texture(GLMaterial::TextureType::Opacity);
				tex.path = fullPath.toStdString();
				material.setTexture(GLMaterial::TextureType::Opacity, tex);
			}
		}
	}

	qDebug() << "=== loadTexturesIntoMaterial END ===";
}
