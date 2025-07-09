#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QSettings>

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

private slots:   
    void onOkClicked();
    void onCancelClicked();
    void onApplyClicked();
    void onRestoreDefaults();

	void on_buttonResetUVPrompt_clicked();

private:
    void applySettings();
    void restoreDefaultSettings();
    void loadSettings();
    void saveSettings();

private:
    Ui::SettingsDialog *ui;

	std::unique_ptr<QSettings> _settings;
};

#endif // SETTINGSDIALOG_H
