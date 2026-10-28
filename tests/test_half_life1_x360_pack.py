"""Synthetic tests for Xbox 360 H360 CPU-staging mesh format. No game data."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.half_life1_x360_pack import MeshError, _encode, _key, _normal, _parse_obj, parse_header, pack_scene, estimate_texture_memory


class Xbox360MeshTests(unittest.TestCase):
    def test_flat_normal(self):
        self.assertEqual(_normal((0,0,0),(1,0,0),(0,0,1)), (0,-127,0))

    def test_material_key_stable_and_bounded(self):
        self.assertEqual(_key('lab_wall'), _key('lab_wall'))
        self.assertLessEqual(len(_key('very_long_material_texture_'*3)), 31)

    def test_valid_mesh_roundtrip(self):
        tri=[((0,0,0),(0,0)),((1,0,0),(1,0)),((0,0,1),(0,1))]
        payload, groups, tris = _encode({'lab_wall':[tri, tri]})
        result = parse_header(payload)
        self.assertEqual(result['groups'],1)
        self.assertEqual(result['triangles'],2)
        self.assertEqual(result['vertices'],3)
        self.assertEqual(result['indices'],6)
        self.assertEqual(tris,2)
        self.assertEqual(result['payload_bytes'],len(payload))

    def test_corrupt_header_rejected(self):
        with self.assertRaises(MeshError):
            parse_header(b'XXXX')

    def test_obj_and_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root/'hl1_test_world.obj').write_text(
                'mtllib test.mtl\nusemtl test_wall\n'
                'v 0 0 0\nv 1 0 0\nv 0 0 1\n'
                'vt 0 0\nvt 1 0\nvt 0 1\n'
                'f 1/1 2/2 3/3\n', encoding='utf-8')
            (root/'material_manifest.json').write_text(json.dumps({
                'exported_triangles':1, 'starter_pbr_resolution':512,
                'materials':[{'material_id':'test_wall','preset':'concrete'}],
            }))
            report=pack_scene(root)
            self.assertEqual(report['triangles'],1)
            self.assertEqual(report['groups'],1)
            self.assertTrue((root/'hl1_test_world.h360').exists())
            self.assertEqual(report['texture_estimate']['unique_material_sets'],1)

    def test_untrusted_index_rejected(self):
        tri=[((0,0,0),(0,0)),((1,0,0),(1,0)),((0,0,1),(0,1))]
        payload,_,_=_encode({'lab_wall':[tri]})
        altered=bytearray(payload)
        from tools.half_life1_x360_pack import HEADER
        off=HEADER.unpack_from(payload)[-1]
        altered[off:off+2]=b'\xff\xff'
        with self.assertRaises(MeshError):
            parse_header(altered)

    def test_missing_material_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            (root/'hl1_test_world.obj').write_text(
                'usemtl missing\nv 0 0 0\nv 1 0 0\nv 0 0 1\n'
                'vt 0 0\nvt 1 0\nvt 0 1\nf 1/1 2/2 3/3\n')
            (root/'material_manifest.json').write_text('{"exported_triangles":1,"materials":[]}')
            with self.assertRaises(MeshError):
                pack_scene(root)

if __name__=='__main__':
    unittest.main()
