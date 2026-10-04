#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Local JSON-lines worker. Network access is disabled during translation."""
import argparse
import contextlib
import json
import os
from pathlib import Path
import re
import socket
import sys


class Dictionary:
    """CC-CEDICT words, with exact English-definition reverse lookup."""

    def __init__(self, path):
        self.chinese = {}
        self.english = {}
        if not path or not Path(path).is_file():
            return
        with open(path, encoding="utf-8-sig") as stream:
            for line in stream:
                if line.startswith("#"):
                    continue
                match = re.match(r"^(\S+) (\S+) \[([^]]+)\] /(.+)/\s*$", line)
                if not match:
                    continue
                traditional, simplified, pinyin, meanings = match.groups()
                definitions = meanings.split("/")
                for word in {traditional, simplified}:
                    self.chinese.setdefault(word, []).append("; ".join(definitions))
                for definition in definitions:
                    # Searching English definitions is useful, but it is not a
                    # comprehensive English-to-Chinese dictionary.
                    key = definition.casefold().strip()
                    if key.startswith("to "):
                        key = key[3:]
                    if re.fullmatch(r"[a-z][a-z '\-]{0,60}", key):
                        self.english.setdefault(key, []).append(f"{simplified} [{pinyin}] — {'; '.join(definitions)}")

    def lookup(self, text, source):
        values = (self.chinese if source == "zh-CN" else self.english).get(text.strip().casefold(), [])
        return "\n".join(dict.fromkeys(values[:8]))


def deny_network(*args, **kwargs):
    raise RuntimeError("Network access is disabled during offline translation. Run offline setup to download missing models.")


def local_translate(text, source, target):
    # Load the heavyweight runtime once, only when a dictionary entry is absent.
    import argostranslate.settings as settings
    settings.device = "cpu"
    settings.model_provider = settings.ModelProvider.OPENNMT
    settings.intra_threads = 2
    import stanza
    from stanza.pipeline.core import DownloadMethod
    import functools
    if not getattr(stanza.Pipeline, "_hover_offline", False):
        stanza.Pipeline = functools.partial(stanza.Pipeline, download_method=DownloadMethod.NONE)
        stanza.Pipeline._hover_offline = True
    import argostranslate.translate as translate
    source_code = "zh" if source == "zh-CN" else "en"
    target_code = "zh" if target == "zh-CN" else "en"
    languages = {language.code: language for language in translate.get_installed_languages()}
    if source_code not in languages or target_code not in languages:
        raise RuntimeError("English / Chinese translation models are missing. Use Install offline models in Settings.")
    translation = languages[source_code].get_translation(languages[target_code])
    if translation is None:
        raise RuntimeError(f"The {source_code} → {target_code} model is missing. Use Install offline models in Settings.")
    return translation.translate(text)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--worker", action="store_true", required=True)
    parser.add_argument("--packages-dir", required=True)
    parser.add_argument("--dictionary", default="")
    args = parser.parse_args()
    os.environ["ARGOS_PACKAGES_DIR"] = args.packages_dir
    os.environ["ARGOS_DEVICE_TYPE"] = "cpu"
    os.environ["ARGOS_MODEL_PROVIDER"] = "OPENNMT"
    os.environ["ARGOS_CHUNK_TYPE"] = "STANZA"
    os.environ["ARGOS_INTRA_THREADS"] = "2"
    # Disable connections before importing any model library, including implicit
    # dependency downloads. Setup is a separate, explicitly invoked program.
    socket.socket.connect = deny_network
    socket.socket.connect_ex = deny_network
    socket.create_connection = deny_network
    dictionary = Dictionary(args.dictionary)
    print(json.dumps({"ready": True}), flush=True)
    for line in sys.stdin:
        identifier = None
        try:
            request = json.loads(line)
            identifier = request["id"]
            text, source, target = request["text"], request["source"], request["target"]
            if not isinstance(text, str) or not text.strip() or len(text) > 10000:
                raise ValueError("Use between 1 and 10,000 characters.")
            if source not in ("en", "zh-CN") or target not in ("en", "zh-CN"):
                raise ValueError("Choose English or Simplified Chinese.")
            if source == "zh-CN":
                text = re.sub(r"(?<=[\u3400-\u9fff])\s+(?=[\u3400-\u9fff])", "", text)
            with contextlib.redirect_stdout(sys.stderr):
                result = text if source == target else dictionary.lookup(text, source)
                if not result:
                    result = local_translate(text, source, target)
            response = {"id": identifier, "translation": result}
        except ImportError:
            response = {"id": identifier, "error": "The offline runtime is not installed. Use Install offline models in Settings."}
        except Exception as error:
            response = {"id": identifier, "error": str(error)[:500]}
        print(json.dumps(response, ensure_ascii=False), flush=True)


if __name__ == "__main__":
    main()
