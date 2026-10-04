# Validation

The application was built and exercised on Debian 13 amd64 with Qt 6.8.2,
Tesseract 5.5.0, the English/Simplified Chinese models, and Xvfb at
1200 × 900, 24-bit color, 100% scaling.

`ctest` runs two suites:

| Suite | Coverage |
| --- | --- |
| `core` | Hover delay and movement policy, dismissal, OCR line selection, two-language detection, persisted settings, endpoint validation, corrupt settings, real English/Chinese OCR, missing-model failures, both translation directions, timeout recovery, malformed replies and stale-request isolation |
| `hover_x11` | Real X11 screenshot-to-OCR-to-HTTP-to-popup flow in both directions, cache, cancellation on movement/pause, settings exclusion, same-target suppression, native Escape dismissal, focus/selection/clipboard preservation, and actual CLI/GUI process startup and shutdown |

The initial successful run reports **21 functional test cases**, plus four
QtTest initialization/cleanup entries, with no failures or skipped cases.
The two CTest suites pass. Reproduce them using the commands in
[README.md](../README.md).

The network fixture serves deterministic Mozhi-compatible replies on localhost.
The application uses Crow's actual HTTP translator and real OCR; test replies
do not establish the availability or quality of a public translation service.
The screenshots in the README come from this workflow and contain sample text.

The default public instance responded with HTTP 500, `instance has been rate
limited`, during the publication check. A successful live English/Chinese
translation has therefore not been established. Retry with non-sensitive
sample text or configure another trusted compatible instance; do not treat an
OCR or fixture pass as a live-service pass.

Desktop coverage is limited to one X11 display at 100% scaling. Native Wayland,
multiple displays, fractional scaling, production desktop window managers,
and live service behavior require separate validation. Low-contrast/small
fonts, layout boundaries and very long lines can also affect OCR accuracy.

Packaging checks verify executable startup, dependency resolution with Debian
APT simulation, readable installation directories, notices and documentation,
and a matching source archive containing the application and build scripts.
The binary uses external system libraries/models and has no cloud SDK path in
its installed runtime search path.
