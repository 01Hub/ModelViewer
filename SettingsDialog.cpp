#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include <QMessageBox>


SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);

	_settings = std::make_unique<QSettings>(new QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName()));

    // Connect to specific buttons
    QPushButton* okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    if (okButton)
    {
        connect(okButton, &QPushButton::clicked, this, &SettingsDialog::onOkClicked);
    }

    QPushButton* cancelButton = ui->buttonBox->button(QDialogButtonBox::Cancel);
    if (cancelButton)
    {
        connect(cancelButton, &QPushButton::clicked, this, &SettingsDialog::onCancelClicked);
    }

    QPushButton* applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);
    if (applyButton)
    {
        connect(applyButton, &QPushButton::clicked, this, &SettingsDialog::onApplyClicked);
    }

    QPushButton* restoreButton = ui->buttonBox->button(QDialogButtonBox::RestoreDefaults);
    if (restoreButton)
    {
        connect(restoreButton, &QPushButton::clicked, this, &SettingsDialog::onRestoreDefaults);
    }
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::onOkClicked()
{
	QDialog::accept();
}

void SettingsDialog::onCancelClicked()
{
	QDialog::reject();
}

void SettingsDialog::onApplyClicked()
{
   
}

void SettingsDialog::onRestoreDefaults()
{
    _settings->clear();
	restoreDefaultSettings();
    qDebug() << "All settings have been reset to defaults.";
    QMessageBox::information(this, "Settings Reset", "All settings have been cleared.");
}

void SettingsDialog::applySettings()
{
}

void SettingsDialog::restoreDefaultSettings()
{
}

void SettingsDialog::loadSettings()
{
}

void SettingsDialog::on_buttonResetUVPrompt_clicked()
{
    _settings->remove("UVMethod");
    _settings->remove("RememberUVMethod");

    qDebug() << "UV Prompt settings have been reset.";
    QMessageBox::information(this, "Settings Reset", "UV Prompt settings have been cleared.");
}
