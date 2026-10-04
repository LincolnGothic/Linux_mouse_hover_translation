# Repository description

Suggested GitHub About description:

> X11 mouse-hover translation between English and Simplified Chinese, with local Tesseract OCR, Mozhi translation, and GPL-3.0-or-later licensing.

Suggested topics:

```text
linux x11 translation ocr tesseract qt6 cpp chinese english mouse-hover gpl-3-0
```

The README is available in English and Simplified Chinese, with screenshots,
supported-platform limits, dependencies, installation, usage, privacy,
development, tests and licensing. Copyright and provenance are in `NOTICE.md`
and `docs/UPSTREAM.md`; corresponding-source distribution is in `docs/RELEASE.md`.

With GitHub API access and repository administration permission, set the About
field and topics with:

```bash
gh repo edit LincolnGothic/Linux_mouse_hover_translation \
  --description 'X11 mouse-hover translation between English and Simplified Chinese, with local Tesseract OCR, Mozhi translation, and GPL-3.0-or-later licensing.' \
  --add-topic linux --add-topic x11 --add-topic translation --add-topic ocr \
  --add-topic tesseract --add-topic qt6 --add-topic cpp --add-topic chinese \
  --add-topic english --add-topic mouse-hover --add-topic gpl-3-0
```

Create a GitHub release from the validated version tag, keeping the binary and
matching source together:

```bash
gh release create v0.1.0 --repo LincolnGothic/Linux_mouse_hover_translation \
  --verify-tag --title 'Hover Translate 0.1.0 — X11 MVP' \
  --notes-file docs/RELEASE-NOTES-0.1.0.md \
  build/release/hover-translate_0.1.0_amd64.deb \
  build/release/hover-translate-0.1.0-Source.tar.gz build/release/SHA256SUMS
```

These commands are publication instructions, not evidence that GitHub settings
or release assets have been published. Verify the corresponding GitHub pages
after running them. No access token should be placed in this repository.
