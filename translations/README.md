# Qt Translation Completion Script

Automatically complete Qt `.ts` translation files using the free LibreTranslate API.

## 📋 Overview

This script translates all unfinished entries in Qt `.ts` translation files using LibreTranslate, a free and open-source machine translation API.

**Supported Languages:**
- 🇩🇪 German (Deutsch)
- 🇫🇷 French (Français)
- 🇮🇹 Italian (Italiano)
- 🇪🇸 Spanish (Español)

## 🚀 Quick Start

### Prerequisites

- Python 3.8 or higher
- Docker Desktop (for local LibreTranslate server)
- `requests` Python library

### Installation

1. **Install Docker Desktop**
   - Download from [docker.com](https://www.docker.com/products/docker-desktop/)
   - Follow installation instructions for your OS

2. **Install Python Dependencies**
   ```bash
   pip install requests
   ```

### Usage

1. **Start LibreTranslate Server**
   
   Open a terminal and run:
   ```bash
   docker run -ti --rm -p 5000:5000 libretranslate/libretranslate
   ```
   
   Keep this terminal open. You should see output like:
   ```
   [INFO] Starting LibreTranslate...
   [INFO] Running on http://0.0.0.0:5000
   ```

2. **Update Script Configuration**
   
   Edit `complete_translations.py` and ensure these settings:
   ```python
   # Use local Docker instance
   API_URL = "http://localhost:5000/translate"
   
   # Fast rate limit for local server
   RATE_LIMIT_DELAY = 0.1
   ```

3. **Run the Translation Script**
   
   In a new terminal (keep Docker running):
   ```bash
   python complete_translations.py
   ```

4. **Select Languages**
   
   The script will prompt you:
   ```
   Which languages would you like to translate?
   1. All languages
   2. Select specific language(s)
   ```
   
   Choose option `1` for all languages, or `2` to select specific ones.

5. **Wait for Completion**
   
   The script will:
   - Show real-time progress
   - Display statistics for each language
   - Save completed files with `_complete.ts` suffix

## ⚙️ Configuration Options

### Local Docker Server (Recommended)

**Best for:**
- Complete control
- Fast translation
- Unlimited requests
- Privacy (no data sent externally)

```python
API_URL = "http://localhost:5000/translate"
API_KEY = None
RATE_LIMIT_DELAY = 0.1  # Fast
```

**Time:** ~30-60 minutes for all 4 languages

---

### Public LibreTranslate API

**Best for:**
- No Docker installation
- Quick testing
- One-time use

```python
API_URL = "https://libretranslate.com/translate"
API_KEY = None
RATE_LIMIT_DELAY = 1.0  # Slower to be polite
```

**Time:** ~2-4 hours for all 4 languages

⚠️ **Note:** Public API may be rate-limited or unavailable

---

### Paid LibreTranslate Instance

**Best for:**
- Guaranteed availability
- Faster than public API
- Supporting the project

```python
API_URL = "https://libretranslate.com/translate"
API_KEY = "your-api-key-here"
RATE_LIMIT_DELAY = 0.2
```

Get an API key at [libretranslate.com](https://libretranslate.com)

## 📁 File Structure

```
your-project/
├── complete_translations.py      # This script
├── README.md                      # This file
├── modelviewer_de.ts             # Input: German translation
├── modelviewer_fr.ts             # Input: French translation
├── modelviewer_it.ts             # Input: Italian translation
├── modelviewer_es.ts             # Input: Spanish translation
├── modelviewer_de_complete.ts    # Output: Completed German
├── modelviewer_fr_complete.ts    # Output: Completed French
├── modelviewer_it_complete.ts    # Output: Completed Italian
└── modelviewer_es_complete.ts    # Output: Completed Spanish
```

## 🎯 What Gets Translated

The script automatically translates all entries marked as:
```xml
<translation type="unfinished"></translation>
```

or

```xml
<translation type="unfinished">Partial translation</translation>
```

### Technical Terms Preserved

These terms are **not** translated:
- Coordinate axes: `X`, `Y`, `Z`, `R`, `G`, `B`, `A`, `S`, `T`, `U`, `V`
- Units: `mm`, `deg`, `nm`, `px`
- Technical acronyms: `PBR`, `IBL`, `HDR`, `UV`, `IOR`, `FPS`
- Common markers: `...`, `0/0`, `OK`, `N/A`

## 📊 Output Statistics

After completion, you'll see statistics like:

```
================================================================================
Results - German (Deutsch)
================================================================================
Total unfinished:          307
Newly translated:          285
Already done:               15
Skipped (technical):         7
Failed:                      0
Remaining unfinished:        0
Completion rate:          100.0%
✓ Saved to: modelviewer_de_complete.ts
================================================================================
```

## 🔧 Troubleshooting

### Docker Container Won't Start

**Problem:** Port 5000 already in use

**Solution:** 
```bash
# Use a different port
docker run -ti --rm -p 5001:5000 libretranslate/libretranslate

# Update script
API_URL = "http://localhost:5001/translate"
```

---

### Connection Refused Error

**Problem:** Script can't connect to LibreTranslate

**Solutions:**
1. Make sure Docker container is running
2. Check Docker logs for errors
3. Verify port number matches in script and Docker command
4. Try accessing http://localhost:5000 in your browser

---

### Rate Limit Errors (Public API)

**Problem:** `429 Too Many Requests`

**Solution:** Increase delay in script:
```python
RATE_LIMIT_DELAY = 2.0  # Slower but more reliable
```

---

### Translation Quality Issues

**Problem:** Some translations don't make sense

**Solutions:**
1. Review and edit in Qt Linguist after completion
2. Use DeepL API for higher quality (see below)
3. Create a terminology glossary for technical terms

---

### Python Version Error

**Problem:** Script requires Python 3.8+

**Solution:**
```bash
# Check version
python --version

# Use specific version if needed
python3.9 complete_translations.py
```

## 🎨 Using Qt Linguist for Review

After completing translations, review and refine them:

```bash
# Open completed file in Qt Linguist
linguist modelviewer_de_complete.ts
```

**Review checklist:**
- [ ] Technical terms are correct
- [ ] UI labels fit in available space
- [ ] Keyboard shortcuts are properly localized
- [ ] Tone is consistent with existing translations
- [ ] HTML formatting is preserved

## 🔄 Alternative Translation Services

### DeepL API (Best Quality)

```bash
pip install deepl
```

```python
import deepl

translator = deepl.Translator("YOUR_API_KEY")
result = translator.translate_text(
    "Hello World",
    target_lang="DE"
)
```

- **Cost:** ~$5-10 for all files
- **Quality:** ⭐⭐⭐⭐⭐
- **Signup:** [deepl.com/pro-api](https://www.deepl.com/pro-api)

---

### Google Cloud Translation

```bash
pip install google-cloud-translate
```

```python
from google.cloud import translate_v2

translator = translate_v2.Client()
result = translator.translate(
    "Hello World",
    target_language='de'
)
```

- **Cost:** ~$5-8 for all files
- **Quality:** ⭐⭐⭐⭐
- **Signup:** [cloud.google.com/translate](https://cloud.google.com/translate)

## 📈 Performance Tips

### Speed Up Local Translation

1. **Increase parallel requests** (requires script modification)
2. **Use SSD for Docker volumes**
3. **Allocate more RAM to Docker** (4GB recommended)

### Reduce API Costs

1. **Batch similar strings** before translating
2. **Use translation memory** for repeated phrases
3. **Pre-translate technical terms** manually

## 🛡️ Best Practices

1. **Backup Original Files**
   ```bash
   cp modelviewer_de.ts modelviewer_de.ts.backup
   ```

2. **Test with One Language First**
   - Translate German only
   - Review quality
   - Adjust settings if needed
   - Then translate remaining languages

3. **Version Control**
   ```bash
   git add modelviewer_*_complete.ts
   git commit -m "Add completed translations"
   ```

4. **Quality Assurance**
   - Test in actual application
   - Check text overflow in UI
   - Verify special characters display correctly
   - Test on target language OS if possible

## 📝 Script Customization

### Modify Language List

Edit the `languages` list in `main()`:

```python
languages = [
    {
        'code': 'de',
        'name': 'German (Deutsch)',
        'input': 'modelviewer_de.ts',
        'output': 'modelviewer_de_complete.ts'
    },
    # Add more languages here
]
```

### Add Custom Skip Terms

Edit the `SKIP_TERMS` set:

```python
SKIP_TERMS = {
    'X', 'Y', 'Z',
    'CAD',  # Add your terms
    'BIM',
    # ...
}
```

### Change Output Format

Modify the file writing section:

```python
# Pretty print XML
from xml.dom import minidom

xml_str = ET.tostring(root, encoding='utf-8')
pretty_xml = minidom.parseString(xml_str).toprettyxml(indent="  ")

with open(output_file, 'w', encoding='utf-8') as f:
    f.write(pretty_xml)
```

## 🤝 Contributing

Suggestions for improvement:

1. **Add more language support**
2. **Implement translation caching**
3. **Add GUI for non-technical users**
4. **Support for other translation services**
5. **Parallel processing for speed**

## 📄 License

This script is provided as-is for use with Qt translation files.

LibreTranslate is licensed under AGPL-3.0. See [libretranslate.com](https://libretranslate.com) for details.

## 🔗 Resources

- **LibreTranslate:** https://libretranslate.com
- **Qt Linguist Documentation:** https://doc.qt.io/qt-6/linguist-manual.html
- **Docker Documentation:** https://docs.docker.com
- **DeepL API:** https://www.deepl.com/pro-api
- **Google Cloud Translation:** https://cloud.google.com/translate

## ⚡ Quick Reference

### Start Docker Server
```bash
docker run -ti --rm -p 5000:5000 libretranslate/libretranslate
```

### Run Script
```bash
python complete_translations.py
```

### Stop Docker Server
Press `Ctrl+C` in the Docker terminal

### Check Translation
```bash
linguist modelviewer_de_complete.ts
```

### Use in Qt Project
Replace original `.ts` files and run:
```bash
lrelease modelviewer_de_complete.ts
```

---

**Happy Translating! 🌍**

For issues or questions, refer to the troubleshooting section above.
