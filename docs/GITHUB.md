# Repository description

Suggested GitHub About description:

> Offline English/Chinese translation with local OCR, dictionary lookup, Wayland screen-region capture and X11 hover; GPL-3.0-or-later.

Suggested topics:

```text
linux x11 wayland translation offline argos-translate dictionary ocr tesseract qt6 cpp chinese english mouse-hover gpl-3-0
```

The README is available in English and Simplified Chinese, with screenshots,
supported-platform limits, dependencies, installation, usage, privacy,
development, tests and licensing. Copyright and provenance are in `NOTICE.md`
and `docs/UPSTREAM.md`; corresponding-source distribution is in `docs/RELEASE.md`.

With GitHub API access and repository administration permission, set the About
field and topics with:

```bash
gh repo edit LincolnGothic/Linux_mouse_hover_translation \
  --description 'Offline English/Chinese translation with local OCR, dictionary lookup, Wayland screen-region capture and X11 hover; GPL-3.0-or-later.' \
  --add-topic linux --add-topic x11 --add-topic translation --add-topic ocr \
  --add-topic tesseract --add-topic qt6 --add-topic cpp --add-topic chinese \
  --add-topic english --add-topic mouse-hover --add-topic gpl-3-0 \
  --add-topic offline --add-topic argos-translate --add-topic wayland --add-topic dictionary
```

Create a GitHub release from the validated version tag, keeping the binary and
matching source together:

```bash
gh release create v0.2.0 --repo LincolnGothic/Linux_mouse_hover_translation \
  --verify-tag --latest --title 'Hover Translate 0.2.0 — offline translation' \
  --notes-file docs/RELEASE-NOTES-0.2.0.md \
  build-ubuntu2604/release/hover-translate_0.2.0_amd64.deb \
  build-ubuntu2604/release/hover-translate-0.2.0-Source.tar.gz \
  build-ubuntu2604/release/SHA256SUMS
```

These are reference commands only. Do not publish local changes without the
user's current instruction. Complete the real-model checks in docs/TESTING.md
before presenting a release as verified. A preview with unverified model
behavior must be clearly labeled as a prerelease. After the release checks pass,
mark the intended stable version as Latest so it appears in GitHub's sidebar.

These commands are publication instructions, not evidence that GitHub settings
or release assets have been published. Verify the corresponding GitHub pages
after running them. No access token should be placed in this repository.
