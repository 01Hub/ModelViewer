
#include "ViewToolbar.h"
#include "FlyOutViewButton.h"
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

    QToolButton* btn = new QToolButton(this);
    btn->setStyleSheet(buttonStyleSheet);
    btn->setIcon(QIcon(":/new/prefix1/res/rotateview.png"));
    btn->setIconSize(QSize(48, 48));
    btn->setToolTip("Rotate View");
    btn->setAutoRaise(true);
    layout->addWidget(btn);
    connect(btn, &QToolButton::clicked, this, [this]() { emit rotateViewRequested(); });

    btn = new QToolButton(this);
    btn->setStyleSheet(buttonStyleSheet);
    btn->setIcon(QIcon(":/new/prefix1/res/panview.png"));
    btn->setIconSize(QSize(48, 48));
    btn->setToolTip("Pan View");
    btn->setAutoRaise(true);
    layout->addWidget(btn);
    connect(btn, &QToolButton::clicked, this, [this]() { emit panViewRequested(); });

    btn = new QToolButton(this);
    btn->setStyleSheet(buttonStyleSheet);
    btn->setIcon(QIcon(":/new/prefix1/res/zoomview.png"));
    btn->setIconSize(QSize(48, 48));
    btn->setToolTip("Zoom View");
    btn->setAutoRaise(true);
    layout->addWidget(btn);
    connect(btn, &QToolButton::clicked, this, [this]() { emit zoomViewRequested(); });

    btn = new QToolButton(this);
    btn->setStyleSheet(buttonStyleSheet);
    btn->setIcon(QIcon(":/new/prefix1/res/fit-all.png"));
    btn->setIconSize(QSize(48, 48));
    btn->setToolTip("Fit All");
    btn->setAutoRaise(true);
    layout->addWidget(btn);
    connect(btn, &QToolButton::clicked, this, [this]() { emit fitToViewRequested(); });

    btn = new QToolButton(this);
    btn->setStyleSheet(buttonStyleSheet);
    btn->setIcon(QIcon(":/new/prefix1/res/window-zoom.png"));
    btn->setIconSize(QSize(48, 48));
    btn->setToolTip("Window Zoom");
    btn->setAutoRaise(true);
    layout->addWidget(btn);
    connect(btn, &QToolButton::clicked, this, [this]() { emit windowZoomRequested(); });

    // Camera Modes
    _toolButtonCameraModes = new FlyOutViewButton(this);
    _toolButtonCameraModes->setIcon(QIcon(":/new/prefix1/res/camera_orbit_64.png"));
    _toolButtonCameraModes->setIconSize(QSize(48, 48));
    _toolButtonCameraModes->setToolTip("Camera Modes");
    _toolButtonCameraModes->setPopupMode(QToolButton::DelayedPopup);
    _toolButtonCameraModes->setAutoRaise(true);
    layout->addWidget(_toolButtonCameraModes);

    QMenu* camModeMenu = new QMenu;
    camModeMenu->setStyleSheet(flyoutStyleSheet);
    QAction* orbit = camModeMenu->addAction(QIcon(":/new/prefix1/res/camera_orbit_64.png"), "Orbit");
	orbit->setShortcut(QKeySequence(Qt::Key_1));
    QAction* fly = camModeMenu->addAction(QIcon(":/new/prefix1/res/camera_fly_64.png"), "Fly");
	fly->setShortcut(QKeySequence(Qt::Key_2));
    QAction* firstperson = camModeMenu->addAction(QIcon(":/new/prefix1/res/camera_first_person_64.png"), "First Person");
	firstperson->setShortcut(QKeySequence(Qt::Key_3));

    connect(orbit, &QAction::triggered, this,
        [this, orbit]() {
            _toolButtonCameraModes->setDefaultAction(orbit);
            emit cameraModeSelected("Orbit");
        }
    );

    connect(fly, &QAction::triggered, this,
        [this, fly]() {
            _toolButtonCameraModes->setDefaultAction(fly);
            emit cameraModeSelected("Fly");
        }
    );

    connect(firstperson, &QAction::triggered, this,
        [this, firstperson]() {
            _toolButtonCameraModes->setDefaultAction(firstperson);
            emit cameraModeSelected("First Person");
        }
    );

    _cameraModeActions[CameraModeActions::ORBIT] = orbit;
    _cameraModeActions[CameraModeActions::FLY] = fly;
    _cameraModeActions[CameraModeActions::FIRST_PERSON] = firstperson;

    _toolButtonCameraModes->setMenu(camModeMenu);
    _toolButtonCameraModes->setDefaultAction(orbit);

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
    createBtn(":/new/prefix1/res/top.png", "Top View", "Top")->setShortcut(Qt::CTRL | Qt::Key_T);
    createBtn(":/new/prefix1/res/front.png", "Front View", "Front")->setShortcut(Qt::CTRL | Qt::Key_F);
	createBtn(":/new/prefix1/res/left.png", "Left View", "Left")->setShortcut(Qt::CTRL | Qt::Key_L);
	createBtn(":/new/prefix1/res/bottom.png", "Bottom View", "Bottom")->setShortcut(Qt::CTRL | Qt::Key_B);
	createBtn(":/new/prefix1/res/back.png", "Rear View", "Rear")->setShortcut(Qt::CTRL | Qt::Key_R);
	createBtn(":/new/prefix1/res/right.png", "Right View", "Right")->setShortcut(Qt::CTRL | Qt::Key_J);

    // Isometric Views
    _toolButtonViewModes = new FlyOutViewButton(this);
    _toolButtonViewModes->setIcon(QIcon(":/new/prefix1/res/isometric.png"));
    _toolButtonViewModes->setIconSize(QSize(48, 48));
    _toolButtonViewModes->setToolTip("Axonometric View");
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
    QAction* iso = axoMenu->addAction(QIcon(":/new/prefix1/res/isometric.png"), "Isometric");
    QList<QKeySequence> shortcuts;
    shortcuts << QKeySequence("Ctrl+1") << QKeySequence("Home");	
	iso->setShortcuts(shortcuts);
    QAction* dim = axoMenu->addAction(QIcon(":/new/prefix1/res/dimetric.png"), "Dimetric");
    dim->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
    QAction* tri = axoMenu->addAction(QIcon(":/new/prefix1/res/trimetric.png"), "Trimetric");
    tri->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3));

    connect(iso, &QAction::triggered, this, 
        [this, iso]() 
        {
            _toolButtonViewModes->setDefaultAction(iso);
            emit axonometricSelected("Isometric"); 
        }
    );

    connect(dim, &QAction::triggered, this, 
        [this, dim]() 
        {
            _toolButtonViewModes->setDefaultAction(dim);
            emit axonometricSelected("Dimetric"); 
        }
    );

    connect(tri, &QAction::triggered, this, 
        [this, tri]() 
        {
			_toolButtonViewModes->setDefaultAction(tri);
            emit axonometricSelected("Trimetric"); 
        }
    );

    _viewModeActions[ViewModeActions::ISOMETRIC] = iso;
    _viewModeActions[ViewModeActions::DIMETRIC] = dim;
    _viewModeActions[ViewModeActions::TRIMETRIC] = tri;    

    _toolButtonViewModes->setMenu(axoMenu);
    _toolButtonViewModes->setDefaultAction(iso);


    // Ortho/Perspective Projections
    QToolButton* projToggleButton = new QToolButton(this);
    projToggleButton->setStyleSheet(buttonStyleSheet);
    projToggleButton->setCheckable(true);
    projToggleButton->setChecked(false);
    projToggleButton->setIcon(QIcon(":/new/prefix1/res/Ortho.png"));
    projToggleButton->setIconSize(QSize(48, 48));
    projToggleButton->setToolTip("Toggle Projection");
    layout->addWidget(projToggleButton);

    connect(projToggleButton, &QToolButton::toggled, this, [this, projToggleButton](bool checked) {
        if (!checked)
        {
            projToggleButton->setIcon(QIcon(":/new/prefix1/res/Ortho.png"));
            projToggleButton->setToolTip("Switch to Perspective");
        }
        else
        {
            projToggleButton->setIcon(QIcon(":/new/prefix1/res/Perspective.png"));
            projToggleButton->setToolTip("Switch to Orthographic");
        }
        emit projectionToggled(!checked);
        });


    // Multi View
    QToolButton* multiBtn = new QToolButton(this);
    multiBtn->setStyleSheet(buttonStyleSheet);
    multiBtn->setIcon(QIcon(":/new/prefix1/res/multiview.png"));
    multiBtn->setIconSize(QSize(48, 48));
    multiBtn->setToolTip("Toggle Multi-View");
    multiBtn->setCheckable(true);
    multiBtn->setAutoRaise(true);
    layout->addWidget(multiBtn);
    connect(multiBtn, &QToolButton::toggled, this, [this](bool checked) { emit multiViewToggled(checked); });
    // When this button is clicked, uncheck all buttons in the group
    connect(multiBtn, &QPushButton::clicked, this, [=]() {
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
    _toolButtonDisplayModes->setToolTip("Display Modes");
    _toolButtonDisplayModes->setPopupMode(QToolButton::DelayedPopup);
    _toolButtonDisplayModes->setAutoRaise(true);
    layout->addWidget(_toolButtonDisplayModes);

    QMenu* dispModeMenu = new QMenu;
    dispModeMenu->setStyleSheet(flyoutStyleSheet);
    QAction* realistic = dispModeMenu->addAction(QIcon(":/new/prefix1/res/realshaded.png"), "Realistic");
    QAction* shaded = dispModeMenu->addAction(QIcon(":/new/prefix1/res/shaded.png"), "Shaded");
    QAction* wireframe = dispModeMenu->addAction(QIcon(":/new/prefix1/res/wireframe.png"), "Wireframe");
    QAction* wireshaded = dispModeMenu->addAction(QIcon(":/new/prefix1/res/wireshaded.png"), "Wire Shaded");

    connect(realistic, &QAction::triggered, this,
        [this, realistic]() {
            _toolButtonDisplayModes->setDefaultAction(realistic);
            emit displayModeSelected("Realistic");
        }
    );

    connect(shaded, &QAction::triggered, this,
        [this, shaded]() {
            _toolButtonDisplayModes->setDefaultAction(shaded);
            emit displayModeSelected("Shaded");
        }
    );

    connect(wireframe, &QAction::triggered, this,
        [this, wireframe]() {
            _toolButtonDisplayModes->setDefaultAction(wireframe);
            emit displayModeSelected("Wireframe");
        }
    );

    connect(wireshaded, &QAction::triggered, this,
        [this, wireshaded]() {
            _toolButtonDisplayModes->setDefaultAction(wireshaded);
            emit displayModeSelected("WireShaded");
        }
    );

    _displayModeActions[DisplayModeActions::REALSHADED] = realistic;
    _displayModeActions[DisplayModeActions::WIREFRAME] = wireframe;
    _displayModeActions[DisplayModeActions::WIRESHADED] = wireshaded;
    _displayModeActions[DisplayModeActions::SHADED] = shaded;

    _toolButtonDisplayModes->setMenu(dispModeMenu);
    _toolButtonDisplayModes->setDefaultAction(shaded);


    // Section View
    QToolButton* sectionBtn = new QToolButton(this);
    sectionBtn->setStyleSheet(buttonStyleSheet);
    sectionBtn->setIcon(QIcon(":/new/prefix1/res/section.png"));
    sectionBtn->setIconSize(QSize(48, 48));
    sectionBtn->setToolTip("Clipping Planes");
    sectionBtn->setCheckable(true);
    sectionBtn->setAutoRaise(true);
    layout->addWidget(sectionBtn);
    connect(sectionBtn, &QToolButton::toggled, this, [this](bool checked) { emit sectionViewToggled(checked); });

    // Swap Visible View
    QToolButton* swapBtn = new QToolButton(this);
    swapBtn->setStyleSheet(buttonStyleSheet);
    swapBtn->setIcon(QIcon(":/new/prefix1/res/swapvisible.png"));
    swapBtn->setIconSize(QSize(48, 48));
    swapBtn->setToolTip("Clipping Planes");
    swapBtn->setCheckable(true);
    swapBtn->setAutoRaise(true);
    layout->addWidget(swapBtn);
    connect(swapBtn, &QToolButton::toggled, this, [this](bool checked) { emit swapVisibleToggled(checked); });

    // Show/Hide Axis
    QToolButton* axisBtn = new QToolButton(this);
    axisBtn->setStyleSheet(buttonStyleSheet);
    axisBtn->setIcon(QIcon(":/new/prefix1/res/showAxis.png"));    
    axisBtn->setIconSize(QSize(48, 48));
    axisBtn->setToolTip("Show/Hide Axis");
    axisBtn->setCheckable(true);
    axisBtn->setChecked(true);
    axisBtn->setAutoRaise(true);
    layout->addWidget(axisBtn);    
    connect(axisBtn, &QToolButton::toggled, this, [this, axisBtn](bool checked) {
        if (checked)
        {
            axisBtn->setIcon(QIcon(":/new/prefix1/res/showAxis.png"));
            axisBtn->setToolTip("Show the trihedron");
        }
        else
        {
            axisBtn->setIcon(QIcon(":/new/prefix1/res/hideAxis.png"));
            axisBtn->setToolTip("Hide the trihedron");            
        }
        emit axisDisplayToggled(checked);
        });


    // Toolbar animations
    _toolbarAnimation = new QPropertyAnimation(this, "geometry", this);
    _toolbarAnimation->setDuration(300);
    _toolbarAnimation->setEasingCurve(QEasingCurve::OutCubic);
}

void ViewToolbar::showAnimated() {
    if (_toolbarAnimation->state() == QAbstractAnimation::Running)
        _toolbarAnimation->stop();
    _toolbarAnimation->setStartValue(geometry());
    _toolbarAnimation->setEndValue(_visibleRect);
    _toolbarAnimation->start();
}

void ViewToolbar::hideAnimated() {
    if (_toolbarAnimation->state() == QAbstractAnimation::Running)
        _toolbarAnimation->stop();
    _toolbarAnimation->setStartValue(geometry());
    _toolbarAnimation->setEndValue(_hiddenRect);
    _toolbarAnimation->start();
}

void ViewToolbar::reposition(int widgetWidth, int widgetHeight) {
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
    QColor bg(255, 255, 255, 180);
    QColor border(100, 100, 100, 160);

    // Draw rounded rectangle background
    painter.setBrush(bg);
    painter.setPen(QPen(border, 1));
    painter.drawRoundedRect(r.adjusted(0, 0, -1, -1), 4, 4);

    QWidget::paintEvent(event); // Optional, not strictly needed here
}


