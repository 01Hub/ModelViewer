
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
#include <QShortcut>
#include <QScrollBar>
#include <QTimer>

ViewToolbar::ViewToolbar(QWidget* parent)
    : QWidget(parent)
    , _isRepositioning(false)
    , _autoScrollTimer(nullptr)
    , _hoverDelayTimer(nullptr)
    , _autoScrollLeft(true)
{
    setStyleSheet("background: rgba(255, 255, 255, 100); border: 1px solid gray; border-radius: 4px;");
    setFixedHeight(76);

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

    QString scrollButtonStyleSheet(
        "QToolButton {"
        "    border: none;"
        "    background: rgba(100, 100, 100, 150);"
        "    padding: 2px;"
        "    border-radius: 4px;"
        "    min-width: 20px;"
        "    max-width: 20px;"
        "}"
        "QToolButton:hover {"
        "    background-color: rgba(0, 120, 215, 180);"
        "}"
        "QToolButton:pressed {"
        "    background-color: rgba(0, 120, 215, 220);"
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

    // Main layout for the entire toolbar
    QHBoxLayout* outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(2, 2, 2, 2);
    outerLayout->setSpacing(0);

    // Left scroll button
    _scrollLeftBtn = new QToolButton(this);
    _scrollLeftBtn->setStyleSheet(scrollButtonStyleSheet);
    _scrollLeftBtn->setText("<");
    _scrollLeftBtn->setFixedSize(20, 68);
    _scrollLeftBtn->setVisible(false);
    _scrollLeftBtn->installEventFilter(this);
    outerLayout->addWidget(_scrollLeftBtn);
    connect(_scrollLeftBtn, &QToolButton::clicked, this, &ViewToolbar::scrollLeft);

    // Create scroll area
    _scrollArea = new QScrollArea(this);
    _scrollArea->setFrameShape(QFrame::NoFrame);
    _scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _scrollArea->setWidgetResizable(false);
    _scrollArea->setFixedHeight(72);
    _scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    outerLayout->addWidget(_scrollArea, 1);

    // Container widget for buttons inside scroll area
    _buttonContainer = new QWidget();
    _buttonContainer->setStyleSheet("background: transparent;");
    _buttonContainer->setFixedHeight(72);
    _mainLayout = new QHBoxLayout(_buttonContainer);
    _mainLayout->setContentsMargins(4, 4, 4, 4);
    _mainLayout->setSpacing(6);

    _scrollArea->setWidget(_buttonContainer);

    // Right scroll button
    _scrollRightBtn = new QToolButton(this);
    _scrollRightBtn->setStyleSheet(scrollButtonStyleSheet);
    _scrollRightBtn->setText(">");
    _scrollRightBtn->setFixedSize(20, 68);
    _scrollRightBtn->setVisible(false);
    _scrollRightBtn->installEventFilter(this);
    outerLayout->addWidget(_scrollRightBtn);
    connect(_scrollRightBtn, &QToolButton::clicked, this, &ViewToolbar::scrollRight);

    // Connect scroll area scrollbar to update button states
    connect(_scrollArea->horizontalScrollBar(), &QScrollBar::valueChanged,
        this, &ViewToolbar::updateScrollButtons);

    // Now add all the toolbar buttons to _mainLayout
    // (Keep all your existing button creation code here, just replace 'layout' with '_mainLayout')

    _btnRotateView = new QToolButton(this);
    _btnRotateView->setStyleSheet(buttonStyleSheet);
    _btnRotateView->setIcon(QIcon(":/icons/res/rotateview.png"));
    _btnRotateView->setIconSize(QSize(48, 48));
    _btnRotateView->setToolTip(tr("Rotate View"));
    _btnRotateView->setAutoRaise(true);
    _mainLayout->addWidget(_btnRotateView);
    connect(_btnRotateView, &QToolButton::clicked, this, [this]() { emit rotateViewRequested(); });

    _btnPanView = new QToolButton(this);
    _btnPanView->setStyleSheet(buttonStyleSheet);
    _btnPanView->setIcon(QIcon(":/icons/res/panview.png"));
    _btnPanView->setIconSize(QSize(48, 48));
    _btnPanView->setToolTip(tr("Pan View"));
    _btnPanView->setAutoRaise(true);
    _mainLayout->addWidget(_btnPanView);
    connect(_btnPanView, &QToolButton::clicked, this, [this]() { emit panViewRequested(); });

    _btnZoomView = new QToolButton(this);
    _btnZoomView->setStyleSheet(buttonStyleSheet);
    _btnZoomView->setIcon(QIcon(":/icons/res/zoomview.png"));
    _btnZoomView->setIconSize(QSize(48, 48));
    _btnZoomView->setToolTip(tr("Zoom View"));
    _btnZoomView->setAutoRaise(true);
    _mainLayout->addWidget(_btnZoomView);
    connect(_btnZoomView, &QToolButton::clicked, this, [this]() { emit zoomViewRequested(); });

    _btnFitAll = new QToolButton(this);
    _btnFitAll->setStyleSheet(buttonStyleSheet);
    _btnFitAll->setIcon(QIcon(":/icons/res/fit-all.png"));
    _btnFitAll->setIconSize(QSize(48, 48));
    _btnFitAll->setToolTip(tr("Fit All"));
    _btnFitAll->setAutoRaise(true);
    _mainLayout->addWidget(_btnFitAll);
    connect(_btnFitAll, &QToolButton::clicked, this, [this]() { emit fitToViewRequested(); });

    _btnWindowZoom = new QToolButton(this);
    _btnWindowZoom->setStyleSheet(buttonStyleSheet);
    _btnWindowZoom->setIcon(QIcon(":/icons/res/window-zoom.png"));
    _btnWindowZoom->setIconSize(QSize(48, 48));
    _btnWindowZoom->setToolTip(tr("Window Zoom"));
    _btnWindowZoom->setAutoRaise(true);
    _mainLayout->addWidget(_btnWindowZoom);
    connect(_btnWindowZoom, &QToolButton::clicked, this, [this]() { emit windowZoomRequested(); });

    // Camera Modes
    _toolButtonCameraModes = new FlyOutViewButton(this);
    _toolButtonCameraModes->setIcon(QIcon(":/icons/res/camera_orbit.png"));
    _toolButtonCameraModes->setIconSize(QSize(48, 48));
    _toolButtonCameraModes->setToolTip(tr("Camera Modes"));
    _toolButtonCameraModes->setPopupMode(QToolButton::DelayedPopup);
    _toolButtonCameraModes->setAutoRaise(true);
    _mainLayout->addWidget(_toolButtonCameraModes);

    QMenu* camModeMenu = new QMenu;
    camModeMenu->setStyleSheet(flyoutStyleSheet);
    _orbitAction = camModeMenu->addAction(QIcon(":/icons/res/camera_orbit.png"), tr("Orbit"));
    _orbitAction->setShortcut(QKeySequence(Qt::Key_1));
    _flyAction = camModeMenu->addAction(QIcon(":/icons/res/camera_fly.png"), tr("Fly"));
    _flyAction->setShortcut(QKeySequence(Qt::Key_2));
    _firstPersonAction = camModeMenu->addAction(QIcon(":/icons/res/camera_first_person.png"), tr("First Person"));
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

    // All views - Group the buttons so that only one can be checked at a time
    QButtonGroup* buttonGroup = new QButtonGroup(this);
    buttonGroup->setExclusive(true);
    auto createBtn = [this, buttonStyleSheet, buttonGroup](const QString& icon, const QString& tooltip, const QString& view, const QKeySequence& key) {
        QToolButton* btn = new QToolButton(this);
        btn->setStyleSheet(buttonStyleSheet);
        btn->setIcon(QIcon(icon));
        btn->setIconSize(QSize(48, 48));
        btn->setToolTip(tooltip);
        btn->setAutoRaise(true);
        btn->setCheckable(true);
        btn->setShortcut(key);
        buttonGroup->addButton(btn);
        _mainLayout->addWidget(btn);
        connect(btn, &QToolButton::clicked, this, [this, view]() { emit viewSelected(view); });

        QShortcut* shortcut = new QShortcut(key, this);
        connect(shortcut, &QShortcut::activated, btn, &QToolButton::click);

        return btn;
        };

    _btnTopView = createBtn(":/icons/res/top.png", tr("Top View"), tr("Top"), Qt::CTRL | Qt::Key_T);
    _btnFrontView = createBtn(":/icons/res/front.png", tr("Front View"), tr("Front"), Qt::CTRL | Qt::Key_F);
    _btnLeftView = createBtn(":/icons/res/left.png", tr("Left View"), tr("Left"), Qt::CTRL | Qt::Key_L);
    _btnBottomView = createBtn(":/icons/res/bottom.png", tr("Bottom View"), tr("Bottom"), Qt::CTRL | Qt::Key_B);
    _btnRearView = createBtn(":/icons/res/back.png", tr("Rear View"), tr("Rear"), Qt::CTRL | Qt::Key_R);
    _btnRightView = createBtn(":/icons/res/right.png", tr("Right View"), tr("Right"), Qt::CTRL | Qt::Key_J);

    // Isometric Views
    _toolButtonViewModes = new FlyOutViewButton(this);
    _toolButtonViewModes->setIcon(QIcon(":/icons/res/isometric.png"));
    _toolButtonViewModes->setIconSize(QSize(48, 48));
    _toolButtonViewModes->setToolTip(tr("Axonometric View"));
    _toolButtonViewModes->setPopupMode(QToolButton::DelayedPopup);
    _toolButtonViewModes->setAutoRaise(true);

    QShortcut* defaultShortcut = new QShortcut(QKeySequence(Qt::Key_Home), this);
    connect(defaultShortcut, &QShortcut::activated, _toolButtonViewModes, &QToolButton::click);

    _mainLayout->addWidget(_toolButtonViewModes);

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
    _isoAction = axoMenu->addAction(QIcon(":/icons/res/isometric.png"), tr("Isometric"));
    _isoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    _dimAction = axoMenu->addAction(QIcon(":/icons/res/dimetric.png"), tr("Dimetric"));
    _dimAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
    _triAction = axoMenu->addAction(QIcon(":/icons/res/trimetric.png"), tr("Trimetric"));
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
    _projToggleButton->setIcon(QIcon(":/icons/res/Ortho.png"));
    _projToggleButton->setIconSize(QSize(48, 48));
    _projToggleButton->setToolTip(tr("Toggle Projection"));
    _mainLayout->addWidget(_projToggleButton);

    connect(_projToggleButton, &QToolButton::toggled, this, [this](bool checked) {
        if (!checked)
        {
            _projToggleButton->setIcon(QIcon(":/icons/res/Ortho.png"));
            _projToggleButton->setToolTip(tr("Switch to Perspective"));
        }
        else
        {
            _projToggleButton->setToolTip(tr("Switch to Orthographic"));
            _projToggleButton->setIcon(QIcon(":/icons/res/Perspective.png"));
        }
        emit projectionToggled(!checked);
        });

    // Multi View
    _multiBtn = new QToolButton(this);
    _multiBtn->setStyleSheet(buttonStyleSheet);
    _multiBtn->setIcon(QIcon(":/icons/res/multiview.png"));
    _multiBtn->setIconSize(QSize(48, 48));
    _multiBtn->setToolTip(tr("Toggle Multi-View"));
    _multiBtn->setCheckable(true);
    _multiBtn->setAutoRaise(true);
    _mainLayout->addWidget(_multiBtn);
    connect(_multiBtn, &QToolButton::toggled, this, [this](bool checked) { emit multiViewToggled(checked); });

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
    _toolButtonDisplayModes->setIcon(QIcon(":/icons/res/shaded.png"));
    _toolButtonDisplayModes->setIconSize(QSize(48, 48));
    _toolButtonDisplayModes->setToolTip(tr("Display Modes"));
    _toolButtonDisplayModes->setPopupMode(QToolButton::DelayedPopup);
    _toolButtonDisplayModes->setAutoRaise(true);
    _mainLayout->addWidget(_toolButtonDisplayModes);

    QMenu* dispModeMenu = new QMenu;
    dispModeMenu->setStyleSheet(flyoutStyleSheet);
    _realistic = dispModeMenu->addAction(QIcon(":/icons/res/realshaded.png"), tr("Realistic"));
    _realistic->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_R));
    _shaded = dispModeMenu->addAction(QIcon(":/icons/res/shaded.png"), tr("Shaded"));
    _shaded->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_S));
    _wireframe = dispModeMenu->addAction(QIcon(":/icons/res/wireframe.png"), tr("Wireframe"));
    _wireframe->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_W));
    _wireshaded = dispModeMenu->addAction(QIcon(":/icons/res/wireshaded.png"), tr("Wire Shaded"));
    _wireshaded->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_E));

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
    _sectionBtn->setIcon(QIcon(":/icons/res/section.png"));
    _sectionBtn->setIconSize(QSize(48, 48));
    _sectionBtn->setToolTip(tr("Clipping Planes"));
    _sectionBtn->setCheckable(true);
    _sectionBtn->setAutoRaise(true);
    _mainLayout->addWidget(_sectionBtn);
    connect(_sectionBtn, &QToolButton::toggled, this, [this](bool checked) { emit sectionViewToggled(checked); });

    // Swap Visible View
    _swapBtn = new QToolButton(this);
    _swapBtn->setStyleSheet(buttonStyleSheet);
    _swapBtn->setIcon(QIcon(":/icons/res/swapvisible.png"));
    _swapBtn->setIconSize(QSize(48, 48));
    _swapBtn->setToolTip(tr("Swap Visible"));
    _swapBtn->setCheckable(true);
    _swapBtn->setAutoRaise(true);
    _mainLayout->addWidget(_swapBtn);
    connect(_swapBtn, &QToolButton::toggled, this, [this](bool checked) { emit swapVisibleToggled(checked); });

    // Show/Hide Axis
    _axisBtn = new QToolButton(this);
    _axisBtn->setStyleSheet(buttonStyleSheet);
    _axisBtn->setIcon(QIcon(":/icons/res/showAxis.png"));
    _axisBtn->setIconSize(QSize(48, 48));
    _axisBtn->setToolTip(tr("Show/Hide Axis"));
    _axisBtn->setCheckable(true);
    _axisBtn->setChecked(true);
    _axisBtn->setAutoRaise(true);
    _mainLayout->addWidget(_axisBtn);
    connect(_axisBtn, &QToolButton::toggled, this, [this](bool checked) {
        if (checked)
        {
            _axisBtn->setIcon(QIcon(":/icons/res/showAxis.png"));
            _axisBtn->setToolTip(tr("Show the trihedron"));
        }
        else
        {
            _axisBtn->setIcon(QIcon(":/icons/res/hideAxis.png"));
            _axisBtn->setToolTip(tr("Hide the trihedron"));
        }
        emit axisDisplayToggled(checked);
        });

    // Toolbar animations
    _toolbarAnimation = new QPropertyAnimation(this, "geometry", this);
    _toolbarAnimation->setDuration(300);
    _toolbarAnimation->setEasingCurve(QEasingCurve::OutCubic);

    // Auto-scroll timer
    _autoScrollTimer = new QTimer(this);
    _autoScrollTimer->setInterval(50); // Scroll every 50ms for smooth scrolling
    connect(_autoScrollTimer, &QTimer::timeout, this, [this]() {
        if (_autoScrollLeft)
        {
            scrollLeft();
        }
        else
        {
            scrollRight();
        }
        });

    // Hover delay timer
    _hoverDelayTimer = new QTimer(this);
    _hoverDelayTimer->setSingleShot(true);
    _hoverDelayTimer->setInterval(300); // 300ms delay before auto-scroll starts

    retranslateUI();

    connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this, [this]() {
        retranslateUI();
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
    // Prevent recursive calls
    if (_isRepositioning)
        return;

    _isRepositioning = true;

    // Calculate maximum toolbar width
    int maxToolbarWidth = widgetWidth - 20; // 10px margin on each side

    // Calculate minimum width needed for all buttons
    int totalButtonWidth = 0;
    for (int i = 0; i < _mainLayout->count(); ++i)
    {
        QLayoutItem* item = _mainLayout->itemAt(i);
        if (item && item->widget())
        {
            totalButtonWidth += item->widget()->sizeHint().width();
        }
    }

    // Add spacing and margins
    int buttonCount = _mainLayout->count();
    totalButtonWidth += (buttonCount - 1) * _mainLayout->spacing();
    totalButtonWidth += _mainLayout->contentsMargins().left() + _mainLayout->contentsMargins().right();

    // Account for outer layout margins
    int outerMargins = 4; // 2px on each side

    // Determine toolbar width
    int toolbarWidth;
    bool needsScrolling = (totalButtonWidth + outerMargins) > maxToolbarWidth;

    if (needsScrolling)
    {
        // Use max width when scrolling is needed
        toolbarWidth = maxToolbarWidth;
        _buttonContainer->setMinimumWidth(totalButtonWidth);
        _buttonContainer->setMaximumWidth(totalButtonWidth);
    }
    else
    {
        // Use exact width when no scrolling
        toolbarWidth = totalButtonWidth + outerMargins;
        _buttonContainer->setMinimumWidth(totalButtonWidth);
        _buttonContainer->setMaximumWidth(totalButtonWidth);
    }

    resize(toolbarWidth, 76);

    int x = (widgetWidth - toolbarWidth) / 2;
    int y = widgetHeight - 76 - 10;
    move(x, y);

    _visibleRect = QRect(x, y, toolbarWidth, 76);
    _hiddenRect = _visibleRect.translated(0, 80);

    // Update scroll button visibility after positioning is done
    _isRepositioning = false;
    checkScrollButtonsVisibility();
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

void ViewToolbar::setSwapVisibleChecked(bool checked)
{
	bool oldState = _swapBtn->blockSignals(true);
	_swapBtn->setChecked(checked);
	_swapBtn->blockSignals(oldState);
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

void ViewToolbar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    if (_isRepositioning)
        return;

    // After resize, check if we need scrolling and adjust toolbar width accordingly
    QTimer::singleShot(0, this, [this]() {
        if (parentWidget())
        {
            reposition(parentWidget()->width(), parentWidget()->height());
        }
        });
}

bool ViewToolbar::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == _scrollLeftBtn)
    {
        if (event->type() == QEvent::Enter)
        {
            // Disconnect any previous connection
            disconnect(_hoverDelayTimer, &QTimer::timeout, this, nullptr);
            // Connect to left scroll check
            connect(_hoverDelayTimer, &QTimer::timeout, this, &ViewToolbar::checkAndStartAutoScrollLeft);
            _hoverDelayTimer->start();
        }
        else if (event->type() == QEvent::Leave)
        {
            stopAutoScroll();
        }
    }
    else if (obj == _scrollRightBtn)
    {
        if (event->type() == QEvent::Enter)
        {
            // Disconnect any previous connection
            disconnect(_hoverDelayTimer, &QTimer::timeout, this, nullptr);
            // Connect to right scroll check
            connect(_hoverDelayTimer, &QTimer::timeout, this, &ViewToolbar::checkAndStartAutoScrollRight);
            _hoverDelayTimer->start();
        }
        else if (event->type() == QEvent::Leave)
        {
            stopAutoScroll();
        }
    }
    return QWidget::eventFilter(obj, event);
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

void ViewToolbar::scrollLeft()
{
    QScrollBar* scrollBar = _scrollArea->horizontalScrollBar();
    int currentValue = scrollBar->value();
    int step = 100; // Adjust scroll speed as needed
    scrollBar->setValue(currentValue - step);
}

void ViewToolbar::scrollRight()
{
    QScrollBar* scrollBar = _scrollArea->horizontalScrollBar();
    int currentValue = scrollBar->value();
    int step = 100; // Adjust scroll speed as needed
    scrollBar->setValue(currentValue + step);
}

void ViewToolbar::updateScrollButtons()
{
    QScrollBar* scrollBar = _scrollArea->horizontalScrollBar();

    // Show/hide left button
    _scrollLeftBtn->setEnabled(scrollBar->value() > scrollBar->minimum());

    // Show/hide right button
    _scrollRightBtn->setEnabled(scrollBar->value() < scrollBar->maximum());
}

void ViewToolbar::checkScrollButtonsVisibility()
{
    if (_isRepositioning)
        return;

    // Calculate the total width needed for all buttons using sizeHint
    int totalButtonWidth = 0;
    for (int i = 0; i < _mainLayout->count(); ++i)
    {
        QLayoutItem* item = _mainLayout->itemAt(i);
        if (item && item->widget())
        {
            totalButtonWidth += item->widget()->sizeHint().width();
        }
    }

    // Add spacing and margins
    int buttonCount = _mainLayout->count();
    totalButtonWidth += (buttonCount - 1) * _mainLayout->spacing();
    totalButtonWidth += _mainLayout->contentsMargins().left() + _mainLayout->contentsMargins().right();

    // Calculate how much space we have
    int toolbarWidth = width();
    int outerMargins = 4; // 2px each side
    int scrollButtonWidth = 20;

    // Available width if scroll buttons are NOT visible
    int availableWidthNoScroll = toolbarWidth - outerMargins;

    // Available width if scroll buttons ARE visible
    int availableWidthWithScroll = toolbarWidth - outerMargins - (2 * scrollButtonWidth);

    // Determine if scrolling is needed based on available space WITHOUT scroll buttons
    bool needsScrolling = totalButtonWidth > availableWidthNoScroll;

    _scrollLeftBtn->setVisible(needsScrolling);
    _scrollRightBtn->setVisible(needsScrolling);

    if (needsScrolling)
    {
        // Set button container to full content width
        _buttonContainer->setMinimumWidth(totalButtonWidth);
        _buttonContainer->setMaximumWidth(totalButtonWidth);
        updateScrollButtons();
    }
    else
    {
        // No scrolling needed - container should match available space
        _buttonContainer->setMinimumWidth(totalButtonWidth);
        _buttonContainer->setMaximumWidth(totalButtonWidth);
        _scrollArea->horizontalScrollBar()->setValue(0);
    }
}


void ViewToolbar::startAutoScroll(bool scrollLeft)
{
    _autoScrollLeft = scrollLeft;
    if (!_autoScrollTimer->isActive())
    {
        // Initial scroll immediately
        if (scrollLeft)
        {
            this->scrollLeft();
        }
        else
        {
            this->scrollRight();
        }
        // Then start timer for continuous scrolling
        _autoScrollTimer->start();
    }
}

void ViewToolbar::stopAutoScroll()
{
    _autoScrollTimer->stop();
    _hoverDelayTimer->stop();
}

void ViewToolbar::checkAndStartAutoScrollLeft()
{
    if (_scrollLeftBtn->underMouse())
    {
        startAutoScroll(true);
    }
}

void ViewToolbar::checkAndStartAutoScrollRight()
{
    if (_scrollRightBtn->underMouse())
    {
        startAutoScroll(false);
    }
}
