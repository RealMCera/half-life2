"""Synthetic GoldSrc BSP fixtures; contains no owned game data."""
import struct
import tempfile
from pathlib import Path
import unittest

from tools.half_life1_modern_scene import _axis, _clean_name, bsp_to_obj, GraphicsError


def sample_bsp():
    data = bytearray(124)
    struct.pack_into('<i', data, 0, 30)
    lumps = {}
    def add(index, blob):
        start = len(data)
        data.extend(blob)
        lumps[index] = (start,len(blob))
    mip = bytearray(4 + 4 + 40)
    struct.pack_into('<i', mip, 0, 1)
    struct.pack_into('<i', mip, 4, 8)
    struct.pack_into('<16sII', mip, 8, b'lab_wall', 64, 64)
    add(2, mip)
    vertices = b''.join(struct.pack('<3f', *v) for v in [(0,0,0),(64,0,0),(64,64,0),(0,64,0)])
    add(3, vertices)
    add(6, struct.pack('<8fii', 1,0,0,0,0,1,0,0,0,0))
    add(7, struct.pack('<Hhihh4si', 0,0,0,4,0,b'\0\0\0\0',-1))
    add(12, b''.join(struct.pack('<HH', i,(i+1)%4) for i in range(4)))
    add(13, struct.pack('<4i',0,1,2,3))
    add(14, struct.pack('<9f7i',*(0.0 for _ in range(9)),*(0 for _ in range(5)),0,1))
    for n,(start,size) in lumps.items():
        struct.pack_into('<II',data,4+n*8,start,size)
    return bytes(data)


class SceneExportTests(unittest.TestCase):
    def test_coordinate_conversion(self):
        self.assertEqual(_axis((2,3,4), 1), (2,4,-3))

    def test_texture_names_unique_after_clean(self):
        self.assertNotEqual(_clean_name('+0_test'), _clean_name('-0_test'))

    def test_world_export(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)/'lab.obj'
            report = bsp_to_obj(sample_bsp(),out)
            self.assertEqual(report['exported_triangles'],2)
            self.assertEqual(report['material_count'],1)
            self.assertEqual(report['worldspawn_faces'],1)
            self.assertEqual(report['skipped_faces'],0)
            txt=out.read_text()
            self.assertEqual(txt.count('\nf '),2)
            self.assertIn('mtllib lab.mtl',txt)
            self.assertIn('v 1.625600',txt)
            self.assertTrue((out.with_name('material_manifest.json')).exists())
            self.assertTrue((out.with_suffix('.mtl')).exists())

    def test_corrupt_geometry_rejected(self):
        broken=bytearray(sample_bsp())
        struct.pack_into('<II', broken,4+7*8,100000,20)
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(GraphicsError):
                bsp_to_obj(broken, Path(temp)/'bad.obj')

if __name__=='__main__':
    unittest.main()
