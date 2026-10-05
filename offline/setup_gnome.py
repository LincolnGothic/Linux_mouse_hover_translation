#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Install and enable Hover Translate's GNOME extension for the current user."""
import ast
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time

UUID = "hover-translate@lincolngothic.github.io"


def command(*args):
    return subprocess.run(args, text=True, capture_output=True, check=True).stdout.strip()


def main():
    version = command("gnome-shell", "--version")
    match = re.search(r"GNOME Shell\s+(\d+)", version)
    source = Path(__file__).resolve().parent / "gnome-extension" / UUID
    metadata = json.loads((source / "metadata.json").read_text())
    if not match or match[1] not in metadata["shell-version"]:
        raise RuntimeError(f"This extension supports GNOME {', '.join(metadata['shell-version'])}; detected {version}.")
    if command("gsettings", "get", "org.gnome.shell", "allow-extension-installation") == "false":
        raise RuntimeError("GNOME policy disables user extension installation. The desktop administrator must enable it.")
    data = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share"))
    parent = data / "gnome-shell/extensions"
    parent.mkdir(parents=True, exist_ok=True)
    target = parent / UUID
    if target.is_symlink():
        raise RuntimeError("The extension directory is a symbolic link. Manage that installation manually.")
    if target.exists():
        existing = json.loads((target / "metadata.json").read_text())
        if existing.get("uuid") != UUID:
            raise RuntimeError("Existing extension has different metadata; it was left unchanged.")
        backups = data / "hover-translate/extension-backups"
        backups.mkdir(parents=True, exist_ok=True)
        shutil.copytree(target, backups / f"{UUID}-{time.time_ns()}")
    with tempfile.TemporaryDirectory(prefix=".hover-install-", dir=parent) as temporary:
        staged = Path(temporary) / UUID
        shutil.copytree(source, staged)
        # Replace only this app's extension, retaining the previous copy above.
        if target.exists():
            shutil.rmtree(target)
        staged.rename(target)
    current = command("gsettings", "get", "org.gnome.shell", "enabled-extensions")
    enabled = ast.literal_eval(current.removeprefix("@as "))
    if not isinstance(enabled, list) or not all(isinstance(item, str) for item in enabled):
        raise RuntimeError("Could not read GNOME's enabled extensions. Enable Hover Translate in the Extensions app.")
    if UUID not in enabled:
        command("gsettings", "set", "org.gnome.shell", "enabled-extensions", json.dumps(enabled + [UUID]))
    excluded = ast.literal_eval(command("gsettings", "get", "org.gnome.shell", "disabled-extensions").removeprefix("@as "))
    if isinstance(excluded, list) and UUID in excluded:
        command("gsettings", "set", "org.gnome.shell", "disabled-extensions", json.dumps([item for item in excluded if item != UUID]))
    disabled = command("gsettings", "get", "org.gnome.shell", "disable-user-extensions") == "true"
    print("GNOME hover extension installed for your account.")
    print("Sign out and back in, reopen Hover Translate, then enable hover and click Apply.")
    if disabled:
        print("User extensions are globally disabled. Turn them on in GNOME Extensions; other extension settings were preserved.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        detail = error.stderr.strip() if isinstance(error, subprocess.CalledProcessError) and error.stderr else str(error)
        print(f"GNOME setup failed: {detail}", file=sys.stderr)
        sys.exit(1)
