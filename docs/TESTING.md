# Validation and known limitations — 0.2.0

The application builds on Debian 13 / Qt 6.8.2 and on **Ubuntu 26.04.1 amd64 /
Qt 6.10.2 / GCC 15.2**. The offline runtime dependencies install on Ubuntu's
Python 3.14.4, with CPU PyTorch 2.14.1 and Argos Translate 1.11.0; `pip check`
reports no broken requirements. The cloud proxy's trusted CA bundle was supplied
to the test container; TLS verification remained enabled.

## Functional checks

```bash
dbus-run-session -- xvfb-run -a -s '-screen 0 1200x900x24' \
  ctest --test-dir build --output-on-failure
bash tools/test-wayland.sh build
```

| Suite | What it exercises |
| --- | --- |
| `core` | Hover timing, OCR geometry/languages, settings persistence, online HTTP fixture errors/cancellation, actual offline dictionary worker in both directions, queued stale results, worker timeout/recovery, missing runtime errors |
| `hover_x11` | Real screen capture/Tesseract/online-fixture/popup flow, cache, movement/pause/Escape, focus/selection/clipboard preservation, actual CLI and Settings window |
| `portal_capture` | Real D-Bus Screenshot request/response protocol with an in-process portal fixture, cancellation, timeout/late response, rejection of nonlocal image URIs, preview cropping, real OCR, actual Python dictionary lookup and clipboard preservation |

The three suites contain **31 functional cases**, plus six QtTest
initialization/cleanup entries. Fixtures are labeled and use synthetic text;
they are not public-service or sentence-model tests.

The Wayland helper starts an isolated D-Bus session, Xvfb and a nested Weston
compositor. The test application uses **Qt's native Wayland backend**, with
Wayland keyboard/pointer seats. All five portal cases also pass there. It uses
the same screenshot fixture, not a real GNOME capture backend. On Wayland,
focus changes can reannounce clipboard offers; tests check unchanged contents
at every notification rather than interpreting notifications as clipboard writes.

## Sentence models: download blocked in cloud and GitHub Actions

This cloud environment currently returns **HTTP 403** for `argos-net.com`,
before model downloads complete. Its optional `www.mdbg.net` CC-CEDICT export
is also blocked. The required host additions are saved in the environment
configuration draft, but saving a draft does not apply it to the running cloud.
The real-model step in GitHub Actions also returns HTTP 403 when downloading
the English-to-Chinese model, after the runtime dependencies install successfully.
Version 0.2.0 is therefore published as a preview with this limitation recorded.

The real Argos runtime successfully loads through the application and reports
the missing English/Chinese models. **No real sentence-model translation or
translation-quality result has been established in this environment.** The
dictionary tests use small test-authored entries and do not establish a full
CC-CEDICT download.

After the model host is permitted:

```bash
python3 offline/setup_offline.py --data-dir /path/to/offline-data
python3 tests/check_offline_models.py ./build/hover-translate \
  --python /path/to/offline-data/argos-env/bin/python \
  --models-dir /path/to/offline-data/argos-packages
```

This check forces offline mode and disables dictionary lookup to ensure the
two results come from real models. The updated GitHub workflow requires it
before packaging/releasing; check the Actions run for the relevant revision
for its result. Downloaded models retain their metadata/notices. The setup tool records
their authoritative HTTPS URL and downloaded SHA-256 and checks archive integrity.

## User-desktop limitations

- Actual Ubuntu GNOME screenshot authorization, backend behavior, multi-display
  capture and fractional scaling need verification on the user's desktop.
- Wayland supports explicit screenshot/region and typed-text workflows.
  Automatic mouse hover across native Wayland applications remains unsupported.
- X11 automatic hover retains its one-display, 100%-scaling restriction.
- OCR can miss small/low-contrast text and English/Chinese mixtures. Source
  language detection is a Latin/Han heuristic, not general language detection.
- Offline translation quality/speed depend on the installed models and hardware.
- CC-CEDICT supplies Chinese word definitions and limited English reverse lookup;
  it is not a comprehensive English-to-Chinese dictionary.
- Optional online Mozhi behavior depends on the selected instance. The earlier
  default-instance check returned a rate-limit error. Offline mode does not use it.

## Packaging

The 0.2.0 `.deb` is built in Ubuntu 26.04 and uses `dpkg-shlibdeps` to compute
native library dependencies. It includes the executable, setup/worker scripts,
desktop entry, icon, documentation and notices; it does not include downloaded
models, dictionaries or Python libraries. Keep the complete matching source
archive beside the binary. See [RELEASE.md](RELEASE.md).

On 2026-10-04, the repeatable `tools/build-ubuntu26.04.sh` build completed,
including all three suites, the native Wayland fixture checks and both packages.
The `.deb` installs with `apt` in the Ubuntu test container. Its installed
version command, setup-helper help and local dictionary lookup in both
directions pass. The installed executable also loads the real Argos runtime
and reports the missing sentence models, consistent with the download blocker.

The corresponding-source archive contains all **62 intended files**, including
local changes and new files, verified byte for byte against the working tree.
It excludes build output, Python environments, downloaded models and Git
metadata. SHA-256 checksums are supplied with the two artifacts. The binary
requires Ubuntu 26.04's Qt libraries (including Qt Core 6.10.2); use a native
rebuild for distributions with older libraries.
