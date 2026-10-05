# Hover Translate 0.3.0 — GNOME Wayland hover (preview)

Automatic translation where you point now has a GNOME 50 Wayland integration
for Ubuntu 26.04 amd64. The included extension reads the compositor pointer,
captures the hovered line locally and shows a shell popup beside it.

## Install and enable

Download `hover-translate_0.3.0_amd64.deb`, quit the older app through its tray
menu, then run `sudo apt install ./hover-translate_0.3.0_amd64.deb`.
Launch `/usr/bin/hover-translate`:

1. Click **Set up GNOME hover**, then **sign out and back in**.
2. Keep **Offline** selected and install the offline models. Keep the app open
   until the one-time download finishes; click **Test translation**.
3. Choose the target language, enable hover and click **Apply**.
   Pause over a line in another app. Move the pointer or press Escape to dismiss.

Requires **GNOME 50**. Other Wayland compositors are not supported by this
extension. Setup preserves other extensions and backs up earlier copies.
If user extensions are globally disabled, use GNOME Extensions to turn them on.
Models/runtime/dictionary are downloaded separately and not bundled.

## Fixes and validation

- Official model/dictionary downloads now identify Hover Translate truthfully,
  fixing the HTTP 403/1010 response to Python's default downloader identity.
- Bounded local sentence splitting fixes incompatibility between legacy Argos
  model Stanza resources and current libraries, with no hidden model downloads.
- Small/dark OCR crops gain scaling, normalization, margins and segmentation
  fallback. Low-confidence region text remains available for correction.
- GNOME hover cancels stale work on pointer movement, pause, lock/overview,
  extension disable or app exit. Screenshots stay in memory; the clipboard is
  unchanged. Escape is grabbed only while the popup is visible.
- Five regular suites pass; native Wayland portal-fixture checks pass.
- A real headless GNOME 50.1 / Mutter 50.1 compositor on Ubuntu 26.04 passes
  native Wayland pointer → screenshot → OCR → offline model → shell popup checks
  in both directions, plus Escape/movement/pause behavior.
- Full official Argos en→zh/zh→en model and MDBG CC-CEDICT downloads succeed.
  Actual model smoke outputs: `Hello world → 哈罗世界`; `你好世界 → Hello, world.`

## Preview limits and open source

Physical desktops, GPU drivers, fractional/mixed monitor scaling, other GNOME
versions and actual manual screenshot permission dialogs remain unverified.
Translation/OCR quality is imperfect; review results. X11 hover retains its
single-monitor / 100% scaling limit. No Shell Eval or GNOME unsafe mode is used.

The application and original extension are GPL-3.0-or-later, retaining Crow
Translate 4.1.0 notices and marked changes. `hover-translate-0.3.0-Source.tar.gz`
is the complete corresponding source, including the extension and build/setup
scripts. Download it beside the installer. SHA256SUMS covers both files.
Separate model/dictionary downloads retain their own licenses and attribution.

[Ubuntu guide](https://github.com/LincolnGothic/Linux_mouse_hover_translation/blob/v0.3.0/docs/UBUNTU-26.04.md)
· [Validation](https://github.com/LincolnGothic/Linux_mouse_hover_translation/blob/v0.3.0/docs/TESTING.md)
