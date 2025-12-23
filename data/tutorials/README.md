# ModelViewer Tutorial - Multi-Page Version

## 📦 Package Contents

- **index.html** - Landing page with course overview
- **lesson01.html to lesson14.html** - Individual lesson pages
- **common-styles.css** - Shared stylesheet for all lessons
- **screenshots/** - Directory for tutorial screenshots (empty, add your own)
- **README.md** - This file

## 🚀 Quick Start

### 1. Test Locally (Immediate)
```bash
# Open index.html in your browser
firefox index.html
# or
chrome index.html
# or just double-click index.html
```

### 2. Install in ModelViewer

Copy this entire directory to:
```
MODELVIEWER_DATA_DIR/data/tutorials/
```

**Example paths:**
- Linux: `/usr/local/share/modelviewer/data/tutorials/`
- Windows: `C:\Program Files\ModelViewer\data\tutorials\`
- macOS: `/Applications/ModelViewer.app/Contents/Resources/data/tutorials/`

### 3. Add Launcher Code

In `MainWindow.cpp`:
```cpp
#include <QDesktopServices>
#include <QUrl>
#include <QFile>

void MainWindow::on_actionTutorial_triggered()
{
    QString path = QString(MODELVIEWER_DATA_DIR) + "/data/tutorials/index.html";
    
    if (QFile::exists(path)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    } else {
        QMessageBox::warning(this, tr("Tutorial Not Found"),
            tr("Tutorial file not found at: %1").arg(path));
    }
}
```

Add menu action with shortcut: **Ctrl+F1**

## 📖 Lesson Status

### ✅ Complete (Full Content)
- Lesson 1: Getting Started
- Lesson 2: Opening Models
- Lesson 3: Basic Navigation
- Lesson 4: Selecting Objects

### 📝 Placeholders (Need Content)
- Lessons 5-14 have basic structure but need detailed content added

## 🖼️ Adding Screenshots

Place PNG/JPG files in the `screenshots/` directory with these exact names:

```
screenshots/
├── tutorial_01_main_window.png
├── tutorial_01_interface_labeled.png
├── tutorial_02_file_menu.png
├── tutorial_02_file_dialog.png
├── tutorial_03_rotate_gesture.png
└── ... (87 total - see SCREENSHOT_GUIDE.md)
```

Screenshots automatically display when present, show placeholders when missing.

## ✏️ Completing Lessons 5-14

Each lesson file follows this structure:

1. Open the lesson file (e.g., `lesson05.html`)
2. Find the content section between lesson-header and lesson-navigation
3. Replace placeholder with actual content
4. Use these HTML components:

### Available Components

**Sections:**
```html
<h2>Section Title</h2>
<p>Paragraph text...</p>
```

**Steps:**
```html
<div class="step">
    <div class="step-title">Step 1: Do This</div>
    <div class="step-content">Description here...</div>
</div>
```

**Screenshots:**
```html
<div class="screenshot-container">
    <img src="screenshots/tutorial_XX_name.png" alt="Description"
         onerror="this.style.display='none'; this.nextElementSibling.style.display='inline-block';">
    <div class="screenshot-placeholder" style="display:none;">
        Screenshot: tutorial_XX_name.png
    </div>
    <span class="screenshot-caption">Caption text</span>
</div>
```

**Notes:**
```html
<div class="note note-tip">Tip content...</div>
<div class="note note-info">Info content...</div>
<div class="note note-warning">Warning content...</div>
```

**Tables:**
```html
<table>
    <thead><tr><th>Header 1</th><th>Header 2</th></tr></thead>
    <tbody>
        <tr><td>Data 1</td><td>Data 2</td></tr>
    </tbody>
</table>
```

**Keyboard Keys:**
```html
<kbd>Ctrl</kbd> + <kbd>O</kbd>
```

## 🎨 Customization

### Colors
Edit `common-styles.css`:
```css
/* Sidebar background */
#sidebar {
    background: linear-gradient(180deg, #2c3e50 0%, #34495e 100%);
}

/* Accent color (borders, buttons) */
/* Change #3498db to your color */
```

### Layout
```css
#sidebar {
    width: 280px;  /* Sidebar width */
}

#main-content {
    max-width: 1200px;  /* Content max width */
}
```

## 🌍 Localization

Create translated versions:
```
tutorials/
├── index.html (English)
├── index_de.html (German)
├── lesson01.html (English)
├── lesson01_de.html (German)
└── ...
```

Update launcher to detect language:
```cpp
QString lang = QLocale::system().name().left(2);
QString indexFile = (lang == "en") ? "index.html" : QString("index_%1.html").arg(lang);
```

## 📊 Features

✅ **Multi-page navigation** - One lesson per page
✅ **Progress tracking** - "Lesson X of 14" indicator
✅ **Sidebar navigation** - Jump to any lesson
✅ **Previous/Next buttons** - Sequential learning
✅ **Responsive design** - Works on mobile
✅ **Screenshot fallbacks** - Placeholders until images added
✅ **Professional styling** - Modern, clean design
✅ **No dependencies** - Pure HTML/CSS
✅ **Fast loading** - Each page loads independently

## 🧪 Testing

### Local Testing
1. Open `index.html` in browser
2. Click "Start Tutorial" button
3. Test navigation (sidebar, prev/next)
4. Verify all 14 lessons load
5. Check responsive design (resize window)

### Integration Testing
1. Copy to ModelViewer data directory
2. Launch from Help menu (Ctrl+F1)
3. Verify tutorial opens in browser
4. Test all lessons
5. Add one screenshot, verify it displays

## 🆚 Advantages Over Single-Page

✅ **Focused learning** - One topic at a time
✅ **Progress feeling** - Complete lessons one by one
✅ **Faster loading** - Only one lesson loads
✅ **Better bookmarking** - URL per lesson
✅ **Less overwhelming** - Not seeing all 14 lessons at once
✅ **Mental chunking** - Natural break points

## 📝 TODO

- [ ] Complete content for lessons 5-14
- [ ] Create all 87 screenshots
- [ ] Test on different browsers
- [ ] Add print styles
- [ ] Consider adding "Mark Complete" feature with localStorage
- [ ] Add search functionality across lessons
- [ ] Consider adding quiz/exercise pages

## 📄 License

Same as ModelViewer project.

## 🙋 Support

For issues or questions:
- Check that files are in correct directory
- Verify MODELVIEWER_DATA_DIR is set correctly
- Test by opening index.html directly in browser
- Check browser console for errors (F12)

---

**Ready to use! Just add your screenshots and expand lessons 5-14 with content.** 🚀
