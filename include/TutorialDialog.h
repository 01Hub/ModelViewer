#ifndef TUTORIALDIALOG_H
#define TUTORIALDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QTextBrowser>
#include <QPushButton>
#include <QSplitter>

class TutorialDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TutorialDialog(QWidget* parent = nullptr);
    ~TutorialDialog() override = default;

    // Public method to jump to specific lesson
    void showLesson(int lessonIndex);

private slots:
    void onLessonSelected(QListWidgetItem* current, QListWidgetItem* previous);
    void onPreviousClicked();
    void onNextClicked();
    void onCloseClicked();

private:
    void setupUI();
    void populateLessonList();
    void updateNavigationButtons();

    QString loadHtmlFile(const QString& fileName);
    QString getTutorialPath() const;

    // Helper methods
    QString createStyledHtml(const QString& title, const QString& content);
    QString createSection(const QString& heading, const QString& content);
    QString createStep(int stepNumber, const QString& title, const QString& description);
    QString createScreenshotPlaceholder(const QString& filename, const QString& caption,
        int width = 600, int height = 400);
    QString createNote(const QString& noteType, const QString& content);
    QString createKeyboardKey(const QString& key);
    QString createTable(const QStringList& headers, const QList<QStringList>& rows);

    // UI Components
    QSplitter* m_splitter;
    QListWidget* m_lessonList;
    QTextBrowser* m_contentBrowser;
    QPushButton* m_previousButton;
    QPushButton* m_nextButton;
    QPushButton* m_closeButton;

    // Lesson management
    QStringList m_lessonTitles;
    int m_currentLessonIndex;
};

#endif // TUTORIALDIALOG_H
