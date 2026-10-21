#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
if [[ ${HOVER_WAYLAND_TEST_BUS:-} != 1 ]]; then
    wayland_profile_dir=$(mktemp -d /tmp/hover-wayland-profile.XXXXXX)
    trap 'rm -rf "$wayland_profile_dir"' EXIT
    export XDG_CONFIG_HOME="$wayland_profile_dir/config" XDG_DATA_HOME="$wayland_profile_dir/data"
    mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME"
    cat > "$wayland_profile_dir/bus.conf" <<'XML'
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type><listen>unix:tmpdir=/tmp</listen>
  <policy context="default">
    <allow send_destination="*"/><allow receive_sender="*"/><allow own="*"/>
  </policy>
</busconfig>
XML
    dbus-run-session --config-file="$wayland_profile_dir/bus.conf" -- \
        env HOVER_WAYLAND_TEST_BUS=1 bash "$0" "${1:-build}"
    exit
fi
if [[ ${HOVER_WAYLAND_TEST_DISPLAY:-} != 1 ]]; then
    exec xvfb-run -a -s '-screen 0 1200x900x24' env HOVER_WAYLAND_TEST_DISPLAY=1 bash "$0" "${1:-build}"
fi
test_build_dir=$(realpath "${1:-build}")
test_runtime_dir=$(mktemp -d /tmp/hover-wayland.XXXXXX)
chmod 700 "$test_runtime_dir"
export XDG_RUNTIME_DIR="$test_runtime_dir"
# The nested X11 backend supplies a real Wayland keyboard/pointer seat, so
# clipboard-preservation assertions exercise an actual clipboard offer.
weston --backend=x11-backend.so --renderer=pixman --width=1200 --height=900 \
    --socket=hover-test-wayland >"$test_runtime_dir/weston.log" 2>&1 &
test_weston_pid=$!
trap 'kill "$test_weston_pid" 2>/dev/null || true; wait "$test_weston_pid" 2>/dev/null || true; rm -rf "$test_runtime_dir"' EXIT
for ((test_attempt=0; test_attempt<100; ++test_attempt)); do
    if [[ -S "$test_runtime_dir/hover-test-wayland" ]]; then break; fi
    if ! kill -0 "$test_weston_pid" 2>/dev/null; then cat "$test_runtime_dir/weston.log"; exit 1; fi
    sleep 0.05
done
export WAYLAND_DISPLAY=hover-test-wayland
export XDG_SESSION_TYPE=wayland
export QT_QPA_PLATFORM=wayland
export QT_QUICK_BACKEND=software
"$test_build_dir/tests/test_portal"
