#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Isolate desktop environment, settings, usage and portal fixtures from the host.
set -euo pipefail
test_build_dir=$(realpath "${1:-build}")
if (($#)); then shift; fi
test_profile_dir=$(mktemp -d /tmp/hover-x11.XXXXXX)
trap 'rm -rf "$test_profile_dir"' EXIT
export XDG_RUNTIME_DIR="$test_profile_dir/runtime"
export XDG_CONFIG_HOME="$test_profile_dir/config"
export XDG_DATA_HOME="$test_profile_dir/data"
mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME" "$XDG_DATA_HOME"
chmod 700 "$XDG_RUNTIME_DIR"
unset WAYLAND_DISPLAY QT_QPA_PLATFORMTHEME
export XDG_SESSION_TYPE=x11 XDG_CURRENT_DESKTOP= QT_QPA_PLATFORM=xcb
export NO_AT_BRIDGE=1 QT_ACCESSIBILITY=0
# No service activation: test_portal owns the portal name itself, and Qt must
# not start the host desktop's portal/keyring daemons on this private bus.
cat > "$test_profile_dir/bus.conf" <<'XML'
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type><listen>unix:tmpdir=/tmp</listen>
  <policy context="default">
    <allow send_destination="*"/><allow receive_sender="*"/><allow own="*"/>
  </policy>
</busconfig>
XML
dbus-run-session --config-file="$test_profile_dir/bus.conf" -- \
    xvfb-run -a -s '-screen 0 1200x900x24' \
    ctest --test-dir "$test_build_dir" --output-on-failure "$@"
