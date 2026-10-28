"""Tests for synthetic PBR starter textures; no game assets."""
import json
import tempfile
from pathlib import Path
import unittest

from tools.half_life1_pbr_starter import PRESETS, build_pack, choose_preset, generate_preset


class PBRTests(unittest.TestCase):
    def test_assignments(self):
        self.assertEqual(choose_preset('crete4_flr03'), 'concrete')
        self.assertEqual(choose_preset('frostgrate'), 'brushed_metal')
        self.assertEqual(choose_preset('LABWALL01'), 'painted_panel')

    def test_maps_are_bounded(self):
        for name in PRESETS:
            maps = generate_preset(name, 64)
            self.assertEqual(set(maps), {'basecolor','normal','roughness','metallic'})
            self.assertEqual(maps['normal'].shape, (64,64,3))
            self.assertEqual(maps['roughness'].shape,(64,64))
            self.assertLessEqual(maps['metallic'].max(), 255)

    def test_build_pack_updates_manifest(self):
        with tempfile.TemporaryDirectory() as temp:
            directory=Path(temp)
            info={'materials':[{'source_texture':'crete4_flr03','material_id':'test_concrete'}]}
            (directory/'material_manifest.json').write_text(json.dumps(info))
            (directory/'hl1_test_world.obj').write_text('mtllib hl1_test_world.mtl\n')
            count=build_pack(directory,resolution=64)
            self.assertEqual(count,1)
            data=json.loads((directory/'material_manifest.json').read_text())
            self.assertTrue((directory/data['materials'][0]['pbr']['normal']).exists())
            self.assertIn('map_Kd', (directory/'hl1_test_world.mtl').read_text())

if __name__=='__main__': unittest.main()
