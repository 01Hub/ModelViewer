#include "TutorialDialog.h"
#include "config.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QApplication>
#include <QScreen>
#include <QScrollBar>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QBuffer>
#include <QFont>

TutorialDialog::TutorialDialog(QWidget* parent)
    : QDialog(parent)
    , m_currentLessonIndex(0)
{
    setupUI();
    populateLessonList();
    setWindowTitle(tr("ModelViewer Tutorial"));

    // Set dialog size to 80% of screen
    QScreen* screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int width = static_cast<int>(screenGeometry.width() * 0.8);
    int height = static_cast<int>(screenGeometry.height() * 0.8);
    resize(width, height);

    // Select first lesson
    m_lessonList->setCurrentRow(0);
}

void TutorialDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Create splitter for lesson list and content
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // Lesson list on the left
    m_lessonList = new QListWidget();
    m_lessonList->setMaximumWidth(300);
    m_lessonList->setStyleSheet(
        "QListWidget {"
        "    border: 1px solid palette(mid);"
        "    border-radius: 4px;"
        "    background-color: palette(base);"
        "    color: palette(text);"
        "}"
        "QListWidget::item {"
        "    padding: 10px;"
        "    border-bottom: 1px solid palette(midlight);"
        "}"
        "QListWidget::item:selected {"
        "    background-color: palette(highlight);"
        "    color: palette(highlighted-text);"
        "}"
        "QListWidget::item:hover {"
        "    background-color: palette(alternate-base);"
        "}"
    );


    // Content browser on the right
	m_contentBrowser = new QTextBrowser();
	m_contentBrowser->setOpenExternalLinks(false);
	m_contentBrowser->setStyleSheet(
        "QTextBrowser {"
        "    background-color: palette(base);"
        "    color: palette(text);"
        "    border: 1px solid palette(mid);"
        "    border-radius: 4px;"
        "    padding: 6px;"
        "}"

        /* Scrollbars inherit theme naturally */
        "QTextBrowser QScrollBar {"
        "    background: palette(base);"
        "}"

        /* Selection inside text */
        "QTextBrowser::selection {"
        "    background: palette(highlight);"
        "    color: palette(highlighted-text);"
        "}"

        /* Links */
        "QTextBrowser a {"
        "    color: palette(link);"
        "    text-decoration: none;"
        "}"
        "QTextBrowser a:hover {"
        "    text-decoration: underline;"
        "}"

        /* Headings */
        "QTextBrowser h1, QTextBrowser h2, QTextBrowser h3 {"
        "    color: palette(text);"
        "}"

        /* Subtle separators (if you use <hr>) */
        "QTextBrowser hr {"
        "    border: none;"
        "    border-top: 1px solid palette(midlight);"
        "}"
    );

    connect(m_contentBrowser, &QTextBrowser::anchorClicked, [this](const QUrl& url) {
        if (url.fileName() == "index.html")
        {
            // Handle index specifically or just load it
            m_contentBrowser->setHtml(loadHtmlFile("index.html"));
        }
        else
        {
            // Optional: logic to change m_lessonList selection based on filename clicked
        }
        });


    m_splitter->addWidget(m_lessonList);
    m_splitter->addWidget(m_contentBrowser);
    m_splitter->setStretchFactor(0, 0); // Lesson list doesn't stretch
    m_splitter->setStretchFactor(1, 1); // Content stretches

    mainLayout->addWidget(m_splitter);

    // Navigation buttons at bottom
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_previousButton = new QPushButton(tr("◀ Previous"), this);
    m_previousButton->setMinimumWidth(120);

    buttonLayout->addWidget(m_previousButton);
    buttonLayout->addStretch();

    m_closeButton = new QPushButton(tr("Close"), this);
    m_closeButton->setMinimumWidth(100);

    buttonLayout->addWidget(m_closeButton);
    buttonLayout->addStretch();

    m_nextButton = new QPushButton(tr("Next ▶"), this);
    m_nextButton->setMinimumWidth(120);

    buttonLayout->addWidget(m_nextButton);

    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(m_lessonList, &QListWidget::currentItemChanged,
        this, &TutorialDialog::onLessonSelected);
    connect(m_previousButton, &QPushButton::clicked,
        this, &TutorialDialog::onPreviousClicked);
    connect(m_nextButton, &QPushButton::clicked,
        this, &TutorialDialog::onNextClicked);
    connect(m_closeButton, &QPushButton::clicked,
        this, &TutorialDialog::onCloseClicked);
}

void TutorialDialog::populateLessonList()
{
    m_lessonTitles = {
        tr("1. Getting Started"), tr("2. Opening Models"), tr("3. Basic Navigation"),
        tr("4. Selecting Objects"), tr("5. View Modes"), tr("6. Camera Modes"),
        tr("7. Display Modes"), tr("8. Manipulating Objects"), tr("9. Materials & Textures"),
        tr("10. Lighting & Environment"), tr("11. Working with Visibility"),
        tr("12. Advanced Features"), tr("13. Performance Optimization"), tr("14. Tips & Workflows")
    };

    for (const QString& title : m_lessonTitles)
    {
        m_lessonList->addItem(title);
    }
}

void TutorialDialog::onLessonSelected(QListWidgetItem* current, QListWidgetItem* previous)
{
    Q_UNUSED(previous);
    if (!current) return;

    m_currentLessonIndex = m_lessonList->row(current);

    // Construct filename: lesson01.html, lesson02.html, etc.
    QString fileName = QString("lesson%1.html").arg(m_currentLessonIndex + 1, 2, 10, QChar('0'));

    // Load content
    QString content = loadHtmlFile(fileName);

    // Set search paths so images/css in the same directory are found automatically
    m_contentBrowser->setSearchPaths({ getTutorialPath() });

    m_contentBrowser->setHtml(content);
    m_contentBrowser->verticalScrollBar()->setValue(0);
    updateNavigationButtons();
}

void TutorialDialog::onPreviousClicked()
{
    if (m_currentLessonIndex > 0)
    {
        m_lessonList->setCurrentRow(m_currentLessonIndex - 1);
    }
}

void TutorialDialog::onNextClicked()
{
    if (m_currentLessonIndex < m_lessonTitles.count() - 1)
    {
        m_lessonList->setCurrentRow(m_currentLessonIndex + 1);
    }
}

void TutorialDialog::onCloseClicked()
{
    accept();
}

void TutorialDialog::updateNavigationButtons()
{
    m_previousButton->setEnabled(m_currentLessonIndex > 0);
    m_nextButton->setEnabled(m_currentLessonIndex < m_lessonTitles.count() - 1);
}

QString TutorialDialog::getTutorialPath() const
{
    // Uses the path you specified: QString(MODELVIEWER_DATA_DIR) + "/data/tutorials/"
    return QString(MODELVIEWER_DATA_DIR) + "/data/tutorials/";
}

QString TutorialDialog::loadHtmlFile(const QString& fileName)
{
    QString filePath = getTutorialPath() + fileName;
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return tr("<h1>Error</h1><p>Could not load lesson file: %1</p>").arg(fileName);
    }

    QTextStream in(&file);
    return in.readAll();
}

void TutorialDialog::showLesson(int lessonIndex)
{
    if (lessonIndex >= 0 && lessonIndex < m_lessonTitles.count())
    {
        m_lessonList->setCurrentRow(lessonIndex);
    }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

QString TutorialDialog::createStyledHtml(const QString& title, const QString& content)
{
    QString html = QString(
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<style>"
        "body { "
        "    font-family: 'Segoe UI', Arial, sans-serif; "
        "    font-size: 10pt; "
        "    margin: 20px; "
        "    line-height: 1.6; "
        "    color: #333; "
        "}"
        "h1 { "
        "    color: #2c3e50; "
        "    font-size: 24pt; "
        "    border-bottom: 3px solid #3498db; "
        "    padding-bottom: 10px; "
        "    margin-bottom: 20px; "
        "}"
        "h2 { "
        "    color: #34495e; "
        "    font-size: 16pt; "
        "    margin-top: 30px; "
        "    margin-bottom: 15px; "
        "    border-left: 4px solid #3498db; "
        "    padding-left: 10px; "
        "}"
        ".step { "
        "    background-color: #f8f9fa; "
        "    border-left: 4px solid #27ae60; "
        "    padding: 15px; "
        "    margin: 15px 0; "
        "    border-radius: 4px; "
        "}"
        ".step-title { "
        "    font-weight: bold; "
        "    color: #27ae60; "
        "    font-size: 11pt; "
        "    margin-bottom: 8px; "
        "}"
        ".step-content { "
        "    color: #555; "
        "}"
        ".screenshot { "
        "    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); "
        "    border: 3px dashed #555; "
        "    border-radius: 8px; "
        "    padding: 40px; "
        "    margin: 20px 0; "
        "    text-align: center; "
        "    color: white; "
        "    font-weight: bold; "
        "    min-height: 200px; "
        "    display: flex; "
        "    flex-direction: column; "
        "    justify-content: center; "
        "    align-items: center; "
        "}"
        ".screenshot-filename { "
        "    font-size: 14pt; "
        "    margin-bottom: 15px; "
        "    text-shadow: 2px 2px 4px rgba(0,0,0,0.3); "
        "}"
        ".screenshot-caption { "
        "    font-size: 10pt; "
        "    font-weight: normal; "
        "    font-style: italic; "
        "    opacity: 0.9; "
        "}"
        ".note { "
        "    padding: 12px 15px; "
        "    margin: 15px 0; "
        "    border-radius: 4px; "
        "    border-left: 4px solid; "
        "}"
        ".note-tip { "
        "    background-color: #e8f5e9; "
        "    border-color: #4caf50; "
        "    color: #2e7d32; "
        "}"
        ".note-info { "
        "    background-color: #e3f2fd; "
        "    border-color: #2196f3; "
        "    color: #1565c0; "
        "}"
        ".note-warning { "
        "    background-color: #fff3e0; "
        "    border-color: #ff9800; "
        "    color: #e65100; "
        "}"
        ".note::before { "
        "    font-weight: bold; "
        "    margin-right: 8px; "
        "}"
        ".note-tip::before { content: '💡 TIP: '; }"
        ".note-info::before { content: 'ℹ️ INFO: '; }"
        ".note-warning::before { content: '⚠️ WARNING: '; }"
        "kbd { "
        "    background-color: #f4f4f4; "
        "    border: 1px solid #ccc; "
        "    border-radius: 3px; "
        "    box-shadow: 0 1px 0 rgba(0,0,0,0.2), 0 0 0 2px #fff inset; "
        "    color: #333; "
        "    display: inline-block; "
        "    font-family: 'Courier New', monospace; "
        "    font-size: 9pt; "
        "    font-weight: bold; "
        "    line-height: 1; "
        "    padding: 4px 8px; "
        "    white-space: nowrap; "
        "}"
        "table { "
        "    border-collapse: collapse; "
        "    width: 100%%; "
        "    margin: 15px 0; "
        "    box-shadow: 0 2px 4px rgba(0,0,0,0.1); "
        "}"
        "th { "
        "    background-color: #3498db; "
        "    color: white; "
        "    padding: 12px; "
        "    text-align: left; "
        "    font-weight: bold; "
        "    font-size: 10pt; "
        "}"
        "td { "
        "    border: 1px solid #ddd; "
        "    padding: 10px; "
        "    font-size: 10pt; "
        "}"
        "tr:nth-child(even) { "
        "    background-color: #f8f9fa; "
        "}"
        "tr:hover { "
        "    background-color: #e8f4f8; "
        "}"
        "ul, ol { "
        "    margin-left: 20px; "
        "    padding-left: 20px; "
        "}"
        "li { "
        "    margin-bottom: 10px; "
        "    line-height: 1.6; "
        "}"
        "p { "
        "    margin: 12px 0; "
        "}"
        "</style>"
        "</head>"
        "<body>"
        "<h1>%1</h1>"
        "%2"
        "</body>"
        "</html>"
    ).arg(title, content);

    return html;
}

QString TutorialDialog::createSection(const QString& heading, const QString& content)
{
    if (content.isEmpty())
        return QString("<h2>%1</h2>").arg(heading);

    return QString("<h2>%1</h2>%2").arg(heading, content);
}

QString TutorialDialog::createStep(int stepNumber, const QString& title, const QString& description)
{
    return QString(
        "<div class='step'>"
        "<div class='step-title'>Step %1: %2</div>"
        "<div class='step-content'>%3</div>"
        "</div>"
    ).arg(stepNumber).arg(title, description);
}

QString TutorialDialog::createScreenshotPlaceholder(const QString& filename, const QString& caption,
    int width, int height)
{
    // Try to load image from data directory
    QString imagePath = QString(MODELVIEWER_DATA_DIR) + "/data/tutorials/screenshots/" + filename;
    QImage image;

    if (QFile::exists(imagePath))
    {
        // Load actual screenshot
        if (image.load(imagePath))
        {
            // Scale image if needed to fit within max dimensions while preserving aspect ratio
            if (image.width() > width || image.height() > height)
            {
                image = image.scaled(width * 1.5, height * 1.5, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }
        else
        {
            // File exists but couldn't be loaded - create error placeholder
            image = QImage(width, height, QImage::Format_RGB32);
            image.fill(QColor(255, 240, 240)); // Light red background

            QPainter painter(&image);
            painter.setPen(QColor(180, 0, 0)); // Dark red text
            QFont font = painter.font();
            font.setPointSize(12);
            font.setBold(true);
            painter.setFont(font);
            painter.drawText(image.rect(), Qt::AlignCenter,
                QString("Failed to load:\n%1").arg(filename));
        }
    }
    else
    {
        // File doesn't exist - create simple placeholder
        image = QImage(width, height, QImage::Format_RGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        painter.setPen(QColor(160, 160, 160)); // Gray text
        QFont font = painter.font();
        font.setPointSize(14);
        painter.setFont(font);
        painter.drawText(image.rect(), Qt::AlignCenter, "Screenshot");
    }

    // Convert image to base64 data URL for embedding in HTML
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    QString base64Image = QString::fromLatin1(byteArray.toBase64());

    return QString(
        "<div style='text-align:center; margin: 20px 0;'>"
        "<img src='data:image/png;base64,%1' alt='%2' "
        "style='max-width:100%%; width:auto; border:1px solid #ddd; border-radius:4px;'/>"
        "<div style='font-style:italic; color:#666; margin-top:3px; font-size:9pt;'>%4</div>"
        "</div>"
    ).arg(base64Image, caption).arg(caption);
}

QString TutorialDialog::createNote(const QString& noteType, const QString& content)
{
    QString noteClass = "note note-" + noteType;
    return QString("<div class='%1'>%2</div>").arg(noteClass, content);
}

QString TutorialDialog::createKeyboardKey(const QString& key)
{
    return QString("<kbd>%1</kbd>").arg(key);
}

QString TutorialDialog::createTable(const QStringList& headers, const QList<QStringList>& rows)
{
    QString table = "<table>";

    // Headers
    table += "<tr>";
    for (const QString& header : headers)
    {
        table += QString("<th>%1</th>").arg(header);
    }
    table += "</tr>";

    // Rows
    for (const QStringList& row : rows)
    {
        table += "<tr>";
        for (const QString& cell : row)
        {
            table += QString("<td>%1</td>").arg(cell);
        }
        table += "</tr>";
    }

    table += "</table>";
    return table;
}
