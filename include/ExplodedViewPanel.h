#pragma once

#include <QWidget>
#include "ui_ExplodedViewPanel.h"

class GLWidget;

class ExplodedViewPanel : public QWidget, private Ui::ExplodedViewPanel
{
    Q_OBJECT

public:
    explicit ExplodedViewPanel(GLWidget* parent = nullptr);

    void applyContrastTheme(const QColor& textColor);

private slots:
    void on_comboBoxMode_currentIndexChanged(int index);
    void on_sliderExplosion_valueChanged(int value);
    void on_pushButtonSelectAssembly_toggled(bool checked);
    void on_pushButtonSelectAnchor_toggled(bool checked);
    void on_pushButtonCapture_clicked();
    void on_pushButtonReset_clicked();

private:
    void updateCaptureButton();

    GLWidget* _glWidget;
};
