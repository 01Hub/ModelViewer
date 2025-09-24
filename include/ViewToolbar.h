
#pragma once

#include <QWidget>
#include <QToolButton>
#include <QAction>
#include <QPropertyAnimation>

class FlyOutViewButton;

enum class CameraModeActions { ORBIT, FLY, FIRST_PERSON };
enum class ViewModeActions { ISOMETRIC, DIMETRIC, TRIMETRIC };
enum class DisplayModeActions { SHADED, WIREFRAME, WIRESHADED, REALSHADED };

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

    void setSwapVisibleChecked(bool checked);

signals:
    void cameraModeSelected(const QString& type);
    void viewSelected(const QString& viewName);
    void axonometricSelected(const QString& type);
    void displayModeSelected(const QString& type);
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
    
protected slots:
    void paintEvent(QPaintEvent* event);    

private:
    void retranslateUI();

private:

    QToolButton* _btnRotateView;
    QToolButton* _btnPanView;
    QToolButton* _btnZoomView;
    QToolButton* _btnFitAll;
    QToolButton* _btnWindowZoom;
    
    QAction* _orbitAction;
    QAction* _flyAction;
    QAction* _firstPersonAction;

    QToolButton* _btnTopView;
    QToolButton* _btnFrontView;
    QToolButton* _btnLeftView;
    QToolButton* _btnBottomView;
    QToolButton* _btnRearView;
    QToolButton* _btnRightView;

    QAction* _isoAction;
    QAction* _dimAction;
    QAction* _triAction;

    QToolButton* _projToggleButton;

    QToolButton* _multiBtn;

    QAction* _realistic;
    QAction* _shaded;
    QAction* _wireframe;
    QAction* _wireshaded;

    QToolButton* _sectionBtn;

    QToolButton* _swapBtn;

    QToolButton* _axisBtn;

    FlyOutViewButton* _toolButtonCameraModes;
    QMap<CameraModeActions, QAction*> _cameraModeActions;

    FlyOutViewButton* _toolButtonViewModes;
    QMap<ViewModeActions, QAction*> _viewModeActions;

    FlyOutViewButton* _toolButtonDisplayModes;
    QMap<DisplayModeActions, QAction*> _displayModeActions;

    QPropertyAnimation* _toolbarAnimation;
    QRect _visibleRect;
    QRect _hiddenRect;
};
