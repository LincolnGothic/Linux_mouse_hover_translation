#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import importlib.util
import io
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

    def test_local_sentence_chunks_need_no_model_or_network(self):
        splitter = bridge.LocalSentenceSplitter(None)
        self.assertEqual(splitter.split_sentences("Hello world. 你好世界！Hello again!"),["Hello world.","你好世界！","Hello again!"])
        for text in ["中"*1200,"verylongword"*100,"word "*1000]:
            chunks = splitter.split_sentences(text)
            self.assertTrue(all(0<len(chunk)<=500 for chunk in chunks))
            self.assertEqual("".join(chunks).replace(" ",""),text.replace(" ",""))

if __name__ == "__main__": unittest.main()
