# Changelog

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
