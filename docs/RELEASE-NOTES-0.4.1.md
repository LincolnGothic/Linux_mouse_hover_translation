# Hover Translate 0.4.1 — fourth-version update (preview)

Distant words on one visual line are now treated as separate text runs. For
example, hovering “Mode” in a table header does not translate it together with
“What it translates” in the next column. Line and Sentence modes respect large
horizontal gaps; hovering the gap itself does not select a neighboring cell.

Word is the default for new settings; existing saved modes are retained.
Chinese lookup uses character positions and longest matching CC-CEDICT words.
English lookup tries common inflections such as running → run while preserving
exact entries. This improves the existing dictionary; English coverage remains
limited, and Chinese matching is not a full linguistic segmenter.

Source highlighting shows the selected OCR text. Hold Shift for Word, or
Ctrl+Shift for Sentence; releasing restores the saved mode. These options can
be disabled in settings. Enter the popup promptly to Copy translation, Pin/Unpin
or Close it. A pinned popup pauses further hover capture. Esc, pause, desktop
lock/overview, app exit and extension disable clear the popup/highlight. Copy is
explicit; automatic translation does not overwrite the clipboard or save history.

## Install / upgrade on Ubuntu 26.04 amd64

Quit the old app, then run `sudo apt install ./hover-translate_0.4.1_amd64.deb`
and reopen `/usr/bin/hover-translate`. **Click Set up GNOME hover, then sign out
and back in to load extension version 4.** Existing models and settings are kept.
An older extension is paused with an upgrade message. First-time users still
need the one-time offline model/dictionary download; no API key is required.

## Validation, limits and open source

Five regular suites check real OCR including Chinese glyphs, word/gap/column
selection, settings, dictionary morphology, X11 Copy/Pin, local protocol fixtures
and cancellation. A real headless GNOME 50 compositor exercises pointer capture,
source highlights, temporary modifiers, Copy/Pin/Escape, separated headers,
wrapped sentences, and actual offline models. Physical desktop/fractional scale
and actual manual portal permission dialogs remain unverified.

Sentence mode stays within three lines/300 characters and falls back to the
current text run when joining is uncertain. OCR, unusual spacing, punctuation,
word segmentation and translation quality remain imperfect. No new dictionary
dataset is bundled; CC-CEDICT retains its complete attribution/share-alike header.

GPL-3.0-or-later source and original Crow notices are retained. The matching
`hover-translate-0.4.1-Source.tar.gz` includes all application/extension/build/setup
source and tests. SHA256SUMS covers the installer and source. Dictionaries/models
are downloaded separately with their original notices.
