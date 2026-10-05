#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real model smoke checks, separate from dictionary/protocol fixtures."""
import argparse
import os
import re
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("binary")
parser.add_argument("--python", required=True)
parser.add_argument("--models-dir", required=True)
args = parser.parse_args()
cases = [
    ("Hello world", "zh-CN", [("你好", "哈罗"), ("世界",)]),
    ("你好世界", "en", [("hello",), ("world",)]),
]
environment = dict(os.environ, QT_QPA_PLATFORM="offscreen")
with tempfile.TemporaryDirectory() as directory:
    for text, target, words in cases:
        result = subprocess.run([args.binary, "--config", directory + "/settings.ini",
            "--provider", "offline", "--translate", text, "--target", target,
            "--python", args.python, "--models-dir", args.models_dir, "--no-dictionary"],
            text=True, capture_output=True, env=environment, timeout=120)
        if result.returncode:
            raise SystemExit(result.stderr.strip() or "Offline translation failed")
        normalized = re.sub(r"\s+", "", result.stdout.lower())
        assert all(any(word in normalized for word in alternatives) for alternatives in words), f"Unexpected translation: {result.stdout!r}"
        print(f"PASS real offline model: {text} → {result.stdout.strip()}")
