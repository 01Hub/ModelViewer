#ifndef QUICKHELPDIALOG_H
#define QUICKHELPDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QTextBrowser>
#include <QPushButton>
#include <QVBoxLayout>

class QuickHelpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QuickHelpDialog(QWidget* parent = nullptr);
    ~QuickHelpDialog() override = default;

private:
    void setupUI();
    void setupMouseControlsTab();
    void setupKeyboardShortcutsTab();
    void setupViewToolbarTab();
    void setupMenuShortcutsTab();
    void setupCameraModesTab();
    void setupDisplayModesTab();
    void setupTipsAndTricksTab();

    QString createStyledHtml(const QString& title, const QString& content);
    QString createSection(const QString& heading, const QString& content);
    QString createTable(const QStringList& headers, const QList<QStringList>& rows);

    QTabWidget* m_tabWidget;
    QTextBrowser* m_mouseControlsBrowser;
    QTextBrowser* m_keyboardBrowser;
    QTextBrowser* m_toolbarBrowser;
    QTextBrowser* m_menuBrowser;
    QTextBrowser* m_cameraBrowser;
    QTextBrowser* m_displayBrowser;
    QTextBrowser* m_tipsBrowser;
    QPushButton* m_closeButton;
};

#endif // QUICKHELPDIALOG_H
