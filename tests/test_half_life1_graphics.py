"""Synthetic fixture tests; no game assets included."""
import struct
import unittest

from tools.half_life1_graphics import GraphicsError, classify, parse_bsp_textures, tga_bytes


def synthetic_bsp(name='crete4_flr03'):
    data = bytearray(124 + 4 + 4 + 40)
    struct.pack_into('<i', data, 0, 30)
    struct.pack_into('<II', data, 4 + 2*8, 124, len(data)-124)
    struct.pack_into('<II', data, 124, 1, 8)
    struct.pack_into('<16sII', data, 124+8, name.encode(), 128, 128)
    return bytes(data)


class GraphicsTests(unittest.TestCase):
    def test_bsp_tex_name_and_size(self):
        self.assertEqual(parse_bsp_textures(synthetic_bsp()),
                         [{'name': 'crete4_flr03', 'width': 128, 'height': 128}])

    def test_bad_bsp_rejected(self):
        with self.assertRaises(GraphicsError):
            parse_bsp_textures(b'not a bsp')
        malformed = bytearray(synthetic_bsp())
        struct.pack_into('<II', malformed, 4+2*8, 50000, 200)
        with self.assertRaises(GraphicsError):
            parse_bsp_textures(malformed)

    def test_material_classification(self):
        self.assertEqual(classify('crete4_flr03'), 'concrete')
        self.assertEqual(classify('frostgrate'), 'metal')
        self.assertIsNone(classify('+0~fifties_lgt2'))
        self.assertIsNone(classify('poster15'))

    def test_tga_dimensions(self):
        tga = tga_bytes('metal', 16, 16)
        self.assertEqual(len(tga), 18 + 16 * 16 * 3)
        self.assertEqual(tga[2], 2)
        self.assertEqual(struct.unpack_from('<HH', tga, 12), (16, 16))
        self.assertEqual(tga[16], 24)
        self.assertEqual(tga[17] & 0x20, 0x20)
        with self.assertRaises(GraphicsError):
            tga_bytes('unknown')


if __name__ == '__main__':
    unittest.main()
