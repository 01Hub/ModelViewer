#include "ThemeManager.h"
#include <QGuiApplication>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif

// Implementation
ThemeManager::ThemeManager(QObject* parent)
	: QObject(parent)
	, m_currentTheme(System)
	, m_settings(new QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName(), this))
{

}

void ThemeManager::setTheme(Theme theme)
{
	Theme oldTheme = m_currentTheme;
	m_currentTheme = theme;

	switch (theme)
	{
	case System:
		applySystemAwareTheme();
		break;
	case Light:
		applyLightTheme();
		break;
	case Dark:
		applyDarkTheme();
		break;
	case DarkOrange:
		applyThemefromStyleSheet("DarkOrange");
		break;	
	case Dracula:
        applyThemefromStyleSheet("Dracula");
		break;
	case Eclippy:
		applyThemefromStyleSheet("Eclippy");
		break;
	case GruvboxFusion:
        applyThemefromStyleSheet("GruvboxFusion");
		break;
	case LightGray:
		applyThemefromStyleSheet("LightGray");
		break;
	case Manjaroness:
		applyThemefromStyleSheet("Manjaroness");
		break;
	case MaterialDark:
		applyThemefromStyleSheet("MaterialDark");
		break;
	case Monokai:
        applyThemefromStyleSheet("Monokai");
		break;
    case NordFusion:
		applyThemefromStyleSheet("NordFusion");
		break;
	case OneDark:
        applyThemefromStyleSheet("OneDark");
		break;
    case SolarizedDark:
        applyThemefromStyleSheet("SolarizedDark");
        break;
    case SolarizedLight:
        applyThemefromStyleSheet("SolarizedLight");
        break;
	case Takezo:
		applyThemefromStyleSheet("Takezo");
		break;
	case TokyoNightFusion:
        applyThemefromStyleSheet("TokyoNightFusion");
		break;
	default:
		qWarning() << "Unknown theme selected:" << theme;
	}

	if (oldTheme != theme)
		emit themeChanged(theme);
}

void ThemeManager::applySystemTheme()
{
	// Reset to system default
	qApp->setStyleSheet("");

	// Use system palette
	qApp->setPalette(qApp->style()->standardPalette());

	// Optionally apply dark theme if system is dark
	if (isSystemInDarkMode())
	{
		applyDarkTheme();
	}
}

void ThemeManager::applySystemAwareTheme()
{
	QString styleName = getCurrentStyleName();
	bool dark = isSystemInDarkMode();

	// Set platform-appropriate style first

#if defined(Q_OS_MAC)
	qApp->setStyle(QStyleFactory::create("macOS"));
#else
	qApp->setStyle(QStyleFactory::create("Fusion")); // fallback for other platforms
#endif

	if (dark)
	{
#ifdef Q_OS_WIN
		QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat);
		if (settings.value("AppsUseLightTheme") == 0)
		{
			QPalette darkPalette;
			QColor darkColor = QColor(45, 45, 45);
			QColor disabledColor = QColor(127, 127, 127);
			darkPalette.setColor(QPalette::Window, darkColor);
			darkPalette.setColor(QPalette::WindowText, Qt::white);
			darkPalette.setColor(QPalette::Base, QColor(18, 18, 18));
			darkPalette.setColor(QPalette::AlternateBase, darkColor);
			darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
			darkPalette.setColor(QPalette::ToolTipText, Qt::white);
			darkPalette.setColor(QPalette::Text, Qt::white);
			darkPalette.setColor(QPalette::Disabled, QPalette::Text, disabledColor);
			darkPalette.setColor(QPalette::Button, darkColor);
			darkPalette.setColor(QPalette::ButtonText, Qt::white);
			darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledColor);
			darkPalette.setColor(QPalette::BrightText, Qt::red);
			darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
			darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
			darkPalette.setColor(QPalette::HighlightedText, Qt::black);
			darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledColor);
			qApp->setPalette(darkPalette);
			qApp->setStyleSheet("QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }");
		}
		else
		{
			QApplication::setPalette(getDarkPalette());
			qApp->setStyleSheet(getDarkExtrasStyleSheet()); // optional: menus/tooltips
		}
#else
		QApplication::setPalette(getDarkPalette());
		qApp->setStyleSheet(getDarkExtrasStyleSheet()); // optional: menus/tooltips
#endif
	}
	else
	{
#ifdef Q_OS_WIN
		QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat);
		if (settings.value("AppsUseLightTheme") == 1)
		{
			QPalette lightPalette;
			QColor lightColor = QColor(240, 240, 240);
			QColor disabledColor = QColor(127, 127, 127);
			lightPalette.setColor(QPalette::Window, lightColor);
			lightPalette.setColor(QPalette::WindowText, Qt::black);
			lightPalette.setColor(QPalette::Base, Qt::white);
			lightPalette.setColor(QPalette::AlternateBase, QColor(233, 233, 233));
			lightPalette.setColor(QPalette::ToolTipBase, Qt::black);
			lightPalette.setColor(QPalette::ToolTipText, Qt::black);
			lightPalette.setColor(QPalette::Text, Qt::black);
			lightPalette.setColor(QPalette::Disabled, QPalette::Text, disabledColor);
			lightPalette.setColor(QPalette::Button, lightColor);
			lightPalette.setColor(QPalette::ButtonText, Qt::black);
			lightPalette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledColor);
			lightPalette.setColor(QPalette::BrightText, Qt::red);
			lightPalette.setColor(QPalette::Link, QColor(0, 0, 238));
			lightPalette.setColor(QPalette::Highlight, QColor(0, 120, 215));
			lightPalette.setColor(QPalette::HighlightedText, Qt::white);
			lightPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledColor);
			qApp->setPalette(lightPalette);
			qApp->setStyleSheet("QToolTip { color: #000000; background-color: #ffffcc; border: 1px solid black; }");
		}
		else
		{
			QApplication::setPalette(getLightPalette());
			qApp->setStyleSheet(""); // no styles needed for light
		}
#else
		QApplication::setPalette(getLightPalette());
		qApp->setStyleSheet(""); // no styles needed for light
#endif
	}
	qDebug() << "Applied" << (dark ? "dark" : "light") << "theme using" << QApplication::style()->objectName();
}

void ThemeManager::applyLightTheme()
{
	//qApp->setStyleSheet(getLightStyleSheet());
	qApp->setStyle(QStyleFactory::create("Fusion"));
	qApp->setPalette(getLightPalette());
}

void ThemeManager::applyDarkTheme()
{
	//qApp->setStyleSheet(getDarkStyleSheet());
	qApp->setStyle(QStyleFactory::create("Fusion"));
	qApp->setPalette(getDarkPalette());
	qApp->setStyleSheet(getDarkExtrasStyleSheet());
}

bool ThemeManager::isSystemInDarkMode() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
	return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
#if defined(Q_OS_WIN)
	QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
		QSettings::NativeFormat);
	return settings.value("AppsUseLightTheme", 1).toInt() == 0;
#elif defined(Q_OS_MACOS)
	// macOS system appearance detection via Objective-C bridge
	return QSysInfo::productVersion().startsWith("10.14") || QSysInfo::productVersion() > "10.14";
#elif defined(Q_OS_LINUX)
	// Basic fallback, more robust check requires DBus or gsettings
	QByteArray desktop = qgetenv("XDG_CURRENT_DESKTOP");
	QByteArray theme = qgetenv("GTK_THEME");
	return desktop.contains("GNOME") && theme.contains("dark");
#else
	return false; // Default to light
#endif
#endif
}

void ThemeManager::applyThemeForColorScheme(Qt::ColorScheme scheme)
{
	if (QApplication::style()->objectName().toLower() != "fusion")
	{
		QApplication::setStyle(QStyleFactory::create("Fusion"));
	}

	if (scheme == Qt::ColorScheme::Dark)
	{
		applyDarkTheme();
	}
	else
	{
		applyLightTheme();
	}

	qDebug() << "Applied" << (scheme == Qt::ColorScheme::Dark ? "Dark" : "Light") << "theme";
}

void ThemeManager::applyThemefromStyleSheet(const QString& styleSheet)
{
	QString qssFilePath = QString(":/styles/themes/%1.qss").arg(styleSheet);
	if (!qssFilePath.isEmpty())
	{
		QFile qssFile(qssFilePath);
		if (qssFile.open(QFile::ReadOnly))
		{
			QString styleSheet = QString::fromUtf8(qssFile.readAll());
			qApp->setStyleSheet(styleSheet);
			qssFile.close();
		}
	}
	else
	{
		qDebug() << "Failed to load stylesheet from" << qssFilePath;
	}
}

QPalette ThemeManager::getLightPalette() const
{
	QPalette palette;
	palette.setColor(QPalette::Window, QColor("#f4f4f4"));
	palette.setColor(QPalette::WindowText, QColor("#2e2e2e"));
	palette.setColor(QPalette::Base, QColor("#ffffff"));
	palette.setColor(QPalette::AlternateBase, QColor("#f0f0f0"));
	palette.setColor(QPalette::ToolTipBase, QColor("#ffffdc"));
	palette.setColor(QPalette::ToolTipText, QColor("#2e2e2e"));
	palette.setColor(QPalette::Text, QColor("#2e2e2e"));
	palette.setColor(QPalette::Button, QColor("#e5e5e5"));
	palette.setColor(QPalette::ButtonText, QColor("#2e2e2e"));
	palette.setColor(QPalette::BrightText, Qt::red);
	palette.setColor(QPalette::Highlight, QColor("#b3d4fc"));
	palette.setColor(QPalette::HighlightedText, QColor("#1e1e1e"));
	return palette;
}

QPalette ThemeManager::getDarkPalette() const
{
	QPalette palette;

	// Base UI background
	palette.setColor(QPalette::Window, QColor("#2b2b2b"));          // Main window background
	palette.setColor(QPalette::Base, QColor("#3a3a3a"));            // Input fields
	palette.setColor(QPalette::AlternateBase, QColor("#323232"));  // Alternating row color

	// Text colors (no pure white!)
	palette.setColor(QPalette::WindowText, QColor("#d0d0d0"));      // Labels, titles
	palette.setColor(QPalette::Text, QColor("#d0d0d0"));            // Editable text
	palette.setColor(QPalette::ButtonText, QColor("#d0d0d0"));      // Text on buttons
	palette.setColor(QPalette::HighlightedText, QColor("#f0f0f0")); // Selected text

	// Tooltips
	palette.setColor(QPalette::ToolTipBase, QColor("#404040"));
	palette.setColor(QPalette::ToolTipText, QColor("#e0e0e0"));

	// Buttons and controls
	palette.setColor(QPalette::Button, QColor("#444444"));
	palette.setColor(QPalette::BrightText, QColor("#ff6a6a"));      // For errors

	// Highlight / selection
	palette.setColor(QPalette::Highlight, QColor("#467cbf"));       // Calm muted blue
	palette.setColor(QPalette::Link, QColor("#589df6"));
	palette.setColor(QPalette::LinkVisited, QColor("#ab82ff"));

	// Disabled state (greyed out)
	palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#707070"));
	palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#707070"));
	palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#707070"));

	return palette;
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

QString ThemeManager::getCurrentStyleName() const
{
	QStyle* style = QApplication::style();
	return style ? style->objectName() : "Unknown";
}


QString ThemeManager::getDarkExtrasStyleSheet() const
{
	return R"(
        QMenuBar {
            background-color: #2b2b2b;
            color: #d0d0d0;
        }
        QMenuBar::item:selected {
            background-color: #3d3d3d;
        }
        QMenu {
            background-color: #2b2b2b;
            color: #d0d0d0;
            border: 1px solid #444444;
        }
        QMenu::item:selected {
            background-color: #3d3d3d;
        }
        QToolTip {
            background-color: #353535;
            color: #e0e0e0;
            border: 1px solid #6a6a6a;
        }
    )";
}