#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

# Rootless SDK for the Debian 13 cloud image. Desktop users can instead use apt.
. /etc/os-release
if [[ ${ID:-} != debian || ${VERSION_CODENAME:-} != trixie || $(uname -m) != x86_64 ]]; then
    echo 'This helper requires Debian 13 amd64. See README.md for desktop dependencies.' >&2
    exit 2
fi
export HOVER_TOOLS_ROOT=${HOVER_TOOLS_ROOT:-/workspace/.hover-translation-tools}
mkdir -p "$HOVER_TOOLS_ROOT/apt" "$HOVER_TOOLS_ROOT/sysroot" "$HOVER_TOOLS_ROOT/cache"
python3 - <<'PY'
import os
from pathlib import Path
root = Path(os.environ['HOVER_TOOLS_ROOT']).resolve()
if '"' in str(root) or '\n' in str(root):
    raise SystemExit('SDK path cannot contain quotes or newlines')
apt = root / 'apt'
for suffix in ('lists/partial', 'archives/partial', 'empty-config', 'empty-sources'):
    (apt / suffix).mkdir(parents=True, exist_ok=True)
(apt / 'debian.sources').write_text('Types: deb\nURIs: https://deb.debian.org/debian\n'
    'Suites: trixie\nComponents: main\nSigned-By: /usr/share/keyrings/debian-archive-keyring.gpg\n')
settings = {
    'Dir::Etc::parts': apt / 'empty-config',
    'Dir::Etc::sourcelist': apt / 'debian.sources',
    'Dir::Etc::sourceparts': apt / 'empty-sources',
    'Dir::State::lists': apt / 'lists',
    'Dir::Cache::archives': apt / 'archives',
    'APT::Sandbox::User': __import__('pwd').getpwuid(os.getuid()).pw_name,
    'Debug::NoLocking': 'true', 'APT::Update::Error-Mode': 'any', 'Acquire::Retries': '0',
}
(root / 'apt.conf').write_text(''.join(f'{key} "{value}";\n' for key, value in settings.items()))
PY
export APT_CONFIG="$HOVER_TOOLS_ROOT/apt.conf"
# Use the signed authoritative Debian repository; leave /etc and trust intact.
/usr/bin/apt-get update
/usr/bin/apt-get --download-only --no-install-recommends -y install \
    build-essential pkg-config cmake ninja-build qt6-base-dev qt6-base-dev-tools qt6-scxml-dev qt6-svg-plugins \
    libtesseract-dev libleptonica-dev libxcb1-dev libxcb-xtest0-dev \
    xvfb xauth x11-utils fonts-noto-cjk tesseract-ocr-eng tesseract-ocr-chi-sim tesseract-ocr-osd
python3 - <<'PY'
import hashlib, json, os, shutil, subprocess
from pathlib import Path
root = Path(os.environ['HOVER_TOOLS_ROOT']).resolve()
sdk = root / 'sysroot'
state = root / 'apt/extracted.json'
extracted = json.loads(state.read_text()) if state.exists() else {}
library_dir = sdk / 'usr/lib/x86_64-linux-gnu'
library_dir.mkdir(parents=True, exist_ok=True)
# Remove links to host runtime files before extraction, so an archive can never
# overwrite a system file through one of our compatibility links.
for entry in library_dir.iterdir():
    if entry.is_symlink() and os.readlink(entry).startswith('/usr/lib/'):
        entry.unlink()
for package in sorted((root / 'apt/archives').glob('*.deb')):
    digest = hashlib.sha256(package.read_bytes()).hexdigest()
    if extracted.get(package.name) != digest:
        subprocess.run(['dpkg-deb', '-x', str(package), str(sdk)], check=True)
        extracted[package.name] = digest
# Apt skips dependencies already installed in the base image. Expose those
# matching runtime libraries to Qt's relocatable CMake package and linker.
for library in Path('/usr/lib/x86_64-linux-gnu').glob('*.so*'):
    target = library_dir / library.name
    if library.is_file() and not os.path.lexists(target):
        target.symlink_to(library)
models = sdk / 'usr/share/tesseract-ocr/5/tessdata'
models.mkdir(parents=True, exist_ok=True)
for language in ('eng', 'chi_sim', 'osd'):
    target = models / f'{language}.traineddata'
    source = Path('/usr/share/tesseract-ocr/5/tessdata') / target.name
    if not target.exists() and source.exists():
        shutil.copy2(source, target)
    if not target.exists():
        raise SystemExit(f'Missing OCR model: {language}')
state.write_text(json.dumps(extracted, indent=2) + '\n')
print(f'Prepared rootless SDK at {sdk}')
PY
