"""Synthetic-fixture tests for tools.half_life1_xbox; no game content."""

import io
import struct
import unittest
from pathlib import Path
from types import SimpleNamespace

from tools.half_life1_xbox import (
    HalfLifeImageError,
    parse_xbe_header,
    read_pak_listing,
    safe_relative_path,
)


def make_xbe():
    data = bytearray(0x300)
    data[:4] = b"XBEH"
    struct.pack_into("<I", data, 0x104, 0x10000)
    struct.pack_into("<I", data, 0x118, 0x10184)
    struct.pack_into("<I", data, 0x11C, 13)
    data[0x184 + 12:0x184 + 32] = "Half-LifeX".encode("utf-16le")
    return bytes(data)


def make_pak(names):
    index = b"".join(struct.pack("<56sII", name.encode(), 12, 0) for name in names)
    return b"PACK" + struct.pack("<II", 12, len(index)) + index


class HalfLifeXTests(unittest.TestCase):
    def test_xbe_header(self):
        info = parse_xbe_header(make_xbe())
        self.assertEqual(info["title"], "Half-LifeX")
        self.assertEqual(info["section_count"], 13)
        self.assertEqual(len(info["executable_sha256"]), 64)

    def test_non_xbe_rejected(self):
        with self.assertRaises(HalfLifeImageError):
            parse_xbe_header(b"not-an-xbe")

    def test_pak_names(self):
        blob = make_pak(["maps/c0a0.bsp", "maps/t0a0.bsp"])
        iso = SimpleNamespace(base=0, _f=io.BytesIO(blob))
        files = read_pak_listing(iso, SimpleNamespace(sector=0, size=len(blob)))
        self.assertEqual(files, ["maps/c0a0.bsp", "maps/t0a0.bsp"])

    def test_invalid_pak_rejected(self):
        blob = b"PACK" + struct.pack("<II", 12, 4096)
        iso = SimpleNamespace(base=0, _f=io.BytesIO(blob))
        with self.assertRaises(HalfLifeImageError):
            read_pak_listing(iso, SimpleNamespace(sector=0, size=len(blob)))

    def test_traversal_rejected(self):
        self.assertEqual(safe_relative_path("valve/pak0.pak"), Path("valve", "pak0.pak"))
        for path in ("../secret", "/absolute", "C:\\Windows\\file", "foo//bar", "foo/./bar", "foo/../bar"):
            with self.subTest(path=path), self.assertRaises(HalfLifeImageError):
                safe_relative_path(path)


if __name__ == "__main__":
    unittest.main()
