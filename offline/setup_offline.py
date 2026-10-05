#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""One-time setup; installs into user data without changing system Python."""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import urllib.request
import venv
import zipfile

INDEX = "https://raw.githubusercontent.com/argosopentech/argospm-index/main/index.json"
DICTIONARY = "https://www.mdbg.net/chinese/export/cedict/cedict_1_0_ts_utf-8_mdbg.txt.gz"
USER_AGENT = "HoverTranslate/0.4.2 (+https://github.com/LincolnGothic/Linux_mouse_hover_translation)"


def open_download(url, timeout=60):
    if not url.startswith("https://"):
        raise ValueError("Downloads require HTTPS.")
    # The model host rejects Python's default urllib user-agent (HTTP 403/1010).
    # Identify this application truthfully; retain TLS and redirect checks.
    response = urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": USER_AGENT}), timeout=timeout)
    if not response.geturl().startswith("https://"):
        response.close()
        raise ValueError("Download redirected to a non-HTTPS address.")
    return response


def download(url, destination):
    if not url.startswith("https://"):
        raise ValueError("Downloads require HTTPS.")
    digest = hashlib.sha256()
    with open_download(url) as response, open(destination, "wb") as out:
        written = 0
        while chunk := response.read(1024 * 1024):
            out.write(chunk)
            digest.update(chunk)
            written += len(chunk)
        expected = response.headers.get("Content-Length")
        if expected is not None and written != int(expected):
            raise ValueError("Incomplete download. Try setup again.")
    return digest.hexdigest()


def install_models(directory):
    directory.mkdir(parents=True, exist_ok=True)
    with open_download(INDEX, timeout=30) as response:
        index = json.load(response)
    records = []
    for source, target in (("en", "zh"), ("zh", "en")):
        existing = False
        for metadata in directory.glob("*/metadata.json"):
            try:
                data = json.loads(metadata.read_text())
                if data.get("from_code") == source and data.get("to_code") == target:
                    if (metadata.parent / "model" / "model.bin").is_file():
                        existing = True
            except (OSError, ValueError):
                pass
        if existing:
            print(f"Already installed: {source} → {target}", flush=True)
            continue
        choices = [p for p in index if p.get("from_code") == source and p.get("to_code") == target]
        package = next((p for p in choices if p.get("package_version") == "1.9"), None)
        if package is None:
            raise RuntimeError(f"The supported {source} → {target} model version 1.9 is unavailable.")
        url = package["links"][0]
        print(f"Downloading offline model: {source} → {target}", flush=True)
        with tempfile.TemporaryDirectory(dir=directory) as temporary:
            archive = Path(temporary) / "model.argosmodel"
            checksum = download(url, archive)
            stage = Path(temporary) / "extracted"
            with zipfile.ZipFile(archive) as zipped:
                if zipped.testzip() is not None:
                    raise ValueError("Corrupt model archive.")
                for entry in zipped.infolist():
                    parts = Path(entry.filename).parts
                    if Path(entry.filename).is_absolute() or ".." in parts or "\\" in entry.filename:
                        raise ValueError("Unsafe path in model archive.")
                    if (entry.external_attr >> 16) & 0o170000 == 0o120000:
                        raise ValueError("Model archive contains a symbolic link.")
                zipped.extractall(stage)
            roots = [p for p in stage.iterdir() if p.is_dir() and (p / "metadata.json").is_file()]
            if len(roots) != 1:
                raise ValueError("Unexpected model archive structure.")
            root = roots[0]
            metadata = json.loads((root / "metadata.json").read_text())
            if metadata.get("from_code") != source or metadata.get("to_code") != target:
                raise ValueError("Downloaded model has the wrong language pair.")
            if not (root / "model/model.bin").is_file():
                raise ValueError("Downloaded model is incomplete.")
            destination = directory / root.name
            if destination.exists():
                import shutil
                shutil.rmtree(destination)
            root.rename(destination)
            records.append({"source": source, "target": target, "version": "1.9", "url": url, "sha256": checksum})
    manifest = directory / "download-manifest.json"
    previous = json.loads(manifest.read_text()) if manifest.exists() else []
    manifest.write_text(json.dumps(previous + records, indent=2) + "\n")


def install_dictionary(directory):
    print("Downloading CC-CEDICT word definitions…", flush=True)
    with tempfile.TemporaryDirectory(dir=directory) as temporary:
        archive = Path(temporary) / "cedict.gz"
        checksum = download(DICTIONARY, archive)
        data = gzip.decompress(archive.read_bytes())
        text = data.decode("utf-8")
        if "CC-CEDICT" not in text[:10000] or "Creative Commons" not in text[:10000]:
            raise ValueError("Dictionary license header is missing; the file was not installed.")
        target = directory / "cedict.u8"
        pending = directory / "cedict.u8.tmp"
        pending.write_bytes(data)
        pending.replace(target)
        # Retain the complete copyright and license header in the dictionary.
        (directory / "cedict-download.json").write_text(json.dumps({"url": DICTIONARY, "sha256": checksum}, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-dir", default=str(Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share")) / "hover-translate"))
    parser.add_argument("--models-only", action="store_true", help="Use an already installed runtime.")
    parser.add_argument("--dictionary-only", action="store_true")
    parser.add_argument("--without-dictionary", action="store_true")
    args = parser.parse_args()
    directory = Path(args.data_dir).expanduser().resolve()
    directory.mkdir(parents=True, exist_ok=True)
    if args.dictionary_only:
        install_dictionary(directory)
        return
    if not args.models_only:
        runtime = directory / "argos-env"
        if not (runtime / "bin/python").exists():
            print("Preparing the local translation runtime…", flush=True)
            venv.EnvBuilder(with_pip=True).create(runtime)
        python = str(runtime / "bin/python")
        subprocess.run([python, "-m", "pip", "install", "--disable-pip-version-check", "--index-url",
                        "https://download.pytorch.org/whl/cpu", "torch==2.14.1"], check=True)
        subprocess.run([python, "-m", "pip", "install", "--disable-pip-version-check", "--index-url",
                        "https://pypi.org/simple", "-r", str(Path(__file__).with_name("requirements.txt"))], check=True)
    install_models(directory / "argos-packages")
    if not args.without_dictionary:
        try:
            install_dictionary(directory)
        except Exception as error:
            print(f"Translation models installed. Optional dictionary download failed: {error}", flush=True)
    print("Offline English / Chinese setup is complete. Test translation in Settings.", flush=True)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"Offline setup failed: {error}", file=sys.stderr)
        sys.exit(1)
