# Hover Translate 0.5.0 — optional Azure Translator (preview)

Hover Translate now supports explicit Offline, Azure and Mozhi provider paths.
Existing users retain their saved provider; missing settings still default to
Offline. OCR, GNOME/Wayland/X11 capture, region selection and popups stay local.
CC-CEDICT translation and word selection are Offline-only. Settings display only
the selected provider's fields and preserve the small-window scrolling/fixed actions.

## Install or upgrade on Ubuntu 26.04 amd64

Quit the old app, verify SHA256SUMS, then run:

```bash
sudo apt install ./hover-translate_0.5.0_amd64.deb
```

Reopen `/usr/bin/hover-translate`. Saved provider/settings/models are retained.
GNOME extension protocol/version 4 is unchanged, so an existing compatible extension
needs no reinstall. First-time users can use Hover → Set up GNOME hover and
Translation → Install offline models. Other distributions should build from source
with their system libraries; the binary targets Ubuntu 26.04 amd64.

## Azure configuration and privacy

Create your own Microsoft Azure Translator resource. Select **Translation →
Microsoft Azure Translator**, click **Configure Azure credentials**, enter its region
and key through the masked local prompt, then Apply/Test translation. Keys stay in
a separate owner-only private credentials file, directory/file modes 700/600, outside
normal INI settings. Ownership/permission/link checks fail closed. This file is
unencrypted at rest; protect the account, disk and backups. Never share keys in chat.

Offline translation stays local. Azure sends recognized/source text to Microsoft
Azure Translator, **never screenshots**. Qt Network calls REST v3 directly; no
Azure SDK or developer-owned shared key is bundled. Microsoft's current quota/pricing
applies; Azure is not guaranteed permanently free. There is no automatic provider
fallback. Missing/auth/quota/network/timeout/language/invalid-response errors are
actionable and do not expose sensitive headers or server bodies.

Monthly local usage stores only month, submitted Azure/Mozhi request count and source
Unicode character count; cache hits/local translation do not count. It rolls over
with the local month, contains no text/keys and sends no telemetry. Counts are
estimates rather than billing records. Cancellation, request IDs, stale-response
protection and provider-specific GNOME/X11 cache separation are preserved.

## Validation, source and limits

The tag release workflow requires all seven CTest suites, GNOME policy JS, native
Wayland tests, real Offline English/Chinese model translation and live headless
GNOME 50 hover before publishing. Azure requests are mocked; tests need no real key
and never make live Azure calls. The binary and matching GPL-3.0-or-later source
`hover-translate-0.5.0-Source.tar.gz` are published together with `SHA256SUMS`.
Original Crow notices remain; no new runtime dependency was introduced.

Only the global Azure HTTPS resource-key endpoint is supported; no custom/VNet
endpoints or Entra ID. Physical desktops, fractional scaling, other GNOME versions
and manual permission dialogs remain unverified. OCR/model/dictionary quality limits
remain as documented. Public Mozhi availability is not guaranteed. No runtime,
translation models, fonts or dictionaries are bundled.
