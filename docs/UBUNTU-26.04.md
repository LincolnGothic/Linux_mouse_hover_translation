# Ubuntu 26.04 quick start

1. Install the new **amd64** package:

   ```bash
   sudo apt install ./hover-translate_0.2.0_amd64.deb
   hover-translate
   ```

2. Leave Translation set to **Offline**, click **Install offline models**, and
   keep the app open until setup finishes. This one-time download needs internet
   and storage for CPU libraries and two models. Click **Test translation**
   afterward. No API account or key is needed.

3. Set the target language. Click **Translate screen region**, approve/select
   the screenshot in Ubuntu's desktop dialog, then drag over text in the local
   preview. The reader shows recognized text and its translation. Correct OCR
   text if necessary and press **Ctrl+Enter** to translate again.

4. Add a shortcut in **Settings → Keyboard → Custom Shortcuts** (possibly
   inside “View and Customize Shortcuts”). Use `hover-translate --capture`
   and choose an unused key combination.

Ubuntu normally uses Wayland. Automatic hover is available only under Xorg;
region and typed-text translation are this version's Wayland workflows.

If capture reports an unavailable portal on standard Ubuntu GNOME:

```bash
sudo apt install xdg-desktop-portal xdg-desktop-portal-gnome
```

Sign out and back in afterward. Other desktops need their own portal backend.

For setup errors, run `hover-translate-offline-setup` in a terminal. Downloads
use PyPI, the official PyTorch CPU index, Argos's model host, and MDBG's
CC-CEDICT export. Corporate proxies may need their managed CA configuration;
keep TLS verification enabled.

Files live under `~/.local/share/hover-translate`. Optional settings can point
at an existing Python runtime and Argos package directory. Download only models
with `hover-translate-offline-setup --models-only`; retry the optional dictionary
with `hover-translate-offline-setup --dictionary-only`.

CC-CEDICT supplies Chinese word definitions and limited English reverse lookup;
sentences use Argos. Test representative English and Chinese sentences before
relying on translation quality.

GitHub releases supply the Ubuntu installer, matching complete source archive
and checksums together. Check the selected release's validation notes.
