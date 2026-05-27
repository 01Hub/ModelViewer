#include "AnimationsPanel.h"

#include "GLWidget.h"
#include "SceneGraph.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>
#include <cmath>

namespace
{
QIcon makeCircleIcon(bool filled, const QColor& color)
{
	constexpr int size = 16;
	QPixmap pixmap(size, size);
	pixmap.fill(Qt::transparent);

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(QPen(color, 1.5));
	painter.setBrush(filled ? QBrush(color) : Qt::NoBrush);
	painter.drawEllipse(2, 2, size - 4, size - 4);
	return QIcon(pixmap);
}

QString formatTime(double seconds)
{
	const int totalMs = qMax(0, static_cast<int>(seconds * 1000.0));
	const int minutes = totalMs / 60000;
	const int secs = (totalMs / 1000) % 60;
	const int centiseconds = (totalMs / 10) % 100;
	return QStringLiteral("%1:%2.%3")
		.arg(minutes)
		.arg(secs, 2, 10, QLatin1Char('0'))
		.arg(centiseconds, 2, 10, QLatin1Char('0'));
}

QString formatSpeedLabel(double speed)
{
	if (std::abs(speed - std::round(speed)) < 0.0001)
		return QStringLiteral("%1x").arg(speed, 0, 'f', 1);
	if (std::abs(speed * 10.0 - std::round(speed * 10.0)) < 0.0001)
		return QStringLiteral("%1x").arg(speed, 0, 'f', 1);
	return QStringLiteral("%1x").arg(speed, 0, 'f', 2);
}
}

AnimationsPanel::AnimationsPanel(QWidget* parent)
	: QWidget(parent)
{
	auto* rootLayout = new QVBoxLayout(this);
	rootLayout->setContentsMargins(0, 0, 0, 0);
	rootLayout->setSpacing(8);

	_tree = new QTreeWidget(this);
	_tree->setHeaderHidden(true);
	_tree->setColumnCount(1);
	_tree->setRootIsDecorated(true);
	_tree->setIndentation(16);
	_tree->setAlternatingRowColors(true);
	_tree->setSelectionMode(QAbstractItemView::NoSelection);
	_tree->setFocusPolicy(Qt::NoFocus);
	rootLayout->addWidget(_tree, 1);

	auto* controlsLayout = new QVBoxLayout();
	controlsLayout->setContentsMargins(8, 0, 8, 8);
	controlsLayout->setSpacing(6);

	auto* buttonRow = new QHBoxLayout();
	buttonRow->setContentsMargins(0, 0, 0, 0);
	buttonRow->setSpacing(8);

	_playPauseButton = new QPushButton(tr("Play"), this);
	_loopCheck = new QCheckBox(tr("Loop"), this);
	_speedLabel = new QLabel(tr("Speed"), this);
	_speedCombo = new QComboBox(this);
	_timeLabel = new QLabel(tr("0:00.00 / 0:00.00"), this);
	_timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

	const QList<double> speedOptions{ 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0 };
	for (double speed : speedOptions)
		_speedCombo->addItem(formatSpeedLabel(speed), speed);
	_speedCombo->setCurrentIndex(_speedCombo->findData(1.0));
	_speedCombo->setToolTip(tr("Playback speed"));

	buttonRow->addWidget(_playPauseButton);
	buttonRow->addWidget(_loopCheck);
	buttonRow->addSpacing(12);
	buttonRow->addWidget(_speedLabel);
	buttonRow->addWidget(_speedCombo);
	buttonRow->addSpacing(12);
	buttonRow->addStretch(1);
	buttonRow->addWidget(_timeLabel);
	controlsLayout->addLayout(buttonRow);

	_timelineSlider = new QSlider(Qt::Horizontal, this);
	_timelineSlider->setRange(0, 1000);
	_timelineSlider->setEnabled(false);
	controlsLayout->addWidget(_timelineSlider);

	rootLayout->addLayout(controlsLayout);

	connect(_tree, &QTreeWidget::itemClicked, this, &AnimationsPanel::onItemClicked);
	connect(_playPauseButton, &QPushButton::clicked, this, &AnimationsPanel::onPlayPauseClicked);
	connect(_loopCheck, &QCheckBox::toggled, this, &AnimationsPanel::onLoopCheckChanged);
	connect(_speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AnimationsPanel::onPlaybackSpeedChanged);
	connect(_timelineSlider, &QSlider::sliderPressed, this, &AnimationsPanel::onSliderPressed);
	connect(_timelineSlider, &QSlider::sliderReleased, this, &AnimationsPanel::onSliderReleased);
	connect(_timelineSlider, &QSlider::valueChanged, this, &AnimationsPanel::onSliderValueChanged);
}

void AnimationsPanel::setSceneGraph(SceneGraph* sg)
{
	_sceneGraph = sg;
}

void AnimationsPanel::setGLWidget(GLWidget* glWidget)
{
	_glWidget = glWidget;
}

void AnimationsPanel::refresh()
{
	_tree->clear();
	_currentDurationSeconds = 0.0;

	if (!_sceneGraph)
	{
		updateControlsForSelection();
		return;
	}

	const QString activeFile = _glWidget ? _glWidget->activeAnimationFile() : QString();
	const int activeClip = _glWidget ? _glWidget->activeAnimationClip() : -1;

	const QStringList files = _sceneGraph->filesWithAnimations();
	for (const QString& sourceFile : files)
	{
		const GltfAnimationData data = _sceneGraph->animationDataForFile(sourceFile);
		QTreeWidgetItem* fileItem = makeFileItem(sourceFile, QFileInfo(sourceFile).fileName());
		_tree->addTopLevelItem(fileItem);

		const int sceneGraphActiveClip = _sceneGraph->activeAnimationClipForFile(sourceFile);
		const int effectiveActiveClip = (sourceFile == activeFile && activeClip >= 0) ? activeClip : sceneGraphActiveClip;

		for (int clipIndex = 0; clipIndex < data.clips.size(); ++clipIndex)
		{
			const GltfAnimationClip& clip = data.clips[clipIndex];
			QString label = clip.name.isEmpty() ? tr("Clip %1").arg(clipIndex + 1) : clip.name;
			fileItem->addChild(makeClipItem(label, clipIndex, clip.durationSeconds, effectiveActiveClip == clipIndex));
		}

		fileItem->setExpanded(true);
	}

	updateControlsForSelection();
}

void AnimationsPanel::setDetachedOverlayMode(bool enabled)
{
	if (_overlayMode == enabled)
		return;

	if (enabled)
	{
		_savedStyleSheet = styleSheet();
		_savedPlayPauseStyle = _playPauseButton ? _playPauseButton->styleSheet() : QString();
		_savedPalette = _tree->palette();
		_savedViewportPalette = _tree->viewport()->palette();
		_savedAutoFill = autoFillBackground();
		_savedViewportAutoFill = _tree->viewport()->autoFillBackground();

		QPalette palette = _savedPalette;
		QColor base = palette.color(QPalette::Base);
		QColor alternate = palette.color(QPalette::AlternateBase);
		base.setAlpha(0);
		alternate.setAlpha(0);
		palette.setColor(QPalette::Base, base);
		palette.setColor(QPalette::AlternateBase, alternate);

		_tree->setPalette(palette);
		_tree->viewport()->setPalette(palette);
		_tree->setAutoFillBackground(false);
		_tree->viewport()->setAutoFillBackground(false);
		_tree->setAttribute(Qt::WA_NoSystemBackground, true);
		_tree->viewport()->setAttribute(Qt::WA_NoSystemBackground, true);
		_tree->viewport()->setAttribute(Qt::WA_StyledBackground, false);
		setAutoFillBackground(false);
		setAttribute(Qt::WA_NoSystemBackground, true);
		setProperty("detachedOverlayMode", true);
		_tree->setProperty("detachedOverlayMode", true);
		_tree->viewport()->setProperty("detachedOverlayMode", true);
		setStyleSheet(QStringLiteral(
			"QPushButton, QCheckBox, QLabel, QSlider { background: transparent; }"
		));
		updateDetachedPlayButtonStyle();
	}
	else
	{
		setStyleSheet(_savedStyleSheet);
		if (_playPauseButton)
			_playPauseButton->setStyleSheet(_savedPlayPauseStyle);
		_tree->setPalette(_savedPalette);
		_tree->viewport()->setPalette(_savedViewportPalette);
		_tree->setAutoFillBackground(_savedAutoFill);
		_tree->viewport()->setAutoFillBackground(_savedViewportAutoFill);
		_tree->setAttribute(Qt::WA_NoSystemBackground, false);
		_tree->viewport()->setAttribute(Qt::WA_NoSystemBackground, false);
		_tree->viewport()->setAttribute(Qt::WA_StyledBackground, false);
		setAttribute(Qt::WA_NoSystemBackground, false);
		setProperty("detachedOverlayMode", false);
		_tree->setProperty("detachedOverlayMode", false);
		_tree->viewport()->setProperty("detachedOverlayMode", false);
		setAutoFillBackground(_savedAutoFill);
	}

	_overlayMode = enabled;
	refreshDetachedOverlayTheme();
	_tree->viewport()->update();
	_tree->update();
	update();
}

void AnimationsPanel::refreshDetachedOverlayTheme()
{
	if (!_overlayMode || !_tree)
		return;

	const bool lightText = property("overlayViewerLightText").toBool();
	const QColor textColor = lightText ? QColor(255, 255, 255) : QColor(0, 0, 0);
	_detachedOverlayFillColor = lightText ? QColor(255, 255, 255, 65) : QColor(0, 0, 0, 45);

	QPalette treePalette = _tree->palette();
	treePalette.setColor(QPalette::Text, textColor);
	treePalette.setColor(QPalette::WindowText, textColor);
	treePalette.setColor(QPalette::ButtonText, textColor);
	treePalette.setColor(QPalette::HighlightedText, textColor);
	_tree->setPalette(treePalette);

	QPalette viewportPalette = _tree->viewport()->palette();
	viewportPalette.setColor(QPalette::Text, textColor);
	viewportPalette.setColor(QPalette::WindowText, textColor);
	viewportPalette.setColor(QPalette::ButtonText, textColor);
	viewportPalette.setColor(QPalette::HighlightedText, textColor);
	_tree->viewport()->setPalette(viewportPalette);

	updateDetachedPlayButtonStyle();
	if (_sceneGraph)
		refresh();
	else
		updateControlsForSelection();
}

void AnimationsPanel::paintEvent(QPaintEvent* event)
{
	if (_overlayMode)
	{
		QPainter painter(this);
		painter.setCompositionMode(QPainter::CompositionMode_Source);
		painter.fillRect(event->rect(), _detachedOverlayFillColor);
	}

	QWidget::paintEvent(event);
}

void AnimationsPanel::onItemClicked(QTreeWidgetItem* item, int /*column*/)
{
	if (!item || item->data(0, IsFileItemRole).toBool())
		return;

	QTreeWidgetItem* parentItem = item->parent();
	if (!parentItem)
		return;

	const QString sourceFile = parentItem->data(0, SourceFileRole).toString();
	const int clipIndex = item->data(0, ClipIndexRole).toInt();
	if (sourceFile.isEmpty() || clipIndex < 0)
		return;

	markActiveClip(sourceFile, clipIndex);
	emit clipActivated(sourceFile, clipIndex);
}

void AnimationsPanel::onPlayPauseClicked()
{
	const bool playing = _glWidget ? _glWidget->isAnimationPlaying() : false;
	emit playbackToggled(!playing);
}

void AnimationsPanel::onLoopCheckChanged(bool checked)
{
	if (_syncingControls)
		return;

	emit loopToggled(checked);
}

void AnimationsPanel::onPlaybackSpeedChanged(int index)
{
	if (_syncingControls || !_speedCombo || index < 0)
		return;

	emit playbackSpeedChanged(_speedCombo->itemData(index).toDouble());
}

void AnimationsPanel::onSliderPressed()
{
	_scrubbing = true;
}

void AnimationsPanel::onSliderReleased()
{
	_scrubbing = false;
	onSliderValueChanged(_timelineSlider->value());
}

void AnimationsPanel::onSliderValueChanged(int value)
{
	if (_syncingControls || _currentDurationSeconds <= 0.0)
		return;

	const double timeSeconds = (static_cast<double>(value) / static_cast<double>(_timelineSlider->maximum())) * _currentDurationSeconds;
	_timeLabel->setText(QStringLiteral("%1 / %2").arg(formatTime(timeSeconds), formatTime(_currentDurationSeconds)));

	if (_scrubbing)
		emit seekRequested(timeSeconds);
}

QTreeWidgetItem* AnimationsPanel::makeFileItem(const QString& sourceFile, const QString& displayName) const
{
	auto* item = new QTreeWidgetItem();
	item->setText(0, displayName);
	item->setToolTip(0, sourceFile);
	item->setData(0, SourceFileRole, sourceFile);
	item->setData(0, IsFileItemRole, true);
	item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
	QFont font = item->font(0);
	font.setBold(true);
	item->setFont(0, font);
	return item;
}

QTreeWidgetItem* AnimationsPanel::makeClipItem(const QString& label, int clipIndex, double durationSeconds, bool active) const
{
	auto* item = new QTreeWidgetItem();
	item->setText(0, QStringLiteral("%1 (%2)").arg(label, formatTime(durationSeconds)));
	item->setData(0, ClipIndexRole, clipIndex);
	item->setData(0, DurationRole, durationSeconds);
	item->setData(0, IsFileItemRole, false);
	item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
	item->setIcon(0, active ? activeIcon() : inactiveIcon());
	return item;
}

void AnimationsPanel::markActiveClip(const QString& sourceFile, int clipIndex)
{
	for (int topIndex = 0; topIndex < _tree->topLevelItemCount(); ++topIndex)
	{
		QTreeWidgetItem* fileItem = _tree->topLevelItem(topIndex);
		if (fileItem->data(0, SourceFileRole).toString() != sourceFile)
			continue;

		for (int childIndex = 0; childIndex < fileItem->childCount(); ++childIndex)
		{
			QTreeWidgetItem* child = fileItem->child(childIndex);
			const bool isActive = child->data(0, ClipIndexRole).toInt() == clipIndex;
			child->setIcon(0, isActive ? activeIcon() : inactiveIcon());
		}
		break;
	}
}

void AnimationsPanel::updateControlsForSelection()
{
	_syncingControls = true;

	QString activeFile = _glWidget ? _glWidget->activeAnimationFile() : QString();
	int activeClip = _glWidget ? _glWidget->activeAnimationClip() : -1;
	if (activeFile.isEmpty() && _sceneGraph)
	{
		const QStringList files = _sceneGraph->filesWithAnimations();
		if (!files.isEmpty())
		{
			activeFile = files.front();
			activeClip = _sceneGraph->activeAnimationClipForFile(activeFile);
		}
	}

	double currentTime = _glWidget ? _glWidget->currentAnimationTimeSeconds() : 0.0;
	const bool playing = _glWidget ? _glWidget->isAnimationPlaying() : false;
	const bool looping = _glWidget ? _glWidget->isAnimationLooping() : false;
	const double speed = _glWidget ? _glWidget->animationPlaybackSpeed() : 1.0;

	_currentDurationSeconds = 0.0;
	if (_sceneGraph && !activeFile.isEmpty())
	{
		const GltfAnimationData data = _sceneGraph->animationDataForFile(activeFile);
		if (activeClip >= 0 && activeClip < data.clips.size())
			_currentDurationSeconds = data.clips[activeClip].durationSeconds;
	}

	_playPauseButton->setEnabled(_currentDurationSeconds > 0.0);
	_playPauseButton->setText(playing ? tr("Pause") : tr("Play"));
	_loopCheck->setEnabled(_currentDurationSeconds > 0.0);
	_loopCheck->setChecked(looping);
	_speedCombo->setEnabled(_currentDurationSeconds > 0.0);
	{
		QSignalBlocker blocker(_speedCombo);
		int speedIndex = _speedCombo->findData(speed);
		if (speedIndex < 0)
			speedIndex = _speedCombo->findData(1.0);
		_speedCombo->setCurrentIndex(speedIndex);
	}
	_timelineSlider->setEnabled(_currentDurationSeconds > 0.0);

	if (!_scrubbing)
	{
		const int sliderValue = (_currentDurationSeconds > 0.0)
			? qRound((currentTime / _currentDurationSeconds) * _timelineSlider->maximum())
			: 0;
		QSignalBlocker blocker(_timelineSlider);
		_timelineSlider->setValue(qBound(0, sliderValue, _timelineSlider->maximum()));
	}

	_timeLabel->setText(QStringLiteral("%1 / %2").arg(formatTime(currentTime), formatTime(_currentDurationSeconds)));
	_syncingControls = false;
}

QIcon AnimationsPanel::activeIcon() const
{
	const QColor color = _tree ? _tree->palette().color(QPalette::Text)
	                           : palette().color(QPalette::Text);
	return makeCircleIcon(true, color);
}

QIcon AnimationsPanel::inactiveIcon() const
{
	QColor color = _tree ? _tree->palette().color(QPalette::Text)
	                     : palette().color(QPalette::Text);
	color.setAlpha(160);
	return makeCircleIcon(false, color);
}

void AnimationsPanel::updateDetachedPlayButtonStyle()
{
	if (!_playPauseButton)
		return;

	if (!_overlayMode)
	{
		_playPauseButton->setStyleSheet(_savedPlayPauseStyle);
		return;
	}

	const bool lightText = property("overlayPanelLightText").toBool()
		|| palette().color(QPalette::Text).lightnessF() > 0.5;
	const QColor buttonText = lightText ? QColor(255, 255, 255) : QColor(0, 0, 0);
	const QColor buttonFill = lightText ? QColor(24, 24, 24, 210) : QColor(255, 255, 255, 215);
	const QColor buttonBorder = lightText ? QColor(255, 255, 255, 110) : QColor(0, 0, 0, 90);
	const QColor buttonDisabledText = lightText ? QColor(255, 255, 255, 120) : QColor(0, 0, 0, 120);

	_playPauseButton->setStyleSheet(QStringLiteral(
		"QPushButton {"
		"  background-color: rgba(%1, %2, %3, %4);"
		"  color: rgb(%5, %6, %7);"
		"  border: 1px solid rgba(%8, %9, %10, %11);"
		"  border-radius: 4px;"
		"  padding: 3px 10px;"
		"}"
		"QPushButton:disabled {"
		"  color: rgba(%12, %13, %14, %15);"
		"}"
	)
		.arg(buttonFill.red()).arg(buttonFill.green()).arg(buttonFill.blue()).arg(buttonFill.alpha())
		.arg(buttonText.red()).arg(buttonText.green()).arg(buttonText.blue())
		.arg(buttonBorder.red()).arg(buttonBorder.green()).arg(buttonBorder.blue()).arg(buttonBorder.alpha())
		.arg(buttonDisabledText.red()).arg(buttonDisabledText.green()).arg(buttonDisabledText.blue()).arg(buttonDisabledText.alpha()));
}
