"""Synthetic tests for the Xenia LUT experiment patch; no proprietary code."""
from pathlib import Path
import tempfile
import unittest

from tools.patch_xsdk_xenia_lut_diagnostic import ORIGINAL, REPLACEMENT, patch


class LutPatchTest(unittest.TestCase):
    def test_patches_only_exact_known_block(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            path = root / "src" / "display.c"
            path.parent.mkdir()
            path.write_text("prefix\n" + ORIGINAL + "suffix\n")
            result = patch(root)
            self.assertEqual(result, path)
            source = path.read_text()
            self.assertIn(REPLACEMENT, source)
            self.assertIn("prefix", source)
            self.assertIn("suffix", source)
            self.assertNotIn(ORIGINAL, source)
            with self.assertRaises(ValueError):
                patch(root)  # fail-closed, no ambiguous double patch

    def test_refuses_changed_upstream(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "src" / "display.c"
            path.parent.mkdir()
            path.write_text("a different upstream implementation")
            with self.assertRaises(ValueError):
                patch(td)
            self.assertEqual(path.read_text(), "a different upstream implementation")


if __name__ == "__main__":
    unittest.main()
