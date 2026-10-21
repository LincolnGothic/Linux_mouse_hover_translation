# Windows x64

The current 0.7.0 source adds **Google Translate — free, experimental**, refreshed
settings/popups and missed-hover fixes. The latest downloadable preview below is
0.6.0 until 0.7.0 artifacts are published. Both platforms use **Hover Translate**;
the repository is `LincolnGothic/hover-translate`. Existing app/data IDs are retained.

Hover Translate 0.6.0 is a native Qt Windows preview. It supports automatic
Word, Line and Sentence OCR under the pointer, source highlighting,
Copy/Pin/Close popups, Escape dismissal, typed text and manual region selection.
Manual capture previews the primary screen. OCR and screenshots remain local.

## Install

Download `hover-translate-0.6.0-windows-x64.zip` and `SHA256SUMS` from the
[v0.6.0 release](https://github.com/LincolnGothic/hover-translate/releases/tag/v0.6.0).
Check the ZIP hash with `Get-FileHash -Algorithm SHA256`, extract it, open
PowerShell in the extracted `HoverTranslate-0.6.0` folder, then run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install-windows.ps1 -InstallDirectory D:\software\HoverTranslate\app-0.6.0
```

The installer creates a new folder and a desktop shortcut. It does not change
the global execution policy or add autostart. Offline setup downloads private
CPython 3.10.11, CPU PyTorch, Argos 1.11.0, English/Chinese 1.9 sentence models
and optional CC-CEDICT into the app's `data` folder. An initial internet
connection and sufficient free disk space are required; later Offline
translation does not use the network. Runtime/model download manifests record
URLs and SHA256 values. Do not move only the EXE; keep the complete folder.

To install without downloading the translation runtime, add `-SkipOfflineSetup`.
Later click **Translation → Install offline models**. OCR models are bundled.
Setup errors remain visible; retry setup to resume completed resources.

Open **Hover Translate** from the desktop shortcut, test translation, choose
the target language and Word/Line/Sentence mode, enable hover and click Apply.
The app starts paused for new settings. Hold Shift for Word or Ctrl+Shift for
Sentence. Click Pin to keep a result; Escape dismisses it. The clipboard changes
only when Copy is clicked. Closing settings keeps the tray available; use
**Quit** in the tray menu to stop the app.

Settings are `data/settings.ini`. Offline resources are `data/argos-env`,
`data/argos-packages` and `data/cedict.u8`. Logs/download caches stay beneath
`data`. All paths follow the installed application folder.

## Optional online providers

Offline remains the default. In 0.7.0, choose **Google Translate — free,
experimental** for keyless text translation directly through Google. No offline
translation runtime is needed; `-SkipOfflineSetup` avoids that optional download.
Screenshots stay local. Google's unofficial endpoint may change, block or
rate-limit requests; no provider is used as an automatic fallback.

Azure and Mozhi are also explicit choices. Azure sends
only source/recognized text to Microsoft; screenshots remain local. Configure
your own Azure resource key in the application's masked local dialog. Keys use
Windows Credential Manager for the current user, scoped to this installed
folder, and are never written to INI settings or monthly usage metadata.
Microsoft quotas/pricing apply. Moving the folder changes the credential
identity; reconfigure Azure for that folder. Mozhi sends text to the selected
server. Online choices never change automatically.

## Build and verify

Install the official [MSYS2 UCRT64 environment](https://www.msys2.org/), then:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-scxml \
  mingw-w64-ucrt-x86_64-qt6-svg mingw-w64-ucrt-x86_64-tesseract-ocr \
  mingw-w64-ucrt-x86_64-tesseract-data-eng mingw-w64-ucrt-x86_64-tesseract-data-chi_sim
```

From PowerShell in the source folder:

```powershell
.\tools\build-windows.ps1 -MsysRoot C:\msys64 -BuildDirectory .\build-windows
python tools/package-windows.py --source . --build build-windows --prefix C:/msys64/ucrt64 --output release-windows
```

The build runs shared core/settings/Google/Azure tests, native controlled-window
capture/hover tests and offline setup tests. The native suite needs an unlocked
interactive desktop; do not use the mouse or lock the session while it runs.
Real-model validation requires separately installed models:

```powershell
python tests/check_offline_models.py build-windows/hover-translate.exe --python <runtime-python> --models-dir <argos-packages>
```

The ZIP contains transitive runtime DLLs, Qt plugins, English/Chinese Tesseract
data, application notices and dependency licenses. It is accompanied by the
complete matching GPL application source and checksums. Runtime DLLs remain
replaceable; MSYS2 library recipes/source references are available from
https://github.com/msys2/MINGW-packages and https://packages.msys2.org/.

## Preview limits

OCR quality depends on font size, contrast and layout. Protected content,
elevated windows and secure desktops are not guaranteed capture targets.
Locking the desktop cancels hover. Multi-monitor and mixed-scale layouts use
monitor-local coordinate conversion; only configurations actually exercised
by the native tests are validated. Manual region selection currently uses the
primary monitor. See the release notes for recorded validation.
