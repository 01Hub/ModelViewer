#include "QuickHelpDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QApplication>
#include <QScreen>

QuickHelpDialog::QuickHelpDialog(QWidget* parent)
	: QDialog(parent)
{
	setupUI();
	setWindowTitle(tr("Quick Help - ModelViewer"));

	// Set dialog size to 70% of screen
	QScreen* screen = QApplication::primaryScreen();
	QRect screenGeometry = screen->geometry();
	int width = static_cast<int>(screenGeometry.width() * 0.7);
	int height = static_cast<int>(screenGeometry.height() * 0.7);
	resize(width, height);
}

void QuickHelpDialog::setupUI()
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);

	// Create tab widget
	m_tabWidget = new QTabWidget(this);

	// Create browsers for each tab
	m_mouseControlsBrowser = new QTextBrowser();
	m_keyboardBrowser = new QTextBrowser();
	m_toolbarBrowser = new QTextBrowser();
	m_menuBrowser = new QTextBrowser();
	m_cameraBrowser = new QTextBrowser();
	m_displayBrowser = new QTextBrowser();
	m_tipsBrowser = new QTextBrowser();

	// Set open external links for all browsers
	m_mouseControlsBrowser->setOpenExternalLinks(false);
	m_keyboardBrowser->setOpenExternalLinks(false);
	m_toolbarBrowser->setOpenExternalLinks(false);
	m_menuBrowser->setOpenExternalLinks(false);
	m_cameraBrowser->setOpenExternalLinks(false);
	m_displayBrowser->setOpenExternalLinks(false);
	m_tipsBrowser->setOpenExternalLinks(false);

	// Add tabs
	m_tabWidget->addTab(m_mouseControlsBrowser, tr("Mouse Controls"));
	m_tabWidget->addTab(m_keyboardBrowser, tr("Keyboard Shortcuts"));
	m_tabWidget->addTab(m_toolbarBrowser, tr("View Toolbar"));
	m_tabWidget->addTab(m_cameraBrowser, tr("Camera Modes"));
	m_tabWidget->addTab(m_displayBrowser, tr("Display Modes"));
	m_tabWidget->addTab(m_menuBrowser, tr("Menu Shortcuts"));
	m_tabWidget->addTab(m_tipsBrowser, tr("Tips & Tricks"));

	// Setup content for each tab
	setupMouseControlsTab();
	setupKeyboardShortcutsTab();
	setupViewToolbarTab();
	setupCameraModesTab();
	setupDisplayModesTab();
	setupMenuShortcutsTab();
	setupTipsAndTricksTab();

	mainLayout->addWidget(m_tabWidget);

	// Close button
	QHBoxLayout* buttonLayout = new QHBoxLayout();
	buttonLayout->addStretch();
	m_closeButton = new QPushButton(tr("Close"), this);
	m_closeButton->setMinimumWidth(100);
	connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
	buttonLayout->addWidget(m_closeButton);

	mainLayout->addLayout(buttonLayout);
}

void QuickHelpDialog::setupMouseControlsTab()
{
	QStringList headers = { tr("Action"), tr("Mouse Control"), tr("Alternative") };
	QList<QStringList> rows = {
		{tr("Rotate View"), tr("Ctrl + Left Button + Drag"), tr("Enable 'Rotate View' mode from toolbar")},
		{tr("Pan View"), tr("Ctrl + Right Button + Drag"), tr("Enable 'Pan View' mode from toolbar")},
		{tr("Zoom View"), tr("Mouse Wheel<br/>OR<br/>Ctrl + Middle Button + Drag"), tr("Enable 'Zoom View' mode from toolbar")},
		{tr("Center Pan"), tr("Middle Button Click (release at new position)"), tr("N/A")},
		{tr("Select Object"), tr("Left Button Click"), tr("N/A")},
		{tr("Multi-Select"), tr("Left Button + Drag (rubber band)"), tr("Hold Ctrl while clicking")},
		{tr("Window Zoom"), tr("Enable mode, then Left Button + Drag"), tr("Right-click menu")},
		{tr("Context Menu"), tr("Right Button Click"), tr("N/A")}
	};

	QString content = createSection(tr("View Manipulation"),
		tr("The mouse controls allow intuitive 3D view manipulation:")) +
		createTable(headers, rows);

	content += createSection(tr("Important Notes"),
		tr("<ul>"
			"<li><b>Inertia:</b> Mouse movements support inertial scrolling for smooth navigation</li>"
			"<li><b>Cursor Changes:</b> The cursor changes to indicate the active manipulation mode</li>"
			"<li><b>Mode Activation:</b> You can activate view modes from the toolbar or right-click menu, "
			"then use Left Button to perform the action</li>"
			"<li><b>Large Models:</b> For models larger than 50MB, a low-resolution preview is shown during manipulation</li>"
			"</ul>"));

	m_mouseControlsBrowser->setHtml(createStyledHtml(tr("Mouse Controls"), content));
}

void QuickHelpDialog::setupKeyboardShortcutsTab()
{
	QString content;

	// View Navigation
	QStringList navHeaders = { tr("Key"), tr("Action") };
	QList<QStringList> navRows = {
		{tr("W, A, S, D"), tr("Navigate in current camera mode:<br/>"
							 "• <b>Orbit Mode:</b> Pan view (W=up, S=down, A=left, D=right)<br/>"
							 "• <b>Fly/First Person:</b> Move forward/backward/left/right")},
		{tr("Q, E"), tr("Move up/down (Fly mode only)")},
		{tr("I, K"), tr("Rotate view around X-axis (up/down)")},
		{tr("J, L"), tr("Rotate view around Y-axis (left/right)")},
		{tr("M, N"), tr("Rotate view around Z-axis (clockwise/counter-clockwise)")},
		{tr("X, Z"), tr("Zoom in/out (Orbit mode only)")},
		{tr("F"), tr("Fit All - frame entire scene in view")},
		{tr("Ctrl + P"), tr("Toggle between Orthographic and Perspective projection") },
		{tr("Ctrl + M"), tr("Toggle between multi-view and single view")},
		{tr("Ctrl + T"), tr("Top View")},
		{tr("Ctrl + B"), tr("Bottom View")},
		{tr("Ctrl + F"), tr("Front View")},
		{tr("Ctrl + R"), tr("Rear View")},
		{tr("Ctrl + L"), tr("Left View")},
		{tr("Ctrl + J"), tr("Right View")},
		{tr("Home"), tr("Axonometric View")},
		{tr("F"), tr("Fit All")}
	};
	content += createSection(tr("View Navigation"), "") + createTable(navHeaders, navRows);

	// Camera Modes
	QList<QStringList> camRows = {
		{tr("1"), tr("Switch to Orbit camera mode")},
		{tr("2"), tr("Switch to Fly camera mode")},
		{tr("3"), tr("Switch to First Person camera mode")}
	};
	content += createSection(tr("Camera Modes"), "") + createTable(navHeaders, camRows);

	// Selection and Visibility
	QList<QStringList> selRows = {
		{tr("Delete"), tr("Delete selected objects")},
		{tr("Space"), tr("Hide selected objects (or show if swapped)")},
		{tr("Shift + Space"), tr("Show only selected objects")},
		{tr("Alt + S"), tr("Swap visible/hidden objects")},
		{tr("Esc"), tr("Cancel current operation and deselect all")}
	};
	content += createSection(tr("Selection & Visibility"), "") + createTable(navHeaders, selRows);

	// File Operations
	QList<QStringList> fileRows = {
		{tr("Ctrl + Shift + I"), tr("Import model into current scene")},
		{tr("Ctrl + Shift + E"), tr("Export selected objects")}
	};
	content += createSection(tr("File Operations"), "") + createTable(navHeaders, fileRows);

	content += createSection(tr("Tips"),
		tr("<ul>"
			"<li>Hold keys continuously for smooth navigation</li>"
			"<li>Camera mode affects how W/A/S/D keys behave</li>"
			"<li>In First Person mode, pitch is limited to ±60 degrees</li>"
			"<li>In Fly mode, pitch is limited to ±89 degrees</li>"
			"</ul>"));

	m_keyboardBrowser->setHtml(createStyledHtml(tr("Keyboard Shortcuts"), content));
}

void QuickHelpDialog::setupViewToolbarTab()
{
	QString content;

	QStringList headers = { tr("Button"), tr("Function"), tr("Description") };
	QList<QStringList> rows = {
		{tr("Rotate View"), tr("Activate rotation mode"),
		 tr("Click to enable, then use Left Mouse to rotate the view")},
		{tr("Pan View"), tr("Activate pan mode"),
		 tr("Click to enable, then use Left Mouse to pan the view")},
		{tr("Zoom View"), tr("Activate zoom mode"),
		 tr("Click to enable, then drag Left Mouse vertically to zoom")},
		{tr("Fit All"), tr("Frame scene"),
		 tr("Fits entire scene in the viewport (Shortcut: F)")},
		{tr("Window Zoom"), tr("Zoom to area"),
		 tr("Drag a rectangle to zoom into that specific area")},
		{tr("Camera Modes"), tr("Switch camera type"),
		 tr("Choose between Orbit, Fly, or First Person camera modes<br/>"
			"Shortcuts: 1=Orbit, 2=Fly, 3=First Person")},
		{tr("Orthographic Views"), tr("Standard views"),
		 tr("Quick access to Top, Front, Left, Bottom, Rear, Right views")},
		{tr("Axonometric Views"), tr("3D standard views"),
		 tr("Switch to Isometric, Dimetric, or Trimetric projections")},
		{tr("Projection Toggle"), tr("Ortho ↔ Perspective"),
		 tr("Switch between orthographic and perspective projection")},
		{tr("Multi-View"), tr("Four viewport layout"),
		 tr("Show Top, Front, Right, and Isometric views simultaneously")},
		{tr("Display Modes"), tr("Rendering style"),
		 tr("Choose Realistic, Shaded, Wireframe, or WireShaded display")},
		{tr("Section View"), tr("Clipping planes"),
		 tr("Enable interactive clipping planes for cross-sections")},
		{tr("Swap Visible"), tr("Invert visibility"),
		 tr("Show hidden objects and hide visible ones")},
		{tr("Show/Hide Axis"), tr("Toggle axis display"),
		 tr("Show or hide the 3D coordinate axis indicator")}
	};

	content += createSection(tr("Toolbar Buttons"), "") + createTable(headers, rows);

	content += createSection(tr("Auto-Hide Behavior"),
		tr("<ul>"
			"<li>The toolbar automatically appears at the bottom of the viewport</li>"
			"<li>Move mouse to bottom edge to reveal the toolbar</li>"
			"<li>Toolbar hides after 2 seconds of inactivity</li>"
			"<li>Toolbar remains visible when mouse is over it or menus are open</li>"
			"<li>Scroll buttons appear if toolbar is wider than viewport</li>"
			"</ul>"));

	m_toolbarBrowser->setHtml(createStyledHtml(tr("View Toolbar"), content));
}

void QuickHelpDialog::setupCameraModesTab()
{
	QString content;

	content += createSection(tr("Orbit Camera Mode (Key: 1)"),
		tr("<p><b>Best for:</b> Examining objects from all angles, CAD-like viewing</p>"
			"<p><b>Behavior:</b></p>"
			"<ul>"
			"<li>Camera orbits around the model center point</li>"
			"<li>Rotation keeps the model in view</li>"
			"<li>Up direction is always maintained</li>"
			"<li><b>W/A/S/D:</b> Pan the view (up/down/left/right)</li>"
			"<li><b>X/Z:</b> Zoom in/out</li>"
			"<li><b>I/K:</b> Rotate around X-axis</li>"
			"<li><b>J/L:</b> Rotate around Y-axis</li>"
			"</ul>"));

	content += createSection(tr("Fly Camera Mode (Key: 2)"),
		tr("<p><b>Best for:</b> Free exploration of large scenes, architectural walkthroughs</p>"
			"<p><b>Behavior:</b></p>"
			"<ul>"
			"<li>Camera moves freely through 3D space</li>"
			"<li>Mouse controls look direction</li>"
			"<li>No restrictions on viewing angle</li>"
			"<li><b>W/S:</b> Move forward/backward in viewing direction</li>"
			"<li><b>A/D:</b> Strafe left/right</li>"
			"<li><b>Q/E:</b> Move down/up vertically</li>"
			"<li><b>Mouse:</b> Look around (pitch limited to ±89°)</li>"
			"</ul>"));

	content += createSection(tr("First Person Camera Mode (Key: 3)"),
		tr("<p><b>Best for:</b> Ground-level exploration, character perspective</p>"
			"<p><b>Behavior:</b></p>"
			"<ul>"
			"<li>Similar to Fly mode but with constraints</li>"
			"<li>Pitch restricted to ±60° (more natural for ground movement)</li>"
			"<li>Typically used for walking simulations</li>"
			"<li><b>W/S:</b> Move forward/backward</li>"
			"<li><b>A/D:</b> Strafe left/right</li>"
			"<li><b>Mouse:</b> Look around (pitch limited to ±60°)</li>"
			"<li>Note: No vertical Q/E movement in this mode</li>"
			"</ul>"));

	content += createSection(tr("Switching Modes"),
		tr("<p>You can switch between camera modes in several ways:</p>"
			"<ul>"
			"<li>Press <b>1</b>, <b>2</b>, or <b>3</b> on keyboard</li>"
			"<li>Use the Camera Modes button on the View Toolbar</li>"
			"<li>The toolbar button updates to show current mode</li>"
			"</ul>"));

	m_cameraBrowser->setHtml(createStyledHtml(tr("Camera Modes"), content));
}

void QuickHelpDialog::setupDisplayModesTab()
{
	QString content;

	QStringList headers = { tr("Display Mode"), tr("Shortcut Key"), tr("Description"), tr("Use Case")};
	QList<QStringList> rows = {
		{tr("Realistic"),
		 tr("Shift + R"),
		 tr("Full PBR rendering with all material properties, textures, lighting, shadows, and reflections"),
		 tr("Final presentation, material evaluation, photorealistic visualization")},

		{tr("Shaded"),
		 tr("Shift + S"),
		 tr("Solid colored surfaces with basic lighting (Ambient-Diffuse-Specular model)"),
		 tr("General modeling work, performance, shape evaluation")},

		{tr("Wireframe"),
		 tr("Shift + W"),
		 tr("Shows only edges of polygons, no filled surfaces"),
		 tr("Topology inspection, vertex/edge checking, technical drawings")},

		{tr("WireShaded"),
		 tr("Shift + E"),
		 tr("Combination of shaded surfaces with visible wireframe edges"),
		 tr("Modeling work where you need to see both shape and topology")}
	};

	content += createSection(tr("Available Display Modes"), "") + createTable(headers, rows);

	content += createSection(tr("Rendering Features"),
		tr("<p>The Realistic mode includes advanced rendering features:</p>"
			"<ul>"
			"<li><b>PBR Materials:</b> Physically Based Rendering with metallic/roughness workflow</li>"
			"<li><b>Image-Based Lighting:</b> Environmental lighting from HDRI maps</li>"
			"<li><b>Shadows:</b> Real-time shadow mapping with adjustable quality</li>"
			"<li><b>Reflections:</b> Environment reflections on surfaces</li>"
			"<li><b>Advanced Materials:</b> Support for transmission, clearcoat, sheen, iridescence, anisotropy</li>"
			"<li><b>HDR & Tone Mapping:</b> High dynamic range with multiple tone mapping algorithms</li>"
			"<li><b>Gamma Correction:</b> Proper color space handling</li>"
			"</ul>"));

	content += createSection(tr("Performance Considerations"),
		tr("<ul>"
			"<li><b>Realistic mode</b> is most demanding - may be slower on complex scenes</li>"
			"<li><b>Shaded mode</b> offers good balance of appearance and performance</li>"
			"<li><b>Wireframe mode</b> is fastest but least visually informative</li>"
			"<li>For large models (>50MB), low-resolution preview is automatically enabled during manipulation</li>"
			"</ul>"));

	m_displayBrowser->setHtml(createStyledHtml(tr("Display Modes"), content));
}

void QuickHelpDialog::setupMenuShortcutsTab()
{
	QString content;

	QStringList headers = { tr("Menu"), tr("Shortcut"), tr("Action") };

	// File Menu
	QList<QStringList> fileRows = {
		{tr("File → New"), tr("Ctrl+N"), tr("Create new viewer session")},
		{tr("File → Open"), tr("Ctrl+O"), tr("Open a 3D model file")},
		{tr("File → Import"), tr("Ctrl+Shift+I"), tr("Import model into current scene")},
		{tr("File → Export"), tr("Ctrl+Shift+E"), tr("Export selected objects")},
		{tr("File → Save"), tr("Ctrl+S"), tr("Save current scene")},
		{tr("File → Save As"), tr("Ctrl+Shift+S"), tr("Save scene with new name")},
		{tr("File → Close"), tr("Ctrl+W"), tr("Close current document")},
		{tr("File → Exit"), tr("Ctrl+Q"), tr("Exit application")}
	};
	content += createSection(tr("File Menu"), "") + createTable(headers, fileRows);

	// Edit Menu  
	QList<QStringList> editRows = {
		{tr("Edit → Undo"), tr("Ctrl+Z"), tr("Undo last operation")},
		{tr("Edit → Redo"), tr("Ctrl+Y"), tr("Redo previously undone operation")}
	};
	content += createSection(tr("Edit Menu"), "") + createTable(headers, editRows);

	// Window Menu
	QList<QStringList> windowRows = {
		{tr("Window → Next"), tr("Ctrl+Tab"), tr("Switch to next document window")},
		{tr("Window → Previous"), tr("Ctrl+Shift+Tab"), tr("Switch to previous document window")}
	};
	content += createSection(tr("Window Menu"), "") + createTable(headers, windowRows);

	// Context Menu (Right-Click)
	content += createSection(tr("Right-Click Context Menu"),
		tr("<p>Right-clicking in the viewport provides quick access to common operations:</p>"));

	QList<QStringList> contextRows = {
		{tr("When object selected:"), "", tr("")},
		{tr("  Center Screen"), tr(""), tr("Center view on selected object")},
		{tr("  Center Object List"), tr(""), tr("Scroll object list to selected item")},
		{tr("  Hide/Show"), tr("Space"), tr("Toggle visibility of selected objects")},
		{tr("  Show Only"), tr("Shift+Space"), tr("Show only selected, hide all others")},
		{tr("  Visualization Settings"), tr(""), tr("Open material/appearance settings")},
		{tr("  Transformations"), tr(""), tr("Open transformation panel (move/rotate/scale)")},
		{tr("  Generate UVs"), tr(""), tr("Auto-generate texture coordinates")},
		{tr("  Duplicate"), tr(""), tr("Create copy of selected objects")},
		{tr("  Delete"), tr("Delete"), tr("Remove selected objects")},
		{tr("  Mesh Info"), tr(""), tr("Display detailed mesh information")},
		{tr(""), "", tr("")},
		{tr("When no selection:"), "", tr("")},
		{tr("  Fit All"), tr("F"), tr("Frame entire scene")},
		{tr("  Zoom Area"), tr(""), tr("Enable window zoom mode")},
		{tr("  Select/Zoom/Pan/Rotate"), tr(""), tr("Activate view manipulation modes")},
		{tr("  Show All"), tr("Shift+A"), tr("Make all objects visible")},
		{tr("  Hide All"), tr("Alt+A"), tr("Hide all objects")},
		{tr("  Swap Visible"), tr("Alt+S"), tr("Invert visibility of all objects")},
		{tr("  Background Color"), tr(""), tr("Change viewport background color")}
	};
	content += createTable({ tr("Action"), tr("Shortcut"), tr("Description") }, contextRows);

	m_menuBrowser->setHtml(createStyledHtml(tr("Menu Shortcuts"), content));
}

void QuickHelpDialog::setupTipsAndTricksTab()
{
	QString content;

	content += createSection(tr("Getting Started"),
		tr("<ul>"
			"<li><b>Opening Files:</b> Drag and drop files directly onto the window, or use File → Open</li>"
			"<li><b>First View:</b> Press 'F' to frame your model perfectly in the viewport</li>"
			"<li><b>Quick Navigation:</b> Use Middle Mouse for rotation, Mouse Wheel for zoom, Right Mouse for pan</li>"
			"<li><b>Recent Files:</b> Access recently opened files from File → Recent menu</li>"
			"</ul>"));

	content += createSection(tr("Selection Techniques"),
		tr("<ul>"
			"<li><b>Single Select:</b> Left-click on an object in the viewport</li>"
			"<li><b>Multi-Select:</b> Drag a rubber band rectangle around multiple objects</li>"
			"<li><b>Toggle Selection:</b> Click on an already selected object to deselect it</li>"
			"<li><b>Select from List:</b> Use the object list panel on the left side</li>"
			"<li><b>Search Objects:</b> Use the search box above the object list to filter by name</li>"
			"</ul>"));

	content += createSection(tr("Working with Visibility"),
		tr("<ul>"
			"<li><b>Hide Selected:</b> Press Space to temporarily hide objects you don't need</li>"
			"<li><b>Isolate:</b> Press Shift+Space to focus on selected objects only</li>"
			"<li><b>Swap Visible:</b> Press Alt+S to see what's hidden (and hide what's visible)</li>"
			"<li><b>Show All:</b> Press Shift+A to bring everything back</li>"
			"<li><b>Visual Indicator:</b> Hidden objects are grayed out in the object list</li>"
			"</ul>"));

	content += createSection(tr("View Organization"),
		tr("<ul>"
			"<li><b>Multiple Sessions:</b> File → New creates additional viewer windows</li>"
			"<li><b>Window Layouts:</b> Use Window menu to tile or cascade multiple documents</li>"
			"<li><b>Multi-View Mode:</b> Enable from toolbar to see four viewports simultaneously</li>"
			"<li><b>Standard Views:</b> Use toolbar buttons for instant Top/Front/Side views</li>"
			"<li><b>Axonometric Views:</b> Choose Isometric/Dimetric/Trimetric for technical drawings</li>"
			"</ul>"));

	content += createSection(tr("Performance Tips"),
		tr("<ul>"
			"<li><b>Large Models:</b> Automatic low-res preview during manipulation for models >50MB</li>"
			"<li><b>Display Mode:</b> Switch to Shaded or Wireframe for better performance</li>"
			"<li><b>Progressive Loading:</b> Large files load progressively with status updates</li>"
			"<li><b>Shadow Quality:</b> Adjust in Environment settings if shadows are slow</li>"
			"<li><b>Hidden Objects:</b> Hidden objects are still in memory but not rendered</li>"
			"</ul>"));

	content += createSection(tr("Materials and Appearance"),
		tr("<ul>"
			"<li><b>Visualization Settings:</b> Right-click object → Visualization Settings</li>"
			"<li><b>Material Editor:</b> Use the left panel to edit colors, roughness, metallic properties</li>"
			"<li><b>Texture Mapping:</b> Apply textures through the Texture Mapping panel</li>"
			"<li><b>Environment:</b> Enable SkyBox and IBL for realistic lighting</li>"
			"<li><b>Display Modes:</b> Switch to Realistic mode to see full PBR materials</li>"
			"</ul>"));

	content += createSection(tr("Advanced Features"),
		tr("<ul>"
			"<li><b>Clipping Planes:</b> Use Section View to cut through models and see internals</li>"
			"<li><b>Transformations:</b> Move, rotate, scale objects individually or in groups</li>"
			"<li><b>Floor Plane:</b> Enable in Environment settings for shadow casting and reflections</li>"
			"<li><b>Shadows:</b> Toggle real-time shadows in Environment settings</li>"
			"<li><b>Window Zoom:</b> Zoom precisely into a specific region of interest</li>"
			"<li><b>UV Generation:</b> Auto-generate texture coordinates for objects without UVs</li>"
			"</ul>"));

	content += createSection(tr("Troubleshooting"),
		tr("<ul>"
			"<li><b>Lost Objects:</b> Press 'F' to fit all, or check if objects are hidden</li>"
			"<li><b>Stuck in Mode:</b> Press Esc to cancel any active operation</li>"
			"<li><b>Can't Select:</b> Make sure you're not in a view manipulation mode (check cursor)</li>"
			"<li><b>Black Screen:</b> Check display mode and lighting settings</li>"
			"<li><b>Slow Performance:</b> Try switching to Shaded mode or hiding some objects</li>"
			"</ul>"));

	content += createSection(tr("Customization"),
		tr("<ul>"
			"<li><b>Settings:</b> File → Settings to configure MSAA, anisotropic filtering, and theme</li>"
			"<li><b>Background:</b> Right-click → Background Color to customize viewport background</li>"
			"<li><b>Theme:</b> Choose between Light, Dark, or System theme in Settings</li>"
			"<li><b>Language:</b> Change interface language in Settings dialog</li>"
			"<li><b>Axis Position:</b> Configure corner axis triad position in Settings</li>"
			"</ul>"));

	m_tipsBrowser->setHtml(createStyledHtml(tr("Tips & Tricks"), content));
}

QString QuickHelpDialog::createStyledHtml(const QString& title,
	const QString& content)
{
	QString html = QString(
		"<!DOCTYPE html>"
		"<html>"
		"<head>"
		"<style>"
		"body { font-family: Arial, sans-serif; font-size: 10pt; margin: 10px; }"

		"h1 { color: #2c3e50; font-size: 18pt; "
		"     border-bottom: 2px solid #3498db; padding-bottom: 5px; }"

		"h2 { color: #34495e; font-size: 14pt; "
		"     margin-top: 20px; margin-bottom: 10px; }"

		/* Table styling */
		"table { border-collapse: collapse; width: 100%%; margin: 10px 0; }"

		"th { background-color: #3498db; color: white; "
		"     border: 1px solid #ccc; "
		"     padding: 8px; text-align: left; font-weight: bold; }"

		"td { border: 1px solid #ddd; padding: 8px; vertical-align: top; }"

		"tbody tr:nth-child(even) { background-color: #f2f2f2; }"
		"tbody tr:hover { background-color: #e8f4f8; }"

		"ul { margin-left: 20px; }"
		"li { margin-bottom: 8px; line-height: 1.6; }"

		"code { background-color: #f4f4f4; padding: 2px 6px; "
		"       border-radius: 3px; font-family: 'Courier New', monospace; }"

		"kbd { background-color: #eef2f5; color: #2c3e50; "
		"      border: 1px solid #bfc7ce; "
		"      border-radius: 4px; "
		"      padding: 2px 6px; "
		"      font-family: 'Courier New', monospace; "
		"      font-size: 9pt; "
		"      white-space: nowrap; }"

		"p { line-height: 1.6; margin: 10px 0; }"
		".section { margin-bottom: 25px; }"
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


QString QuickHelpDialog::createSection(const QString& heading, const QString& content)
{
	if (content.isEmpty())
		return QString("<h2>%1</h2>").arg(heading);

	return QString("<div class='section'><h2>%1</h2>%2</div>").arg(heading, content);
}

QString QuickHelpDialog::createTable(const QStringList& headers,
	const QList<QStringList>& rows)
{
	QString table =
		"<table border=\"1\" cellspacing=\"0\" cellpadding=\"4\">";

	// Column widths
	table +=
		"<colgroup>"
		"  <col style=\"width:18%\">"
		"  <col style=\"width:14%\">"
		"  <col style=\"width:38%\">"
		"  <col style=\"width:30%\">"
		"</colgroup>";

	// Find shortcut column index
	int shortcutCol = -1;
	for (int i = 0; i < headers.size(); ++i)
	{
		if (headers[i] == tr("Shortcut Key") ||
			headers[i] == tr("Shortcut") ||
			headers[i] == tr("Key") ||
			headers[i] == tr("Button") ||
			headers[i] == tr("Mouse Control"))
		{
			shortcutCol = i;
			break;
		}
	}

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

		for (int i = 0; i < row.size(); ++i)
		{
			QString cell = row[i];

			if (i == shortcutCol && shortcutCol != -1)
				cell = QString("<kbd>%1</kbd>").arg(cell);

			table += QString("<td>%1</td>").arg(cell);
		}

		table += "</tr>";
	}

	table += "</table>";
	return table;
}
