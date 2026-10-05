# Hover Translate

[English](README.md) · [简体中文](README.zh-CN.md)

Open-source **English ↔ Simplified Chinese** translation where you point.
Version **0.4.2** provides a tabbed settings window that fits smaller screens, with fixed actions and working vertical scrolling.
Automatic hover on **Ubuntu 26.04 / GNOME 50 Wayland** uses the included GNOME extension. Local Tesseract OCR, Argos sentence
translation and optional CC-CEDICT word definitions require no API key.
Licensed **GPL-3.0-or-later**, with preserved [Crow notices](NOTICE.md) and
[upstream provenance](docs/UPSTREAM.md).

**This is a preview.** Real pointer capture, OCR, sentence translation and
shell popups pass in a headless GNOME 50 compositor. Physical desktops,
fractional scaling and other GNOME versions remain unverified. The installer
does not bundle translation models. See [validation](docs/TESTING.md).

## Install on Ubuntu 26.04 amd64

[Download the release](https://github.com/LincolnGothic/Linux_mouse_hover_translation/releases/tag/v0.4.2),
including the installer, matching complete source and checksums.
Quit an older running app before upgrading:

```bash
sudo apt install ./hover-translate_0.4.2_amd64.deb
/usr/bin/hover-translate
```

1. In **Hover**, click **Set up GNOME hover**. The helper installs/enables this extension for
   your account, preserves other extensions and backs up earlier copies.
   **Sign out and back in**, then reopen Hover Translate. GNOME loads the new
   extension at login; the hover checkbox becomes available.
2. In **Translation**, keep the engine **Offline**, click **Install offline models**, and keep
   the app open until installation finishes. This downloads a private CPU
   Python runtime, Argos Translate 1.11.0, both 1.9 translation models and an
   optional dictionary under `~/.local/share/hover-translate`.
3. Set the target to **Simplified Chinese** for English → Chinese, or
   **English** for Chinese → English. Click **Test translation**.
4. Choose **Hover text → Word, Line or Sentence**. Check **Enable hover translation** and click **Apply**. Pause over a line
   in another app. Move the pointer or press **Escape** to dismiss the popup.

The extension requires **GNOME 50**. Do not disable GNOME version checks.
If user extensions are globally disabled, turn them on in GNOME Extensions;
the helper deliberately preserves that global setting. Keep the app running:
closing the last window on a desktop without a tray quits the app.
Detailed help: [Ubuntu guide](docs/UBUNTU-26.04.md).

0.3.0 fixes the official hosts' HTTP 403/1010 rejection of Python's default
user-agent by truthfully identifying Hover Translate. It also replaces
incompatible legacy Stanza sentence metadata with bounded local splitting.
Translation stays offline, with no hidden sentence-model downloads.
For setup details/errors, run `hover-translate-offline-setup` in a terminal.
The earlier v0.2.0 does not contain Wayland hover support or these fixes.

## Settings window

- **Hover:** enable translation, target language, Word/Line/Sentence, delay, highlighting, shortcuts and GNOME setup.
- **Translation:** offline/online engine, Mozhi server, dictionary toggle, model installation and test translation.
- **Advanced:** optional OCR, Python, translation-model and dictionary paths.

**Screen region**, **Translate text**, status, **About**, **Apply** and **Close** stay
visible outside the scrolling pages. Smaller windows scroll vertically with the
mouse wheel or scrollbar; there is no horizontal scrollbar. Changes take effect
when you click Apply. Saved choices and offline downloads are preserved.

## Desktop workflows

| Mode | Desktop | Interaction |
| --- | --- | --- |
| Automatic hover | GNOME 50 Wayland + included extension | Set up the extension, sign out/in, enable hover and pause over a line |
| Automatic hover | X11/Xorg, one monitor at 100% scaling | Enable hover, apply settings and pause over a line |
| Screen region | Wayland/X11 + screenshot portal/backend | Approve a screenshot, then drag around text in the preview |
| Typed text | Wayland/X11 | Type or paste text, then press Ctrl+Enter |

**Hover text** controls automatic hover only:

- **Word:** translates the OCR word under the pointer, removing surrounding
  punctuation. This is the **default for new settings**; saved modes are preserved.
  Chinese character boxes are matched against CC-CEDICT for the longest known
  word containing the pointer. This is dictionary matching, not a full linguistic
  segmenter; without a matching entry, OCR tokens remain the fallback.
- **Line:** nearby text on the current visual line. **Large horizontal gaps split
  the line into separate runs**, so distant table headers are not translated together.
- **Sentence:** follows punctuation across nearby wrapped lines in the same
  OCR paragraph and column, limited to **3 lines and 300 characters**. Different
  columns, paragraph breaks and large horizontal/vertical gaps stop joining. When a complete bounded
  sentence cannot be found, the current line is used instead; lines longer than
  300 characters are not selected in this mode. Abbreviations and OCR errors can
  confuse punctuation-based sentence boundaries. The popup shows selected source
  text so you can check the result. Use manual region selection for longer text.

Enable **Highlight selected source text** to see exactly which recognized text
will be translated. Hold **Shift** for a temporary Word lookup, or **Ctrl+Shift**
for Sentence; releasing the keys restores the saved mode. You can disable these
modifiers in settings. Move into the popup promptly (a 350 ms grace period allows
this), then click **Copy translation**, **Pin/Unpin** or **Close**. A pinned popup
stays until closed, Esc, pause, lock/overview, app exit or extension disable.
Only an explicit Copy click writes to the clipboard. Pinning pauses further hover
lookups until the popup is closed/unpinned; no permanent history is saved.

**Upgrading from 0.4.1:** quit the old process, install the new package and reopen.
The GNOME extension remains version 4; no reinstall or logout is needed if it is
already active. From 0.4.0 or earlier, click **Hover → Set up GNOME hover**, then
sign out and back in. App settings and downloaded models are retained.

GNOME hover uses the compositor's pointer and screenshot APIs. Capture is
limited to a 700 × 160 logical-pixel region clipped to the hovered monitor
and window. Images travel in memory over your local session bus; no screenshot
files or clipboard writes are needed. GNOME draws the popup near the pointer,
including above fullscreen windows. Escape is grabbed only while it is shown.
Lock/overview, movement, pause, app exit and extension disable invalidate work.
The app does not use Shell Eval or enable GNOME unsafe mode. The extension is
original GPL source included in both the installer and corresponding source.

KDE and other Wayland compositors need their own integration; this extension
does not enable hover on them. Mixed-scale/fractional monitor behavior needs
physical-desktop testing. Small, stylized or blurred text can still defeat OCR.
0.3.0 enlarges small crops, normalizes dark backgrounds, adds margins and retries
segmentation. Low-confidence region text stays editable instead of disappearing.

Manual capture uses the desktop screenshot permission dialog. Select a few
complete lines in the local preview; Enter uses the whole image and Escape
cancels. Correct OCR text and press Ctrl+Enter to translate again. An optional
keyboard shortcut can run `hover-translate --capture`. Actual GNOME portal
permission dialogs remain separate from the automatic extension's validated
capture path. GNOME manual capture needs `xdg-desktop-portal-gnome`.

## Dictionary and privacy

CC-CEDICT primarily supplies Chinese → English definitions, with limited exact
English-definition reverse lookup. English lookup also tries common inflections
(e.g. running → run), labeling the matched base word. Exact entries take priority.
This improves lookup but does not make CC-CEDICT a comprehensive English–Chinese dictionary. Unmatched words and sentences use Argos;
uncheck **Show dictionary definitions** to force sentence-model translation.
The dictionary has no expiry and works without scheduled updates. Updating
occasionally (for example, every 1–3 months) is optional and may add/correct
entries. Run `hover-translate-offline-setup --dictionary-only`, then restart the
app to clear cached definitions. This explicitly downloads the current dictionary
and preserves its notices; it does not update Argos sentence models.

Optional path fields can point at an existing Python runtime, Argos package
folder or CC-CEDICT `.u8` file. Blank fields use installed defaults.

The offline worker blocks network connections. Only explicitly requested setup
downloads resources; system Python is unchanged. OCR/translation are local,
with a bounded memory cache and no saved text history. Models and dictionary
retain their complete upstream notices and are downloaded separately.
**Online → Mozhi** explicitly sends recognized text to the chosen server,
which may forward it to Google. Public instances may fail or rate-limit;
offline errors never automatically switch to an online provider.

Settings normally live in `~/.config/LincolnGothic/HoverTranslate/settings.ini`.

## Build and validate

Ubuntu 26.04 or Debian 13, Qt 6.8+:

```bash
sudo apt install build-essential cmake ninja-build pkg-config \
  qt6-base-dev qt6-base-dev-tools qt6-scxml-dev qt6-svg-plugins qt6-wayland \
  libtesseract-dev libleptonica-dev libxcb1-dev \
  tesseract-ocr-eng tesseract-ocr-chi-sim fonts-noto-cjk python3-venv
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=OFF
cmake --build build --parallel 3
./build/hover-translate
```

For the six regular test suites, install `libxcb-xtest0-dev xvfb xauth
x11-utils dbus-x11 weston`, build with `-DBUILD_TESTING=ON`, then run:

```bash
dbus-run-session -- xvfb-run -a -s '-screen 0 1200x900x24' \
  ctest --test-dir build --output-on-failure
bash tools/test-wayland.sh build
node tests/test_gnome_policy.mjs
```

A real isolated GNOME test additionally needs `gnome-shell gjs python3-gi
python3-cairo gir1.2-gtk-4.0 adwaita-icon-theme`:

```bash
bash tools/test-gnome.sh build
```

The helper creates private D-Bus/configuration directories and a headless GNOME
compositor; it never modifies the user's existing desktop session. Without
`HOVER_GNOME_MODEL_PYTHON` and `HOVER_GNOME_MODELS_DIR` it exercises a labeled
dictionary fixture. With them it also verifies real sentence-model hover in
both directions. See [test details](docs/TESTING.md).

The rootless cloud SDK is activated with `source tools/activate-cloud.sh`.
For reproducible Ubuntu/Docker packaging and all real-model/GNOME checks:

```bash
bash tools/build-ubuntu26.04.sh --model-check
```

## Command line and distribution

```bash
hover-translate --capture
hover-translate --reader
hover-translate --ocr image.png
hover-translate --translate 'Hello world' --target zh-CN --no-dictionary
hover-translate --translate '你好世界' --target en --no-dictionary
```

OCR, translation, help and version work without a display. Use `--python`,
`--models-dir` and `--dictionary` for custom offline paths.
`--provider mozhi --instance https://your-mozhi-host` explicitly selects online
translation; `--instance` alone preserves the old online CLI behavior.
`--provider offline` takes precedence.

Keep the GPL, copyright notices and marked modifications. Share the binary
with its **complete matching source**, including the extension and build/setup
scripts: `hover-translate-0.4.2-Source.tar.gz`. Separately redistributed models
and dictionary data have their own licenses. See [release instructions](docs/RELEASE.md),
[notices](NOTICE.md), [contributing](CONTRIBUTING.md) and [changelog](CHANGELOG.md).
