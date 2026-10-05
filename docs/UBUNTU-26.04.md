# Ubuntu 26.04 / GNOME 50 quick start — 0.4.2

## Upgrade

Quit the old app through its tray menu first. Install the amd64 package:

```bash
sudo apt install ./hover-translate_0.4.2_amd64.deb
/usr/bin/hover-translate --version
/usr/bin/hover-translate
```

Version must show **0.4.2**. An older local build or launcher can shadow the
packaged binary; `/usr/bin/hover-translate` explicitly starts the new package.
The upgrade preserves app settings and offline models. From 0.4.1, quit, upgrade
and reopen; the unchanged version 4 GNOME extension needs no reinstall/logout.
From 0.4.0 or earlier, use Hover → Set up GNOME hover, then sign out/in.
Do not delete your configuration.

## Settings layout

Hover controls and GNOME setup are in **Hover**. Engine, model installation and
test translation are in **Translation**. Optional resource paths are in
**Advanced**. The status and Apply/Close/reader actions remain visible; use the
vertical wheel/scrollbar when a tab is taller than the window. No horizontal
scrolling is needed.

## Automatic hover on Wayland

1. In **Hover**, click **Set up GNOME hover**. This installs the extension for your account
   and enables its own UUID while preserving other extension settings. Earlier
   copies are backed up under `~/.local/share/hover-translate/extension-backups`.
2. **Sign out and back in**, then reopen Hover Translate. GNOME loads the
   extension at login. Check its state in GNOME Extensions if needed:

   ```bash
   gnome-shell --version
   gnome-extensions info hover-translate@lincolngothic.github.io
   echo "$XDG_SESSION_TYPE"
   ```

3. Keep **Translation → Offline**, click **Install offline models**, and keep
   the app open until setup completes. Set the desired target and click
   **Test translation**. Models and dictionary are downloaded separately.
4. Choose **Hover text → Word, Line or Sentence**, then check **Enable hover translation**, click **Apply**, and pause over a line
   in another app. Move the pointer or press **Escape** to dismiss the popup.

Only GNOME 50 is declared compatible. Do not disable extension version checks.
If GNOME Extensions globally disables user extensions, turn its main switch
on yourself; setup preserves that switch. Managed desktops may forbid user
extensions. KDE and other Wayland compositors need a separate integration.
If you disable the extension or quit the app, automatic capture stops.
Without a tray, keep an app window open.

## OCR and translation troubleshooting

- **Hover checkbox unavailable:** extension is not active, the shell version
  is unsupported, or this is another Wayland compositor. Set up the extension
  and sign out/in; reinstalling translation models does not activate it.
- **No readable text found:** OCR failed before translation. Enlarge the
  original text and select a few complete lines with a margin. Leave the OCR
  model folder blank to use `tesseract-ocr-eng` and `tesseract-ocr-chi-sim`.
  Low-confidence text remains available for correction in 0.3.0.
- **No translation:** first test typed text to separate OCR from translation.
  Run `hover-translate-offline-setup` for the full setup error. The fixed
  downloader identifies Hover Translate to official model/dictionary hosts;
  TLS verification stays enabled. Network/proxy issues can still fail downloads.
- First model use loads CPU libraries and may be slow. Later requests reuse
  the worker while the app is running. OCR and model quality need review.

Optional Python, model and dictionary paths can use an existing local setup;
leave them blank for per-user defaults under `~/.local/share/hover-translate`.
Target **Simplified Chinese** for English input; **English** for Chinese input.

```bash
hover-translate-offline-setup
hover-translate-offline-setup --models-only
hover-translate-offline-setup --dictionary-only
```

CC-CEDICT supplies word definitions and limited exact English reverse lookup;
sentences require the Argos models. Full model/dictionary downloads and real
sentence translation have been validated with 0.3.0.

## Hover text and dictionary updates

Word is the default for new settings; saved choices are retained. Chinese lookup
uses actual character boxes and longest known CC-CEDICT matches, falling back to
OCR tokens. Line/Sentence split distant labels at large horizontal gaps, including
inside an OCR line. Sentence still limits joining to three lines/300 characters.
Use the source highlight to check selection. Shift temporarily uses Word and
Ctrl+Shift uses Sentence; releasing restores your saved mode. Both modifiers and
highlighting can be disabled. Enter the popup promptly to copy, pin/unpin or close
it. Pin pauses further captures; Esc, pause, lock, app exit and extension disable
clear it. Only Copy changes the clipboard, and no persistent history is stored.

CC-CEDICT does not expire. Optional updates every 1–3 months can add entries:
`hover-translate-offline-setup --dictionary-only`. Restart afterward to clear
cached definitions. This does not refresh the Argos sentence models.

## Manual capture

Click **Translate screen region**, approve/select a screenshot in the desktop
permission dialog, then drag around text in the preview. Enter uses the whole
image, Escape cancels. Ctrl+Enter translates edited text again.
A custom keyboard shortcut can invoke `hover-translate --capture`.

If GNOME manual capture reports an unavailable portal:

```bash
sudo apt install xdg-desktop-portal xdg-desktop-portal-gnome
```

Sign out/in after installing the backend. Other desktops require their own
portal backend. The automatic GNOME extension captures through the compositor;
its operation does not depend on this manual portal dialog.

## Limits

0.4.2 remains a preview: tested in headless GNOME 50.1 on Ubuntu 26.04,
including native Wayland targets and real models in both directions.
Physical desktops, fractional monitor scaling, other GNOME releases and
actual manual screenshot permission dialogs remain unverified. X11 hover
retains its single-monitor / 100% scaling limit. See [testing](TESTING.md).
