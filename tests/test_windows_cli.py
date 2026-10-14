# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise Windows GUI-executable diagnostics through redirected pipes."""
import os
import subprocess
import sys
import unittest

BINARY = sys.argv.pop(1)
ENVIRONMENT = dict(os.environ, QT_QPA_PLATFORM="windows")

class WindowsCliTest(unittest.TestCase):
    def run_app(self, *arguments):
        return subprocess.run([BINARY, *arguments], capture_output=True, text=True,
                              encoding="utf-8", env=ENVIRONMENT, timeout=10)

    def test_diagnostics_use_stdout(self):
        for flag in ("--help", "--help-all", "--version"):
            with self.subTest(flag=flag):
                result=self.run_app(flag)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertTrue(result.stdout.strip())

    def test_control_looking_values_are_text(self):
        for text in ("--version", "--help", "-v", "-h"):
            with self.subTest(text=text):
                result=self.run_app("--translate",text,"--target","en")
                self.assertEqual(result.returncode,0,result.stderr)
                self.assertEqual(result.stdout.strip(),text)

    def test_unknown_option_reports_error(self):
        result=self.run_app("--not-a-hover-option")
        self.assertEqual(result.returncode,2)
        self.assertTrue(result.stderr.strip())

if __name__ == "__main__": unittest.main()
