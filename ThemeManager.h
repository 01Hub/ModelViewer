#pragma once

#include <QObject>
#include <QApplication>
#include <QStyle>
#include <QStyleFactory>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QTextStream>

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    enum Theme
    {
        System = 0,
        Light = 1,
        Dark = 2
    };

    explicit ThemeManager(QObject* parent = nullptr);

    void setTheme(Theme theme);
    Theme currentTheme() const { return m_currentTheme; }

    void saveThemePreference();
    void loadThemePreference();

signals:
    void themeChanged(Theme theme);

private:
    void applySystemTheme();
    void applyLightTheme();
    void applyDarkTheme();
    bool isSystemDark() const;
    QString getLightStyleSheet() const;
    QString getDarkStyleSheet() const;

    Theme m_currentTheme;
    QSettings* m_settings;
};

