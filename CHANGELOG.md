# Changelog

## 0.4.1 — separate distant labels and improve word lookup

- Large horizontal gaps split an OCR line into text runs in Line and Sentence
  modes. Blank space between distant table cells does not select either cell.
- Word is the default for new settings; saved mode choices are preserved.
- Chinese Word lookup uses actual character boxes and longest known CC-CEDICT
  matches. English reverse lookup handles common inflections and labels the
  matched base word; exact entries take priority. Coverage is still limited.
- Optional source highlighting and temporary Shift → Word / Ctrl+Shift → Sentence
  overrides, restoring the saved mode when released.
- Popup Copy translation, Pin/Unpin and Close actions. A short pointer-movement
  grace period allows entry; pinned popups stop subsequent capture until closed.
  Only explicit Copy writes the clipboard; no persistent history is saved.
- GNOME extension version 4 is required. Reinstall it through Set up GNOME hover
  and sign out/in after upgrading. Old extensions are paused with an upgrade hint.
- Added scaled gap-selection, actual Chinese character/dictionary OCR, English
  inflection, X11 Copy/Pin and native GNOME modifier/highlight/control tests.

## 0.4.0 — word, line and bounded sentence hover

- Added saved **Hover text** modes: Word, Line and Sentence. Line remains the
  default for existing and new settings; changes apply to GNOME and X11 hover.
- Word mode uses actual OCR word boxes, removes surrounding punctuation and
  avoids selecting a neighboring word from blank space. Chinese token boundaries
  depend on Tesseract and may select one character rather than a dictionary word.
- Sentence mode uses paragraph-aware OCR, pointer-to-word offsets and punctuation
  to join wrapped text within the same paragraph/column, at most three lines and
  300 characters. Uncertain or longer joins fall back to the current line;
  a current line over 300 characters is not selected in Sentence mode.
- Added actual OCR wrapped-sentence tests and native GNOME mode-switch checks.
- Documented optional dictionary updates and the need to restart after an update.
  Existing offline models and the GNOME extension protocol are unchanged.

## 0.3.0 — GNOME Wayland hover

- GNOME 50 extension reads the compositor pointer, captures only the hovered
  window/monitor region, and displays a shell popup with Escape dismissal.
  Capture and translation are canceled when the pointer moves, the app pauses,
  the desktop locks, or the backend disconnects. Screenshots stay in memory.
- Per-user GNOME setup button installs/enables this extension, preserves other
  extension settings, and backs up previous copies. Sign out/in after setup.
- Model/dictionary downloader identifies the app with an honest user-agent,
  fixing the official hosts' HTTP 403/1010 response to Python urllib's default.
- Deterministic bounded sentence splitting replaces incompatible legacy Stanza
  metadata, without downloading another runtime model or changing installed
  libraries. Actual Argos English/Chinese models work in both directions.
- OCR enlarges small text, normalizes dark backgrounds, adds crop margins and
  retries segmentation. Geometry remains in original image coordinates.
  Low-confidence region text remains editable instead of being discarded.
- Added bridge/stale-result/HiDPI/OCR/download checks and a real GNOME 50
  pointer/capture/OCR/model/popup test on Ubuntu 26.04. Physical user desktops,
  fractional monitor scaling and other GNOME versions remain unverified.

## 0.2.0 — offline translation and screen regions

- Offline is now the default: persistent Argos worker, two translation
  directions, and optional CC-CEDICT word definitions/reverse lookup.
- One-time, per-user CPU runtime/model/dictionary setup, preserving TLS
  verification and dictionary/model notices; system Python is not modified.
- Translation worker blocks network connections and never falls back online.
- Wayland/X11 screenshot portal capture, local region preview, OCR, editable
  source text and Ctrl+Enter translation. Desktop shortcuts can use `--capture`.
- Wayland disables the unsupported automatic-hover checkbox; X11 hover remains.
- Ubuntu 26.04 / Qt 6.10 build compatibility and native dependency calculation.
- Added dictionary and portal/OCR/cropping/cancellation tests. Actual sentence
  model checks remain blocked by cloud network policy; see docs/TESTING.md.

## 0.1.0 — initial MVP

- X11 hover capture limited to the window under the pointer, with local
  Tesseract English/Simplified Chinese OCR and line coordinates.
- A saved English or Simplified Chinese target and configurable Mozhi server.
- Non-focusable translation popup, Escape/movement dismissal, and pause controls.
- Cancellation of stale jobs, a bounded translation cache, and server timeouts.
- Settings, privacy explanation, GPL notices and About dialog.
- Headless OCR/translation diagnostics and real X11/OCR integration tests.
- Debian 13 amd64 packaging and corresponding source archive.
- Crow Translate 4.1.0 provenance, preserved copyright notices, and marked
  imported-file modifications under GPL-3.0-or-later.

Initial limits: X11 only, one monitor, 100% scaling, two languages, and online
translation through a user-selected Mozhi instance. Public-server availability
is not guaranteed; see [README.md](README.md) and [docs/TESTING.md](docs/TESTING.md).
