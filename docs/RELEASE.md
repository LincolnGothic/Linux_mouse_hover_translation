# Building and sharing a release

The application is GPL-3.0-or-later. Keep `LICENSE`, `LICENSES/`, `NOTICE.md`,
the upstream notices, and the marked modifications with the source. New
contributions must be compatible with that license. Before importing another
component, check its license and record its origin and modifications.

After the build and X11 tests pass, generate both packages from the same source
tree:

```bash
umask 022
cpack --config build/CPackConfig.cmake -B build/release
cpack --config build/CPackSourceConfig.cmake -B build/release
(cd build/release && sha256sum *.deb *Source.tar.gz > SHA256SUMS)
```

The Debian binary package targets Debian 13 amd64 and dynamically links system
libraries. It depends on the English and Simplified Chinese OCR packages. It
does not bundle Qt, OCR libraries, models, fonts or a translation server.
Other distributions should build from source with Qt 6.8 or newer.

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
dpkg-deb --info build/release/hover-translate_0.1.0_amd64.deb
dpkg-deb --contents build/release/hover-translate_0.1.0_amd64.deb
tar -tzf build/release/hover-translate-0.1.0-Source.tar.gz
```

Before announcing public-server support, test the selected Mozhi instance in
both directions with non-sensitive sample text. Confirm its Google engine and
supported language codes. Public instances can change, throttle requests or
stop operating. Licensing compliance does not resolve a server operator's
terms, privacy practices or rights to third-party services.

The GitHub workflow builds, runs both suites, and uploads artifacts on `main`
and pull requests. Pushing a version tag triggers the same checks and then
creates a GitHub MVP prerelease with the binary, matching source archive and
checksum file together. The release job uses GitHub's own scoped workflow token;
no personal token belongs in the repository.

Create the version tag from the source revision intended for the release.
Include the tested platform and known limitations in the release notes; see
[RELEASE-NOTES-0.1.0.md](RELEASE-NOTES-0.1.0.md). Check the workflow result and
release assets before announcing the release. Use `docs/GITHUB.md` for a manual
upload if the workflow is unavailable.
Users who only modify and run the program privately do not have to publish
their private changes solely because of GPL.
