"""Host-side regression test for the portable, *original* Xbox 360 H360 CPU preview.

Compile with the GitHub Ubuntu runner's host GCC + a stub xsdk logger,
then run the exact C renderer against a minimal synthetic H360 v1 mesh.
No Xbox SDK, Half-Life assets or Xenia required for this regression.
This DOES NOT establish that the PPC XEX or its file-writing works in Xenia.
"""
from collections import OrderedDict
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

from tools.half_life1_x360_pack import _encode

ROOT = Path(__file__).resolve().parents[1]
RENDER_DIR = ROOT / "projects" / "half-life1-xbox" / "x360_probe"


class PreviewTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.gcc = shutil.which("gcc")
        if not cls.gcc:
            raise unittest.SkipTest("Host GCC not installed; test is for Linux CI")

    def compile(self, root):
        inc = root / "stub" / "xsdk"
        inc.mkdir(parents=True)
        (inc / "log.h").write_text(
            '#ifndef XSDK_LOG_H\n#define XSDK_LOG_H\n'
            'void xsdk_log_write(const char *fmt, ...);\n#endif\n'
        )
        testmain = root / "testmain.c"
        testmain.write_text(
            '#include <stdio.h>\n#include <stdarg.h>\n#include "wireframe.h"\n'
            'void xsdk_log_write(const char *fmt, ...) {\n'
            'va_list args; va_start(args, fmt); vfprintf(stdout, fmt, args);'
            ' va_end(args); puts(""); }\n'
            'int main(int argc, char **argv) {'
            'return argc != 2 || !hl1_preview_h360(argv[1]);}\n'
        )
        output = root / "preview_test"
        subprocess.run(
            [self.gcc, "-std=gnu17", "-Wall", "-Wextra", "-Werror", "-O1",
             "-I", str(root / "stub"), "-I", str(RENDER_DIR),
             str(testmain), str(RENDER_DIR / "wireframe.c"),
             "-lm", "-o", str(output)],
            check=True, capture_output=True, text=True,
        )
        return output

    def fixture(self):
        # All geometry generated here; no Valve game assets.
        triangle = (
            ((0.0, 0.0, 0.0), (0.0, 0.0)),
            ((2.0, 0.0, 0.0), (1.0, 0.0)),
            ((0.0, 1.0, 0.0), (0.0, 1.0)),
        )
        return _encode(OrderedDict([("SYNTHETIC_TEST", [triangle])]))[0]

    def test_full_mesh_and_preview_bmp(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            app = self.compile(root)
            mesh = root / "triangle.h360"
            mesh.write_bytes(self.fixture())
            proc = subprocess.run([str(app), str(mesh)], cwd=root, capture_output=True, text=True)
            self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
            self.assertIn("FULL_MESH_PASS", proc.stdout)
            self.assertIn("BMP_PREVIEW_PASS", proc.stdout)
            self.assertIn("ASCII_PREVIEW_PASS", proc.stdout)
            self.assertEqual(proc.stdout.count("HL1_WIRE|"), 24)
            bmp = root / r"cache:\hl1_wireframe.bmp"
            self.assertTrue(bmp.is_file())
            data = bmp.read_bytes()
            self.assertEqual(data[:2], b"BM")
            self.assertEqual(struct.unpack_from("<I", data, 18)[0], 640)
            self.assertEqual(struct.unpack_from("<I", data, 22)[0], 360)
            self.assertEqual(len(data), 54 + 640 * 360 * 3)
            self.assertTrue(any(data[i:i+3] != b"\x20\x18\x12"
                                for i in range(54, len(data), 3)))

    def test_ascii_preview_survives_bitmap_write_denial(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            app = self.compile(root)
            mesh = root / "triangle.h360"
            mesh.write_bytes(self.fixture())
            # On Linux these are literal filenames. Pre-create directories
            # using each name to force fopen(...,"wb") to fail.
            for name in ("cache:", "cache0:", "cache1:", "game:"):
                (root / (name + "\\hl1_wireframe.bmp")).mkdir()
            proc = subprocess.run(
                [str(app), str(mesh)], cwd=root, capture_output=True, text=True)
            self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
            self.assertIn("FULL_MESH_PASS", proc.stdout)
            self.assertIn("ASCII_PREVIEW_PASS", proc.stdout)
            self.assertIn("BMP_PREVIEW_UNAVAILABLE", proc.stdout)

    def test_reject_invalid_group_index(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            app = self.compile(root)
            payload = bytearray(self.fixture())
            index_offset = struct.unpack_from(">I", payload, 28)[0]
            payload[index_offset:index_offset + 2] = b"\x00\x63"
            mesh = root / "bad.h360"
            mesh.write_bytes(payload)
            proc = subprocess.run([str(app), str(mesh)], cwd=root, capture_output=True, text=True)
            self.assertNotEqual(proc.returncode, 0)
            self.assertIn("FULL_MESH_FAIL", proc.stdout)
            self.assertFalse((root / r"cache:\hl1_wireframe.bmp").exists())


if __name__ == "__main__":
    unittest.main()
