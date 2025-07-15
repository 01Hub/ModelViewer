// LanguageManager.cpp
#include "LanguageManager.h"
#include <QApplication>
#include <QLibraryInfo>
#include <QDebug>

#include "config.h"

LanguageManager::LanguageManager() {}

LanguageManager& LanguageManager::instance()
{
    static LanguageManager mgr;
    return mgr;
}

void LanguageManager::loadLanguage(const QString& langCode)
{
    QString qtPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
    QString appPath = QString(MODELVIEWER_DATA_DIR) + "/translations";

    QString baseLang = langCode.section('_', 0, 0);

    // Uninstall existing
    qApp->removeTranslator(&m_appTranslator);
    qApp->removeTranslator(&m_qtTranslator);

    bool loaded = false;

    if (m_qtTranslator.load("qt_" + baseLang, qtPath))
    {
        qApp->installTranslator(&m_qtTranslator);
    }
    else if (m_qtTranslator.load("qt_" + langCode, qtPath))
    {
        qApp->installTranslator(&m_qtTranslator);
    }

    QStringList candidates = {
        QString("modelviewer_%1").arg(langCode),
        QString("modelviewer_%1").arg(baseLang)
    };

    for (const QString& file : candidates) {
        if (m_appTranslator.load(file, appPath)) {
            qApp->installTranslator(&m_appTranslator);
            m_currentLanguage = langCode;
            loaded = true;
            break;
        }
    }

    if (loaded) {
        emit languageChanged();  // Notify everyone!
    } else {
        qDebug() << "No language loaded for" << langCode;
    }
}
