#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
build_source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_model_check=${1:-}
if [[ -n "$build_model_check" && "$build_model_check" != --model-check ]]; then
    echo "Usage: bash tools/build-ubuntu26.04.sh [--model-check]" >&2; exit 2
fi
docker run --rm \
    --env HTTPS_PROXY --env HTTP_PROXY --env ALL_PROXY --env NO_PROXY \
    --env SSL_CERT_FILE=/tmp/host-trusted-ca.pem \
    --env REQUESTS_CA_BUNDLE=/tmp/host-trusted-ca.pem \
    --env PIP_CERT=/tmp/host-trusted-ca.pem \
    --env HOVER_MODEL_CHECK="$build_model_check" \
    --mount "type=bind,src=$build_source_dir,dst=/src" \
    --mount "type=bind,src=/etc/ssl/certs/ca-certificates.crt,dst=/tmp/host-trusted-ca.pem,readonly" \
    --workdir /src ubuntu:26.04 bash -eu -c '
umask 022
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
  ca-certificates build-essential cmake ninja-build pkg-config file \
  qt6-base-dev qt6-base-dev-tools qt6-scxml-dev qt6-svg-plugins qt6-wayland \
  libtesseract-dev libleptonica-dev libxcb1-dev libxcb-xtest0-dev \
  tesseract-ocr-eng tesseract-ocr-chi-sim fonts-noto-cjk \
  python3-venv xvfb xauth x11-utils dbus-x11 weston xdg-desktop-portal
cmake -S . -B build-ubuntu2604 -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON
cmake --build build-ubuntu2604 --parallel 3
dbus-run-session -- xvfb-run -a -s "-screen 0 1200x900x24" ctest --test-dir build-ubuntu2604 --output-on-failure
bash tools/test-wayland.sh build-ubuntu2604
if [[ "$HOVER_MODEL_CHECK" == --model-check ]]; then
  python3 offline/setup_offline.py --data-dir /src/build-ubuntu2604/offline-data --without-dictionary
  python3 tests/check_offline_models.py ./build-ubuntu2604/hover-translate \
    --python /src/build-ubuntu2604/offline-data/argos-env/bin/python \
    --models-dir /src/build-ubuntu2604/offline-data/argos-packages
else
  echo "Sentence-model checks were not run; this build validates dictionary and capture fixtures."
fi
cpack --config build-ubuntu2604/CPackConfig.cmake -B build-ubuntu2604/release
cpack --config build-ubuntu2604/CPackSourceConfig.cmake -B build-ubuntu2604/release
cd build-ubuntu2604/release
sha256sum *.deb *Source.tar.gz > SHA256SUMS
sha256sum -c SHA256SUMS
'
