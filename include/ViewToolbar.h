#pragma once

#include <QWidget>
#include <QToolButton>
#include <QAction>
#include <QPropertyAnimation>
#include <QHBoxLayout>
#include <QScrollArea>

class FlyOutViewButton;

enum class CameraModeActions { ORBIT, FLY, FIRST_PERSON };
enum class StandardViewActions { TOP, FRONT, LEFT, BOTTOM, REAR, RIGHT };
enum class ViewModeActions { ISOMETRIC, DIMETRIC, TRIMETRIC };
enum class DisplayModeActions { SHADED, WIREFRAME, WIRESHADED, REALSHADED };
enum class RenderingModeActions { ADS, PBR };

class ViewToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit ViewToolbar(QWidget* parent = nullptr);

    void showAnimated();
    void hideAnimated();
    QRect visibleRect() const;
    QRect hiddenRect() const;
    void reposition(int widgetWidth, int widgetHeight);
    bool isFlyoutMenuVisible() const;

    void setDefaultCameraModeAction(CameraModeActions mode);
    void setDefaultViewModeAction(ViewModeActions mode);
    void setDefaultDisplayModeAction(DisplayModeActions mode);
    void setDefaultRenderingModeAction(RenderingModeActions mode);
    void updateRenderingModeButton(const QString& mode);

    void setSwapVisibleChecked(bool checked);

signals:
    void cameraModeSelected(const QString& type);
    void viewSelected(const QString& viewName);
    void axonometricSelected(const QString& type);
    void displayModeSelected(const QString& type);
    void renderingModeSelected(const QString& mode);
    void projectionToggled(bool isOrtho);
    void fitToViewRequested();
    void zoomViewRequested();
    void panViewRequested();
    void rotateViewRequested();
    void windowZoomRequested();
    void multiViewToggled(bool enabled);
    void sectionViewToggled(bool enabled);
    void swapVisibleToggled(bool enabled);
    void axisDisplayToggled(bool enabled);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void retranslateUI();
    void updateScrollButtons();
    void scrollLeft();
    void scrollRight();
    void checkScrollButtonsVisibility();
    void startAutoScroll(bool scrollLeft);
    void stopAutoScroll();
    void checkAndStartAutoScrollLeft();
    void checkAndStartAutoScrollRight();

private:
    // Scroll infrastructure
    QWidget* _buttonContainer;
    QScrollArea* _scrollArea;
    QHBoxLayout* _mainLayout;
    QToolButton* _scrollLeftBtn;
    QToolButton* _scrollRightBtn;
    bool _isRepositioning;
    QTimer* _autoScrollTimer;
    bool _autoScrollLeft;
    QTimer* _hoverDelayTimer;

    // Navigation buttons
    QToolButton* _btnRotateView;
    QToolButton* _btnPanView;
    QToolButton* _btnZoomView;
    QToolButton* _btnFitAll;
    QToolButton* _btnWindowZoom;

    // Camera mode actions
    QAction* _orbitAction;
    QAction* _flyAction;
    QAction* _firstPersonAction;

    // View mode actions (grouped in dropdown menu)
    QAction* _topViewAction;
    QAction* _frontViewAction;
    QAction* _leftViewAction;
    QAction* _bottomViewAction;
    QAction* _rearViewAction;
    QAction* _rightViewAction;

    // Rendering mode actions
    QAction* _adsAction;
    QAction* _pbrAction;

    // Axonometric actions
    QAction* _isoAction;
    QAction* _dimAction;
    QAction* _triAction;

    // Projection toggle
    QToolButton* _projToggleButton;

    // Multi view
    QToolButton* _multiBtn;

    // Display mode actions
    QAction* _realistic;
    QAction* _shaded;
    QAction* _wireframe;
    QAction* _wireshaded;

    // Other buttons
    QToolButton* _sectionBtn;
    QToolButton* _swapBtn;
    QToolButton* _axisBtn;

    // Flyout buttons and action maps
    FlyOutViewButton* _toolButtonCameraModes;
    QMap<CameraModeActions, QAction*> _cameraModeActions;

    FlyOutViewButton* _toolButtonViews;
    QMap<StandardViewActions, QAction*> _standardViewActions;

    FlyOutViewButton* _toolButtonRenderingMode;
    QMap<RenderingModeActions, QAction*> _renderingModeActions;

    FlyOutViewButton* _toolButtonViewModes;
    QMap<ViewModeActions, QAction*> _viewModeActions;

    FlyOutViewButton* _toolButtonDisplayModes;
    QMap<DisplayModeActions, QAction*> _displayModeActions;

    // Animation
    QPropertyAnimation* _toolbarAnimation;
    QRect _visibleRect;
    QRect _hiddenRect;
};
