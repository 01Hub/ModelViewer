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
    Theme currentTheme() const { return _currentTheme; }

    void applyThemeForColorScheme(bool isDarkMode);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    void applyThemeForColorScheme(Qt::ColorScheme scheme);
#endif

	void applyThemefromStyleSheet(const QString& styleSheet);

    bool isSystemInDarkMode() const;
        
signals:
    void themeChanged(Theme theme);

private:
    void applySystemTheme();
    void applySystemAwareTheme();
    void applyLightTheme();
    void applyDarkTheme();


    QPalette getLightPalette() const;
    QPalette getDarkPalette() const;

    QString getLightStyleSheet() const;
    QString getDarkStyleSheet() const;

    QString getCurrentStyleName() const;

    QString getDarkExtrasStyleSheet() const;

    Theme _currentTheme;
    QSettings* _settings;
};

