# Upstream provenance

This derivative imports a small part of
[KDE/crow-translate](https://github.com/KDE/crow-translate), version **4.1.0**,
tag `v4.1.0`, commit
`35dc1c5b497c141be7d4e5e613c77caf958b80a0`.
The tag resolves to that commit. The original release tree is available at
<https://github.com/KDE/crow-translate/tree/35dc1c5b497c141be7d4e5e613c77caf958b80a0>.
Every imported component is GPL-3.0-or-later.

| Upstream file (under `src/`) | Local file (under `third_party/crow/`) | Changes dated 2026-10-04 |
| --- | --- | --- |
| `onlinetranslator.cpp` | `onlinetranslator.cpp` | None |
| `onlinetranslator.h` | `onlinetranslator.h` | Removed an unused `QMediaPlayer` include; explicitly included `QObject` for Qt 6.10 compatibility |
| `ocr/aocrprovider.h` | `aocrprovider.h` | None |
| `ocr/tesseractocr.cpp` | `tesseractocr.cpp` | Added line geometry and confidence; worker completion handling; atomic cancellation; strict validation that both requested OCR models loaded |
| `ocr/tesseractocr.h` | `tesseractocr.h` | Exposes OCR line results and tracks the asynchronous job, language configuration and cancellation state |

Original SHA-256 hashes before modifications:

```text
9a374511ec88fda2f80ffbd889a9cd6766a0536136cac7e08c8070c1e608ba1d  onlinetranslator.cpp
657a3ad92720cb83ffdf1537ebeaa0a1501baf8c523a4c982e25dfa605d6d859  onlinetranslator.h
985f794fcc23d5e836f736627d5299bfdec6696a79b6cd5acf444510d6651015  aocrprovider.h
4f07044fe4fc9734ecae2e0a5fea942322833287037b6630eea0ae4cd39d38eb  tesseractocr.cpp
ddaaf947d9fe6f020b174146b5fa9fa8f990a613b15e480185f2ac556cccf8a5  tesseractocr.h
```

New code provides the X11 hover policy and capture, constrained language
selection, settings, non-focusable popup, short-lived Escape grab, request
timeouts, cancellation, bounded in-memory cache, command-line tools and tests.
The upstream translator retains its other engine code; this frontend selects
its Mozhi Google engine when the user chooses online mode. New offline code
uses a separate Argos/dictionary worker, with no changes to Crow's online
engine protocol. New code also provides desktop-portal screenshot capture,
region selection and editable text translation. No speech models, Crow icons, translations,
screenshots, other third-party source or updater code are imported.

Upstream's complete GPL text is retained as `LICENSE` and
`LICENSES/GPL-3.0-or-later.txt`. Qt, Tesseract, Leptonica and XCB remain external
dependencies. See [NOTICE.md](../NOTICE.md) for their licensing context.
