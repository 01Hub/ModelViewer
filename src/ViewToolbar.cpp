
#include "ViewToolbar.h"
#include "FlyOutViewButton.h"
#include <QHBoxLayout>
#include <QToolButton>
#include <QMenu>
#include <QAction>

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
    m_toolButtonCameraModes = new FlyOutViewButton(this);
    m_toolButtonCameraModes->setIcon(QIcon(":/new/prefix1/res/camera_orbit_64.png"));
    m_toolButtonCameraModes->setIconSize(QSize(48, 48));
    m_toolButtonCameraModes->setToolTip("Camera Modes");
    m_toolButtonCameraModes->setPopupMode(QToolButton::DelayedPopup);
    m_toolButtonCameraModes->setAutoRaise(true);
    layout->addWidget(m_toolButtonCameraModes);

    QMenu* camModeMenu = new QMenu;
    camModeMenu->setStyleSheet(flyoutStyleSheet);
    QAction* orbit = camModeMenu->addAction(QIcon(":/new/prefix1/res/camera_orbit_64.png"), "Orbit");
    QAction* fly = camModeMenu->addAction(QIcon(":/new/prefix1/res/camera_fly_64.png"), "Fly");
    QAction* firstperson = camModeMenu->addAction(QIcon(":/new/prefix1/res/camera_first_person_64.png"), "First Person");

    connect(orbit, &QAction::triggered, this,
        [this, orbit]() {
            m_toolButtonCameraModes->setDefaultAction(orbit);
            emit cameraModeSelected("Orbit");
        }
    );

    connect(fly, &QAction::triggered, this,
        [this, fly]() {
            m_toolButtonCameraModes->setDefaultAction(fly);
            emit cameraModeSelected("Fly");
        }
    );

    connect(firstperson, &QAction::triggered, this,
        [this, firstperson]() {
            m_toolButtonCameraModes->setDefaultAction(firstperson);
            emit cameraModeSelected("First Person");
        }
    );

    m_toolButtonCameraModes->setMenu(camModeMenu);
    m_toolButtonCameraModes->setDefaultAction(orbit);

    // All views
    auto createBtn = [this, layout, buttonStyleSheet](const QString& icon, const QString& tooltip, const QString& view) {
        QToolButton* btn = new QToolButton(this);
        btn->setStyleSheet(buttonStyleSheet);
        btn->setIcon(QIcon(icon));
        btn->setIconSize(QSize(48, 48));
        btn->setToolTip(tooltip);
        btn->setAutoRaise(true);
        layout->addWidget(btn);
        connect(btn, &QToolButton::clicked, this, [this, view]() { emit viewSelected(view); });
        };
    createBtn(":/new/prefix1/res/top.png", "Top View", "Top");
    createBtn(":/new/prefix1/res/front.png", "Front View", "Front");
    createBtn(":/new/prefix1/res/left.png", "Left View", "Left");
    createBtn(":/new/prefix1/res/bottom.png", "Bottom View", "Bottom");
    createBtn(":/new/prefix1/res/back.png", "Rear View", "Rear");
    createBtn(":/new/prefix1/res/right.png", "Right View", "Right");

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

    // Isometric Views
    m_toolButtonIsometricView = new FlyOutViewButton(this);
    m_toolButtonIsometricView->setIcon(QIcon(":/new/prefix1/res/isometric.png"));
    m_toolButtonIsometricView->setIconSize(QSize(48, 48));
    m_toolButtonIsometricView->setToolTip("Axonometric View");
    m_toolButtonIsometricView->setPopupMode(QToolButton::DelayedPopup);
    m_toolButtonIsometricView->setAutoRaise(true);
    layout->addWidget(m_toolButtonIsometricView);

    QMenu* axoMenu = new QMenu;
    axoMenu->setStyleSheet(flyoutStyleSheet);
    QAction* iso = axoMenu->addAction(QIcon(":/new/prefix1/res/isometric.png"), "Isometric");
    QAction* dim = axoMenu->addAction(QIcon(":/new/prefix1/res/dimetric.png"), "Dimetric");
    QAction* tri = axoMenu->addAction(QIcon(":/new/prefix1/res/trimetric.png"), "Trimetric");

    connect(iso, &QAction::triggered, this, 
        [this, iso]() 
        {
            m_toolButtonIsometricView->setDefaultAction(iso);
            emit axonometricSelected("Isometric"); 
        }
    );

    connect(dim, &QAction::triggered, this, 
        [this, dim]() 
        {
            m_toolButtonIsometricView->setDefaultAction(dim);
            emit axonometricSelected("Dimetric"); 
        }
    );

    connect(tri, &QAction::triggered, this, 
        [this, tri]() 
        {
			m_toolButtonIsometricView->setDefaultAction(tri);
            emit axonometricSelected("Trimetric"); 
        }
    );

    m_toolButtonIsometricView->setMenu(axoMenu);
    m_toolButtonIsometricView->setDefaultAction(iso);


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

    // Display Modes
    m_toolButtonDisplayModes = new FlyOutViewButton(this);
    m_toolButtonDisplayModes->setIcon(QIcon(":/new/prefix1/res/shaded.png"));
    m_toolButtonDisplayModes->setIconSize(QSize(48, 48));
    m_toolButtonDisplayModes->setToolTip("Display Modes");
    m_toolButtonDisplayModes->setPopupMode(QToolButton::DelayedPopup);
    m_toolButtonDisplayModes->setAutoRaise(true);
    layout->addWidget(m_toolButtonDisplayModes);

    QMenu* dispModeMenu = new QMenu;
    dispModeMenu->setStyleSheet(flyoutStyleSheet);
    QAction* realistic = dispModeMenu->addAction(QIcon(":/new/prefix1/res/realshaded.png"), "Realistic");
    QAction* shaded = dispModeMenu->addAction(QIcon(":/new/prefix1/res/shaded.png"), "Shaded");
    QAction* wireframe = dispModeMenu->addAction(QIcon(":/new/prefix1/res/wireframe.png"), "Wireframe");
    QAction* wireshaded = dispModeMenu->addAction(QIcon(":/new/prefix1/res/wireshaded.png"), "Wire Shaded");

    connect(realistic, &QAction::triggered, this,
        [this, realistic]() {
            m_toolButtonDisplayModes->setDefaultAction(realistic);
            emit displayModeSelected("Realistic");
        }
    );

    connect(shaded, &QAction::triggered, this,
        [this, shaded]() {
            m_toolButtonDisplayModes->setDefaultAction(shaded);
            emit displayModeSelected("Shaded");
        }
    );

    connect(wireframe, &QAction::triggered, this,
        [this, wireframe]() {
            m_toolButtonDisplayModes->setDefaultAction(wireframe);
            emit displayModeSelected("Wireframe");
        }
    );

    connect(wireshaded, &QAction::triggered, this,
        [this, wireshaded]() {
            m_toolButtonDisplayModes->setDefaultAction(wireshaded);
            emit displayModeSelected("WireShaded");
        }
    );

    m_toolButtonDisplayModes->setMenu(dispModeMenu);
    m_toolButtonDisplayModes->setDefaultAction(shaded);


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
    axisBtn->setIcon(QIcon(":/new/prefix1/res/hideAxis.png"));    
    axisBtn->setIconSize(QSize(48, 48));
    axisBtn->setToolTip("Show/Hide Axis");
    axisBtn->setCheckable(true);
    axisBtn->setAutoRaise(true);
    layout->addWidget(axisBtn);
    connect(axisBtn, &QToolButton::toggled, this, [this](bool checked) { emit axisDisplayToggled(checked); });


    // Toolbar animations
    m_toolbarAnimation = new QPropertyAnimation(this, "geometry", this);
    m_toolbarAnimation->setDuration(300);
    m_toolbarAnimation->setEasingCurve(QEasingCurve::OutCubic);
}

void ViewToolbar::showAnimated() {
    if (m_toolbarAnimation->state() == QAbstractAnimation::Running)
        m_toolbarAnimation->stop();
    m_toolbarAnimation->setStartValue(geometry());
    m_toolbarAnimation->setEndValue(m_visibleRect);
    m_toolbarAnimation->start();
}

void ViewToolbar::hideAnimated() {
    if (m_toolbarAnimation->state() == QAbstractAnimation::Running)
        m_toolbarAnimation->stop();
    m_toolbarAnimation->setStartValue(geometry());
    m_toolbarAnimation->setEndValue(m_hiddenRect);
    m_toolbarAnimation->start();
}

void ViewToolbar::reposition(int widgetWidth, int widgetHeight) {
    adjustSize();
    QSize sz = size();
    int x = (widgetWidth - sz.width()) / 2;
    int y = widgetHeight - sz.height() - 10;
    move(x, y);
    m_visibleRect = QRect(x, y, sz.width(), sz.height());
    m_hiddenRect = m_visibleRect.translated(0, 80);
}

QRect ViewToolbar::visibleRect() const { return m_visibleRect; }
QRect ViewToolbar::hiddenRect() const { return m_hiddenRect; }

bool ViewToolbar::isFlyoutMenuVisible() const
{
    return (m_toolButtonIsometricView &&
        m_toolButtonIsometricView->menu() &&
        m_toolButtonIsometricView->menu()->isVisible()) || 
        (m_toolButtonCameraModes &&
            m_toolButtonCameraModes->menu() &&
            m_toolButtonCameraModes->menu()->isVisible()) ||
        (m_toolButtonDisplayModes &&
            m_toolButtonDisplayModes->menu() &&
            m_toolButtonDisplayModes->menu()->isVisible());
}


#include <QPainter>

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
