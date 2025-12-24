#ifndef TUTORIALDIALOG_H
#define TUTORIALDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QUrl>

#ifdef HAVE_WEBENGINE
#include <QWebEngineView>
#include <QWebEnginePage>

// Custom page to intercept link clicks
class TutorialWebPage : public QWebEnginePage
{
    Q_OBJECT
public:
    explicit TutorialWebPage(QObject* parent = nullptr);

protected:
    bool acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame) override;

signals:
    void linkClicked(const QUrl& url);
};
#else
#include <QTextBrowser>
#endif

class TutorialDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TutorialDialog(QWidget* parent = nullptr);
    ~TutorialDialog() = default;

private slots:
    void onLessonSelected(QListWidgetItem* current, QListWidgetItem* previous);
    void onPreviousClicked();
    void onNextClicked();
    void onLinkClicked(const QUrl& url);

private:
    void setupUI();
    void populateLessonList();
    void loadLesson(int listIndex);  // listIndex includes the index page
    void loadIndexPage();
    void updateNavigationButtons();

    QString getTutorialBasePath() const;
    QString getLessonPath(int lessonIndex) const;  // lessonIndex is 1-14 for lessons, -1 for index
    QString getLessonTitle(int lessonIndex) const;
    QString loadHtmlFile(const QString& filename);
    void showError(const QString& title, const QString& message);

    QListWidget* m_lessonList;

#ifdef HAVE_WEBENGINE
    QWebEngineView* m_webView;
    TutorialWebPage* m_webPage;
#else
    QTextBrowser* m_textBrowser;
#endif

    QPushButton* m_previousButton;
    QPushButton* m_nextButton;
    QPushButton* m_closeButton;
    QSplitter* m_splitter;

    int m_currentListIndex;  // Current position in list (0=index, 1-14=lessons)
    static constexpr int TOTAL_LESSONS = 14;
    static constexpr int TOTAL_LIST_ITEMS = TOTAL_LESSONS + 1;  // Include index page
};

#endif // TUTORIALDIALOG_H