# Hover Translate 0.4.0 — word, line and sentence hover (preview)

Choose **Hover text → Word, Line or Sentence**, then click **Apply**. Word selects
only the OCR word under the pointer; Line preserves the previous visual-line
behavior and remains the default. Sentence follows wrapped text in one OCR
paragraph/column, limited to three lines and 300 characters. It falls back to
the current line for uncertain/long joins and skips single lines over 300 characters.
The popup shows the selected source text for checking. Manual region selection
remains available for longer text.

## Upgrade on Ubuntu 26.04 amd64

Quit the older running app, then install `hover-translate_0.4.0_amd64.deb` with
`sudo apt install ./hover-translate_0.4.0_amd64.deb` and reopen `/usr/bin/hover-translate`.
Existing settings/models are retained. The GNOME 50 extension is unchanged, so
a working 0.3.0 installation does not require extension setup or signing out.
For first-time installation, follow the included Ubuntu guide to set up the
GNOME extension and download the separate offline models.

Dictionary updates are optional: CC-CEDICT never expires. Run
`hover-translate-offline-setup --dictionary-only` and restart to clear cached
entries. This does not update the Argos sentence models.

## Validation and limits

Five regular suites cover real OCR, selection boundaries, settings, X11 hover,
GNOME bridge, dictionary, cancellation and screenshot-portal fixtures. A native
GNOME 50 Wayland test switches Word/Line/Sentence using actual compositor
capture/OCR/popups, then exercises real Argos models in both directions.
The native Weston reader and GNOME timing/geometry checks also pass.

Chinese Word mode depends on OCR tokens and can select one character. Sentence
selection uses punctuation heuristics; abbreviations, crop edges and OCR errors
can cause incomplete selection. Physical desktop/fractional-scaling and manual
portal permission dialog limits remain as documented. GNOME 50 is the supported
Wayland compositor; X11 requires one monitor at 100% scaling.

The app is GPL-3.0-or-later, retaining Crow notices and marked imported changes.
The matching `hover-translate-0.4.0-Source.tar.gz` includes the complete extension,
build/setup scripts and tests; SHA256SUMS covers the installer and source.
Models/dictionary are downloaded separately with their original notices.
