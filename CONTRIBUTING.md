# Contributing

Use [GitHub Issues](https://github.com/LincolnGothic/Linux_mouse_hover_translation/issues)
for bugs and proposed features. Describe the behavior, expected result and
reproduction steps. For capture/OCR problems, include desktop/session type,
monitor count, scaling, Qt and Tesseract versions, model names, and a small
non-sensitive text sample. For server problems, include its hostname and the
error; do not post credentials or captured private text.

Build dependencies and desktop commands are in [README.md](README.md).
Contributions to application code use C++17 and Qt 6.8 or newer. Keep changes
focused, preserve cancellation and focus/clipboard behavior, and state any
new platform or service requirement. Add a meaningful test when behavior changes.

Run the two suites under an isolated X11 display:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON
cmake --build build --parallel 4
xvfb-run -a -s '-screen 0 1200x900x24' ctest --test-dir build --output-on-failure
git diff --check
```

The hover test generates sample screenshots under `build/tests/`. The network
fixture uses the Mozhi API shape; live service checks are separate. See
[docs/TESTING.md](docs/TESTING.md) for scope and limitations.

All contributions must be compatible with GPL-3.0-or-later. Keep existing
copyright and SPDX notices. Add the origin, license and exact revision of any
new third-party component, and mark changes to imported files. Follow
[NOTICE.md](NOTICE.md) and [docs/UPSTREAM.md](docs/UPSTREAM.md). Contributions do
not require assigning copyright to the maintainer.

When preparing a release, generate and provide matching corresponding source
alongside binaries using [docs/RELEASE.md](docs/RELEASE.md).
