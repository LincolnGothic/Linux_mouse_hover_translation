# Hover Translate 0.1.0 — open-source X11 MVP

Pause over an English or Simplified Chinese line to get a nearby translation
popup. OCR runs locally with Tesseract. Translation uses the user's selected
Mozhi instance and its Google engine. The app preserves focus, text selections,
and the clipboard, and cancels outdated requests when the pointer moves.

Included: language/server settings, adjustable dwell delay, pause/tray controls,
Escape dismissal, a bounded memory cache, timeout/error handling, About/licenses,
and headless OCR/translation diagnostics.

This derivative uses pinned Crow Translate 4.1.0 components, preserves upstream
copyright notices, marks modified files, and is GPL-3.0-or-later. It is independently
maintained. Corresponding source is provided alongside the binary.

## Downloads and installation

- `hover-translate_0.1.0_amd64.deb`: Debian 13 amd64 binary package.
- `hover-translate-0.1.0-Source.tar.gz`: matching complete source, build scripts,
  tests, documentation, and license notices.
- `SHA256SUMS`: checksums for those two files.

```bash
sha256sum -c SHA256SUMS
sudo apt-get install ./hover-translate_0.1.0_amd64.deb
hover-translate
```

Start paused, test the selected server, choose the target language, then enable
hover and apply. The binary depends on system Qt/Tesseract libraries, language
models and CJK fonts; it does not bundle them. Source builds need Qt 6.8+.

## Validation and limits

Both functional suites pass with real Tesseract English/Chinese OCR, a real X11
reading window, a local Mozhi-compatible server, and actual GUI/CLI processes.
Packaging, corresponding source and installation-directory permissions were
checked. See [docs/TESTING.md](TESTING.md) for the precise scope.

Supported: **X11/Xorg, one monitor, 100% scaling, English and Simplified Chinese**.
Native Wayland, multiple monitors, fractional scaling and offline translation
are outside this initial MVP. OCR can miss small or low-contrast text.

Public translation availability has not been established: the default server
returned a rate-limit error during the publication check. Configure a reachable
trusted compatible instance or retry later. Recognition text is sent to that
server and may be forwarded to Google; screenshots remain local.
