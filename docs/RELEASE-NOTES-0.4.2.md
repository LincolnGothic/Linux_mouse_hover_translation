# Hover Translate 0.4.2 — settings window update (preview)

The previous settings page could be wider than the window: two long button rows
forced horizontal scrolling, while Apply/Close moved with the content. The new
window separates Hover, Translation and Advanced settings and keeps the status,
Screen region, Translate text, About, Apply and Close outside the scrolling pages.
Field labels sit above their controls; long help text wraps. Tall tabs scroll
vertically using the mouse wheel or draggable scrollbar, with no horizontal
scrollbar. The initial window fits the available screen.

## Where to find settings

- **Hover:** enable hover, target language, Word/Line/Sentence, delay, source
  highlighting, temporary mode shortcuts and Set up GNOME hover.
- **Translation:** offline/online engine, Mozhi server, dictionary toggle,
  Install offline models and Test translation.
- **Advanced:** optional OCR folder, Python executable, translation models and
  CC-CEDICT path. Leave these empty to use installed/downloaded resources.

Click Apply to use changed settings. Saved choices and offline models are kept.
Word remains the default for new settings. Existing selection/translation behavior
and the GNOME extension version 4 protocol remain unchanged.

## Install / upgrade on Ubuntu 26.04 amd64

Quit the old app, then run `sudo apt install ./hover-translate_0.4.2_amd64.deb`
and reopen `/usr/bin/hover-translate`.

From 0.4.1 with an active version 4 extension, no extension reinstall or logout
is needed. From 0.4.0 or earlier, use Hover → Set up GNOME hover, then sign out
and back in. First-time users also install offline models from Translation.
No API key is required for offline translation.

## Validation and source

The settings UI suite checks small windows, larger fonts, actual wheel/scrollbar
input, lower controls, fixed actions, preserved paths/modes and Apply/reader/test
wiring. Six regular suites, Node policy, native Weston, real offline models and
real headless GNOME 50 hover are required by the release workflow. Physical user
desktops, fractional scaling and manual portal permission dialogs remain
unverified; OCR/dictionary/translation limitations described in docs/TESTING.md
still apply.

The GPL-3.0-or-later corresponding source is
`hover-translate-0.4.2-Source.tar.gz`, alongside the installer and SHA256SUMS.
Original Crow notices are retained. No new third-party code/data is bundled.
