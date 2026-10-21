#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Isolated real GNOME Shell; never changes a logged-in user's extensions.
set -euo pipefail
if [[ ${HOVER_GNOME_TEST_BUS:-} != 1 ]]; then
    gnome_bus_dir=$(mktemp -d /tmp/hover-gnome-bus.XXXXXX)
    trap 'rm -rf "$gnome_bus_dir"' EXIT
    mkdir -p "$gnome_bus_dir/services"
    # Permit only dconf activation for temporary GNOME settings. Host portals
    # and GVfs must not create FUSE mounts in this disposable test profile.
    cp /usr/share/dbus-1/services/ca.desrt.dconf.service "$gnome_bus_dir/services/"
    cat > "$gnome_bus_dir/bus.conf" <<XML
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type><listen>unix:tmpdir=/tmp</listen>
  <servicedir>$gnome_bus_dir/services</servicedir>
  <policy context="default">
    <allow send_destination="*"/><allow receive_sender="*"/><allow own="*"/>
  </policy>
</busconfig>
XML
    dbus-run-session --config-file="$gnome_bus_dir/bus.conf" -- \
        env HOVER_GNOME_TEST_BUS=1 bash "$0" "${1:-build}"
    exit
fi
gnome_build_dir=$(realpath "${1:-build}")
gnome_test_dir=$(mktemp -d /tmp/hover-gnome.XXXXXX)
chmod 700 "$gnome_test_dir"
export XDG_RUNTIME_DIR="$gnome_test_dir/runtime"
export XDG_DATA_HOME="$gnome_test_dir/data"
export XDG_CONFIG_HOME="$gnome_test_dir/config"
mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME" "$XDG_DATA_HOME/gnome-shell/extensions"
chmod 700 "$XDG_RUNTIME_DIR"
export XDG_CURRENT_DESKTOP=GNOME XDG_SESSION_TYPE=wayland LIBGL_ALWAYS_SOFTWARE=1
export NO_AT_BRIDGE=1 GIO_USE_VFS=local
dbus-update-activation-environment XDG_RUNTIME_DIR XDG_DATA_HOME XDG_CONFIG_HOME XDG_CURRENT_DESKTOP XDG_SESSION_TYPE
gnome_shell_pid=
gnome_system_bus_pid=
cleanup() {
    if [[ -n "$gnome_shell_pid" ]]; then kill "$gnome_shell_pid" 2>/dev/null || true; wait "$gnome_shell_pid" 2>/dev/null || true; fi
    if [[ -n "$gnome_system_bus_pid" ]]; then kill "$gnome_system_bus_pid" 2>/dev/null || true; fi
    cp "$gnome_test_dir/shell.log" "$gnome_build_dir/gnome-test-shell.log" 2>/dev/null || true
    rm -rf "$gnome_test_dir"
}
trap cleanup EXIT
# Shell expects a system bus even in headless mode. This is an isolated bus
# with no host services, not a substitute for screen/pointer APIs under test.
export DBUS_SYSTEM_BUS_ADDRESS="unix:path=$gnome_test_dir/system-bus"
gnome_system_bus_pid=$(dbus-daemon --session --address="$DBUS_SYSTEM_BUS_ADDRESS" --fork --print-pid)
gsettings set org.gnome.shell disable-user-extensions false
gsettings set org.gnome.shell disabled-extensions "['hover-translate@lincolngothic.github.io']"
python3 "$gnome_build_dir/share/hover-translate/setup_gnome.py"
gsettings set org.gnome.mutter experimental-features '[]'
gnome-shell --wayland --no-x11 --headless --virtual-monitor=1200x900 --debug-control >"$gnome_test_dir/shell.log" 2>&1 &
gnome_shell_pid=$!
for ((gnome_attempt=0; gnome_attempt<200; ++gnome_attempt)); do
    if ! kill -0 "$gnome_shell_pid" 2>/dev/null; then cat "$gnome_test_dir/shell.log"; exit 1; fi
    if [[ $(gdbus call --session --dest org.freedesktop.DBus --object-path /org/freedesktop/DBus \
        --method org.freedesktop.DBus.NameHasOwner io.github.LincolnGothic.HoverTranslate.Gnome 2>/dev/null) == *true* ]]; then break; fi
    sleep 0.1
done
if ((gnome_attempt == 200)); then cat "$gnome_test_dir/shell.log"; exit 1; fi
export WAYLAND_DISPLAY=wayland-0 QT_QPA_PLATFORM=wayland
python3 tests/check_gnome_session.py "$gnome_build_dir"
