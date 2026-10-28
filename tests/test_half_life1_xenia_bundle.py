"""No game assets: XEX2 fixture header and generic mesh/texture bytes only."""
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from tools.half_life1_xenia_bundle import BundleError, stage, verify


class TestXeniaBundle(unittest.TestCase):
    def test_stage_and_verify(self):
        with TemporaryDirectory() as td:
            root = Path(td)
            xex = root / "hello.xex"
            xex.write_bytes(b"XEX2" + b"test-not-real-executable")
            scene = root / "scene"
            scene.mkdir()
            (scene / "hl1_c0a0_world.h360").write_bytes(b"H360fixture")
            (scene / "material_manifest.json").write_text("{}")
            (scene / "materials").mkdir()
            (scene / "materials" / "concrete_basecolor.png").write_bytes(b"fakepng")
            output = root / "build-001"
            manifest = stage(xex, output, scene)
            self.assertEqual(len(manifest["files"]), 4)
            self.assertEqual(verify(output), 4)
            self.assertTrue((output / "xenia_payload.zip").exists())
            self.assertEqual((output / "payload" / "default.xex").read_bytes(), xex.read_bytes())

    def test_reject_wrong_xex_header(self):
        with TemporaryDirectory() as td:
            root = Path(td)
            xex = root / "wrong.xex"
            xex.write_bytes(b"XBEH" + b"x" * 12)
            with self.assertRaises(BundleError):
                stage(xex, root / "out")

    def test_reject_overwrite(self):
        with TemporaryDirectory() as td:
            root = Path(td)
            xex = root / "valid.xex"
            xex.write_bytes(b"XEX2" + b"fake")
            stage(xex, root / "out")
            with self.assertRaises(BundleError):
                stage(xex, root / "out")

    def test_detect_corruption(self):
        with TemporaryDirectory() as td:
            root = Path(td)
            xex = root / "valid.xex"
            xex.write_bytes(b"XEX2" + b"fake")
            stage(xex, root / "out")
            (root / "out" / "payload" / "default.xex").write_bytes(b"XEX2" + b"corrupt")
            with self.assertRaises(BundleError):
                verify(root / "out")

    def test_scene_material_file_filter(self):
        with TemporaryDirectory() as td:
            root = Path(td)
            xex = root / "valid.xex"
            xex.write_bytes(b"XEX2" + b"fake")
            scene = root / "scene"
            scene.mkdir()
            (scene / "hl1_test_world.h360").write_bytes(b"H360")
            (scene / "material_manifest.json").write_text("{}")
            (scene / "materials").mkdir()
            (scene / "materials" / "game.iso").write_bytes(b"not-allowed")
            with self.assertRaises(BundleError):
                stage(xex, root / "out", scene)

    def test_reject_manifest_tamper(self):
        with TemporaryDirectory() as td:
            root = Path(td)
            xex = root / "valid.xex"
            xex.write_bytes(b"XEX2" + b"fake")
            stage(xex, root / "out")
            manifest_path = root / "out" / "manifest.json"
            manifest = json.loads(manifest_path.read_text())
            manifest["files"][0]["path"] = "../default.xex"
            manifest_path.write_text(json.dumps(manifest))
            with self.assertRaises(BundleError):
                verify(root / "out")


if __name__ == "__main__":
    unittest.main()
