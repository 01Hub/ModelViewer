#pragma once

#include <QPalette>
#include <QTreeWidget>
#include <QWidget>

class GLWidget;
class QLabel;
class QPushButton;
class QCheckBox;
class QSlider;
class SceneGraph;

class AnimationsPanel : public QWidget
{
	Q_OBJECT

public:
	enum ItemRole
	{
		SourceFileRole = Qt::UserRole,
		ClipIndexRole = Qt::UserRole + 1,
		IsFileItemRole = Qt::UserRole + 2,
		DurationRole = Qt::UserRole + 3,
	};

	explicit AnimationsPanel(QWidget* parent = nullptr);

	void setSceneGraph(SceneGraph* sg);
	void setGLWidget(GLWidget* glWidget);
	void refresh();
	void setDetachedOverlayMode(bool enabled);

signals:
	void clipActivated(const QString& sourceFile, int clipIndex);
	void playbackToggled(bool playing);
	void loopToggled(bool enabled);
	void seekRequested(double timeSeconds);

private slots:
	void onItemClicked(QTreeWidgetItem* item, int column);
	void onPlayPauseClicked();
	void onLoopCheckChanged(bool checked);
	void onSliderPressed();
	void onSliderReleased();
	void onSliderValueChanged(int value);

private:
	QTreeWidgetItem* makeFileItem(const QString& sourceFile, const QString& displayName) const;
	QTreeWidgetItem* makeClipItem(const QString& label, int clipIndex, double durationSeconds, bool active) const;
	void markActiveClip(const QString& sourceFile, int clipIndex);
	void updateControlsForSelection();
	QIcon activeIcon() const;
	QIcon inactiveIcon() const;

	QTreeWidget* _tree = nullptr;
	QPushButton* _playPauseButton = nullptr;
	QCheckBox* _loopCheck = nullptr;
	QSlider* _timelineSlider = nullptr;
	QLabel* _timeLabel = nullptr;

	SceneGraph* _sceneGraph = nullptr;
	GLWidget* _glWidget = nullptr;
	bool _overlayMode = false;
	bool _scrubbing = false;
	bool _syncingControls = false;
	double _currentDurationSeconds = 0.0;

	QPalette _savedPalette;
	QPalette _savedViewportPalette;
	bool _savedAutoFill = false;
	bool _savedViewportAutoFill = false;
	QString _savedStyleSheet;
};
