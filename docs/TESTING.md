# Validation and known limitations — 0.3.0

Build platforms: Debian 13 / Qt 6.8.2 and Ubuntu 26.04 amd64 / Qt 6.10.2 /
GCC 15.2. The native Ubuntu runtime uses Python 3.14, CPU PyTorch 2.14.1 and
Argos Translate 1.11.0. TLS uses trusted CA bundles; verification is enabled.

## Regular suites and native Wayland client

```bash
dbus-run-session -- xvfb-run -a -s '-screen 0 1200x900x24' \
  ctest --test-dir build --output-on-failure
bash tools/test-wayland.sh build
node tests/test_gnome_policy.mjs
```

| Suite | Evidence |
| --- | --- |
| core | Timing, languages, OCR geometry, small/dark text, settings, local HTTP fixture, dictionary worker, cancellation/timeouts and missing runtime |
| hover_x11 | Actual X11 capture/OCR/popup, Escape/pause/movement/cache and focus/selection/clipboard preservation |
| portal_capture | Screenshot D-Bus fixture, actual crop/OCR/dictionary, invalid URI, cancellation/timeout and clipboard preservation |
| gnome_bridge | Local extension-protocol fixture, actual OCR/dictionary, HiDPI ratios, invalid/stale captures and canceled work |
| offline_setup | Truthful downloader identity, complete content, TLS redirect downgrade/truncation rejection and local sentence chunks |

The Qt suites use real Tesseract and Python dictionary lookup; synthetic
samples and local services are labeled. The Weston helper additionally runs
the portal suite with Qt's native Wayland backend. This does not validate an
actual GNOME portal permission dialog.

## Real GNOME 50 automatic hover

`tools/test-gnome.sh` starts an isolated headless **GNOME Shell 50.1 / Mutter
50.1** with a 1200×900 virtual monitor and software rendering. It uses private
session/system buses and user configuration, never an existing desktop.
`setup_gnome.py` installs/enables the actual extension. Tests then use native
Wayland windows and Mutter's compositor input API for actual pointer/keyboard
movement. Screen capture comes from `Shell.Screenshot`, not a PNG fixture.
No Shell Eval or unsafe mode is used.

The live test verifies dwell capture, real OCR, shell popup contents/visibility,
Escape dismissal without immediate reappearance, pointer movement and pause.
It first checks a test-authored dictionary; when real model paths are supplied,
it disables the dictionary and checks English→Chinese and Chinese→English
hover translation using the downloaded models and actual offline worker.

```bash
export HOVER_GNOME_MODEL_PYTHON=/path/to/offline-data/argos-env/bin/python
export HOVER_GNOME_MODELS_DIR=/path/to/offline-data/argos-packages
bash tools/test-gnome.sh build
```

Without those variables the GNOME run explicitly reports dictionary-only
validation. This is not evidence of sentence-model translation. Current
0.3.0 native validation supplied both variables and passed the complete flow.

GNOME logs warnings for OS services absent from the isolated container
(logind/GDM/Polkit/calendar/network), but the actual compositor, native clients,
pointer input, screenshot API, extension and popup are used. The temporary
system bus has no host services and does not substitute for these APIs.

## Official downloads and real sentence models

The previous HTTP 403/1010 failure was reproduced with Python urllib's default
user-agent. The same official URLs accept the truthful HoverTranslate identity.
0.3.0 downloads complete archives, retains TLS/ZIP/path checks and records
SHA-256 manifests. Both models and the full MDBG CC-CEDICT dictionary downloaded
successfully; license/README headers are retained.

Argos 1.9 models contain legacy Stanza resources incompatible with current
Stanza. The worker now uses deterministic bounded sentence chunks instead of
that secondary sentence model. The installed runtime/model files are unchanged;
CTranslate2 still runs the real Argos/OPUS-MT translation models. Socket
connections remain disabled in the worker.

```bash
python3 offline/setup_offline.py --data-dir /path/to/offline-data
python3 tests/check_offline_models.py ./build/hover-translate \
  --python /path/to/offline-data/argos-env/bin/python \
  --models-dir /path/to/offline-data/argos-packages
```

The check uses offline mode and disables the dictionary. Observed outputs:
`Hello world → 哈罗世界` and `你好世界 → Hello, world.` The Chinese smoke check
accepts either 你好 or 哈罗 for hello, plus 世界 for world. These are functionality
checks, not a broad translation-quality evaluation. Neural models, OCR and
simple sentence boundaries can produce inaccurate or awkward results.

The underlying OPUS-MT models identify CC-BY 4.0; MDBG's dictionary identifies
CC-BY-SA 4.0. They are downloaded separately, with original notices retained.

## Reproduce the native package

```bash
bash tools/build-ubuntu26.04.sh --model-check
```

This installs Ubuntu build/GNOME test dependencies in an isolated Docker
container, runs the regular/Weston/GNOME tests, validates real models, and
creates the .deb, exact corresponding source and SHA256SUMS together.
Without `--model-check`, GNOME tests use the dictionary fixture and packaging
must not be described as real sentence-model validation.

## Remaining limits

- Supported Wayland hover compositor: GNOME 50. KDE and other compositors need
  separate integrations. Extension metadata does not claim other versions.
- Physical user desktops, GPU drivers, fractional/mixed monitor scaling and
  actual GNOME manual screenshot permission dialogs remain unverified.
- Geometry tests cover window/monitor clipping, negative origins and HiDPI
  ratios, but do not establish every physical multi-monitor layout.
- X11 hover retains one monitor at 100% scaling.
- Blur/stylized/low-contrast text and lines wider than the crop can defeat OCR.
- Online Mozhi tests are local fixtures; public-server availability is not
  guaranteed. Offline failures do not fall back online.
