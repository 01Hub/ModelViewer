#include "FloatingPanelDialog.h"

#include <QIcon>
#include <QPixmap>
#include <QSizePolicy>

FloatingPanelDialog::FloatingPanelDialog(QWidget* parent, const QString& title,
                                         Qt::WindowFlags extraFlags)
    : QDialog(parent, Qt::Window | extraFlags)
{
    setWindowTitle(title);

    // ---- main layout (toolbar + content) ----
    _mainLayout = new QVBoxLayout(this);
    _mainLayout->setContentsMargins(0, 0, 0, 0);
    _mainLayout->setSpacing(0);

    // ---- thin toolbar strip ----
    _toolbar = new QWidget(this);
    _toolbar->setObjectName("floatingPanelToolbar");
    _toolbar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    _toolbarLayout = new QHBoxLayout(_toolbar);
    _toolbarLayout->setContentsMargins(4, 2, 4, 2);
    _toolbarLayout->setSpacing(4);

    // Spacer pushes pin button to the right
    _toolbarLayout->addStretch();

    // Pin button — checkable, defaults to checked (pinned = Escape blocked)
    _pinButton = new QToolButton(_toolbar);
    _pinButton->setCheckable(true);
    _pinButton->setChecked(true);
    _pinButton->setAutoRaise(true);
    _pinButton->setToolTip(tr("Pin panel (prevent Escape from closing)"));

    // Build a two-state icon: normal = pinned, off = unpinned
    _pinIcon   = QIcon(QPixmap(":/icons/res/pin.png"));
    _unpinIcon = QIcon(QPixmap(":/icons/res/unpin.png"));
    _pinButton->setIcon(_pinIcon);

    // Swap icon and tooltip whenever the pin state changes
    connect(_pinButton, &QToolButton::toggled, this, [this](bool checked) {
        _pinButton->setIcon(checked ? _pinIcon : _unpinIcon);
        _pinButton->setToolTip(checked
            ? tr("Pin panel (prevent Escape from closing)")
            : tr("Unpin panel (Escape will close)"));
    });

    _toolbarLayout->addWidget(_pinButton);
    _mainLayout->addWidget(_toolbar);
}

void FloatingPanelDialog::addContentWidget(QWidget* widget)
{
    // Content gets a 6 px margin and expands to fill all remaining space.
    QWidget* wrapper = new QWidget(this);
    QVBoxLayout* wl  = new QVBoxLayout(wrapper);
    wl->setContentsMargins(6, 4, 6, 6);
    wl->addWidget(widget);
    _mainLayout->addWidget(wrapper, 1 /*stretch*/);
}

bool FloatingPanelDialog::isPinned() const
{
    return _pinButton->isChecked();
}

void FloatingPanelDialog::reject()
{
    if (_pinButton->isChecked())
        return;          // pinned — swallow Escape, do nothing
    QDialog::reject();   // unpinned — close normally
}
