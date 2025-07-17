
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
    FlyOutViewButton* m_toolButtonCameraModes;
    QMap<CameraModeActions, QAction*> m_cameraModeActions;

    FlyOutViewButton* m_toolButtonIsometricView;
    QMap<ViewModeActions, QAction*> m_viewModeActions;

    FlyOutViewButton* m_toolButtonDisplayModes;
    QMap<DisplayModeActions, QAction*> m_displayModeActions;

    QPropertyAnimation* m_toolbarAnimation;
    QRect m_visibleRect;
    QRect m_hiddenRect;
};
