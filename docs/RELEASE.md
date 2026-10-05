# Building and sharing a release

The application is GPL-3.0-or-later. Keep `LICENSE`, `LICENSES/`, `NOTICE.md`,
the upstream notices, and the marked modifications with the source. New
contributions must be compatible with that license. Before importing another
component, check its license and record its origin and modifications.

After the build, portal/X11/Wayland tests and real offline model checks pass,
generate both packages from the same source
tree:

```bash
umask 022
cpack --config build/CPackConfig.cmake -B build/release
cpack --config build/CPackSourceConfig.cmake -B build/release
(cd build/release && sha256sum *.deb *Source.tar.gz > SHA256SUMS)
```

For 0.4.2, build the binary package on Ubuntu 26.04 amd64, with system libraries
installed so `dpkg-shlibdeps` calculates correct dependencies. Install `file`
and `dpkg-dev` for this packaging step. It also depends
on OCR language packages, Python venv support, Qt Wayland and the screenshot
portal. It also includes the original GNOME extension and per-user setup helper.
It does not bundle Qt, GNOME, models, fonts, dictionaries or the Python runtime.
Other distributions should build/package with their own system libraries.
The local cloud package can be prepared for review while model checks are
blocked, but must be labeled with that limitation and must not be announced
as a verified sentence-translation release.

For an Internet download, place the matching source archive beside the binary
with equivalent access and a clear link, as described by GPL section 6(d).
Include the source for your actual version and its build/install scripts;
an upstream-only source link does not cover the modifications. A source archive
includes the vendored Crow components, application, tests, icon, notices and
build/setup scripts. It excludes generated output and the development SDK.

Record the release's source revision, artifact hashes, build platform and test
results. Keep older matching source downloads available while their binaries
remain available. If you use another GPL distribution method, satisfy that
method's complete requirements; a written source offer has obligations beyond
simply promising to share code later.

Check that the package includes the executable, desktop entry, original icon,
license and notices:

```bash
dpkg-deb --info build/release/hover-translate_0.4.2_amd64.deb
dpkg-deb --contents build/release/hover-translate_0.4.2_amd64.deb
tar -tzf build/release/hover-translate-0.4.2-Source.tar.gz
```

Before announcing public-server support, test the selected Mozhi instance in
both directions with non-sensitive sample text. Confirm its Google engine and
supported language codes. Public instances can change, throttle requests or
stop operating. Licensing compliance does not resolve a server operator's
terms, privacy practices or rights to third-party services.

The GitHub workflow builds, runs all suites and real model checks, and uploads artifacts on `main`
and pull requests. Pushing a version tag triggers the same checks and then
creates a GitHub prerelease, with the binary, matching source archive
and checksum file together. All build and real-model checks must pass first.
The release job uses GitHub's own scoped workflow token;
no personal token belongs in the repository.

Create the version tag from the source revision intended for the release.
Include the tested platform and known limitations in the release notes; see
[RELEASE-NOTES-0.4.2.md](RELEASE-NOTES-0.4.2.md). Check the workflow result and
release assets before announcing the release. Use `docs/GITHUB.md` for a manual
upload if the workflow is unavailable.
Users who only modify and run the program privately do not have to publish
their private changes solely because of GPL.
