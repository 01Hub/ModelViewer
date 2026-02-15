#!/usr/bin/env python3
"""
Complete Qt .ts Translation Script using LibreTranslate
========================================================

This script uses the FREE LibreTranslate API to complete all unfinished
translations in Qt .ts files.

INSTALLATION:
-------------
1. Install Python dependencies:
   pip install requests

2. Run the script:
   python complete_translations.py

USAGE OPTIONS:
--------------

Option 1: Use Public LibreTranslate API (FREE, but may be slow)
   - No setup required
   - Rate limited
   - May have availability issues

Option 2: Run Your Own LibreTranslate Server (BEST for large projects)
   - Install with Docker:
     docker run -ti --rm -p 5000:5000 libretranslate/libretranslate
   - Then update API_URL to "http://localhost:5000/translate"
   - Fast, unlimited, completely free

Option 3: Use a Paid LibreTranslate Instance
   - Get API key from https://libretranslate.com
   - More reliable than public API

"""

import xml.etree.ElementTree as ET
import requests
import time
import sys
from pathlib import Path

# ============================================================================
# CONFIGURATION
# ============================================================================

# LibreTranslate API URL
# Option 1 (Public - may be slow/unreliable):
#API_URL = "https://libretranslate.com/translate"

# Option 2 (Local Docker instance - RECOMMENDED):
API_URL = "http://localhost:5000/translate"

# Option 3 (Paid instance with API key):
# API_URL = "https://libretranslate.com/translate"
API_KEY = None  # Set your API key if using paid version

# Rate limiting (seconds between requests)
# Increase if you're using public API, decrease for local/paid
RATE_LIMIT_DELAY = 0.1  # Public API: use 1.0+, Local: use 0.1

# ============================================================================
# TRANSLATION CLIENT
# ============================================================================

class LibreTranslateClient:
    """Client for LibreTranslate API"""
    
    def __init__(self, api_url=API_URL, api_key=API_KEY):
        self.api_url = api_url
        self.api_key = api_key
        
    def translate(self, text, source_lang="en", target_lang="de"):
        """Translate text using LibreTranslate"""
        if not text or not text.strip():
            return None
            
        try:
            payload = {
                "q": text,
                "source": source_lang,
                "target": target_lang,
                "format": "text"
            }
            
            if self.api_key:
                payload["api_key"] = self.api_key
            
            response = requests.post(
                self.api_url,
                json=payload,
                headers={"Content-Type": "application/json"},
                timeout=30
            )
            
            if response.status_code == 200:
                result = response.json()
                return result.get("translatedText", None)
            elif response.status_code == 429:
                print(f"  ⚠ Rate limit hit, waiting 5 seconds...")
                time.sleep(5)
                return self.translate(text, source_lang, target_lang)
            else:
                return None
                
        except requests.exceptions.ConnectionError:
            print(f"  ⚠ Connection error. Is LibreTranslate API accessible?")
            return None
        except Exception as e:
            print(f"  ⚠ Error: {str(e)[:100]}")
            return None

# ============================================================================
# TRANSLATION FUNCTION
# ============================================================================

def translate_ts_file(input_file, output_file, target_lang, lang_name):
    """Complete translation of a Qt .ts file"""
    
    print(f"\n{'='*80}")
    print(f"Translating to {lang_name} ({target_lang.upper()})")
    print(f"{'='*80}")
    
    if not Path(input_file).exists():
        print(f"❌ Input file not found: {input_file}")
        return None
    
    translator = LibreTranslateClient()
    tree = ET.parse(input_file)
    root = tree.getroot()
    
    stats = {
        'total_unfinished': 0,
        'translated': 0,
        'already_done': 0,
        'failed': 0,
        'skipped': 0
    }
    
    # Technical terms that should not be translated
    SKIP_TERMS = {
        'X', 'Y', 'Z', 'R', 'G', 'B', 'A', 'S', 'T', 'U', 'V',
        '...', 'mm', 'deg', 'nm', 'px', '0/0', 'OK', 'N/A',
        'PBR', 'IBL', 'HDR', 'UV', 'IOR', 'FPS'
    }
    
    # Count total unfinished
    for context in root.findall('context'):
        for message in context.findall('message'):
            translation = message.find('translation')
            if translation is not None and translation.get('type') == 'unfinished':
                stats['total_unfinished'] += 1
    
    print(f"Found {stats['total_unfinished']} unfinished entries\n")
    
    if stats['total_unfinished'] == 0:
        print("✓ No unfinished translations found!")
        return stats
    
    # Translate each unfinished entry
    for context in root.findall('context'):
        for message in context.findall('message'):
            translation = message.find('translation')
            
            if translation is not None and translation.get('type') == 'unfinished':
                source = message.find('source')
                
                if source is not None and source.text:
                    source_text = source.text
                    
                    # Skip if already has translation
                    if translation.text and translation.text.strip():
                        translation.attrib.pop('type', None)
                        stats['already_done'] += 1
                        continue
                    
                    # Skip technical terms
                    if source_text.strip() in SKIP_TERMS:
                        translation.text = source_text
                        translation.attrib.pop('type', None)
                        stats['skipped'] += 1
                        continue
                    
                    # Translate
                    translated_text = translator.translate(
                        source_text,
                        source_lang="en",
                        target_lang=target_lang
                    )
                    
                    if translated_text:
                        translation.text = translated_text
                        translation.attrib.pop('type', None)
                        stats['translated'] += 1
                        
                        # Progress indicator
                        if stats['translated'] % 25 == 0:
                            progress = (stats['translated'] + stats['already_done'] + stats['skipped']) / stats['total_unfinished'] * 100
                            print(f"  Progress: {stats['translated']} new translations ({progress:.1f}%)")
                        
                        # Rate limiting
                        time.sleep(RATE_LIMIT_DELAY)
                    else:
                        stats['failed'] += 1
    
    # Save the file
    tree.write(output_file, encoding='utf-8', xml_declaration=True)
    
    # Print statistics
    remaining = sum(1 for c in root.findall('context') 
                    for m in c.findall('message') 
                    if (t := m.find('translation')) is not None and t.get('type') == 'unfinished')
    
    completion = ((stats['total_unfinished'] - remaining) / stats['total_unfinished'] * 100) if stats['total_unfinished'] > 0 else 0
    
    print(f"\n{'='*80}")
    print(f"Results - {lang_name}")
    print(f"{'='*80}")
    print(f"Total unfinished:         {stats['total_unfinished']:4}")
    print(f"Newly translated:         {stats['translated']:4}")
    print(f"Already done:             {stats['already_done']:4}")
    print(f"Skipped (technical):      {stats['skipped']:4}")
    print(f"Failed:                   {stats['failed']:4}")
    print(f"Remaining unfinished:     {remaining:4}")
    print(f"Completion rate:          {completion:5.1f}%")
    print(f"✓ Saved to: {output_file}")
    print(f"{'='*80}\n")
    
    return stats

# ============================================================================
# MAIN FUNCTION
# ============================================================================

def main():
    """Main function to process all language files"""
    
    print("=" * 80)
    print("COMPLETE QT TRANSLATION - LIBRETRANSLATE")
    print("=" * 80)
    print(f"\nAPI URL: {API_URL}")
    print(f"Rate limit delay: {RATE_LIMIT_DELAY}s between requests\n")
    
    # Language configurations
    languages = [
        {
            'code': 'de',
            'name': 'German (Deutsch)',
            'input': 'modelviewer_de.ts',
            'output': 'modelviewer_de_complete.ts'
        },
        {
            'code': 'fr',
            'name': 'French (Français)',
            'input': 'modelviewer_fr.ts',
            'output': 'modelviewer_fr_complete.ts'
        },
        {
            'code': 'it',
            'name': 'Italian (Italiano)',
            'input': 'modelviewer_it.ts',
            'output': 'modelviewer_it_complete.ts'
        },
        {
            'code': 'es',
            'name': 'Spanish (Español)',
            'input': 'modelviewer_es.ts',
            'output': 'modelviewer_es_complete.ts'
        },
    ]
    
    # Ask user which languages to process
    print("Which languages would you like to translate?")
    print("1. All languages")
    print("2. Select specific language(s)")
    
    choice = input("\nEnter your choice (1 or 2): ").strip()
    
    if choice == '2':
        print("\nAvailable languages:")
        for i, lang in enumerate(languages, 1):
            print(f"{i}. {lang['name']}")
        
        selected = input("\nEnter numbers separated by commas (e.g., 1,3): ").strip()
        indices = [int(x.strip()) - 1 for x in selected.split(',')]
        languages = [languages[i] for i in indices if 0 <= i < len(languages)]
    
    if not languages:
        print("No languages selected. Exiting.")
        return
    
    overall_stats = {
        'total_translated': 0,
        'total_failed': 0,
        'total_time': 0
    }
    
    start_time = time.time()
    
    # Process each language
    for lang in languages:
        lang_start = time.time()
        
        stats = translate_ts_file(
            lang['input'],
            lang['output'],
            lang['code'],
            lang['name']
        )
        
        if stats:
            overall_stats['total_translated'] += stats['translated']
            overall_stats['total_failed'] += stats['failed']
        
        lang_time = time.time() - lang_start
        overall_stats['total_time'] = time.time() - start_time
        
        print(f"Time for {lang['name']}: {lang_time/60:.1f} minutes")
        
        # Estimate remaining time
        if len(languages) > 1:
            remaining_langs = len(languages) - (languages.index(lang) + 1)
            if remaining_langs > 0:
                avg_time = overall_stats['total_time'] / (languages.index(lang) + 1)
                est_remaining = avg_time * remaining_langs
                print(f"Estimated time remaining: {est_remaining/60:.1f} minutes\n")
    
    # Final summary
    print("\n" + "=" * 80)
    print("FINAL SUMMARY")
    print("=" * 80)
    print(f"Languages processed:     {len(languages)}")
    print(f"Total new translations:  {overall_stats['total_translated']}")
    print(f"Total failures:          {overall_stats['total_failed']}")
    print(f"Total time:              {overall_stats['total_time']/60:.1f} minutes")
    print("\n✅ Translation complete!")
    print("=" * 80)

# ============================================================================
# ENTRY POINT
# ============================================================================

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n⚠ Translation interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n\n❌ Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
