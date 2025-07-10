#include "ThemeManager.h"

// Implementation
ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
    , m_currentTheme(System)
    , m_settings(new QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName(), this))
{
    loadThemePreference();
}

void ThemeManager::setTheme(Theme theme)
{
    if (m_currentTheme == theme)
        return;

    m_currentTheme = theme;

    switch (theme)
    {
    case System:
        applySystemTheme();
        break;
    case Light:
        applyLightTheme();
        break;
    case Dark:
        applyDarkTheme();
        break;
    }

    saveThemePreference();
    emit themeChanged(theme);
}

void ThemeManager::applySystemTheme()
{
    // Reset to system default
    qApp->setStyleSheet("");

    // Use system palette
    qApp->setPalette(qApp->style()->standardPalette());

    // Optionally apply dark theme if system is dark
    if (isSystemDark())
    {
        applyDarkTheme();
    }
}

void ThemeManager::applyLightTheme()
{
    qApp->setStyleSheet(getLightStyleSheet());
}

void ThemeManager::applyDarkTheme()
{
    qApp->setStyleSheet(getDarkStyleSheet());
}

bool ThemeManager::isSystemDark() const
{
#ifdef Q_OS_WIN
    // Windows 10/11 dark mode detection
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat);
    return settings.value("AppsUseLightTheme", 1).toInt() == 0;
#elif defined(Q_OS_MAC)
    // macOS dark mode detection
    QProcess process;
    process.start("defaults", QStringList() << "read" << "-g" << "AppleInterfaceStyle");
    process.waitForFinished();
    return process.readAllStandardOutput().trimmed() == "Dark";
#else
    // Linux - check if dark theme is being used
    QPalette palette = qApp->palette();
    return palette.color(QPalette::Window).lightness() < 128;
#endif
}

QString ThemeManager::getLightStyleSheet() const
{
    return R"(
        QWidget {
            background-color: #ffffff;
            color: #000000;
        }
        
        QMainWindow {
            background-color: #f0f0f0;
        }
        
        QMenuBar {
            background-color: #ffffff;
            color: #000000;
            border-bottom: 1px solid #cccccc;
        }
        
        QMenuBar::item {
            background-color: transparent;
            padding: 4px 8px;
        }
        
        QMenuBar::item:selected {
            background-color: #e0e0e0;
        }
        
        QMenu {
            background-color: #ffffff;
            color: #000000;
            border: 1px solid #cccccc;
        }
        
        QMenu::item {
            padding: 4px 20px;
        }
        
        QMenu::item:selected {
            background-color: #0078d4;
            color: #ffffff;
        }
        
        QPushButton {
            background-color: #ffffff;
            color: #000000;
            border: 1px solid #cccccc;
            padding: 6px 12px;
            border-radius: 4px;
        }
        
        QPushButton:hover {
            background-color: #f0f0f0;
        }
        
        QPushButton:pressed {
            background-color: #e0e0e0;
        }
        
        QLineEdit {
            background-color: #ffffff;
            color: #000000;
            border: 1px solid #cccccc;
            padding: 4px;
            border-radius: 2px;
        }
        
        QLineEdit:focus {
            border: 2px solid #0078d4;
        }
        
        QTextEdit {
            background-color: #ffffff;
            color: #000000;
            border: 1px solid #cccccc;
        }
        
        QListWidget {
            background-color: #ffffff;
            color: #000000;
            border: 1px solid #cccccc;
            alternate-background-color: #f8f8f8;
        }
        
        QListWidget::item:selected {
            background-color: #0078d4;
            color: #ffffff;
        }
        
        QStatusBar {
            background-color: #f0f0f0;
            color: #000000;
            border-top: 1px solid #cccccc;
        }
    )";
}

QString ThemeManager::getDarkStyleSheet() const
{
    return R"(
        QWidget {
            background-color: #2b2b2b;
            color: #ffffff;
        }
        
        QMainWindow {
            background-color: #1e1e1e;
        }
        
        QMenuBar {
            background-color: #2b2b2b;
            color: #ffffff;
            border-bottom: 1px solid #555555;
        }
        
        QMenuBar::item {
            background-color: transparent;
            padding: 4px 8px;
        }
        
        QMenuBar::item:selected {
            background-color: #404040;
        }
        
        QMenu {
            background-color: #2b2b2b;
            color: #ffffff;
            border: 1px solid #555555;
        }
        
        QMenu::item {
            padding: 4px 20px;
        }
        
        QMenu::item:selected {
            background-color: #0078d4;
            color: #ffffff;
        }
        
        QPushButton {
            background-color: #404040;
            color: #ffffff;
            border: 1px solid #555555;
            padding: 6px 12px;
            border-radius: 4px;
        }
        
        QPushButton:hover {
            background-color: #4a4a4a;
        }
        
        QPushButton:pressed {
            background-color: #353535;
        }
        
        QLineEdit {
            background-color: #404040;
            color: #ffffff;
            border: 1px solid #555555;
            padding: 4px;
            border-radius: 2px;
        }
        
        QLineEdit:focus {
            border: 2px solid #0078d4;
        }
        
        QTextEdit {
            background-color: #404040;
            color: #ffffff;
            border: 1px solid #555555;
        }
        
        QListWidget {
            background-color: #404040;
            color: #ffffff;
            border: 1px solid #555555;
            alternate-background-color: #353535;
        }
        
        QListWidget::item:selected {
            background-color: #0078d4;
            color: #ffffff;
        }
        
        QStatusBar {
            background-color: #2b2b2b;
            color: #ffffff;
            border-top: 1px solid #555555;
        }
        
        QScrollBar:vertical {
            background-color: #2b2b2b;
            width: 12px;
            border: none;
        }
        
        QScrollBar::handle:vertical {
            background-color: #555555;
            border-radius: 6px;
            min-height: 20px;
        }
        
        QScrollBar::handle:vertical:hover {
            background-color: #666666;
        }
        
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            background: none;
            border: none;
        }
    )";
}

void ThemeManager::saveThemePreference()
{
    m_settings->setValue("comboBoxTheme", static_cast<int>(m_currentTheme));
}

void ThemeManager::loadThemePreference()
{
    int theme = m_settings->value("comboBoxTheme", static_cast<int>(System)).toInt();
    setTheme(static_cast<Theme>(theme));
}
