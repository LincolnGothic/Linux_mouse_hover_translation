# Copyright and license notices

Hover Translate 0.4.2 is a modified work using components of Crow Translate
4.1.0. The application and its corresponding source are licensed under
**GNU GPL version 3 or later (GPL-3.0-or-later)**. The complete terms are in
[LICENSE](LICENSE). There is no warranty.

- New application code, tests, scripts, documentation and icon:
  Copyright © 2026 Linux_mouse_hover_translation contributors.
- Crow translation and OCR components:
  Copyright © 2018 Hennadii Chernyshchyk and © 2022 Volk Milit.
- Crow abstract OCR provider:
  Copyright © 2026 Mauritius Clemens.

Original copyright and SPDX notices remain in the imported source files.
Modified imported files carry a modification notice dated 2026-10-04.
[docs/UPSTREAM.md](docs/UPSTREAM.md) identifies the exact upstream revision,
original file hashes, and modifications.

Crow Translate is developed by its upstream contributors. This derivative is
independently maintained and is not an official Crow Translate release. Its
name and icon were created for this project; Crow's branding and other assets
are not included.

The binary uses separately installed shared libraries and OCR models:

| Dependency | Upstream license | Distribution in this project |
| --- | --- | --- |
| Qt 6 Core, Gui, Widgets, Network, Concurrent, StateMachine, DBus, Wayland and SVG plugins | LGPL-3.0 / GPL licenses, plus component-specific third-party notices | Dynamic system libraries; not bundled |
| Tesseract OCR | Apache-2.0 | Dynamic system library; not bundled |
| Leptonica | BSD-2-Clause | Dynamic system library; not bundled |
| XCB | MIT | Dynamic system library; not bundled |
| Tesseract `eng`, `chi_sim`, `osd` models (`tessdata_fast`) | Apache-2.0 | Installed separately; not bundled |
| Noto CJK font used by tests | SIL Open Font License 1.1 | Installed separately; not bundled |
| Argos Translate 1.11.0 | MIT; retained in `LICENSES/Argos-MIT.txt` | Installed into a private user runtime; not bundled |
| CPU PyTorch 2.14.1 and Argos dependencies | Their respective package licenses and third-party notices | Installed separately; not bundled |
| Argos English/Chinese models 1.9 | Model-specific notices in the upstream downloads | Downloaded separately; not bundled |
| CC-CEDICT | Attribution/share-alike license identified in the downloaded edition's complete header | Downloaded from MDBG or supplied by the user; not bundled |
| GNOME Shell / Mutter / GJS / St | Their respective upstream GPL/LGPL licenses | Installed desktop APIs, not bundled; the original extension source is GPL-3.0-or-later |

The downloaded English/Chinese Argos 1.9 model README files identify the
underlying OPUS-MT models as CC-BY 4.0, authored by Jörg Tiedemann and Santhosh
Thottingal. Those original READMEs stay with the models. The downloaded MDBG
CC-CEDICT header identifies CC-BY-SA 4.0 and retains the referenced CEDICT
copyright of Paul Andrew Denisowski. Models and dictionary data are not bundled
in the app installer or source archive.

The Debian development SDK is outside the source tree and is not included in
the application packages. Its packages retain their own copyright files.
If you later bundle libraries, models, fonts or other assets, preserve their
respective notices and meet their distribution requirements too.

Offline is the default. The separate setup tool downloads resources; the
translation worker disables network connections. The dictionary download
preserves its complete copyright/license header. Do not assume a software
library's license also licenses every dictionary or model it can load.

The optional online service is a user-selected Mozhi instance. The app sends recognized
text to that server, which may send it to Google. Server availability, logging
and service terms are controlled by their operators. The GPL grants rights to
this software; it does not grant rights to a third-party service or its brand.

When distributing a modified binary, provide recipients access to its complete
corresponding source under GPL-3.0-or-later, including the scripts needed to
build it. Keep the copyright notices and license, identify your modifications,
and follow [docs/RELEASE.md](docs/RELEASE.md). Merely linking to unmodified Crow
source is not sufficient for this modified application. Private modifications
without distribution do not by themselves require public publication.
