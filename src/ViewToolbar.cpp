
#include "ViewToolbar.h"
#include "FlyOutViewButton.h"
#include "LanguageManager.h"
#include <QHBoxLayout>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QPushButton>
#include <QButtonGroup>
#include <QPainter>

ViewToolbar::ViewToolbar(QWidget* parent)
	: QWidget(parent)
{
	setStyleSheet("background: rgba(255, 255, 255, 100); border: 1px solid gray; border-radius: 4px;");
	setFixedHeight(64);

	QString buttonStyleSheet(
		"QToolButton {"
		"    border: none;"
		"    background: transparent;"
		"    padding: 5px;"
		"    border-radius: 4px;"
		"}"
		"QToolButton:hover {"
		"    background-color: rgba(0, 120, 215, 50);"
		"    border: 1px solid #0078D7;"
		"}"
		"QToolButton:pressed {"
		"    background-color: rgba(0, 120, 215, 100);"
		"    border: 1px solid #005A9E;"
		"}"
		"QToolButton:checked {"
		"    background-color: rgba(0, 150, 100, 100);"
		"    border: 1px solid #008000;"
		"    color: white;"
		"}"
	);

	QString flyoutStyleSheet(
		"QMenu {"
		"    background-color: rgba(255, 255, 255, 100);"
		"    border: 1px solid gray;"
		"    border-radius: 4px;"
		"    padding: 2px;"
		"    icon-size: 42px;"
		"}"
		"QMenu::item {"
		"    background: transparent;"
		"    background-color: #f0f0f0;"
		"    border: 1px solid #c0c0c0;"
		"    border-radius: 4px;"
		"    padding: 5px 8px;"
		"    margin: 3px;"
		"    min-width: 120px;"
		"    min-height: 30px;"
		"    font-weight: normal;"
		"    color: black;"
		"}"
		"QMenu::item:selected {"
		"    background-color: #e0e0ff;"
		"    border: 1px solid #a0a0ff;"
		"    color: black;"
		"}"
		"QMenu::item:pressed {"
		"    background-color: #d0d0ff;"
		"    border: 1px solid #8080ff;"
		"    color: black;"
		"}"
		"QMenu::icon {"
		"    padding-left: 10px;"
		"    padding-right: 8px;"
		"}"
		"QMenu::separator {"
		"    height: 1px;"
		"    background-color: #c0c0c0;"
		"    margin: 4px 8px;"
		"}"
	);

	QHBoxLayout* layout = new QHBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->setSpacing(6);

	_btnRotateView = new QToolButton(this);
	_btnRotateView->setStyleSheet(buttonStyleSheet);
	_btnRotateView->setIcon(QIcon(":/new/prefix1/res/rotateview.png"));
	_btnRotateView->setIconSize(QSize(48, 48));
	_btnRotateView->setToolTip(tr("Rotate View"));
	_btnRotateView->setAutoRaise(true);
	layout->addWidget(_btnRotateView);
	connect(_btnRotateView, &QToolButton::clicked, this, [this]() { emit rotateViewRequested(); });

	_btnPanView = new QToolButton(this);
	_btnPanView->setStyleSheet(buttonStyleSheet);
	_btnPanView->setIcon(QIcon(":/new/prefix1/res/panview.png"));
	_btnPanView->setIconSize(QSize(48, 48));
	_btnPanView->setToolTip(tr("Pan View"));
	_btnPanView->setAutoRaise(true);
	layout->addWidget(_btnPanView);
	connect(_btnPanView, &QToolButton::clicked, this, [this]() { emit panViewRequested(); });

	_btnZoomView = new QToolButton(this);
	_btnZoomView->setStyleSheet(buttonStyleSheet);
	_btnZoomView->setIcon(QIcon(":/new/prefix1/res/zoomview.png"));
	_btnZoomView->setIconSize(QSize(48, 48));
	_btnZoomView->setToolTip(tr("Zoom View"));
	_btnZoomView->setAutoRaise(true);
	layout->addWidget(_btnZoomView);
	connect(_btnZoomView, &QToolButton::clicked, this, [this]() { emit zoomViewRequested(); });

	_btnFitAll = new QToolButton(this);
	_btnFitAll->setStyleSheet(buttonStyleSheet);
	_btnFitAll->setIcon(QIcon(":/new/prefix1/res/fit-all.png"));
	_btnFitAll->setIconSize(QSize(48, 48));
	_btnFitAll->setToolTip(tr("Fit All"));
	_btnFitAll->setAutoRaise(true);
	layout->addWidget(_btnFitAll);
	connect(_btnFitAll, &QToolButton::clicked, this, [this]() { emit fitToViewRequested(); });

	_btnWindowZoom = new QToolButton(this);
	_btnWindowZoom->setStyleSheet(buttonStyleSheet);
	_btnWindowZoom->setIcon(QIcon(":/new/prefix1/res/window-zoom.png"));
	_btnWindowZoom->setIconSize(QSize(48, 48));
	_btnWindowZoom->setToolTip(tr("Window Zoom"));
	_btnWindowZoom->setAutoRaise(true);
	layout->addWidget(_btnWindowZoom);
	connect(_btnWindowZoom, &QToolButton::clicked, this, [this]() { emit windowZoomRequested(); });

	// Camera Modes
	_toolButtonCameraModes = new FlyOutViewButton(this);
	_toolButtonCameraModes->setIcon(QIcon(":/new/prefix1/res/camera_orbit_64.png"));
	_toolButtonCameraModes->setIconSize(QSize(48, 48));
	_toolButtonCameraModes->setToolTip(tr("Camera Modes"));
	_toolButtonCameraModes->setPopupMode(QToolButton::DelayedPopup);
	_toolButtonCameraModes->setAutoRaise(true);
	layout->addWidget(_toolButtonCameraModes);

	QMenu* camModeMenu = new QMenu;
	camModeMenu->setStyleSheet(flyoutStyleSheet);
	_orbitAction = camModeMenu->addAction(QIcon(":/new/prefix1/res/camera_orbit_64.png"), tr("Orbit"));
	_orbitAction->setShortcut(QKeySequence(Qt::Key_1));
	_flyAction = camModeMenu->addAction(QIcon(":/new/prefix1/res/camera_fly_64.png"), tr("Fly"));
	_flyAction->setShortcut(QKeySequence(Qt::Key_2));
	_firstPersonAction = camModeMenu->addAction(QIcon(":/new/prefix1/res/camera_first_person_64.png"), tr("First Person"));
	_firstPersonAction->setShortcut(QKeySequence(Qt::Key_3));

	connect(_orbitAction, &QAction::triggered, this,
		[this]() {
			_toolButtonCameraModes->setDefaultAction(_orbitAction);
			emit cameraModeSelected("Orbit");
		}
	);

	connect(_flyAction, &QAction::triggered, this,
		[this]() {
			_toolButtonCameraModes->setDefaultAction(_flyAction);
			emit cameraModeSelected("Fly");
		}
	);

	connect(_firstPersonAction, &QAction::triggered, this,
		[this]() {
			_toolButtonCameraModes->setDefaultAction(_firstPersonAction);
			emit cameraModeSelected("First Person");
		}
	);

	_cameraModeActions[CameraModeActions::ORBIT] = _orbitAction;
	_cameraModeActions[CameraModeActions::FLY] = _flyAction;
	_cameraModeActions[CameraModeActions::FIRST_PERSON] = _firstPersonAction;

	_toolButtonCameraModes->setMenu(camModeMenu);
	_toolButtonCameraModes->setDefaultAction(_orbitAction);

	// All views
	// Group the buttons so that only one can be checked at a time
	QButtonGroup* buttonGroup = new QButtonGroup(this);
	buttonGroup->setExclusive(true); // This ensures radio-button behavior
	auto createBtn = [this, layout, buttonStyleSheet, buttonGroup](const QString& icon, const QString& tooltip, const QString& view) {
		QToolButton* btn = new QToolButton(this);
		btn->setStyleSheet(buttonStyleSheet);
		btn->setIcon(QIcon(icon));
		btn->setIconSize(QSize(48, 48));
		btn->setToolTip(tooltip);
		btn->setAutoRaise(true);
		btn->setCheckable(true);
		buttonGroup->addButton(btn);
		layout->addWidget(btn);
		connect(btn, &QToolButton::clicked, this, [this, view]() { emit viewSelected(view); });
		return btn;
		};
	_btnTopView = createBtn(":/new/prefix1/res/top.png", tr("Top View"), tr("Top")); _btnTopView->setShortcut(Qt::CTRL | Qt::Key_T);
	_btnFrontView = createBtn(":/new/prefix1/res/front.png", tr("Front View"), tr("Front")); _btnFrontView->setShortcut(Qt::CTRL | Qt::Key_F);
	_btnLeftView = createBtn(":/new/prefix1/res/left.png", tr("Left View"), tr("Left")); _btnLeftView->setShortcut(Qt::CTRL | Qt::Key_L);
	_btnBottomView = createBtn(":/new/prefix1/res/bottom.png", tr("Bottom View"), tr("Bottom")); _btnBottomView->setShortcut(Qt::CTRL | Qt::Key_B);
	_btnRearView = createBtn(":/new/prefix1/res/back.png", tr("Rear View"), tr("Rear")); _btnRearView->setShortcut(Qt::CTRL | Qt::Key_R);
	_btnRightView = createBtn(":/new/prefix1/res/right.png", tr("Right View"), tr("Right")); _btnRightView->setShortcut(Qt::CTRL | Qt::Key_J);

	// Isometric Views
	_toolButtonViewModes = new FlyOutViewButton(this);
	_toolButtonViewModes->setIcon(QIcon(":/new/prefix1/res/isometric.png"));
	_toolButtonViewModes->setIconSize(QSize(48, 48));
	_toolButtonViewModes->setToolTip(tr("Axonometric View"));
	_toolButtonViewModes->setPopupMode(QToolButton::DelayedPopup);
	_toolButtonViewModes->setAutoRaise(true);
	layout->addWidget(_toolButtonViewModes);

	// When this button is clicked, uncheck all buttons in the group
	connect(_toolButtonViewModes, &QPushButton::clicked, this, [=]() {
		buttonGroup->setExclusive(false);
		for (QAbstractButton* btn : buttonGroup->buttons())
		{
			QSignalBlocker blocker(btn);
			btn->setChecked(false);
		}
		buttonGroup->setExclusive(true);
		});

	QMenu* axoMenu = new QMenu;
	axoMenu->setStyleSheet(flyoutStyleSheet);
	_isoAction = axoMenu->addAction(QIcon(":/new/prefix1/res/isometric.png"), tr("Isometric"));
	QList<QKeySequence> shortcuts;
	shortcuts << QKeySequence("Ctrl+1") << QKeySequence("Home");
	_isoAction->setShortcuts(shortcuts);
	_dimAction = axoMenu->addAction(QIcon(":/new/prefix1/res/dimetric.png"), tr("Dimetric"));
	_dimAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
	_triAction = axoMenu->addAction(QIcon(":/new/prefix1/res/trimetric.png"), tr("Trimetric"));
	_triAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3));

	connect(_isoAction, &QAction::triggered, this,
		[this]() {
			_toolButtonViewModes->setDefaultAction(_isoAction);
			emit axonometricSelected("Isometric");
		}
	);

	connect(_dimAction, &QAction::triggered, this,
		[this]() {
			_toolButtonViewModes->setDefaultAction(_dimAction);
			emit axonometricSelected("Dimetric");
		}
	);

	connect(_triAction, &QAction::triggered, this,
		[this]() {
			_toolButtonViewModes->setDefaultAction(_triAction);
			emit axonometricSelected("Trimetric");
		}
	);

	_viewModeActions[ViewModeActions::ISOMETRIC] = _isoAction;
	_viewModeActions[ViewModeActions::DIMETRIC] = _dimAction;
	_viewModeActions[ViewModeActions::TRIMETRIC] = _triAction;

	_toolButtonViewModes->setMenu(axoMenu);
	_toolButtonViewModes->setDefaultAction(_isoAction);


	// Ortho/Perspective Projections
	_projToggleButton = new QToolButton(this);
	_projToggleButton->setStyleSheet(buttonStyleSheet);
	_projToggleButton->setCheckable(true);
	_projToggleButton->setChecked(false);
	_projToggleButton->setIcon(QIcon(":/new/prefix1/res/Ortho.png"));
	_projToggleButton->setIconSize(QSize(48, 48));
	_projToggleButton->setToolTip(tr("Toggle Projection"));
	layout->addWidget(_projToggleButton);

	connect(_projToggleButton, &QToolButton::toggled, this, [this](bool checked) {
		if (!checked)
		{
			_projToggleButton->setIcon(QIcon(":/new/prefix1/res/Ortho.png"));
			_projToggleButton->setToolTip(tr("Switch to Perspective"));
		}
		else
		{
			_projToggleButton->setToolTip(tr("Switch to Orthographic"));
			_projToggleButton->setIcon(QIcon(":/new/prefix1/res/Perspective.png"));
		}
		emit projectionToggled(!checked);
		});


	// Multi View
	_multiBtn = new QToolButton(this);
	_multiBtn->setStyleSheet(buttonStyleSheet);
	_multiBtn->setIcon(QIcon(":/new/prefix1/res/multiview.png"));
	_multiBtn->setIconSize(QSize(48, 48));
	_multiBtn->setToolTip(tr("Toggle Multi-View"));
	_multiBtn->setCheckable(true);
	_multiBtn->setAutoRaise(true);
	layout->addWidget(_multiBtn);
	connect(_multiBtn, &QToolButton::toggled, this, [this](bool checked) { emit multiViewToggled(checked); });
	// When this button is clicked, uncheck all buttons in the group
	connect(_multiBtn, &QPushButton::clicked, this, [=]() {
		buttonGroup->setExclusive(false);
		for (QAbstractButton* btn : buttonGroup->buttons())
		{
			QSignalBlocker blocker(btn);
			btn->setChecked(false);
		}
		buttonGroup->setExclusive(true);
		});

	// Display Modes
	_toolButtonDisplayModes = new FlyOutViewButton(this);
	_toolButtonDisplayModes->setIcon(QIcon(":/new/prefix1/res/shaded.png"));
	_toolButtonDisplayModes->setIconSize(QSize(48, 48));
	_toolButtonDisplayModes->setToolTip(tr("Display Modes"));
	_toolButtonDisplayModes->setPopupMode(QToolButton::DelayedPopup);
	_toolButtonDisplayModes->setAutoRaise(true);
	layout->addWidget(_toolButtonDisplayModes);

	QMenu* dispModeMenu = new QMenu;
	dispModeMenu->setStyleSheet(flyoutStyleSheet);
	_realistic = dispModeMenu->addAction(QIcon(":/new/prefix1/res/realshaded.png"), tr("Realistic"));
	_shaded = dispModeMenu->addAction(QIcon(":/new/prefix1/res/shaded.png"), tr("Shaded"));
	_wireframe = dispModeMenu->addAction(QIcon(":/new/prefix1/res/wireframe.png"), tr("Wireframe"));
	_wireshaded = dispModeMenu->addAction(QIcon(":/new/prefix1/res/wireshaded.png"), tr("Wire Shaded"));

	connect(_realistic, &QAction::triggered, this,
		[this]() {
			_toolButtonDisplayModes->setDefaultAction(_realistic);
			emit displayModeSelected("Realistic");
		}
	);

	connect(_shaded, &QAction::triggered, this,
		[this]() {
			_toolButtonDisplayModes->setDefaultAction(_shaded);
			emit displayModeSelected("Shaded");
		}
	);

	connect(_wireframe, &QAction::triggered, this,
		[this]() {
			_toolButtonDisplayModes->setDefaultAction(_wireframe);
			emit displayModeSelected("Wireframe");
		}
	);

	connect(_wireshaded, &QAction::triggered, this,
		[this]() {
			_toolButtonDisplayModes->setDefaultAction(_wireshaded);
			emit displayModeSelected("WireShaded");
		}
	);

	_displayModeActions[DisplayModeActions::REALSHADED] = _realistic;
	_displayModeActions[DisplayModeActions::WIREFRAME] = _wireframe;
	_displayModeActions[DisplayModeActions::WIRESHADED] = _wireshaded;
	_displayModeActions[DisplayModeActions::SHADED] = _shaded;

	_toolButtonDisplayModes->setMenu(dispModeMenu);
	_toolButtonDisplayModes->setDefaultAction(_shaded);


	// Section View
	_sectionBtn = new QToolButton(this);
	_sectionBtn->setStyleSheet(buttonStyleSheet);
	_sectionBtn->setIcon(QIcon(":/new/prefix1/res/section.png"));
	_sectionBtn->setIconSize(QSize(48, 48));
	_sectionBtn->setToolTip(tr("Clipping Planes"));
	_sectionBtn->setCheckable(true);
	_sectionBtn->setAutoRaise(true);
	layout->addWidget(_sectionBtn);
	connect(_sectionBtn, &QToolButton::toggled, this, [this](bool checked) { emit sectionViewToggled(checked); });

	// Swap Visible View
	_swapBtn = new QToolButton(this);
	_swapBtn->setStyleSheet(buttonStyleSheet);
	_swapBtn->setIcon(QIcon(":/new/prefix1/res/swapvisible.png"));
	_swapBtn->setIconSize(QSize(48, 48));
	_swapBtn->setToolTip(tr("Swap Visible"));
	_swapBtn->setCheckable(true);
	_swapBtn->setAutoRaise(true);
	layout->addWidget(_swapBtn);
	connect(_swapBtn, &QToolButton::toggled, this, [this](bool checked) { emit swapVisibleToggled(checked); });

	// Show/Hide Axis
	_axisBtn = new QToolButton(this);
	_axisBtn->setStyleSheet(buttonStyleSheet);
	_axisBtn->setIcon(QIcon(":/new/prefix1/res/showAxis.png"));
	_axisBtn->setIconSize(QSize(48, 48));
	_axisBtn->setToolTip(tr("Show/Hide Axis"));
	_axisBtn->setCheckable(true);
	_axisBtn->setChecked(true);
	_axisBtn->setAutoRaise(true);
	layout->addWidget(_axisBtn);
	connect(_axisBtn, &QToolButton::toggled, this, [this](bool checked) {
		if (checked)
		{
			_axisBtn->setIcon(QIcon(":/new/prefix1/res/showAxis.png"));
			_axisBtn->setToolTip(tr("Show the trihedron"));
		}
		else
		{
			_axisBtn->setIcon(QIcon(":/new/prefix1/res/hideAxis.png"));
			_axisBtn->setToolTip(tr("Hide the trihedron"));
		}
		emit axisDisplayToggled(checked);
		});


	// Toolbar animations
	_toolbarAnimation = new QPropertyAnimation(this, "geometry", this);
	_toolbarAnimation->setDuration(300);
	_toolbarAnimation->setEasingCurve(QEasingCurve::OutCubic);

	retranslateUI();

	connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this, [this]() {
		retranslateUI();  // if needed
		});
}

void ViewToolbar::showAnimated()
{
	if (_toolbarAnimation->state() == QAbstractAnimation::Running)
		_toolbarAnimation->stop();
	_toolbarAnimation->setStartValue(geometry());
	_toolbarAnimation->setEndValue(_visibleRect);
	_toolbarAnimation->start();
}

void ViewToolbar::hideAnimated()
{
	if (_toolbarAnimation->state() == QAbstractAnimation::Running)
		_toolbarAnimation->stop();
	_toolbarAnimation->setStartValue(geometry());
	_toolbarAnimation->setEndValue(_hiddenRect);
	_toolbarAnimation->start();
}

void ViewToolbar::reposition(int widgetWidth, int widgetHeight)
{
	adjustSize();
	QSize sz = size();
	int x = (widgetWidth - sz.width()) / 2;
	int y = widgetHeight - sz.height() - 10;
	move(x, y);
	_visibleRect = QRect(x, y, sz.width(), sz.height());
	_hiddenRect = _visibleRect.translated(0, 80);
}

QRect ViewToolbar::visibleRect() const { return _visibleRect; }
QRect ViewToolbar::hiddenRect() const { return _hiddenRect; }

bool ViewToolbar::isFlyoutMenuVisible() const
{
	return (_toolButtonViewModes &&
		_toolButtonViewModes->menu() &&
		_toolButtonViewModes->menu()->isVisible()) ||
		(_toolButtonCameraModes &&
			_toolButtonCameraModes->menu() &&
			_toolButtonCameraModes->menu()->isVisible()) ||
		(_toolButtonDisplayModes &&
			_toolButtonDisplayModes->menu() &&
			_toolButtonDisplayModes->menu()->isVisible());
}

void ViewToolbar::setDefaultCameraModeAction(CameraModeActions mode)
{
	if (_cameraModeActions.contains(mode))
		_toolButtonCameraModes->setDefaultAction(_cameraModeActions[mode]);
}

void ViewToolbar::setDefaultViewModeAction(ViewModeActions mode)
{
	if (_viewModeActions.contains(mode))
		_toolButtonViewModes->setDefaultAction(_viewModeActions[mode]);
}

void ViewToolbar::setDefaultDisplayModeAction(DisplayModeActions mode)
{
	if (_displayModeActions.contains(mode))
		_toolButtonDisplayModes->setDefaultAction(_displayModeActions[mode]);
}


void ViewToolbar::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	QRect r = rect();
	QColor bg(255, 255, 255, 100);
	QColor border(100, 100, 100, 160);

	// Draw rounded rectangle background
	painter.setBrush(bg);
	painter.setPen(QPen(border, 1));
	painter.drawRoundedRect(r.adjusted(0, 0, -1, -1), 4, 4);

	QWidget::paintEvent(event); // Optional, not strictly needed here
}

void ViewToolbar::retranslateUI()
{
	// Main navigation buttons
	_btnRotateView->setToolTip(tr("Rotate View"));
	_btnPanView->setToolTip(tr("Pan View"));
	_btnZoomView->setToolTip(tr("Zoom View"));
	_btnFitAll->setToolTip(tr("Fit All"));
	_btnWindowZoom->setToolTip(tr("Window Zoom"));

	// Camera Modes
	_toolButtonCameraModes->setToolTip(tr("Camera Modes"));
	_orbitAction->setText(tr("Orbit"));
	_flyAction->setText(tr("Fly"));
	_firstPersonAction->setText(tr("First Person"));

	// View buttons
	_btnTopView->setToolTip(tr("Top View"));
	_btnTopView->setText(tr("Top"));
	_btnFrontView->setToolTip(tr("Front View"));
	_btnFrontView->setText(tr("Front"));
	_btnLeftView->setToolTip(tr("Left View"));
	_btnLeftView->setText(tr("Left"));
	_btnBottomView->setToolTip(tr("Bottom View"));
	_btnBottomView->setText(tr("Bottom"));
	_btnRearView->setToolTip(tr("Rear View"));
	_btnRearView->setText(tr("Rear"));
	_btnRightView->setToolTip(tr("Right View"));
	_btnRightView->setText(tr("Right"));

	// Axonometric Views
	_toolButtonViewModes->setToolTip(tr("Axonometric View"));
	_isoAction->setText(tr("Isometric"));
	_dimAction->setText(tr("Dimetric"));
	_triAction->setText(tr("Trimetric"));

	// Projection toggle
	_projToggleButton->setToolTip(tr("Toggle Projection"));	

	// Multi View
	_multiBtn->setToolTip(tr("Toggle Multi-View"));

	// Display Modes
	_toolButtonDisplayModes->setToolTip(tr("Display Modes"));
	_realistic->setText(tr("Realistic"));
	_shaded->setText(tr("Shaded"));
	_wireframe->setText(tr("Wireframe"));
	_wireshaded->setText(tr("Wire Shaded"));

	// Section View
	_sectionBtn->setToolTip(tr("Clipping Planes"));

	// Swap Visible View
	_swapBtn->setToolTip(tr("Swap Visible"));

	// Axis
	_axisBtn->setToolTip(tr("Show/Hide Axis"));	
}
