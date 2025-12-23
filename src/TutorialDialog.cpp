#include "TutorialDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QApplication>
#include <QScreen>
#include <QScrollBar>

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
        "    border: 1px solid #ccc;"
        "    border-radius: 4px;"
        "    background-color: #f8f9fa;"
        "}"
        "QListWidget::item {"
        "    padding: 10px;"
        "    border-bottom: 1px solid #e0e0e0;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: #3498db;"
        "    color: white;"
        "}"
        "QListWidget::item:hover {"
        "    background-color: #e8f4f8;"
        "}"
    );

    // Content browser on the right
    m_contentBrowser = new QTextBrowser();
    m_contentBrowser->setOpenExternalLinks(false);

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
        tr("1. Getting Started"),
        tr("2. Opening Models"),
        tr("3. Basic Navigation"),
        tr("4. Selecting Objects"),
        tr("5. View Modes"),
        tr("6. Camera Modes"),
        tr("7. Display Modes"),
        tr("8. Manipulating Objects"),
        tr("9. Materials & Textures"),
        tr("10. Lighting & Environment"),
        tr("11. Working with Visibility"),
        tr("12. Advanced Features"),
        tr("13. Performance Optimization"),
        tr("14. Tips & Workflows")
    };

    for (const QString& title : m_lessonTitles)
    {
        m_lessonList->addItem(title);
    }
}

void TutorialDialog::onLessonSelected(QListWidgetItem* current, QListWidgetItem* previous)
{
    Q_UNUSED(previous);

    if (!current)
        return;

    m_currentLessonIndex = m_lessonList->row(current);

    QString content;
    switch (m_currentLessonIndex)
    {
    case 0: content = createLesson1_GettingStarted(); break;
    case 1: content = createLesson2_OpeningModels(); break;
    case 2: content = createLesson3_BasicNavigation(); break;
    case 3: content = createLesson4_SelectingObjects(); break;
    case 4: content = createLesson5_ViewModes(); break;
    case 5: content = createLesson6_CameraModes(); break;
    case 6: content = createLesson7_DisplayModes(); break;
    case 7: content = createLesson8_ManipulatingObjects(); break;
    case 8: content = createLesson9_MaterialsAndTextures(); break;
    case 9: content = createLesson10_LightingAndEnvironment(); break;
    case 10: content = createLesson11_WorkingWithVisibility(); break;
    case 11: content = createLesson12_AdvancedFeatures(); break;
    case 12: content = createLesson13_PerformanceOptimization(); break;
    case 13: content = createLesson14_TipsAndTricks(); break;
    default: content = "<h1>Invalid lesson</h1>"; break;
    }

    m_contentBrowser->setHtml(content);
    m_contentBrowser->verticalScrollBar()->setValue(0); // Scroll to top
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

void TutorialDialog::showLesson(int lessonIndex)
{
    if (lessonIndex >= 0 && lessonIndex < m_lessonTitles.count())
    {
        m_lessonList->setCurrentRow(lessonIndex);
    }
}

// ============================================================================
// LESSON 1: Getting Started
// ============================================================================

QString TutorialDialog::createLesson1_GettingStarted()
{
    QString content;

    content += createSection(tr("Welcome to ModelViewer!"),
        tr("<p>ModelViewer is a powerful 3D model visualization application that supports "
            "various file formats including OBJ, STL, glTF, FBX, and CAD formats like STEP, "
            "IGES, and BREP.</p>"
            "<p>This tutorial will guide you through all the features and help you become "
            "proficient with the application.</p>"));

    content += createScreenshotPlaceholder("tutorial_01_main_window.png",
        "The ModelViewer main window showing the 3D viewport, object list, and control panels");

    content += createSection(tr("Interface Overview"),
        tr("<p>The ModelViewer interface consists of several key areas:</p>"));

    content += createStep(1, tr("Main Viewport"),
        tr("The large central area where your 3D models are displayed and manipulated. "
            "This is your primary workspace."));

    content += createStep(2, tr("Object List Panel (Left)"),
        tr("Shows all objects in your scene. Click to select objects, use checkboxes to "
            "control visibility, and right-click for context menu options."));

    content += createStep(3, tr("Properties Panel (Left)"),
        tr("Below the object list, contains tabs for Materials, Textures, Transformations, "
            "and Environment settings."));

    content += createStep(4, tr("View Toolbar (Bottom)"),
        tr("Auto-hiding toolbar at the bottom of the viewport with quick access to view "
            "manipulation, camera modes, and display settings."));

    content += createStep(5, tr("Menu Bar (Top)"),
        tr("Standard menu bar with File, Edit, View, Window, and Help menus for accessing "
            "all application features."));

    content += createScreenshotPlaceholder("tutorial_01_interface_labeled.png",
        "Interface areas labeled: Viewport, Object List, Properties, Toolbar, Menu Bar",
        800, 500);

    content += createNote("tip",
        tr("The view toolbar automatically hides after 2 seconds. Move your mouse to the "
            "bottom edge of the viewport to reveal it again."));

    content += createSection(tr("What You'll Learn"),
        tr("<p>Throughout this tutorial, you will learn:</p>"
            "<ul>"
            "<li>How to open and import 3D models</li>"
            "<li>Navigate the 3D viewport using mouse and keyboard</li>"
            "<li>Select and manipulate objects</li>"
            "<li>Use different camera and display modes</li>"
            "<li>Apply materials and textures</li>"
            "<li>Configure lighting and environment</li>"
            "<li>Work with visibility and organization</li>"
            "<li>Use advanced features like clipping planes</li>"
            "<li>Optimize performance for large models</li>"
            "</ul>"));

    content += createNote("info",
        tr("You can navigate between lessons using the list on the left, or use the "
            "Previous/Next buttons at the bottom. Feel free to jump to any lesson that "
            "interests you!"));

    return createStyledHtml(tr("Lesson 1: Getting Started"), content);
}

// ============================================================================
// LESSON 2: Opening Models
// ============================================================================

QString TutorialDialog::createLesson2_OpeningModels()
{
    QString content;

    content += createSection(tr("Introduction"),
        tr("<p>ModelViewer supports a wide variety of 3D file formats. Let's learn how to "
            "open and import models into the application.</p>"));

    content += createSection(tr("Supported File Formats"),
        tr("<p><b>Mesh Formats:</b> OBJ, STL, PLY, glTF (.gltf, .glb), FBX, 3DS, DAE (Collada), "
            "Blender (.blend), and many more via Assimp library</p>"
            "<p><b>CAD Formats:</b> STEP (.stp, .step), IGES (.igs, .iges), BREP (.brep) "
            "via OpenCASCADE library</p>"
            "<p><b>Native Format:</b> MVF (.mvf) - ModelViewer's own format for saving complete scenes</p>"));

    content += createSection(tr("Method 1: Using the Menu"),
        tr("<p>The standard way to open files:</p>"));

    content += createStep(1, tr("Click File → Open"),
        tr("Navigate to the File menu in the menu bar and click Open, or press ") +
        createKeyboardKey("Ctrl+O"));

    content += createScreenshotPlaceholder("tutorial_02_file_menu.png",
        "File menu showing Open option highlighted", 300, 250);

    content += createStep(2, tr("Select Your File"),
        tr("A file dialog will appear. Navigate to your 3D model file and select it."));

    content += createScreenshotPlaceholder("tutorial_02_file_dialog.png",
        "File open dialog showing various 3D file formats", 600, 400);

    content += createStep(3, tr("Wait for Loading"),
        tr("The model will load with a progress bar showing the status. For large files, "
            "this may take a moment."));

    content += createScreenshotPlaceholder("tutorial_02_loading_progress.png",
        "Progress bar showing model loading", 500, 150);

    content += createNote("tip",
        tr("You can cancel the loading process by clicking the 'Cancel Loading' button "
            "in the status bar."));

    content += createSection(tr("Method 2: Drag and Drop"),
        tr("<p>The fastest way to open files:</p>"));

    content += createStep(1, tr("Locate Your File"),
        tr("Find the 3D model file in your file explorer (Windows Explorer, Finder, Nautilus, etc.)"));

    content += createStep(2, tr("Drag to Window"),
        tr("Simply drag the file and drop it anywhere in the ModelViewer window."));

    content += createScreenshotPlaceholder("tutorial_02_drag_drop.png",
        "Demonstrating drag and drop of a 3D file onto the ModelViewer window", 600, 400);

    content += createNote("info",
        tr("You can drag and drop multiple files at once. Each will open in a new session."));

    content += createSection(tr("Method 3: Recent Files"),
        tr("<p>Quickly reopen files you've worked with recently:</p>"));

    content += createStep(1, tr("Access Recent Menu"),
        tr("Go to File → Recent... to see a list of recently opened files."));

    content += createStep(2, tr("Select File"),
        tr("Click on any file in the list to open it immediately."));

    content += createScreenshotPlaceholder("tutorial_02_recent_files.png",
        "Recent files menu showing list of previously opened models", 400, 300);

    content += createSection(tr("Importing Additional Models"),
        tr("<p>You can add models to an existing scene:</p>"));

    content += createStep(1, tr("Use File → Import"),
        tr("Click File → Import or press ") + createKeyboardKey("Ctrl+Shift+I"));

    content += createStep(2, tr("Select Models"),
        tr("Choose one or more files to add to your current scene."));

    content += createStep(3, tr("Models Are Combined"),
        tr("The new models will be added to your object list and appear in the viewport."));

    content += createScreenshotPlaceholder("tutorial_02_multiple_models.png",
        "Viewport showing multiple imported models with object list displaying all items", 700, 450);

    content += createNote("tip",
        tr("Use Import when you want to work with multiple models in the same scene. "
            "Use Open when you want to start fresh with a new model."));

    content += createSection(tr("Progressive Loading"),
        tr("<p>For large files, ModelViewer uses progressive loading:</p>"
            "<ul>"
            "<li>Objects appear as they are loaded</li>"
            "<li>You can interact with objects that have already loaded</li>"
            "<li>The progress bar shows overall completion</li>"
            "<li>You can cancel at any time</li>"
            "</ul>"));

    content += createNote("info",
        tr("After opening a file, the viewport automatically frames the model using 'Fit All' "
            "so you can see it completely. Press ") + createKeyboardKey("F") +
        tr(" at any time to fit all objects in view."));

    return createStyledHtml(tr("Lesson 2: Opening Models"), content);
}

// ============================================================================
// LESSON 3: Basic Navigation
// ============================================================================

QString TutorialDialog::createLesson3_BasicNavigation()
{
    QString content;

    content += createSection(tr("Introduction"),
        tr("<p>Navigating the 3D viewport is essential for examining your models from all angles. "
            "ModelViewer provides intuitive mouse and keyboard controls for smooth navigation.</p>"));

    content += createSection(tr("Mouse Navigation"),
        tr("<p>The primary way to navigate the viewport:</p>"));

    content += createStep(1, tr("Rotating the View"),
        tr("Hold the ") + createKeyboardKey("Middle Mouse Button") +
        tr(" and drag to rotate the camera around your model.<br/><b>Alternative:</b> Hold ") +
        createKeyboardKey("Ctrl") + tr(" + ") + createKeyboardKey("Left Mouse Button") +
        tr(" and drag."));

    content += createScreenshotPlaceholder("tutorial_03_rotate_gesture.png",
        "Illustration showing middle mouse button rotation gesture with arrow indicators", 400, 300);

    content += createNote("tip",
        tr("The rotation uses inertial scrolling - if you move quickly and release, "
            "the view will continue to rotate smoothly and slow down gradually."));

    content += createStep(2, tr("Panning the View"),
        tr("Hold the ") + createKeyboardKey("Right Mouse Button") +
        tr(" and drag to move the camera side-to-side and up-down.<br/><b>Alternative:</b> Hold ") +
        createKeyboardKey("Ctrl") + tr(" + ") + createKeyboardKey("Right Mouse Button") +
        tr(" and drag."));

    content += createScreenshotPlaceholder("tutorial_03_pan_gesture.png",
        "Illustration showing right mouse button panning gesture", 400, 300);

    content += createStep(3, tr("Zooming the View"),
        tr("Use the ") + createKeyboardKey("Mouse Wheel") +
        tr(" to zoom in and out.<br/><b>Alternative:</b> Hold ") +
        createKeyboardKey("Ctrl") + tr(" + ") + createKeyboardKey("Middle Mouse Button") +
        tr(" and drag vertically."));

    content += createScreenshotPlaceholder("tutorial_03_zoom_gesture.png",
        "Illustration showing mouse wheel zoom", 400, 300);

    content += createNote("info",
        tr("The zoom is centered on your mouse position, so point at what you want to "
            "zoom towards before scrolling."));

    content += createStep(4, tr("Quick Center Pan"),
        tr("Click the ") + createKeyboardKey("Middle Mouse Button") +
        tr(" (press and release, don't drag) to pan the view to center on a new point."));

    content += createSection(tr("Keyboard Navigation"),
        tr("<p>For precise control and extended navigation:</p>"));

    QStringList headers = { tr("Keys"), tr("Action") };
    QList<QStringList> rows = {
        {createKeyboardKey("W") + "/" + createKeyboardKey("A") + "/" +
         createKeyboardKey("S") + "/" + createKeyboardKey("D"),
         tr("Move camera (behavior depends on camera mode)")},
        {createKeyboardKey("I") + "/" + createKeyboardKey("K"),
         tr("Rotate around X-axis (pitch up/down)")},
        {createKeyboardKey("J") + "/" + createKeyboardKey("L"),
         tr("Rotate around Y-axis (yaw left/right)")},
        {createKeyboardKey("M") + "/" + createKeyboardKey("N"),
         tr("Rotate around Z-axis (roll clockwise/counter-clockwise)")},
        {createKeyboardKey("X") + "/" + createKeyboardKey("Z"),
         tr("Zoom in/out (Orbit mode only)")},
        {createKeyboardKey("F"),
         tr("Fit all objects in view")}
    };
    content += createTable(headers, rows);

    content += createNote("tip",
        tr("Hold the navigation keys continuously for smooth, continuous movement."));

    content += createSection(tr("Using the View Toolbar"),
        tr("<p>The toolbar provides an alternative way to activate navigation modes:</p>"));

    content += createStep(1, tr("Reveal the Toolbar"),
        tr("Move your mouse to the bottom edge of the viewport to reveal the toolbar."));

    content += createScreenshotPlaceholder("tutorial_03_toolbar_revealed.png",
        "View toolbar appearing at bottom of viewport", 600, 150);

    content += createStep(2, tr("Click Navigation Mode"),
        tr("Click Rotate, Pan, or Zoom button to activate that mode."));

    content += createStep(3, tr("Use Left Mouse Button"),
        tr("Once a mode is active, use the ") + createKeyboardKey("Left Mouse Button") +
        tr(" to perform that action. The cursor changes to indicate the active mode."));

    content += createScreenshotPlaceholder("tutorial_03_mode_cursors.png",
        "Different cursor icons for Rotate, Pan, and Zoom modes", 500, 200);

    content += createNote("info",
        tr("Press ") + createKeyboardKey("Esc") +
        tr(" to cancel any active navigation mode and return to normal selection mode."));

    content += createSection(tr("Fitting Objects to View"),
        tr("<p>The 'Fit All' feature is one of the most useful navigation tools:</p>"));

    content += createStep(1, tr("Press F Key"),
        tr("Simply press ") + createKeyboardKey("F") +
        tr(" to automatically frame all visible objects in the viewport."));

    content += createStep(2, tr("Or Use Toolbar"),
        tr("Click the 'Fit All' button in the view toolbar."));

    content += createScreenshotPlaceholder("tutorial_03_fit_all_before.png",
        "Before Fit All: Object partially visible and off-center", 600, 350);

    content += createScreenshotPlaceholder("tutorial_03_fit_all_after.png",
        "After Fit All: Object perfectly centered and sized to fit viewport", 600, 350);

    content += createNote("tip",
        tr("Use Fit All whenever you feel lost in the 3D space or after hiding/showing objects "
            "to reframe your view."));

    content += createSection(tr("Practice Exercise"),
        tr("<p><b>Try this exercise to master basic navigation:</b></p>"
            "<ol>"
            "<li>Open any 3D model</li>"
            "<li>Use middle mouse to rotate the view 360 degrees around the model</li>"
            "<li>Use right mouse to pan the model to different corners of the screen</li>"
            "<li>Zoom in very close to see surface details</li>"
            "<li>Press F to reset and fit all</li>"
            "<li>Try the keyboard navigation keys (W/A/S/D, I/J/K/L)</li>"
            "<li>Activate Rotate mode from toolbar and use left mouse to rotate</li>"
            "</ol>"
            "<p>Practice until these movements feel natural!</p>"));

    return createStyledHtml(tr("Lesson 3: Basic Navigation"), content);
}

// ============================================================================
// LESSON 4: Selecting Objects
// ============================================================================

QString TutorialDialog::createLesson4_SelectingObjects()
{
    QString content;

    content += createSection(tr("Introduction"),
        tr("<p>Selection is fundamental to working with models in ModelViewer. You need to "
            "select objects before you can manipulate, hide, or modify them.</p>"));

    content += createSection(tr("Single Selection"),
        tr("<p>The most basic selection method:</p>"));

    content += createStep(1, tr("Click an Object"),
        tr("Simply ") + createKeyboardKey("Left Click") +
        tr(" on any object in the 3D viewport to select it."));

    content += createStep(2, tr("Visual Feedback"),
        tr("Selected objects are highlighted with a colored outline in the viewport and "
            "their entry in the object list is also highlighted."));

    content += createScreenshotPlaceholder("tutorial_04_single_selection.png",
        "Object selected showing highlight outline and selection in object list", 700, 450);

    content += createNote("info",
        tr("Clicking on an already selected object will deselect it (toggle selection)."));

    content += createSection(tr("Multiple Selection - Rubber Band"),
        tr("<p>Select multiple objects at once:</p>"));

    content += createStep(1, tr("Click and Drag"),
        tr("") + createKeyboardKey("Left Click") +
        tr(" and hold, then drag to create a selection rectangle (rubber band)."));

    content += createStep(2, tr("Encompass Objects"),
        tr("Any object that falls within the rectangle will be selected."));

    content += createStep(3, tr("Release"),
        tr("Release the mouse button to confirm the selection."));

    content += createScreenshotPlaceholder("tutorial_04_rubber_band.png",
        "Rubber band selection rectangle being dragged across multiple objects", 700, 450);

    content += createNote("tip",
        tr("The rubber band selection is great for selecting groups of objects quickly."));

    content += createSection(tr("Selecting from Object List"),
        tr("<p>An alternative selection method using the side panel:</p>"));

    content += createStep(1, tr("Locate Object"),
        tr("Find the object in the Object List panel on the left side."));

    content += createStep(2, tr("Click Name"),
        tr("Click on the object's name to select it. The viewport will update to show "
            "the selection."));

    content += createScreenshotPlaceholder("tutorial_04_list_selection.png",
        "Object list showing selected item highlighted", 300, 400);

    content += createStep(3, tr("Multi-Select in List"),
        tr("Hold ") + createKeyboardKey("Ctrl") +
        tr(" and click multiple items to add to selection.<br/>Hold ") +
        createKeyboardKey("Shift") +
        tr(" and click to select a range of items."));

    content += createSection(tr("Search and Select"),
        tr("<p>For scenes with many objects:</p>"));

    content += createStep(1, tr("Use Search Box"),
        tr("Type in the search box above the object list to filter objects by name."));

    content += createStep(2, tr("Objects Appear/Hide"),
        tr("Only objects matching your search will be shown in the list."));

    content += createStep(3, tr("Select from Filtered List"),
        tr("Click on the objects you want to select from the filtered results."));

    content += createScreenshotPlaceholder("tutorial_04_search_filter.png",
        "Search box with text and filtered object list showing matching results", 300, 400);

    content += createNote("tip",
        tr("Clear the search box to show all objects again."));

    content += createSection(tr("Deselecting"),
        tr("<p>There are several ways to clear selection:</p>"));

    QStringList headers = { tr("Method"), tr("Result") };
    QList<QStringList> rows = {
        {tr("Click on selected object"), tr("Toggles that object off")},
        {tr("Click empty space in viewport"), tr("Deselects all objects")},
        {tr("Press Esc"), tr("Deselects all objects")},
        {tr("Click different object"), tr("Selects new object, deselects previous")}
    };
    content += createTable(headers, rows);

    content += createSection(tr("Selection Highlighting"),
        tr("<p>Control how selection is displayed:</p>"));

    content += createStep(1, tr("Toggle Highlighting"),
        tr("Use the 'Selection Highlight' checkbox in the left panel to turn selection "
            "highlighting on or off."));

    content += createNote("info",
        tr("Even with highlighting off, you can still see which objects are selected in "
            "the object list."));

    content += createSection(tr("Working with Selected Objects"),
        tr("<p>Once objects are selected, you can:</p>"
            "<ul>"
            "<li>Hide/Show them (") + createKeyboardKey("Space") + tr(" key)</li>"
                "<li>Delete them (") + createKeyboardKey("Delete") + tr(" key)</li>"
                    "<li>Transform them (Move, Rotate, Scale)</li>"
                    "<li>Change their materials and colors</li>"
                    "<li>Apply textures</li>"
                    "<li>Duplicate them</li>"
                    "<li>Center the view on them</li>"
                    "</ul>"));

    content += createSection(tr("Context Menu Actions"),
        tr("<p>Right-click on selected objects for quick actions:</p>"));

    content += createScreenshotPlaceholder("tutorial_04_context_menu.png",
        "Context menu showing options for selected objects", 300, 400);

    content += createSection(tr("Practice Exercise"),
        tr("<p><b>Try this exercise:</b></p>"
            "<ol>"
            "<li>Import or open a model with multiple parts</li>"
            "<li>Practice clicking individual objects to select them</li>"
            "<li>Try rubber band selection to select multiple objects at once</li>"
            "<li>Use Ctrl+Click to add objects to your selection one by one</li>"
            "<li>Select objects from the object list</li>"
            "<li>Use the search box to find specific objects by name</li>"
            "<li>Practice deselecting using different methods</li>"
            "<li>Right-click on a selection to see available actions</li>"
            "</ol>"));

    return createStyledHtml(tr("Lesson 4: Selecting Objects"), content);
}

// ============================================================================
// LESSON 5: View Modes
// ============================================================================

QString TutorialDialog::createLesson5_ViewModes()
{
    QString content;

    content += createSection(tr("Introduction"),
        tr("<p>View modes let you quickly orient your camera to standard positions, perfect "
            "for technical drawings, presentations, or examining models from specific angles.</p>"));

    content += createSection(tr("Accessing View Modes"),
        tr("<p>View modes are available from the View Toolbar:</p>"));

    content += createStep(1, tr("Show Toolbar"),
        tr("Move your mouse to the bottom edge of the viewport to reveal the toolbar."));

    content += createStep(2, tr("Find View Buttons"),
        tr("Look for buttons labeled 'Top', 'Front', 'Left', etc., and the 'Axonometric View' "
            "dropdown button."));

    content += createScreenshotPlaceholder("tutorial_05_toolbar_views.png",
        "View toolbar with orthographic and axonometric view buttons highlighted", 700, 150);

    content += createSection(tr("Orthographic Views"),
        tr("<p>Six standard orthographic views:</p>"));

    content += createStep(1, tr("Top View"),
        tr("Click 'Top' to view the model from directly above (looking down the Z-axis)."));

    content += createScreenshotPlaceholder("tutorial_05_top_view.png",
        "Model shown in top view", 600, 400);

    content += createStep(2, tr("Front View"),
        tr("Click 'Front' to view from the front (looking along the Y-axis)."));

    content += createScreenshotPlaceholder("tutorial_05_front_view.png",
        "Model shown in front view", 600, 400);

    content += createStep(3, tr("Left View"),
        tr("Click 'Left' to view from the left side (looking along the X-axis)."));

    content += createStep(4, tr("Additional Views"),
        tr("Also available: 'Bottom', 'Rear', and 'Right' views for complete coverage."));

    content += createScreenshotPlaceholder("tutorial_05_six_views.png",
        "Grid showing all six orthographic views", 800, 500);

    content += createNote("info",
        tr("Orthographic views are particularly useful for technical drawings and measurements "
            "as they show true dimensions without perspective distortion."));

    content += createSection(tr("Axonometric Views"),
        tr("<p>Three-dimensional standard views:</p>"));

    content += createStep(1, tr("Isometric View"),
        tr("Click the Axonometric button and select 'Isometric'. This shows the model "
            "at equal angles (120°) between all three axes."));

    content += createScreenshotPlaceholder("tutorial_05_isometric.png",
        "Model in isometric view showing equal axis angles", 600, 400);

    content += createStep(2, tr("Dimetric View"),
        tr("Select 'Dimetric' for a view where two axes are equally foreshortened."));

    content += createScreenshotPlaceholder("tutorial_05_dimetric.png",
        "Model in dimetric view", 600, 400);

    content += createStep(3, tr("Trimetric View"),
        tr("Select 'Trimetric' for a view where all three axes are foreshortened differently."));

    content += createScreenshotPlaceholder("tutorial_05_trimetric.png",
        "Model in trimetric view", 600, 400);

    content += createNote("tip",
        tr("Axonometric views are great for technical illustrations and engineering drawings "
            "as they show 3D structure while maintaining parallel lines."));

    content += createSection(tr("Projection Toggle"),
        tr("<p>Switch between orthographic and perspective projection:</p>"));

    content += createStep(1, tr("Find Toggle Button"),
        tr("Look for the projection toggle button in the toolbar (shows camera icon)."));

    content += createStep(2, tr("Orthographic vs Perspective"),
        tr("<b>Orthographic:</b> Parallel lines stay parallel, no vanishing point. "
            "Objects same size regardless of distance.<br/>"
            "<b>Perspective:</b> Realistic view with vanishing points. Distant objects appear smaller."));

    content += createScreenshotPlaceholder("tutorial_05_ortho_vs_persp.png",
        "Side-by-side comparison: Orthographic (left) and Perspective (right) projection", 800, 400);

    content += createNote("tip",
        tr("Use Orthographic for technical work and measurements. "
            "Use Perspective for realistic presentations and visualization."));

    content += createSection(tr("Multi-View Mode"),
        tr("<p>See multiple views simultaneously:</p>"));

    content += createStep(1, tr("Enable Multi-View"),
        tr("Click the 'Multi-View' toggle button in the toolbar."));

    content += createStep(2, tr("Four Viewports"),
        tr("The viewport splits into four: Top, Front, Right, and Isometric views."));

    content += createStep(3, tr("Independent Navigation"),
        tr("You can navigate independently in each viewport. Selection is synchronized "
            "across all views."));

    content += createScreenshotPlaceholder("tutorial_05_multiview.png",
        "Four-viewport layout showing Top, Front, Right, and Isometric views", 800, 600);

    content += createNote("info",
        tr("Multi-view mode is excellent for precision modeling work and understanding "
            "complex geometries."));

    content += createSection(tr("View Animation"),
        tr("<p>When switching views:</p>"
            "<ul>"
            "<li>The camera smoothly animates to the new position</li>"
            "<li>This helps you maintain orientation and context</li>"
            "<li>You can interrupt the animation by interacting with the view</li>"
            "</ul>"));

    content += createSection(tr("Use Cases"),
        tr("<p><b>Top/Front/Side Views:</b> Checking alignment, dimensions, symmetry</p>"
            "<p><b>Isometric View:</b> General 3D overview, presentations, default starting view</p>"
            "<p><b>Dimetric/Trimetric:</b> Technical illustrations, engineering drawings</p>"
            "<p><b>Multi-View:</b> Precision modeling, CAD work, complex assemblies</p>"));

    content += createSection(tr("Practice Exercise"),
        tr("<p><b>Try this exercise:</b></p>"
            "<ol>"
            "<li>Open a model (preferably mechanical or architectural)</li>"
            "<li>Cycle through all six orthographic views (Top, Front, Left, Bottom, Rear, Right)</li>"
            "<li>Switch to Isometric view</li>"
            "<li>Try Dimetric and Trimetric views</li>"
            "<li>Toggle between Orthographic and Perspective projection</li>"
            "<li>Enable Multi-View mode and practice navigating in each viewport</li>"
            "<li>Return to single viewport mode</li>"
            "<li>Notice how each view mode is useful for different purposes</li>"
            "</ol>"));

    return createStyledHtml(tr("Lesson 5: View Modes"), content);
}

// ============================================================================
// LESSON 6: Camera Modes
// ============================================================================

QString TutorialDialog::createLesson6_CameraModes()
{
    QString content;

    content += createSection(tr("Introduction"),
        tr("<p>Camera modes change how you navigate through 3D space. ModelViewer offers three "
            "distinct camera modes, each designed for different purposes.</p>"));

    content += createSection(tr("Switching Camera Modes"),
        tr("<p>There are two ways to switch camera modes:</p>"));

    content += createStep(1, tr("Using Keyboard"),
        tr("Press ") + createKeyboardKey("1") + tr(" for Orbit, ") +
        createKeyboardKey("2") + tr(" for Fly, or ") +
        createKeyboardKey("3") + tr(" for First Person mode."));

    content += createStep(2, tr("Using Toolbar"),
        tr("Click the 'Camera Modes' button in the view toolbar and select from the menu."));

    content += createScreenshotPlaceholder("tutorial_06_camera_menu.png",
        "Camera modes dropdown menu showing Orbit, Fly, and First Person options", 300, 200);

    content += createNote("info",
        tr("The toolbar button updates its icon to show which camera mode is currently active."));

    content += createSection(tr("Orbit Mode (Key: 1)"),
        tr("<p><b>Best for:</b> Examining objects, CAD work, general model inspection</p>"
            "<p><b>How it works:</b> The camera orbits around a center point (usually the model), "
            "keeping the model in view at all times.</p>"));

    content += createStep(1, tr("Mouse Controls"),
        tr("• <b>Middle Mouse Drag:</b> Rotate around the model<br/>"
            "• <b>Right Mouse Drag:</b> Pan the view<br/>"
            "• <b>Mouse Wheel:</b> Zoom in/out"));

    content += createStep(2, tr("Keyboard Controls"),
        tr("• ") + createKeyboardKey("W/S") + tr(": Pan up/down<br/>• ") +
        createKeyboardKey("A/D") + tr(": Pan left/right<br/>• ") +
        createKeyboardKey("X/Z") + tr(": Zoom in/out<br/>• ") +
        createKeyboardKey("I/K") + tr(": Rotate pitch (up/down)<br/>• ") +
        createKeyboardKey("J/L") + tr(": Rotate yaw (left/right)<br/>• ") +
        createKeyboardKey("M/N") + tr(": Rotate roll (clockwise/counter-clockwise)"));

    content += createScreenshotPlaceholder("tutorial_06_orbit_mode.png",
        "Orbit mode showing camera rotating around a centered model with rotation arc", 600, 400);

    content += createNote("tip",
        tr("Orbit mode is the default and most commonly used mode. It's perfect for "
            "examining objects from all angles while keeping them centered."));

    content += createSection(tr("Fly Mode (Key: 2)"),
        tr("<p><b>Best for:</b> Exploring large scenes, architectural walkthroughs, terrain navigation</p>"
            "<p><b>How it works:</b> You control a free-floating camera that can move in any "
            "direction through 3D space, like flying.</p>"));

    content += createStep(1, tr("Mouse Controls"),
        tr("• <b>Mouse Movement:</b> Look around (changes view direction)<br/>"
            "• <b>Drag:</b> Rotate camera (pitch limited to ±89°)"));

    content += createStep(2, tr("Keyboard Controls"),
        tr("• ") + createKeyboardKey("W") + tr(": Move forward in viewing direction<br/>• ") +
        createKeyboardKey("S") + tr(": Move backward<br/>• ") +
        createKeyboardKey("A") + tr(": Strafe left<br/>• ") +
        createKeyboardKey("D") + tr(": Strafe right<br/>• ") +
        createKeyboardKey("Q") + tr(": Move down (decrease altitude)<br/>• ") +
        createKeyboardKey("E") + tr(": Move up (increase altitude)"));

    content += createScreenshotPlaceholder("tutorial_06_fly_mode.png",
        "Fly mode showing camera path through a scene with direction indicators", 600, 400);

    content += createNote("tip",
        tr("Hold the movement keys continuously for smooth flying. The Q/E keys allow "
            "vertical movement independent of viewing direction."));

    content += createSection(tr("First Person Mode (Key: 3)"),
        tr("<p><b>Best for:</b> Ground-level navigation, walking simulations, architectural interiors</p>"
            "<p><b>How it works:</b> Similar to Fly mode but with constraints that simulate "
            "walking on the ground, like a first-person video game.</p>"));

    content += createStep(1, tr("Mouse Controls"),
        tr("• <b>Mouse Movement:</b> Look around (pitch limited to ±60° for more natural movement)"));

    content += createStep(2, tr("Keyboard Controls"),
        tr("• ") + createKeyboardKey("W") + tr(": Walk forward<br/>• ") +
        createKeyboardKey("S") + tr(": Walk backward<br/>• ") +
        createKeyboardKey("A") + tr(": Strafe left<br/>• ") +
        createKeyboardKey("D") + tr(": Strafe right<br/>"
            "<br/><i>Note: No Q/E vertical movement - you stay at ground level</i>"));

    content += createScreenshotPlaceholder("tutorial_06_firstperson_mode.png",
        "First person mode showing ground-level view with horizon line", 600, 400);

    content += createNote("info",
        tr("The tighter pitch constraint (±60°) in First Person mode prevents the "
            "disorientation that can occur from looking too far up or down while moving."));

    content += createSection(tr("Comparison Chart"),
        tr("<p>Quick reference for choosing the right mode:</p>"));

    QStringList headers = { tr("Feature"), tr("Orbit"), tr("Fly"), tr("First Person") };
    QList<QStringList> rows = {
        {tr("Best For"), tr("Object inspection"), tr("Scene exploration"), tr("Ground navigation")},
        {tr("Model Stays Centered"), tr("Yes"), tr("No"), tr("No")},
        {tr("Up Direction"), tr("Fixed"), tr("Free"), tr("Fixed")},
        {tr("Vertical Movement"), tr("Pan only"), tr("Q/E keys"), tr("None")},
        {tr("Pitch Limit"), tr("No limit"), tr("±89°"), tr("±60°")},
        {tr("W/A/S/D Behavior"), tr("Pan view"), tr("Fly direction"), tr("Walk direction")},
        {tr("Use Case"), tr("CAD, modeling"), tr("Architecture, large scenes"), tr("Interiors, walkthrough")}
    };
    content += createTable(headers, rows);

    content += createSection(tr("Tips for Each Mode"),
        tr("<p><b>Orbit Mode Tips:</b></p>"
            "<ul>"
            "<li>Use 'Fit All' (F) often to recenter on your model</li>"
            "<li>Great for presentations and screenshots</li>"
            "<li>Best with orthographic projection for technical work</li>"
            "</ul>"
            "<p><b>Fly Mode Tips:</b></p>"
            "<ul>"
            "<li>Use with perspective projection for realistic feel</li>"
            "<li>Combine mouse look with WASD for smooth navigation</li>"
            "<li>Q/E keys let you rise above or descend below the model</li>"
            "</ul>"
            "<p><b>First Person Mode Tips:</b></p>"
            "<ul>"
            "<li>Best for architectural visualizations</li>"
            "<li>Keeps you at a consistent height (eye level)</li>"
            "<li>More intuitive for users familiar with first-person games</li>"
            "</ul>"));

    content += createSection(tr("Practice Exercise"),
        tr("<p><b>Master all three camera modes:</b></p>"
            "<ol>"
            "<li><b>Orbit Mode:</b> Press 1, then practice rotating around a model using "
            "middle mouse. Try keyboard rotation with I/J/K/L. Press F to recenter.</li>"
            "<li><b>Fly Mode:</b> Press 2, then use WASD to fly around. Press Q to descend "
            "below the model, E to rise above. Use mouse to look in different directions.</li>"
            "<li><b>First Person:</b> Press 3, then imagine you're walking through the scene. "
            "Use WASD to move and mouse to look around. Notice the movement feels different "
            "from Fly mode.</li>"
            "<li>Switch between all three modes several times to feel the difference</li>"
            "<li>Choose your favorite for different tasks!</li>"
            "</ol>"));

    return createStyledHtml(tr("Lesson 6: Camera Modes"), content);
}

// ============================================================================
// LESSON 7: Display Modes
// ============================================================================

QString TutorialDialog::createLesson7_DisplayModes()
{
    QString content;

    content += createSection(tr("Introduction"),
        tr("<p>Display modes control how your models are rendered. Each mode serves different "
            "purposes, from photorealistic visualization to technical wireframe display.</p>"));

    content += createSection(tr("Accessing Display Modes"),
        tr("<p>Change display modes from the View Toolbar:</p>"));

    content += createStep(1, tr("Show Toolbar"),
        tr("Move mouse to bottom of viewport to reveal the toolbar."));

    content += createStep(2, tr("Click Display Modes Button"),
        tr("Find the 'Display Modes' button (shows rendering style icon) and click it."));

    content += createStep(3, tr("Select Mode"),
        tr("Choose from: Realistic, Shaded, Wireframe, or WireShaded."));

    content += createScreenshotPlaceholder("tutorial_07_display_menu.png",
        "Display modes dropdown menu showing all four options", 300, 250);

    content += createSection(tr("Realistic Mode"),
        tr("<p><b>The full-featured rendering mode with advanced graphics.</b></p>"));

    content += createStep(1, tr("Features"),
        tr("• <b>PBR Materials:</b> Physically Based Rendering with metallic/roughness workflow<br/>"
            "• <b>Textures:</b> Full support for all texture maps (albedo, normal, roughness, metallic, AO, etc.)<br/>"
            "• <b>Lighting:</b> Punctual lights (point, directional, spot) and Image-Based Lighting (IBL)<br/>"
            "• <b>Shadows:</b> Real-time shadow mapping with adjustable quality<br/>"
            "• <b>Reflections:</b> Environment reflections on metallic surfaces<br/>"
            "• <b>Advanced Materials:</b> Transmission, clearcoat, sheen, iridescence, anisotropy<br/>"
            "• <b>HDR:</b> High dynamic range rendering with tone mapping"));

    content += createScreenshotPlaceholder("tutorial_07_realistic_mode.png",
        "Model rendered in Realistic mode showing PBR materials, shadows, and reflections", 700, 500);

    content += createNote("tip",
        tr("Use Realistic mode for final presentations, client previews, and when you need "
            "to evaluate how materials and lighting look together."));

    content += createStep(2, tr("Performance Note"),
        tr("Realistic mode is the most computationally intensive. For large or complex scenes, "
            "you may notice slower interaction. Switch to a simpler mode while modeling, then "
            "use Realistic for final viewing."));

    content += createSection(tr("Shaded Mode"),
        tr("<p><b>Solid colored surfaces with basic lighting.</b></p>"));

    content += createStep(1, tr("Features"),
        tr("• <b>Simple Lighting:</b> Ambient-Diffuse-Specular (ADS) lighting model<br/>"
            "• <b>Solid Colors:</b> Objects shown in their assigned colors<br/>"
            "• <b>Fast Rendering:</b> Much faster than Realistic mode<br/>"
            "• <b>Clear Shapes:</b> Good for understanding form and structure"));

    content += createScreenshotPlaceholder("tutorial_07_shaded_mode.png",
        "Same model in Shaded mode with simple lighting and solid colors", 700, 500);

    content += createNote("tip",
        tr("Shaded mode is ideal for modeling work, quickly checking geometry, and when "
            "you need smooth interaction with complex scenes."));

    content += createSection(tr("Wireframe Mode"),
        tr("<p><b>Shows only the edges of polygons.</b></p>"));

    content += createStep(1, tr("Features"),
        tr("• <b>Edges Only:</b> No filled surfaces, only lines<br/>"
            "• <b>Topology Visible:</b> See the underlying polygon structure<br/>"
            "• <b>See Through:</b> Can see all edges including those behind<br/>"
            "• <b>Fastest Rendering:</b> Minimal computational load"));

    content += createScreenshotPlaceholder("tutorial_07_wireframe_mode.png",
        "Model in Wireframe mode showing all edges as lines", 700, 500);

    content += createNote("tip",
        tr("Use Wireframe mode to inspect topology, check polygon count, find modeling "
            "issues, or create technical line drawings."));

    content += createSection(tr("WireShaded Mode"),
        tr("<p><b>Combines shaded surfaces with visible wireframe edges.</b></p>"));

    content += createStep(1, tr("Features"),
        tr("• <b>Best of Both:</b> See surface form AND polygon structure<br/>"
            "• <b>Shaded Surfaces:</b> Solid colored faces with lighting<br/>"
            "• <b>Edge Lines:</b> Wireframe overlay shows all edges<br/>"
            "• <b>Good Balance:</b> More informative than Shaded, clearer than Wireframe"));

    content += createScreenshotPlaceholder("tutorial_07_wireshaded_mode.png",
        "Model in WireShaded mode showing both surfaces and edges", 700, 500);

    content += createNote("tip",
        tr("WireShaded is excellent for modeling work where you need to see both the "
            "surface quality and the edge flow."));

    content += createSection(tr("Comparison"),
        tr("<p>Side-by-side comparison of all modes:</p>"));

    content += createScreenshotPlaceholder("tutorial_07_all_modes.png",
        "Four panels showing the same model in all four display modes", 800, 600);

    content += createSection(tr("When to Use Each Mode"),
        tr("<p><b>Realistic:</b></p>"
            "<ul><li>Final visualization</li><li>Material evaluation</li>"
            "<li>Client presentations</li><li>Marketing renders</li></ul>"
            "<p><b>Shaded:</b></p>"
            "<ul><li>General modeling work</li><li>Form evaluation</li>"
            "<li>Quick previews</li><li>Large assemblies</li></ul>"
            "<p><b>Wireframe:</b></p>"
            "<ul><li>Topology inspection</li><li>Technical drawings</li>"
            "<li>Checking edge flow</li><li>Finding modeling errors</li></ul>"
            "<p><b>WireShaded:</b></p>"
            "<ul><li>Active modeling</li><li>Edge flow analysis</li>"
            "<li>Quality control</li><li>UV layout checking</li></ul>"));

    content += createSection(tr("Performance Impact"),
        tr("<p>Rendering speed from fastest to slowest:</p>"
            "<ol><li><b>Wireframe</b> - Fastest</li>"
            "<li><b>Shaded</b> - Fast</li>"
            "<li><b>WireShaded</b> - Moderate</li>"
            "<li><b>Realistic</b> - Slower (but highest quality)</li></ol>"
            "<p>For models over 50MB, the application automatically enables low-resolution "
            "preview during manipulation in any mode.</p>"));

    content += createSection(tr("Practice Exercise"),
        tr("<p><b>Experience all display modes:</b></p>"
            "<ol>"
            "<li>Open a model with interesting materials and textures</li>"
            "<li>Start in Realistic mode - observe the materials, shadows, reflections</li>"
            "<li>Switch to Shaded mode - notice how much faster it responds</li>"
            "<li>Switch to Wireframe - examine the polygon structure</li>"
            "<li>Switch to WireShaded - see how it combines surface and structure</li>"
            "<li>Rotate the model in each mode to see how it affects performance</li>"
            "<li>Try different models (simple vs complex) in each mode</li>"
            "<li>Decide which modes you prefer for different tasks</li>"
            "</ol>"));

    return createStyledHtml(tr("Lesson 7: Display Modes"), content);
}

// ============================================================================
// Remaining lessons (8-14) continue similarly...
// Due to length constraints, I'll create shortened versions
// ============================================================================

QString TutorialDialog::createLesson8_ManipulatingObjects()
{
    QString content;

    content += createSection(tr("Introduction"),
        tr("<p>Once objects are selected, you can move, rotate, and scale them to arrange "
            "your scene or make corrections.</p>"));

    content += createSection(tr("Accessing Transformations"),
        tr("<p>Right-click on selected objects and choose 'Transformations' from the context menu, "
            "or use the Transformations tab in the left panel.</p>"));

    content += createScreenshotPlaceholder("tutorial_08_transform_panel.png",
        "Transformation panel showing translate, rotate, and scale controls", 300, 500);

    content += createSection(tr("Translation (Moving)"),
        tr("<p>Move objects in X, Y, and Z directions:</p>"));

    content += createStep(1, tr("Set Values"),
        tr("Enter values in DX, DY, DZ fields for precise movement, or use spinners."));

    content += createStep(2, tr("Apply"),
        tr("Click 'Apply' to move the selected objects."));

    content += createScreenshotPlaceholder("tutorial_08_translate.png",
        "Object position before and after translation", 700, 350);

    content += createSection(tr("Rotation"),
        tr("<p>Rotate around X, Y, and Z axes (in degrees):</p>"));

    content += createStep(1, tr("Set Angles"),
        tr("Enter rotation angles in RX, RY, RZ fields."));

    content += createStep(2, tr("Apply"),
        tr("Click 'Apply' to rotate. Rotations are applied in order: X, then Y, then Z."));

    content += createScreenshotPlaceholder("tutorial_08_rotate.png",
        "Object before and after rotation", 700, 350);

    content += createSection(tr("Scale"),
        tr("<p>Change object size:</p>"));

    content += createStep(1, tr("Set Scale Factors"),
        tr("Enter scale values in SX, SY, SZ fields. Values >1 enlarge, <1 shrink."));

    content += createStep(2, tr("Uniform vs Non-Uniform"),
        tr("Use same value for all axes (e.g., 2, 2, 2) for uniform scaling. "
            "Different values create non-uniform scaling (stretching)."));

    content += createScreenshotPlaceholder("tutorial_08_scale.png",
        "Object at different scale factors", 700, 350);

    content += createSection(tr("Baking Transformations"),
        tr("<p>Permanently apply transformations to mesh vertices:</p>"));

    content += createStep(1, tr("Apply Transformations"),
        tr("Set up your desired translation, rotation, and scale."));

    content += createStep(2, tr("Click 'Bake'"),
        tr("Click 'Bake Transformations' to permanently apply them to the geometry. "
            "The transformation values will reset to defaults."));

    content += createNote("warning",
        tr("Baking is permanent! You cannot undo it. Make sure you're happy with "
            "the transformation before baking."));

    content += createSection(tr("Duplicating Objects"),
        tr("<p>Create copies of selected objects:</p>"));

    content += createStep(1, tr("Select Objects"),
        tr("Select the objects you want to duplicate."));

    content += createStep(2, tr("Right-Click → Duplicate"),
        tr("Right-click and choose 'Duplicate' from the context menu."));

    content += createStep(3, tr("Transform Duplicates"),
        tr("The duplicates are created at the same position. Use transformations to move them."));

    content += createScreenshotPlaceholder("tutorial_08_duplicate.png",
        "Original object and its duplicate positioned nearby", 700, 400);

    content += createSection(tr("Centering View on Objects"),
        tr("<p>Frame selected objects in the viewport:</p>"));

    content += createStep(1, tr("Select Objects"),
        tr("Select one or more objects."));

    content += createStep(2, tr("Center Screen"),
        tr("Right-click → 'Center Screen' to frame just the selected objects."));

    content += createNote("tip",
        tr("Use 'Center Screen' when working on a specific part of a large assembly. "
            "Use 'Fit All' (F key) to see everything again."));

    return createStyledHtml(tr("Lesson 8: Manipulating Objects"), content);
}

QString TutorialDialog::createLesson9_MaterialsAndTextures()
{
    QString content;

    content += createSection(tr("Introduction"),
        tr("<p>Materials and textures control how objects appear. ModelViewer supports both "
            "traditional ADS materials and modern PBR (Physically Based Rendering) materials.</p>"));

    content += createSection(tr("Material Editor"),
        tr("<p>Access the material editor from the left panel:</p>"));

    content += createStep(1, tr("Select Objects"),
        tr("Select one or more objects to apply materials to."));

    content += createStep(2, tr("Open Material Tab"),
        tr("Click the 'Material' tab in the left panel."));

    content += createScreenshotPlaceholder("tutorial_09_material_panel.png",
        "Material editor panel showing ADS and PBR properties", 300, 500);

    content += createSection(tr("Basic Material Properties (ADS)"),
        tr("<p>Traditional Ambient-Diffuse-Specular material model:</p>"));

    content += createStep(1, tr("Diffuse Color"),
        tr("The base color of the object. Click the color box to open color picker."));

    content += createStep(2, tr("Ambient Color"),
        tr("Color in shadowed areas. Usually same as diffuse but darker."));

    content += createStep(3, tr("Specular Color"),
        tr("Color of highlights. Usually white or light gray."));

    content += createStep(4, tr("Shininess"),
        tr("Controls specular highlight size. Higher = smaller, sharper highlights."));

    content += createScreenshotPlaceholder("tutorial_09_ads_materials.png",
        "Objects with different ADS material properties", 700, 400);

    content += createSection(tr("PBR Material Properties"),
        tr("<p>Modern physically-based materials for realistic rendering:</p>"));

    content += createStep(1, tr("Base Color/Albedo"),
        tr("The surface color without lighting effects."));

    content += createStep(2, tr("Metallic"),
        tr("0.0 = non-metal (dielectric), 1.0 = pure metal. Controls how light reflects."));

    content += createStep(3, tr("Roughness"),
        tr("0.0 = perfectly smooth mirror, 1.0 = completely rough/matte."));

    content += createStep(4, tr("Ambient Occlusion (AO)"),
        tr("Simulates shadows in crevices and corners. Usually from a texture map."));

    content += createScreenshotPlaceholder("tutorial_09_pbr_materials.png",
        "Spheres showing different metallic and roughness values in a grid", 700, 500);

    content += createSection(tr("Applying Textures"),
        tr("<p>Add image-based detail to surfaces:</p>"));

    content += createStep(1, tr("Open Texture Tab"),
        tr("Click the 'Textures' tab in the left panel."));

    content += createStep(2, tr("Load Texture Images"),
        tr("Click 'Browse' buttons to select texture files (PNG, JPG, TGA, etc.) for different maps."));

    content += createStep(3, tr("Texture Types"),
        tr("<b>ADS Workflow:</b><br/>"
            "• Diffuse map - Base color<br/>"
            "• Normal map - Surface details<br/>"
            "• Specular map - Highlight variation<br/>"
            "• Height/Bump map - Surface displacement<br/>"
            "<br/><b>PBR Workflow:</b><br/>"
            "• Albedo/Base Color map<br/>"
            "• Normal map<br/>"
            "• Metallic map<br/>"
            "• Roughness map<br/>"
            "• AO map<br/>"
            "• Height map"));

    content += createScreenshotPlaceholder("tutorial_09_texture_maps.png",
        "Texture mapping panel showing different texture slots", 300, 500);

    content += createStep(4, tr("Apply Textures"),
        tr("Click 'Apply' to apply loaded textures to selected objects."));

    content += createSection(tr("Generating UV Coordinates"),
        tr("<p>Some models don't have UV coordinates (required for textures):</p>"));

    content += createStep(1, tr("Select Objects"),
        tr("Select objects that need UV coordinates."));

    content += createStep(2, tr("Generate UVs"),
        tr("Right-click → 'Generate UVs' to automatically create texture coordinates."));

    content += createNote("info",
        tr("UV generation uses various algorithms. Results vary depending on geometry. "
            "You may need to use external 3D software for complex UV unwrapping."));

    content += createScreenshotPlaceholder("tutorial_09_uv_generation.png",
        "Object before and after UV generation with texture applied", 700, 350);

    content += createSection(tr("Material Presets"),
        tr("<p>Quick starting points:</p>"
            "<ul>"
            "<li><b>Plastic:</b> Low metallic (0.0), medium roughness (0.4-0.6)</li>"
            "<li><b>Metal:</b> High metallic (0.9-1.0), low roughness (0.1-0.3)</li>"
            "<li><b>Wood:</b> Low metallic (0.0), high roughness (0.7-0.9)</li>"
            "<li><b>Glass:</b> Low metallic, very low roughness, enable transmission</li>"
            "</ul>"));

    return createStyledHtml(tr("Lesson 9: Materials & Textures"), content);
}

QString TutorialDialog::createLesson10_LightingAndEnvironment()
{
    QString content;

    content += createSection(tr("Introduction"),
        tr("<p>Proper lighting is essential for realistic visualization. ModelViewer offers "
            "multiple lighting options including default lights, punctual lights, and "
            "Image-Based Lighting (IBL).</p>"));

    content += createSection(tr("Environment Panel"),
        tr("<p>Access lighting controls:</p>"));

    content += createStep(1, tr("Open Environment Tab"),
        tr("Click the 'Environment' tab in the left panel."));

    content += createScreenshotPlaceholder("tutorial_10_environment_panel.png",
        "Environment panel showing lighting and environment settings", 300, 500);

    content += createSection(tr("Default Lights"),
        tr("<p>Simple three-point lighting setup:</p>"));

    content += createStep(1, tr("Enable Default Lights"),
        tr("Check 'Use Default Lights' checkbox."));

    content += createStep(2, tr("Adjust Light Position"),
        tr("Use the sliders to position the light source in X, Y, and Z."));

    content += createStep(3, tr("Light Properties"),
        tr("The default light includes ambient, diffuse, and specular components that "
            "automatically illuminate your scene."));

    content += createScreenshotPlaceholder("tutorial_10_default_lights.png",
        "Object lit with default lights showing clear form definition", 700, 450);

    content += createSection(tr("Punctual Lights (glTF Lights)"),
        tr("<p>Scene lights from glTF models:</p>"));

    content += createStep(1, tr("Enable Punctual Lights"),
        tr("Check 'Use Punctual Lights' when working with glTF models that include lights."));

    content += createStep(2, tr("Light Types"),
        tr("• <b>Point lights:</b> Emit in all directions from a point<br/>"
            "• <b>Spot lights:</b> Directional cone of light<br/>"
            "• <b>Directional lights:</b> Parallel rays like sunlight"));

    content += createStep(3, tr("Show Lights"),
        tr("Check 'Show Lights' to display light position indicators in the viewport."));

    content += createScreenshotPlaceholder("tutorial_10_punctual_lights.png",
        "Scene with multiple punctual lights and their position indicators", 700, 450);

    content += createSection(tr("Image-Based Lighting (IBL)"),
        tr("<p>Realistic environmental lighting from HDRI images:</p>"));

    content += createStep(1, tr("Enable IBL"),
        tr("Check 'Use IBL' checkbox."));

    content += createStep(2, tr("Select Environment Map"),
        tr("Choose an HDRI image from the dropdown menu, or load a custom one."));

    content += createStep(3, tr("Adjust Exposure"),
        tr("Use 'IBL Exposure' slider to control the intensity of environmental lighting."));

    content += createScreenshotPlaceholder("tutorial_10_ibl_lighting.png",
        "Metallic objects showing reflections from IBL environment", 700, 450);

    content += createNote("tip",
        tr("IBL provides both lighting AND reflections. It's essential for realistic "
            "metallic and glass materials."));

    content += createSection(tr("SkyBox"),
        tr("<p>Visual environment background:</p>"));

    content += createStep(1, tr("Enable SkyBox"),
        tr("Check 'Enable SkyBox' in the Environment panel."));

    content += createStep(2, tr("Select Map"),
        tr("Choose from preset HDR environments or load custom cubemap."));

    content += createStep(3, tr("Blur Option"),
        tr("Check 'Blur SkyBox' for a softer, less distracting background."));

    content += createStep(4, tr("FOV Adjustment"),
        tr("Adjust 'SkyBox FOV' to control how much of the environment is visible."));

    content += createScreenshotPlaceholder("tutorial_10_skybox.png",
        "Scene with visible SkyBox environment in background", 700, 450);

    content += createSection(tr("Shadows"),
        tr("<p>Add depth with shadow mapping:</p>"));

    content += createStep(1, tr("Enable Shadows"),
        tr("Check 'Enable Shadows' in the Environment panel."));

    content += createStep(2, tr("Adjust Quality"),
        tr("Use 'Shadow Quality' dropdown: Low, Medium, High, or Ultra. Higher quality "
            "creates softer, more realistic shadows but is slower."));

    content += createStep(3, tr("Self-Shadows"),
        tr("Enable 'Self-Shadows' for objects to cast shadows on themselves."));

    content += createScreenshotPlaceholder("tutorial_10_shadows.png",
        "Object with and without shadows for comparison", 700, 350);

    content += createSection(tr("Floor Plane"),
        tr("<p>Add a floor for shadows and reflections:</p>"));

    content += createStep(1, tr("Enable Floor"),
        tr("Check 'Show Floor' checkbox."));

    content += createStep(2, tr("Adjust Position"),
        tr("Use 'Floor Offset' slider to position floor relative to model."));

    content += createStep(3, tr("Texture"),
        tr("Floor can show a checkerboard texture. Adjust 'Repeat S' and 'Repeat T' "
            "to control texture tiling."));

    content += createScreenshotPlaceholder("tutorial_10_floor.png",
        "Model on floor plane with shadows and reflections", 700, 450);

    content += createSection(tr("HDR and Tone Mapping"),
        tr("<p>For realistic high dynamic range rendering:</p>"));

    content += createStep(1, tr("Enable HDR Tone Mapping"),
        tr("Check 'HDR Tone Mapping' for proper handling of bright highlights."));

    content += createStep(2, tr("Select Algorithm"),
        tr("Choose tone mapping mode: Reinhard, ACES, Filmic, etc."));

    content += createStep(3, tr("Adjust Exposure"),
        tr("Use 'Env Map Exposure' to control overall brightness."));

    content += createNote("info",
        tr("Tone mapping converts HDR values to displayable range while preserving detail "
            "in both shadows and highlights."));

    content += createSection(tr("Gamma Correction"),
        tr("<p>Ensure proper color display:</p>"));

    content += createStep(1, tr("Enable Gamma Correction"),
        tr("Check 'Gamma Correction' for accurate colors."));

    content += createStep(2, tr("Screen Gamma"),
        tr("Set 'Screen Gamma' to 2.2 (standard for most monitors)."));

    return createStyledHtml(tr("Lesson 10: Lighting & Environment"), content);
}

QString TutorialDialog::createLesson11_WorkingWithVisibility()
{
    QString content;

    content += createSection(tr("Introduction"),
        tr("<p>Managing object visibility is crucial for working with complex scenes. "
            "ModelViewer provides multiple ways to control what's shown and hidden.</p>"));

    content += createSection(tr("Basic Hide/Show"),
        tr("<p>Control individual object visibility:</p>"));

    content += createStep(1, tr("Using Checkboxes"),
        tr("In the Object List, each object has a checkbox. Check = visible, unchecked = hidden."));

    content += createScreenshotPlaceholder("tutorial_11_checkboxes.png",
        "Object list with some checkboxes checked and some unchecked", 300, 400);

    content += createStep(2, tr("Using Keyboard"),
        tr("Select objects and press ") + createKeyboardKey("Space") +
        tr(" to toggle their visibility."));

    content += createStep(3, tr("Using Context Menu"),
        tr("Right-click selected objects and choose 'Hide' or 'Show'."));

    content += createSection(tr("Show Only Selected"),
        tr("<p>Focus on specific objects:</p>"));

    content += createStep(1, tr("Select Objects"),
        tr("Select the objects you want to focus on."));

    content += createStep(2, tr("Show Only"),
        tr("Press ") + createKeyboardKey("Shift+Space") +
        tr(" or right-click → 'Show Only'."));

    content += createStep(3, tr("Result"),
        tr("All other objects are hidden, leaving only your selection visible."));

    content += createScreenshotPlaceholder("tutorial_11_show_only.png",
        "Complex assembly with just a few selected parts visible", 700, 450);

    content += createNote("tip",
        tr("Use 'Show Only' when working on specific components in large assemblies. "
            "Reduces visual clutter and improves performance."));

    content += createSection(tr("Show/Hide All"),
        tr("<p>Quickly affect all objects:</p>"));

    content += createStep(1, tr("Show All"),
        tr("Press ") + createKeyboardKey("Shift+A") +
        tr(" or right-click → 'Show All' to make everything visible."));

    content += createStep(2, tr("Hide All"),
        tr("Press ") + createKeyboardKey("Alt+A") +
        tr(" or right-click → 'Hide All' to hide everything."));

    content += createSection(tr("Swap Visible"),
        tr("<p>Invert visibility state:</p>"));

    content += createStep(1, tr("Activate Swap"),
        tr("Press ") + createKeyboardKey("Alt+S") +
        tr(" or click 'Swap Visible' button in toolbar."));

    content += createStep(2, tr("What Happens"),
        tr("Currently visible objects become hidden, and hidden objects become visible."));

    content += createStep(3, tr("Use Case"),
        tr("Perfect for inspecting internal parts of assemblies or alternating between "
            "different design options."));

    content += createScreenshotPlaceholder("tutorial_11_swap_visible.png",
        "Before and after swapping visible state", 700, 350);

    content += createNote("tip",
        tr("The 'Swap Visible' button in the toolbar shows the current state. When active, "
            "hidden objects are being displayed instead of visible ones."));

    content += createSection(tr("Visual Indicators"),
        tr("<p>How to tell what's hidden:</p>"
            "<ul>"
            "<li>Hidden objects appear grayed out in the Object List</li>"
            "<li>Hidden objects' checkboxes are unchecked</li>"
            "<li>When 'Swap Visible' is active, the role of visible/hidden is reversed</li>"
            "</ul>"));

    content += createSection(tr("Working with Hidden Objects"),
        tr("<p>Hidden objects are still in memory:</p>"
            "<ul>"
            "<li>They can be selected from the Object List</li>"
            "<li>You can apply materials, transformations, etc. to hidden objects</li>"
            "<li>They count towards memory usage</li>"
            "<li>They don't affect rendering performance (since they're not drawn)</li>"
            "<li>Use 'Delete' to actually remove objects from the scene</li>"
            "</ul>"));

    content += createSection(tr("Workflow Example"),
        tr("<p><b>Scenario:</b> You have a car model and want to inspect the engine.</p>"
            "<ol>"
            "<li>Select all body panels (hood, fenders, doors)</li>"
            "<li>Press Space to hide them</li>"
            "<li>Engine is now visible and accessible</li>"
            "<li>When done, press Shift+A to show all again</li>"
            "</ol>"
            "<p><b>Alternative approach:</b></p>"
            "<ol>"
            "<li>Select just the engine components</li>"
            "<li>Press Shift+Space to show only the engine</li>"
            "<li>Work on engine</li>"
            "<li>Press Shift+A to show everything again</li>"
            "</ol>"));

    return createStyledHtml(tr("Lesson 11: Working with Visibility"), content);
}

QString TutorialDialog::createLesson12_AdvancedFeatures()
{
    QString content;

    content += createSection(tr("Introduction"),
        tr("<p>ModelViewer includes several advanced features for specialized visualization "
            "and analysis tasks.</p>"));

    content += createSection(tr("Clipping Planes / Section View"),
        tr("<p>Cut through models to see internal structure:</p>"));

    content += createStep(1, tr("Enable Section View"),
        tr("Click the 'Section View' button in the view toolbar."));

    content += createStep(2, tr("Clipping Plane Editor"),
        tr("A control panel appears with options for XY, YZ, and ZX clipping planes."));

    content += createStep(3, tr("Activate Planes"),
        tr("Enable individual planes and use sliders to position them through the model."));

    content += createStep(4, tr("Flip Direction"),
        tr("Use 'Flip' buttons to reverse which side is cut away."));

    content += createScreenshotPlaceholder("tutorial_12_clipping_planes.png",
        "Model cut by clipping plane showing internal structure", 700, 450);

    content += createNote("tip",
        tr("Use clipping planes to inspect internal features, analyze cross-sections, "
            "or create cutaway views for presentations."));

    content += createSection(tr("Window Zoom"),
        tr("<p>Zoom precisely into a specific area:</p>"));

    content += createStep(1, tr("Activate Window Zoom"),
        tr("Click 'Window Zoom' button in toolbar or right-click → 'Zoom Area'."));

    content += createStep(2, tr("Draw Rectangle"),
        tr("Click and drag to draw a rectangle around the area you want to magnify."));

    content += createStep(3, tr("Zoom Executes"),
        tr("Release the mouse button and the view zooms to fit your selected area."));

    content += createScreenshotPlaceholder("tutorial_12_window_zoom.png",
        "Rectangle being drawn for window zoom with result", 700, 350);

    content += createSection(tr("Normal Visualization"),
        tr("<p>Display surface normals for debugging:</p>"));

    content += createStep(1, tr("Vertex Normals"),
        tr("Enable 'Show Vertex Normals' to display normal vectors at each vertex."));

    content += createStep(2, tr("Face Normals"),
        tr("Enable 'Show Face Normals' to display normal vectors for each polygon face."));

    content += createStep(3, tr("Use Case"),
        tr("Helps diagnose shading problems, reversed faces, or normal map issues."));

    content += createScreenshotPlaceholder("tutorial_12_normals.png",
        "Model with visible normal vectors shown as lines", 700, 450);

    content += createSection(tr("Multiple Sessions"),
        tr("<p>Work with multiple models simultaneously:</p>"));

    content += createStep(1, tr("Open New Session"),
        tr("File → New or ") + createKeyboardKey("Ctrl+N") +
        tr(" creates a new viewer window."));

    content += createStep(2, tr("Arrange Windows"),
        tr("Use Window menu to Tile, Cascade, or arrange custom layouts."));

    content += createStep(3, tr("Independent Views"),
        tr("Each session has its own camera, display settings, and object list."));

    content += createScreenshotPlaceholder("tutorial_12_multiple_sessions.png",
        "Multiple viewer windows tiled showing different models", 800, 500);

    content += createSection(tr("Saving Scenes"),
        tr("<p>Save your complete scene with all settings:</p>"));

    content += createStep(1, tr("Save Project"),
        tr("File → Save or ") + createKeyboardKey("Ctrl+S") +
        tr(" saves to ModelViewer's .mvf format."));

    content += createStep(2, tr("What's Saved"),
        tr("• All models and their geometry<br/>"
            "• Materials and textures<br/>"
            "• Transformations<br/>"
            "• Visibility states<br/>"
            "• Camera position<br/>"
            "• Display settings"));

    content += createStep(3, tr("Reopen Later"),
        tr("File → Open and select your .mvf file to restore the complete scene."));

    content += createSection(tr("Exporting"),
        tr("<p>Export models to different formats:</p>"));

    content += createStep(1, tr("Select Objects"),
        tr("Select the objects you want to export (or leave all selected for full export)."));

    content += createStep(2, tr("File → Export"),
        tr("Or press ") + createKeyboardKey("Ctrl+Shift+E"));

    content += createStep(3, tr("Choose Format"),
        tr("Select target format (OBJ, STL, PLY, etc.) and location."));

    content += createNote("info",
        tr("Export preserves geometry but may not export all material properties depending "
            "on the target format's capabilities."));

    content += createSection(tr("Mesh Information"),
        tr("<p>View detailed statistics about objects:</p>"));

    content += createStep(1, tr("Select Object"),
        tr("Select a single object in the list or viewport."));

    content += createStep(2, tr("Mesh Info"),
        tr("Right-click → 'Mesh Info' to see detailed statistics."));

    content += createStep(3, tr("Information Shown"),
        tr("• Vertex count<br/>"
            "• Triangle/polygon count<br/>"
            "• Bounding box dimensions<br/>"
            "• Surface area<br/>"
            "• Memory usage"));

    content += createScreenshotPlaceholder("tutorial_12_mesh_info.png",
        "Mesh information dialog showing statistics", 400, 300);

    return createStyledHtml(tr("Lesson 12: Advanced Features"), content);
}

QString TutorialDialog::createLesson13_PerformanceOptimization()
{
    QString content;

    content += createSection(tr("Introduction"),
        tr("<p>Working with large models requires optimization strategies. Here's how to "
            "maintain smooth performance even with complex scenes.</p>"));

    content += createSection(tr("Understanding Performance"),
        tr("<p>What affects rendering speed:</p>"
            "<ul>"
            "<li><b>Polygon Count:</b> More polygons = slower rendering</li>"
            "<li><b>Display Mode:</b> Realistic mode is much slower than Shaded/Wireframe</li>"
            "<li><b>Number of Objects:</b> Many objects increase overhead</li>"
            "<li><b>Texture Size:</b> Large textures use more memory and GPU bandwidth</li>"
            "<li><b>Lighting Complexity:</b> Shadows and IBL are computationally expensive</li>"
            "<li><b>Screen Resolution:</b> Higher resolution = more pixels to compute</li>"
            "</ul>"));

    content += createSection(tr("Automatic Optimizations"),
        tr("<p>ModelViewer automatically helps:</p>"));

    content += createStep(1, tr("Low-Res Preview"),
        tr("For models >50MB, a simplified version is automatically shown during rotation, "
            "panning, and zooming. Full detail returns when you stop moving."));

    content += createStep(2, tr("Progressive Loading"),
        tr("Large files load progressively, allowing you to start working before everything "
            "is fully loaded."));

    content += createStep(3, tr("View Frustum Culling"),
        tr("Objects outside the camera view are not rendered (though still in memory)."));

    content += createSection(tr("Manual Optimization Strategies"),
        tr("<p>Techniques you can use:</p>"));

    content += createStep(1, tr("Switch Display Modes"),
        tr("When working on modeling or transformations, switch to Shaded or Wireframe mode. "
            "Save Realistic mode for final review.<br/><br/>"
            "<b>Performance ranking:</b><br/>"
            "Wireframe (fastest) > Shaded > WireShaded > Realistic (slowest)"));

    content += createStep(2, tr("Hide Unnecessary Objects"),
        tr("Use visibility controls to hide objects you're not currently working on. "
            "Hidden objects are not rendered, freeing up GPU resources.<br/><br/>"
            "For assemblies, use 'Show Only' to isolate specific components."));

    content += createStep(3, tr("Reduce Shadow Quality"),
        tr("In Environment settings, lower Shadow Quality from Ultra → High → Medium → Low. "
            "Or disable shadows entirely during modeling work."));

    content += createStep(4, tr("Disable IBL/Reflections"),
        tr("Turn off Image-Based Lighting and environment reflections when not needed. "
            "Re-enable for final visualization."));

    content += createStep(5, tr("Use Orthographic Projection"),
        tr("Orthographic projection is slightly faster than perspective, especially for "
            "technical work where perspective isn't needed."));

    content += createSection(tr("Settings Optimization"),
        tr("<p>Configure application settings:</p>"));

    content += createStep(1, tr("MSAA Level"),
        tr("File → Settings → Rendering. Lower MSAA (anti-aliasing) samples if framerate is poor. "
            "Try: 8x → 4x → 2x → Off"));

    content += createStep(2, tr("Anisotropic Filtering"),
        tr("Reduce anisotropic filtering level: 16x → 8x → 4x → 2x"));

    content += createScreenshotPlaceholder("tutorial_13_settings.png",
        "Settings dialog showing rendering quality options", 500, 400);

    content += createSection(tr("Working with Large Files"),
        tr("<p>Strategies for massive models:</p>"
            "<ol>"
            "<li><b>Import Selectively:</b> If possible, import only the parts you need</li>"
            "<li><b>Use Low-Poly Versions:</b> Maintain separate low-poly versions for modeling work</li>"
            "<li><b>Work in Sections:</b> Use clipping planes to focus on one area at a time</li>"
            "<li><b>Progressive Loading:</b> Let files load completely before heavy interaction</li>"
            "<li><b>Close Unused Sessions:</b> Each viewer window uses system resources</li>"
            "</ol>"));

    content += createSection(tr("Memory Management"),
        tr("<p>Monitor and manage memory usage:</p>"));

    content += createStep(1, tr("Memory Indicators"),
        tr("Watch the status bar for memory usage warnings."));

    content += createStep(2, tr("Delete Unused Objects"),
        tr("Don't just hide objects you don't need – delete them to free memory. "
            "Hidden objects still use RAM and VRAM."));

    content += createStep(3, tr("Texture Sizes"),
        tr("Use appropriately sized textures. 4K textures are often overkill for small objects. "
            "Consider 2K or 1K textures for better performance."));

    content += createSection(tr("Performance Checklist"),
        tr("<p>If experiencing slowdown, try these in order:</p>"
            "<ol>"
            "<li>Switch to Shaded or Wireframe display mode</li>"
            "<li>Hide objects not currently needed</li>"
            "<li>Disable shadows temporarily</li>"
            "<li>Turn off IBL and environment maps</li>"
            "<li>Lower MSAA setting in Settings</li>"
            "<li>Use Orthographic projection</li>"
            "<li>Close unnecessary viewer sessions</li>"
            "<li>Reduce texture sizes</li>"
            "<li>Consider working with a lower-poly version</li>"
            "</ol>"));

    content += createNote("tip",
        tr("For extreme cases (>100M polygons), consider splitting the model into separate "
            "files that can be loaded individually or using dedicated CAD software for the "
            "heaviest work."));

    return createStyledHtml(tr("Lesson 13: Performance Optimization"), content);
}

QString TutorialDialog::createLesson14_TipsAndTricks()
{
    QString content;

    content += createSection(tr("Introduction"),
        tr("<p>This final lesson covers useful tips, shortcuts, and workflows that will make "
            "you more productive in ModelViewer.</p>"));

    content += createSection(tr("Navigation Tips"),
        tr("<ul>"
            "<li><b>Lost in Space?</b> Press ") + createKeyboardKey("F") +
        tr(" to fit all objects in view instantly</li>"
            "<li><b>Quick Pan to Point:</b> Middle-click (don't drag) to recenter view on a point</li>"
            "<li><b>Zoom to Mouse:</b> Mouse wheel zoom focuses on your cursor position</li>"
            "<li><b>Smooth Inertia:</b> Quick mouse gestures create smooth inertial motion</li>"
            "<li><b>Stop Movement:</b> Click any mouse button to stop inertial motion</li>"
            "</ul>"));

    content += createSection(tr("Selection Tricks"),
        tr("<ul>"
            "<li><b>Deselect Fast:</b> Press ") + createKeyboardKey("Esc") +
        tr(" to clear all selections</li>"
            "<li><b>Search Objects:</b> Use the search box above object list to filter by name</li>"
            "<li><b>Center on Selection:</b> Right-click → 'Center Screen' to frame selected objects</li>"
            "<li><b>Scroll to Selection:</b> Right-click → 'Center Object List' to scroll list to selected item</li>"
            "</ul>"));

    content += createSection(tr("Visibility Workflows"),
        tr("<ul>"
            "<li><b>Quick Focus:</b> Select parts → ") + createKeyboardKey("Shift+Space") +
        tr(" to isolate them</li>"
            "<li><b>Compare Alternates:</b> Hide one design → Work → ") +
        createKeyboardKey("Alt+S") + tr(" to swap → Compare</li>"
            "<li><b>Progressive Reveal:</b> Hide all → Show groups one by one to build up understanding</li>"
            "</ul>"));

    content += createSection(tr("Camera and View Tips"),
        tr("<ul>"
            "<li><b>Quick Camera Switch:</b> Press 1/2/3 to switch camera modes instantly</li>"
            "<li><b>Standard Views:</b> Use toolbar buttons for Top/Front/Left views</li>"
            "<li><b>Presentation View:</b> Isometric + Perspective + Realistic mode</li>"
            "<li><b>Technical View:</b> Front view + Orthographic + Shaded mode</li>"
            "<li><b>Analysis View:</b> Wireframe or WireShaded mode for topology</li>"
            "</ul>"));

    content += createSection(tr("Material Workflow"),
        tr("<ol>"
            "<li>Start with basic colors in Shaded mode</li>"
            "<li>Switch to Realistic mode to see PBR rendering</li>"
            "<li>Adjust metallic and roughness values</li>"
            "<li>Add textures (albedo, normal, roughness)</li>"
            "<li>Enable IBL for final lighting</li>"
            "<li>Fine-tune exposure and tone mapping</li>"
            "</ol>"));

    content += createSection(tr("Keyboard Efficiency"),
        tr("<p>Master these for speed:</p>"));

    QStringList headers = { tr("Task"), tr("Shortcut") };
    QList<QStringList> rows = {
        {tr("Fit all"), createKeyboardKey("F")},
        {tr("Hide selected"), createKeyboardKey("Space")},
        {tr("Show only"), createKeyboardKey("Shift+Space")},
        {tr("Swap visible"), createKeyboardKey("Alt+S")},
        {tr("Delete"), createKeyboardKey("Delete")},
        {tr("Deselect all"), createKeyboardKey("Esc")},
        {tr("Import"), createKeyboardKey("Ctrl+Shift+I")},
        {tr("Export"), createKeyboardKey("Ctrl+Shift+E")},
        {tr("Camera modes"), createKeyboardKey("1") + "/" + createKeyboardKey("2") + "/" + createKeyboardKey("3")}
    };
    content += createTable(headers, rows);

    content += createSection(tr("Pro Workflows"),
        tr("<p><b>Mechanical Assembly Inspection:</b></p>"
            "<ol>"
            "<li>Open assembly in Multi-View mode</li>"
            "<li>Use clipping planes to see internals</li>"
            "<li>Enable WireShaded to see part boundaries</li>"
            "<li>Use 'Show Only' to isolate subsystems</li>"
            "</ol>"
            "<p><b>Architectural Walkthrough:</b></p>"
            "<ol>"
            "<li>Switch to First Person camera mode (3)</li>"
            "<li>Enable IBL with architectural HDRI</li>"
            "<li>Turn on floor plane and shadows</li>"
            "<li>Use WASD to walk through spaces</li>"
            "</ol>"
            "<p><b>Product Rendering:</b></p>"
            "<ol>"
            "<li>Realistic display mode</li>"
            "<li>Carefully tune PBR materials</li>"
            "<li>Enable IBL with studio lighting HDRI</li>"
            "<li>Adjust exposure for proper brightness</li>"
            "<li>Use floor plane with reflections</li>"
            "<li>Frame with perspective projection</li>"
            "</ol>"));

    content += createSection(tr("Troubleshooting Quick Fixes"),
        tr("<ul>"
            "<li><b>Black screen:</b> Check if object is there (look at object list), might need lighting</li>"
            "<li><b>Flashing textures:</b> Check if UVs exist, try 'Generate UVs'</li>"
            "<li><b>Weird shading:</b> Enable 'Show Normals' to diagnose normal issues</li>"
            "<li><b>Can't select:</b> Press Esc to cancel any active view mode</li>"
            "<li><b>Model disappeared:</b> Press F to fit all</li>"
            "<li><b>Slow performance:</b> Switch to Shaded mode, hide unused objects</li>"
            "</ul>"));

    content += createSection(tr("Customization"),
        tr("<ul>"
            "<li><b>Background Color:</b> Right-click → 'Background Color' for custom viewport background</li>"
            "<li><b>Theme:</b> File → Settings to switch between Light/Dark/System theme</li>"
            "<li><b>Language:</b> File → Settings to change interface language</li>"
            "<li><b>Quality vs Performance:</b> File → Settings to adjust MSAA and anisotropic filtering</li>"
            "</ul>"));

    content += createSection(tr("File Organization"),
        tr("<ul>"
            "<li>Use recent files menu (File → Recent) for quick access</li>"
            "<li>Save working sessions as .mvf to preserve camera, visibility, etc.</li>"
            "<li>Keep texture files in same folder as models for automatic finding</li>"
            "<li>Use descriptive names in object list (double-click to rename)</li>"
            "</ul>"));

    content += createSection(tr("Learning More"),
        tr("<ul>"
            "<li><b>Help Menu:</b> Press ") + createKeyboardKey("F1") +
        tr(" or Help → Quick Help for reference</li>"
            "<li><b>Experiment:</b> Try all features with test models</li>"
            "<li><b>Settings:</b> Explore File → Settings for additional options</li>"
            "<li><b>Context Menus:</b> Right-click everywhere to discover hidden options</li>"
            "</ul>"));

    content += createSection(tr("Final Thoughts"),
        tr("<p>ModelViewer is a powerful tool with many features. The key to mastery is practice:</p>"
            "<ul>"
            "<li>Start with basic navigation and selection</li>"
            "<li>Gradually explore display modes and camera modes</li>"
            "<li>Learn visibility management for complex scenes</li>"
            "<li>Experiment with materials and lighting</li>"
            "<li>Discover your own workflows</li>"
            "</ul>"
            "<p><b>You've completed the tutorial!</b> You now have the knowledge to effectively "
            "use ModelViewer. Keep experimenting and discovering new techniques.</p>"));

    return createStyledHtml(tr("Lesson 14: Tips & Workflows"), content);
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
    return QString(
        "<div class='screenshot' style='width:%1px; height:%2px;'>"
        "<div class='screenshot-filename'>📷 %3</div>"
        "<div class='screenshot-caption'>%4</div>"
        "</div>"
    ).arg(width).arg(height).arg(filename, caption);
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
