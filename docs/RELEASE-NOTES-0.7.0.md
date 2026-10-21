# Hover Translate 0.7.0 — Google Translate and reliable hover

Hover Translate is the shared name for Linux and Windows. Repository links now
use `LincolnGothic/hover-translate`; installed settings, data folders, launchers
and extension IDs remain compatible.

Select **Translation → Google Translate — free, experimental**, click **Apply**
and **Test translation**. No key, Mozhi server or Argos model download is needed.
Source text goes directly to Google over HTTPS; OCR/screenshots remain local.
The unofficial keyless service may change or block/rate-limit requests. Offline
remains the default, and translation never silently changes providers.

The settings, text reader and desktop popup have new light/dark colors, clearer
headings, more spacing and visible primary/focus states. GNOME's popup has a
refreshed heading, spacing and buttons. Small windows keep their scrolling pages
and fixed actions.

OCR now retries layout/scaling when it misses the pointed word, accepts clear
words in noisy lines, and recaptures once after a missed selection. Small pointer
jitter no longer strands an in-flight lookup. GNOME also detects changes to the
window under the pointer.

On GNOME Wayland, run **Hover → Set up GNOME hover**, then sign out/in to load
**extension version 5**. Keep settings and offline models. New Windows installer
folders use `D:\software\HoverTranslate\app-0.7.0`. Google users can install with
`-SkipOfflineSetup` to avoid optional Argos runtime/model downloads.

Distribute matching complete source, `hover-translate-0.7.0-Source.tar.gz`,
alongside binaries and checksums. See [validation](TESTING.md), including the
remaining physical-desktop and monitor-scaling limits.
