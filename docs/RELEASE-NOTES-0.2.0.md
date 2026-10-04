# Hover Translate 0.2.0 — offline translation and screen regions

Offline translation is now the default. Install the per-user Argos runtime and
English/Chinese models once from Settings; subsequent translation uses no API
key or online service. Optional CC-CEDICT provides Chinese word definitions
and limited English-definition reverse lookup.

Wayland users can choose **Translate screen region** or translate typed text.
The screenshot portal asks the desktop to capture; the preview lets you crop
text before local Tesseract OCR. Assign `hover-translate --capture` to a desktop
shortcut. X11 automatic hover remains available under its original display limits.

The Ubuntu 26.04 amd64 package includes local worker/setup scripts and native
library dependencies. Models and dictionaries download separately with their
notices retained. GPL-3.0-or-later application source is provided alongside the
binary. Crow notices and the Qt 6.10 compatibility modification are retained.

Build, OCR, dictionary, X11 and native Wayland-client tests pass on the prepared
Ubuntu 26.04 environment. Screenshot tests use a local portal fixture. Actual
GNOME permissions still require user-desktop verification. Real sentence-model
checks are blocked in the cloud by the model host's HTTP 403 network policy;
model translation quality is not verified in that cloud environment. The
GitHub release workflow separately requires real offline model checks in both
directions before publishing its artifacts; consult the Actions run for this tag.

Install the Ubuntu 26.04 **amd64** package with:

```bash
sudo apt install ./hover-translate_0.2.0_amd64.deb
hover-translate
```

Keep Translation set to Offline, click Install offline models once and wait
for setup to finish. Then Test translation and use Translate screen region or
Translate text. Models and the optional dictionary require an initial internet
download; translation afterward uses local resources. The earlier v0.1.0
package does not include these features.
