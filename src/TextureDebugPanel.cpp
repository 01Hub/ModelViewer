#include "TextureDebugPanel.h"
#include "GLWidget.h"
#include "ModelViewer.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QShowEvent>
#include <QToolButton>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr int ThumbSize    = 80;   // thumbnail width/height in pixels
static constexpr int ThumbColumns = 3;    // swatches per row in the grid

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{

// Grey checkerboard placeholder for inactive texture slots.
QPixmap inactivePlaceholder()
{
	QPixmap pm(ThumbSize, ThumbSize);
	pm.fill(Qt::transparent);
	QPainter p(&pm);
	const QColor c1(80, 80, 80);
	const QColor c2(55, 55, 55);
	const int sq = ThumbSize / 8;
	for (int row = 0; row < 8; ++row)
	{
		for (int col = 0; col < 8; ++col)
		{
			p.fillRect(col * sq, row * sq, sq, sq,
			           ((row + col) % 2 == 0) ? c1 : c2);
		}
	}
	return pm;
}

// Small coloured dot used for extension flag indicators.
// active=false → grey outline (extension not in use)
// active=true, disabledByUser=false → green filled (active, toggleable)
// active=true, disabledByUser=true  → amber filled with X (active but suppressed)
QPixmap dotPixmap(bool active, bool disabledByUser = false)
{
	const int sz = 14;
	QPixmap pm(sz, sz);
	pm.fill(Qt::transparent);
	QPainter p(&pm);
	p.setRenderHint(QPainter::Antialiasing);

	QColor col;
	bool filled = false;
	if (!active)
	{
		col    = QColor(100, 100, 100);
		filled = false;
	}
	else if (disabledByUser)
	{
		col    = QColor(232, 168, 56);  // amber
		filled = true;
	}
	else
	{
		col    = QColor(72, 199, 116);  // green
		filled = true;
	}

	p.setPen(QPen(col.darker(140), 1));
	p.setBrush(filled ? QBrush(col) : Qt::NoBrush);
	p.drawEllipse(1, 1, sz - 2, sz - 2);

	// Strikethrough X when disabled by user so the state is unmistakeable.
	if (disabledByUser)
	{
		p.setPen(QPen(col.darker(170), 1.5f));
		p.drawLine(4, 4, sz - 4, sz - 4);
		p.drawLine(sz - 4, 4, 4, sz - 4);
	}
	return pm;
}

// Helper: create a horizontal separator line.
QFrame* makeSeparator(QWidget* parent)
{
	auto* line = new QFrame(parent);
	line->setFrameShape(QFrame::HLine);
	line->setFrameShadow(QFrame::Sunken);
	return line;
}

// Helper: clear all widgets from a QGridLayout.
// Uses delete (not deleteLater) so widgets are removed from the parent's
// child list immediately — deleteLater leaves them as visible children of
// the container until the next event-loop iteration, which means the old
// swatches can repaint after clearDynamicContent() has returned.
void clearLayout(QGridLayout* layout)
{
	if (!layout) return;
	while (layout->count() > 0)
	{
		QLayoutItem* item = layout->takeAt(0);
		if (item->widget())
			delete item->widget();   // immediate destruction, not deferred
		delete item;
	}
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
TextureDebugPanel::TextureDebugPanel(QWidget* parent)
	: QDialog(parent,
	          Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint |
	          Qt::WindowMinMaxButtonsHint)
{
	setWindowTitle(tr("Texture Debugger"));
	setObjectName("TextureDebugPanel");
	buildUI();
	restoreWindowGeometry();
}

void TextureDebugPanel::setGLWidget(GLWidget* gl)
{
	_glWidget = gl;
}

void TextureDebugPanel::setModelViewer(ModelViewer* mv)
{
	_modelViewer = mv;
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------
void TextureDebugPanel::buildUI()
{
	auto* root = new QVBoxLayout(this);
	root->setContentsMargins(8, 8, 8, 8);
	root->setSpacing(6);

	// ---- Header row --------------------------------------------------------
	{
		auto* headerRow = new QHBoxLayout;
		_meshNameLabel = new QLabel(tr("No mesh selected"), this);
		QFont f = _meshNameLabel->font();
		f.setBold(true);
		_meshNameLabel->setFont(f);
		_meshNameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

		_refreshButton = new QPushButton(tr("↻ Refresh"), this);
		_refreshButton->setFixedWidth(80);
		connect(_refreshButton, &QPushButton::clicked, this, &TextureDebugPanel::refresh);

		headerRow->addWidget(_meshNameLabel);
		headerRow->addWidget(_refreshButton);
		root->addLayout(headerRow);
	}

	root->addWidget(makeSeparator(this));

	// ---- Textures section --------------------------------------------------
	{
		auto* sectionLabel = new QLabel(tr("TEXTURES"), this);
		QFont f = sectionLabel->font();
		f.setBold(true);
		sectionLabel->setFont(f);

		_showInactiveCheck = new QCheckBox(tr("Show inactive slots"), this);
		_showInactiveCheck->setChecked(false);
		connect(_showInactiveCheck, &QCheckBox::toggled, this, [this](bool) {
			if (!_lastSlots.isEmpty())
				populateThumbnails(_lastSlots);
		});

		auto* texHeaderRow = new QHBoxLayout;
		texHeaderRow->addWidget(sectionLabel);
		texHeaderRow->addStretch();
		texHeaderRow->addWidget(_showInactiveCheck);
		root->addLayout(texHeaderRow);

		// Scrollable thumbnail grid
		_thumbnailContainer = new QWidget(this);
		_thumbnailGrid = new QGridLayout(_thumbnailContainer);
		_thumbnailGrid->setSpacing(6);
		_thumbnailGrid->setContentsMargins(4, 4, 4, 4);

		_thumbnailScroll = new QScrollArea(this);
		_thumbnailScroll->setWidget(_thumbnailContainer);
		_thumbnailScroll->setWidgetResizable(true);
		_thumbnailScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		_thumbnailScroll->setMinimumHeight(200);
		_thumbnailScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		root->addWidget(_thumbnailScroll, /*stretch=*/1);
	}

	root->addWidget(makeSeparator(this));

	// ---- Extensions section ------------------------------------------------
	{
		_extensionGroup  = new QGroupBox(tr("Extensions"), this);
		_extensionLayout = new QGridLayout(_extensionGroup);
		_extensionLayout->setSpacing(4);
		_extensionLayout->setContentsMargins(6, 4, 6, 4);
		root->addWidget(_extensionGroup);
	}

	// ---- Multiplexing health section ---------------------------------------
	{
		_multiplexGroup  = new QGroupBox(tr("Multiplexing Health"), this);
		_multiplexLayout = new QGridLayout(_multiplexGroup);
		_multiplexLayout->setSpacing(4);
		_multiplexLayout->setContentsMargins(6, 4, 6, 4);
		root->addWidget(_multiplexGroup);
	}

	// ---- Status strip -------------------------------------------------------
	// Shows warnings such as "multiple meshes selected — showing first only".
	// Hidden when there is nothing to report.
	root->addWidget(makeSeparator(this));
	{
		_statusLabel = new QLabel(this);
		_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		_statusLabel->setWordWrap(true);
		QFont sf = _statusLabel->font();
		sf.setPointSize(11);
		_statusLabel->setFont(sf);
		_statusLabel->setContentsMargins(2, 2, 2, 2);
		_statusLabel->hide();  // hidden until there is something to say
		root->addWidget(_statusLabel);
	}

	setMinimumWidth(380);
	resize(420, 620);
}

// ---------------------------------------------------------------------------
// Slot: onSelectionChanged
// ---------------------------------------------------------------------------
void TextureDebugPanel::onSelectionChanged(const QList<int>& selectedIds)
{
	if (selectedIds.isEmpty())
	{
		// Clear any debug overrides that were set on the deselected mesh.
		if (_currentMeshId >= 0 && _glWidget)
		{
			_glWidget->clearDebugTextureOverrides(_currentMeshId);
			_glWidget->clearDebugExtensionOverrides(_currentMeshId);
		}
		_disabledUnits.clear();
		_disabledExtensions.clear();

		_currentMeshId = -1;
		_lastSlots.clear();
		clearDynamicContent();
		_meshNameLabel->setText(tr("No mesh selected"));
		_statusLabel->hide();
		_statusLabel->clear();
		return;
	}

	const int meshId = selectedIds.first();

	// Multi-select warning — always update the status strip.
	if (selectedIds.count() > 1)
	{
		_statusLabel->setText(
		    tr("⚠  %1 meshes selected — showing textures for the mesh in the title bar.")
		    .arg(selectedIds.count()));
		_statusLabel->setStyleSheet("color: #e8a838;");
		_statusLabel->show();
	}
	else
	{
		_statusLabel->hide();
		_statusLabel->clear();
	}

	if (meshId == _currentMeshId && !_lastSlots.isEmpty())
		return; // same mesh — no need to re-read

	// Switching to a different mesh: restore the old one first.
	if (_currentMeshId >= 0 && meshId != _currentMeshId && _glWidget)
	{
		_glWidget->clearDebugTextureOverrides(_currentMeshId);
		_glWidget->clearDebugExtensionOverrides(_currentMeshId);
	}
	_disabledUnits.clear();
	_disabledExtensions.clear();

	_currentMeshId = meshId;

	if (isVisible() && _glWidget)
		_glWidget->requestTextureReadback(_currentMeshId);
}

// ---------------------------------------------------------------------------
// Slot: refresh
// ---------------------------------------------------------------------------
void TextureDebugPanel::refresh()
{
	if (_currentMeshId >= 0 && _glWidget)
		_glWidget->requestTextureReadback(_currentMeshId);
	else if (_currentMeshId < 0)
	{
		clearDynamicContent();
		_meshNameLabel->setText(tr("No mesh selected"));
	}
}

// ---------------------------------------------------------------------------
// Slot: onTextureReadbackReady
// ---------------------------------------------------------------------------
// NOTE: parameter is named 'slotInfos', not 'slots', because 'slots' is a
// Qt macro that expands to empty and would silently corrupt every use.
void TextureDebugPanel::onTextureReadbackReady(const QVector<TextureSlotInfo>& slotInfos,
                                               const QString& meshName)
{
	_lastSlots = slotInfos;

	_meshNameLabel->setText(
	    meshName.isEmpty() ? tr("Unknown mesh") : tr("Mesh: %1").arg(meshName));

	populateThumbnails(slotInfos);
	populateExtensions(slotInfos);
	populateMultiplexingHealth(slotInfos);
}

// ---------------------------------------------------------------------------
// populateThumbnails
// ---------------------------------------------------------------------------
void TextureDebugPanel::populateThumbnails(const QVector<TextureSlotInfo>& slotInfos)
{
	clearLayout(_thumbnailGrid);

	const bool showInactive = _showInactiveCheck->isChecked();
	const QPixmap placeholder = inactivePlaceholder();

	int col = 0, row = 0;
	for (const TextureSlotInfo& info : slotInfos)
	{
		if (!info.isActive && !showInactive)
			continue;

		const bool isDisabled = _disabledUnits.contains(info.unitIndex);

		// Cell widget
		auto* cell = new QWidget(_thumbnailContainer);
		auto* cellLayout = new QVBoxLayout(cell);
		cellLayout->setSpacing(2);
		cellLayout->setContentsMargins(0, 0, 0, 0);
		cellLayout->setAlignment(Qt::AlignHCenter);

		// ---- Thumbnail as a checkable QToolButton ----------------------------
		// Checked state = texture is DISABLED.  Clicking toggles the override.
		auto* thumbBtn = new QToolButton(cell);
		thumbBtn->setCheckable(true);
		thumbBtn->setChecked(isDisabled);
		thumbBtn->setFixedSize(ThumbSize, ThumbSize);
		thumbBtn->setAutoRaise(false);
		thumbBtn->setToolTip(isDisabled
		    ? tr("Click to re-enable this texture")
		    : tr("Click to disable this texture in the viewport"));

		// Show the real thumbnail when active and not disabled; checkerboard otherwise.
		const QPixmap& thumbPx = (info.isActive && !info.thumbnail.isNull() && !isDisabled)
		    ? info.thumbnail : placeholder;
		thumbBtn->setIcon(QIcon(thumbPx));
		thumbBtn->setIconSize(QSize(ThumbSize - 4, ThumbSize - 4));

		// Border: red when disabled, amber for multiplexed, default otherwise.
		if (isDisabled)
			thumbBtn->setStyleSheet(
			    "QToolButton { border: 2px solid #e74c3c; background: transparent; }"
			    "QToolButton:checked { border: 2px solid #e74c3c; background: #2a1010; }");
		else if (info.isMultiplexed)
			thumbBtn->setStyleSheet(
			    "QToolButton { border: 1px solid #e8a838; background: transparent; }");
		else
			thumbBtn->setStyleSheet(
			    "QToolButton { border: 1px solid #555; background: transparent; }");

		// Wire toggle: disable/enable the texture in GLWidget.
		const int unitIdx = info.unitIndex;
		connect(thumbBtn, &QToolButton::toggled, this, [this, unitIdx](bool checked) {
			// checked == true  → user wants to DISABLE the texture
			// checked == false → user wants to RE-ENABLE the texture
			if (checked)
				_disabledUnits.insert(unitIdx);
			else
				_disabledUnits.remove(unitIdx);

			if (_glWidget)
				_glWidget->setDebugTextureEnabled(_currentMeshId, unitIdx, !checked);

			// Repopulate so the thumbnail visual state updates immediately.
			if (!_lastSlots.isEmpty())
				populateThumbnails(_lastSlots);
		});

		// ---- Slot name -------------------------------------------------------
		auto* nameLabel = new QLabel(cell);
		nameLabel->setAlignment(Qt::AlignHCenter);
		QFont nf = nameLabel->font();
		nf.setPointSize(11);
		nameLabel->setFont(nf);
		nameLabel->setWordWrap(false);
		nameLabel->setFixedWidth(ThumbSize);
		nameLabel->setText(nameLabel->fontMetrics().elidedText(
		    info.slotName, Qt::ElideRight, ThumbSize));
		if (isDisabled)
			nameLabel->setStyleSheet("color: #e74c3c;");
		else if (!info.isActive)
			nameLabel->setStyleSheet("color: #777;");

		// ---- Unit badge ------------------------------------------------------
		auto* unitLabel = new QLabel(cell);
		unitLabel->setAlignment(Qt::AlignHCenter);
		QFont uf = unitLabel->font();
		uf.setPointSize(11);
		unitLabel->setFont(uf);
		unitLabel->setText(QString("u%1").arg(info.unitIndex));
		if (isDisabled)
			unitLabel->setStyleSheet("color: #e74c3c;");
		else if (info.isMultiplexed)
			unitLabel->setStyleSheet("color: #e8a838; font-weight: bold;");
		else if (!info.isActive)
			unitLabel->setStyleSheet("color: #555;");

		cellLayout->addWidget(thumbBtn);
		cellLayout->addWidget(nameLabel);
		cellLayout->addWidget(unitLabel);

		_thumbnailGrid->addWidget(cell, row, col);

		if (++col >= ThumbColumns)
		{
			col = 0;
			++row;
		}
	}

	// Pad the last row so the grid doesn't stretch oddly
	if (col > 0)
	{
		for (int c = col; c < ThumbColumns; ++c)
			_thumbnailGrid->addWidget(new QWidget(_thumbnailContainer), row, c);
	}

	_thumbnailContainer->adjustSize();
}

// ---------------------------------------------------------------------------
// populateExtensions
// ---------------------------------------------------------------------------
void TextureDebugPanel::populateExtensions(const QVector<TextureSlotInfo>& slotInfos)
{
	clearLayout(_extensionLayout);

	// Build a quick lookup: unit → extensionEnabled
	// extensionEnabled reflects actual KHR extension activity (scalar factors,
	// not just whether a texture is bound), so extension dots light up even
	// when an extension is active via factor alone (no texture).
	QHash<int, bool> activeUnits;
	for (const TextureSlotInfo& info : slotInfos)
		activeUnits[info.unitIndex] = info.extensionEnabled;

	// Extension definitions: { internal key, display name, relevant unit(s) }
	// Key is the stable identifier used by GLWidget::setDebugExtensionEnabled.
	struct ExtDef { QString key; QString name; QVector<int> units; };
	const QVector<ExtDef> extensions = {
		{ "Sheen",                tr("Sheen"),                { 20, 21 } },
		{ "Clearcoat",            tr("Clearcoat"),            { 22, 23, 24 } },
		{ "Iridescence",          tr("Iridescence"),          { 28, 29 } },
		{ "Volume / SSS",         tr("Volume / SSS"),         { 30 } },
		{ "Specular",             tr("Specular"),             { 25, 26 } },
		{ "Anisotropy",           tr("Anisotropy"),           { 27 } },
		{ "Transmission",         tr("Transmission"),         { 18 } },
		{ "Diffuse Transmission", tr("Diffuse Transmission"), { 31, 6 } },
	};

	const int cols = 3;
	int extRow = 0, extCol = 0;
	for (const ExtDef& ext : extensions)
	{
		bool isActive = false;
		for (int u : ext.units)
			if (activeUnits.value(u, false)) { isActive = true; break; }

		const bool isDisabled = _disabledExtensions.contains(ext.key);

		// Build a QToolButton that acts as the indicator and toggle.
		// Inactive extensions: non-interactive grey dot.
		// Active extensions: checkable; checked = disabled by user (amber X).
		auto* btn = new QToolButton(_extensionGroup);
		btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		btn->setIcon(QIcon(dotPixmap(isActive, isDisabled)));
		btn->setIconSize(QSize(14, 14));
		btn->setText(ext.name);
		btn->setAutoRaise(true);

		QFont f = btn->font();
		f.setPointSize(11);
		btn->setFont(f);

		if (!isActive)
		{
			// Extension not in use — show as greyed, no toggle.
			btn->setEnabled(false);
			btn->setStyleSheet("QToolButton { color: #666; }");
			btn->setToolTip(tr("Extension not active on this material"));
		}
		else if (isDisabled)
		{
			// Active but disabled by user.
			btn->setCheckable(true);
			btn->setChecked(true);
			btn->setStyleSheet("QToolButton { color: #e8a838; }");
			btn->setToolTip(tr("Click to re-enable this extension"));
		}
		else
		{
			// Active and running — click to suppress.
			btn->setCheckable(true);
			btn->setChecked(false);
			btn->setStyleSheet("QToolButton { color: #48c774; }");
			btn->setToolTip(tr("Click to disable this extension in the viewport"));
		}

		const QString extKey = ext.key;
		connect(btn, &QToolButton::toggled, this, [this, extKey](bool checked) {
			// checked == true  → disable extension; checked == false → re-enable
			if (checked)
				_disabledExtensions.insert(extKey);
			else
				_disabledExtensions.remove(extKey);

			if (_glWidget)
				_glWidget->setDebugExtensionEnabled(_currentMeshId, extKey, !checked);

			// Repopulate to update dot icons and styles immediately.
			if (!_lastSlots.isEmpty())
				populateExtensions(_lastSlots);
		});

		_extensionLayout->addWidget(btn, extRow, extCol);

		if (++extCol >= cols)
		{
			extCol = 0;
			++extRow;
		}
	}
}

// ---------------------------------------------------------------------------
// populateMultiplexingHealth
// ---------------------------------------------------------------------------
void TextureDebugPanel::populateMultiplexingHealth(const QVector<TextureSlotInfo>& slotInfos)
{
	clearLayout(_multiplexLayout);

	// Only show the four multiplexed units
	int muxRow = 0;
	for (const TextureSlotInfo& info : slotInfos)
	{
		if (!info.isMultiplexed)
			continue;

		// Unit label
		auto* unitLbl = new QLabel(QString("u%1").arg(info.unitIndex), _multiplexGroup);
		QFont f = unitLbl->font();
		f.setFamily("Courier New");
		f.setPointSize(11);
		unitLbl->setFont(f);
		unitLbl->setFixedWidth(30);

		// Slot name
		auto* nameLbl = new QLabel(info.slotName, _multiplexGroup);
		nameLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		QFont nf = nameLbl->font();
		nf.setPointSize(11);
		nameLbl->setFont(nf);

		// State label ("active" / "inactive")
		auto* stateLbl = new QLabel(
		    info.isActive ? tr("active") : tr("inactive"), _multiplexGroup);
		QFont sf = stateLbl->font();
		sf.setPointSize(11);
		sf.setItalic(true);
		stateLbl->setFont(sf);
		stateLbl->setStyleSheet(info.isActive ? "color: #e8a838;" : "color: #777;");

		// Health indicator — when the per-mesh map is active it has overridden the
		// global sampler on this unit for the current draw call.
		const bool overridesGlobal = info.isActive;
		auto* healthLbl = new QLabel(
		    overridesGlobal ? tr("⚠ overrides global") : tr("✓ ok"),
		    _multiplexGroup);
		QFont hf = healthLbl->font();
		hf.setPointSize(11);
		healthLbl->setFont(hf);
		healthLbl->setStyleSheet(overridesGlobal
		    ? "color: #e8a838; font-weight: bold;"
		    : "color: #48c774;");
		// Tooltip shows what global resource this unit is shared with.
		if (!info.multiplexNote.isEmpty())
			healthLbl->setToolTip(info.multiplexNote);

		_multiplexLayout->addWidget(unitLbl,   muxRow, 0);
		_multiplexLayout->addWidget(nameLbl,   muxRow, 1);
		_multiplexLayout->addWidget(stateLbl,  muxRow, 2);
		_multiplexLayout->addWidget(healthLbl, muxRow, 3);
		++muxRow;
	}
}

// ---------------------------------------------------------------------------
// clearDynamicContent
// ---------------------------------------------------------------------------
void TextureDebugPanel::clearDynamicContent()
{
	clearLayout(_thumbnailGrid);
	clearLayout(_extensionLayout);
	clearLayout(_multiplexLayout);
}

// ---------------------------------------------------------------------------
// showEvent / closeEvent — trigger refresh and save geometry
// ---------------------------------------------------------------------------
void TextureDebugPanel::showEvent(QShowEvent* event)
{
	QDialog::showEvent(event);
	// Trigger a readback if a mesh was already selected before the panel opened.
	if (_currentMeshId >= 0 && _lastSlots.isEmpty())
		refresh();
}

void TextureDebugPanel::closeEvent(QCloseEvent* event)
{
	// Restore all textures and extension uniforms when the panel is dismissed
	// so the viewport shows the real material again.
	if (_currentMeshId >= 0 && _glWidget)
	{
		_glWidget->clearDebugTextureOverrides(_currentMeshId);
		_glWidget->clearDebugExtensionOverrides(_currentMeshId);
	}
	_disabledUnits.clear();
	_disabledExtensions.clear();

	saveWindowGeometry();
	QDialog::closeEvent(event);
}

// ---------------------------------------------------------------------------
// Geometry persistence
// ---------------------------------------------------------------------------
void TextureDebugPanel::saveWindowGeometry()
{
	QSettings s(QCoreApplication::organizationName(),
	            QCoreApplication::applicationName());
	s.setValue("TextureDebugPanel/geometry", saveGeometry());
}

void TextureDebugPanel::restoreWindowGeometry()
{
	QSettings s(QCoreApplication::organizationName(),
	            QCoreApplication::applicationName());
	const QByteArray geo = s.value("TextureDebugPanel/geometry").toByteArray();
	if (!geo.isEmpty())
		restoreGeometry(geo);
}
