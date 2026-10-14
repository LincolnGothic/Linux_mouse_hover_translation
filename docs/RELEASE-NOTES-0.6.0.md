# Hover Translate 0.6.0 — native Windows preview

This release adds native Windows x64 support to the existing Qt application,
while retaining Ubuntu GNOME Wayland/X11 support and Offline/Azure/Mozhi.

## Windows

Download `hover-translate-0.6.0-windows-x64.zip`, verify `SHA256SUMS`, extract and
run `install-windows.ps1` to install into a new folder and create a desktop
shortcut. Default installation is `D:\software\HoverTranslate\app-0.6.0`.
See [the Windows guide](WINDOWS.md) for the exact command and build instructions.

Hover capture, word/line/sentence selection, source highlighting, popup
copy/pin/close, native Escape handling, typed translation and primary-screen
region selection are supported. Windows capture handles the hovered monitor's
image scale and refuses secure/locked desktops and the application's own windows.

New installations start paused with Offline selected. A private offline runtime
and both English/Chinese translation models download into app-local `data` during
setup. English/Chinese Tesseract OCR data and required Qt/native DLLs are bundled.
No global Python installation or autostart is added. Azure is opt-in and uses
Windows Credential Manager; no developer-owned key is included. The source and
OCR images remain local; online providers send recognized text only.

## Ubuntu

Quit the previous process and install `hover-translate_0.6.0_amd64.deb`. Saved
Linux settings/providers/models are retained. GNOME extension protocol/version 4
is unchanged. Linux credential files retain their owner-only permission checks.

## Validation and limitations

Windows CI builds the application, runs shared core/settings/mocked Azure/native
capture tests, validates real Offline translation in both directions, and bundles
the Windows ZIP. Linux CI retains CTest, GNOME policy, Wayland and live GNOME
model checks. Azure tests use a local mock and no real Azure key. Matching complete
GPL source, third-party notices and checksums accompany the binary artifacts.

Local Windows verification uses Windows 11, one 2560×1440 monitor at 125% scaling.
The native fixture verifies English/Chinese line selection, English word/sentence
selection, highlighting, cancellation, own-window exclusion, popup actions,
Escape, typed translation and dragged-region capture. Secure-desktop behavior
is guarded in code; actual session locking and additional monitors are unverified.

This is a preview: OCR/model quality varies; secure/protected/elevated windows
are outside guaranteed support. Multi-monitor/fractional-scale configurations
beyond the recorded validation need user-desktop testing. Manual Windows region
selection previews only the primary monitor. Public Mozhi availability is not
guaranteed; Microsoft pricing/quotas apply to Azure.
