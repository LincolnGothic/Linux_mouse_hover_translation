#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the extension on a real, isolated headless GNOME compositor."""
from pathlib import Path
import subprocess
import sys
import gi
gi.require_version("Gio", "2.0")
from gi.repository import Gio, GLib

bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
def call(service, path, interface, method, signature=None, args=()):
    return bus.call_sync(service, path, interface, method,
        GLib.Variant(signature, args) if signature else None, None,
        Gio.DBusCallFlags.NONE, 5000, None).unpack()

service = "io.github.LincolnGothic.HoverTranslate.Gnome"
path = "/io/github/LincolnGothic/HoverTranslate/Gnome"
call(service, path, service, "Configure", "(bi)", (False, 600))
print("PASS real GNOME 50 extension load and Configure")
subprocess.run([str(Path(sys.argv[1]) / "tests/test_gnome_live")], check=True, timeout=45)
