// LanguageManager.h
#pragma once

#include <QObject>
#include <QTranslator>

class LanguageManager : public QObject
{
    Q_OBJECT

public:
    static LanguageManager& instance();

    void loadLanguage(const QString& langCode);
    QString currentLanguage() const { return m_currentLanguage; }

signals:
    void languageChanged();

private:
    LanguageManager();
    QTranslator m_appTranslator;
    QTranslator m_qtTranslator;
    QString m_currentLanguage;
};
