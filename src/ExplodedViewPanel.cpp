#include "ExplodedViewPanel.h"
#include "GLWidget.h"
#include <QMenu>

ExplodedViewPanel::ExplodedViewPanel(GLWidget* parent)
    : QWidget(parent)
    , _glWidget(parent)
{
    setupUi(this);
    frameVector->setVisible(false);

    lineEditAssembly->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(lineEditAssembly, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        QAction* clearAction = menu.addAction(tr("Clear Selection"));
        connect(clearAction, &QAction::triggered, this, [this]() {
            lineEditAssembly->clear();
            lineEditAnchor->clear();
            pushButtonSelectAssembly->setChecked(false);
            pushButtonSelectAnchor->setChecked(false);
            updateCaptureButton();
        });
        menu.exec(lineEditAssembly->mapToGlobal(pos));
    });

    lineEditAnchor->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(lineEditAnchor, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        QAction* clearAction = menu.addAction(tr("Clear Anchor"));
        connect(clearAction, &QAction::triggered, this, [this]() {
            lineEditAnchor->clear();
            pushButtonSelectAnchor->setChecked(false);
        });
        menu.exec(lineEditAnchor->mapToGlobal(pos));
    });
}

void ExplodedViewPanel::applyContrastTheme(const QColor& textColor)
{
    const QString style = QString("color: %1;").arg(textColor.name());
    setStyleSheet(style);
}

void ExplodedViewPanel::on_comboBoxMode_currentIndexChanged(int index)
{
    // Index 4 = Custom Vector
    frameVector->setVisible(index == 4);
}

void ExplodedViewPanel::on_sliderExplosion_valueChanged(int value)
{
    labelDistancePercent->setText(QString("%1%").arg(value));
    updateCaptureButton();
}

void ExplodedViewPanel::on_pushButtonSelectAssembly_toggled(bool checked)
{
    if (checked) {
        pushButtonSelectAnchor->setChecked(false);
        lineEditAssembly->setPlaceholderText(tr("Click mesh or node in scene…"));
    } else {
        lineEditAssembly->setPlaceholderText(tr("Select assembly or meshes…"));
    }
}

void ExplodedViewPanel::on_pushButtonSelectAnchor_toggled(bool checked)
{
    if (checked) {
        pushButtonSelectAssembly->setChecked(false);
        lineEditAnchor->setPlaceholderText(tr("Click mesh or node in scene…"));
    } else {
        lineEditAnchor->setPlaceholderText(tr("Select anchor mesh (optional)…"));
    }
}

void ExplodedViewPanel::on_pushButtonCapture_clicked()
{
    // Full implementation in Phase 1 logic pass
}

void ExplodedViewPanel::on_pushButtonReset_clicked()
{
    // Suppress intermediate signals during reset
    QSignalBlocker b1(comboBoxMode);
    QSignalBlocker b2(sliderExplosion);
    QSignalBlocker b3(doubleSpinBoxVectorX);
    QSignalBlocker b4(doubleSpinBoxVectorY);
    QSignalBlocker b5(doubleSpinBoxVectorZ);

    lineEditAssembly->clear();
    lineEditAnchor->clear();
    pushButtonSelectAssembly->setChecked(false);
    pushButtonSelectAnchor->setChecked(false);
    lineEditAssembly->setPlaceholderText(tr("Select assembly or meshes…"));
    lineEditAnchor->setPlaceholderText(tr("Select anchor mesh (optional)…"));

    comboBoxMode->setCurrentIndex(0);
    frameVector->setVisible(false);

    sliderExplosion->setValue(100);
    labelDistancePercent->setText("100%");

    doubleSpinBoxVectorX->setValue(1.0);
    doubleSpinBoxVectorY->setValue(0.0);
    doubleSpinBoxVectorZ->setValue(0.0);

    updateCaptureButton();
}

void ExplodedViewPanel::updateCaptureButton()
{
    pushButtonCapture->setEnabled(sliderExplosion->value() >= 10);
}
