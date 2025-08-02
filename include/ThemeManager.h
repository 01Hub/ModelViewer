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
        System,
        Light,
        Dark,        
        DarkOrange,        
   		Dracula,
        Eclippy,
        GruvboxFusion,
        LightGray,
        Manjaroness,
        MaterialDark,
		Monokai,
        NordFusion,
		OneDark,        
        SolarizedDark,
        SolarizedLight,
        Takezo,
		TokyoNightFusion,
    };

    explicit ThemeManager(QObject* parent = nullptr);

    void setTheme(Theme theme);
    Theme currentTheme() const { return m_currentTheme; }

    void applyThemeForColorScheme(Qt::ColorScheme scheme);

	void applyThemefromStyleSheet(const QString& styleSheet);
        
signals:
    void themeChanged(Theme theme);

private:
    void applySystemTheme();
    void applySystemAwareTheme();
    void applyLightTheme();
    void applyDarkTheme();
    bool isSystemInDarkMode() const;    

    QPalette getLightPalette() const;
    QPalette getDarkPalette() const;

    QString getLightStyleSheet() const;
    QString getDarkStyleSheet() const;

    QString getCurrentStyleName() const;

    QString getDarkExtrasStyleSheet() const;

    Theme m_currentTheme;
    QSettings* m_settings;
};

