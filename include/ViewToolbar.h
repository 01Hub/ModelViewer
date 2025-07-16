
#pragma once

#include <QWidget>
#include <QToolButton>
#include <QAction>
#include <QPropertyAnimation>

class FlyOutViewButton;

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
    FlyOutViewButton* m_toolButtonIsometricView;
    FlyOutViewButton* m_toolButtonDisplayModes;
    QPropertyAnimation* m_toolbarAnimation;
    QRect m_visibleRect;
    QRect m_hiddenRect;
};
