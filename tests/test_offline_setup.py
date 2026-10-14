#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import importlib.util
import io
import json
import zipfile
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec); spec.loader.exec_module(result)
    return result

root = Path(__file__).resolve().parent.parent
setup = module("hover_setup", root / "offline/setup_offline.py")
bridge = module("hover_bridge", root / "offline/argos_bridge.py")

class Response(io.BytesIO):
    def __init__(self, data, url="https://example.com/model", length=None):
        super().__init__(data); self.url = url
        self.headers = {"Content-Length": str(len(data) if length is None else length)}
    def geturl(self): return self.url

class OfflineSetupTest(unittest.TestCase):
    def test_runtime_python_uses_native_layout(self):
        with tempfile.TemporaryDirectory() as directory:
            runtime = Path(directory) / "argos-env"
            self.assertEqual(setup.runtime_python(runtime, windows=True), runtime / "Scripts/python.exe")
            self.assertEqual(setup.runtime_python(runtime, windows=False), runtime / "bin/python")

    def test_windows_default_data_is_beside_installed_helpers(self):
        expected = root / "data"
        self.assertEqual(setup.default_data_directory(windows=True), expected)

    def test_partial_model_is_preserved_outside_package_search(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary) / "argos-packages"
            directory.mkdir()
            partial = directory / "translate-en_zh-1_9"
            partial.mkdir()
            (partial / "metadata.json").write_text(json.dumps({"from_code": "en", "to_code": "zh"}))
            (partial / "user-note.txt").write_text("keep this")
            installed = directory / "translate-zh_en-1_9"
            (installed / "model").mkdir(parents=True)
            (installed / "metadata.json").write_text(json.dumps({"from_code": "zh", "to_code": "en"}))
            (installed / "model/model.bin").write_bytes(b"existing")
            payload = io.BytesIO()
            with zipfile.ZipFile(payload, "w") as archive:
                archive.writestr("translate-en_zh-1_9/metadata.json", json.dumps({"from_code": "en", "to_code": "zh"}))
                archive.writestr("translate-en_zh-1_9/model/model.bin", b"new model")
            index = [{"from_code": "en", "to_code": "zh", "package_version": "1.9", "links": ["https://example.com/model"]}]
            def download(url, target):
                target.write_bytes(payload.getvalue())
                return "test-checksum"
            with patch.object(setup, "open_download", return_value=Response(json.dumps(index).encode())), patch.object(setup, "download", side_effect=download):
                setup.install_models(directory)
            self.assertEqual((partial / "model/model.bin").read_bytes(), b"new model")
            backups = list((directory.parent / "argos-package-backups").glob("translate-en_zh-1_9*"))
            self.assertEqual(len(backups), 1)
            self.assertEqual((backups[0] / "user-note.txt").read_text(), "keep this")
            self.assertEqual((installed / "model/model.bin").read_bytes(), b"existing")

    def test_identifies_app_and_checks_content(self):
        data = b"model data"
        def request(request, timeout):
            self.assertIn("HoverTranslate/",request.get_header("User-agent"))
            self.assertEqual(request.full_url,"https://example.com/model")
            return Response(data)
        with tempfile.TemporaryDirectory() as directory, patch.object(setup.urllib.request,"urlopen",side_effect=request):
            target = Path(directory) / "model"
            self.assertEqual(setup.download("https://example.com/model",target),hashlib.sha256(data).hexdigest())
            self.assertEqual(target.read_bytes(),data)

    def test_rejects_downgrade_redirect_and_truncation(self):
        with tempfile.TemporaryDirectory() as directory:
            for response in [Response(b"data",url="http://example.com/model"),Response(b"data",length=50)]:
                with patch.object(setup.urllib.request,"urlopen",return_value=response):
                    with self.assertRaises(ValueError): setup.download("https://example.com/model",Path(directory)/"model")

    def test_dictionary_inflections_preserve_exact_lookup(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/"cedict.u8"
            path.write_text("# test-authored definitions\n跑 跑 [pao3] /to run/\n学习 学习 [xue2 xi2] /to study/\n眼镜 眼镜 [yan3 jing4] /glasses/\n",encoding="utf-8")
            dictionary = bridge.Dictionary(path)
            self.assertIn("running → run",dictionary.lookup("running","en"))
            self.assertIn("ran → run",dictionary.lookup("ran","en"))
            self.assertIn("studies → study",dictionary.lookup("studies","en"))
            self.assertIn("眼镜",dictionary.lookup("glasses","en"))
            self.assertNotIn("→",dictionary.lookup("glasses","en"))
            self.assertEqual(dictionary.lookup("unknownword","en"),"")
            self.assertEqual(dictionary.lookup("跑","zh-CN"),"to run")

    def test_local_sentence_chunks_need_no_model_or_network(self):
        splitter = bridge.LocalSentenceSplitter(None)
        self.assertEqual(splitter.split_sentences("Hello world. 你好世界！Hello again!"),["Hello world.","你好世界！","Hello again!"])
        for text in ["中"*1200,"verylongword"*100,"word "*1000]:
            chunks = splitter.split_sentences(text)
            self.assertTrue(all(0<len(chunk)<=500 for chunk in chunks))
            self.assertEqual("".join(chunks).replace(" ",""),text.replace(" ",""))

if __name__ == "__main__": unittest.main()
