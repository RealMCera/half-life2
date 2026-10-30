"""Prototype Half-Life 1 visual remaster overlays from an owned Half-LifeX XISO.

    py -3 -m tools.half_life1_graphics inspect Half-LifeX_Test_v2.iso --map c0a0
    py -3 -m tools.half_life1_graphics stage Half-LifeX_Test_v2.iso --map c0a0 -o local/hl1_remaster

Stages independent, original, procedural detail overlays (uncompressed TGA) and
maps/c0a0_detail.txt. Does NOT convert Source materials, patch an ISO, or build
an executable; an Xbox-compatible Half-LifeX renderer must load them in game.
"""

import argparse
import json
import math
from pathlib import Path
import re
import struct
import sys

from tools.xiso.xdvdfs import Xiso, XisoError, SECTOR


class GraphicsError(ValueError):
    pass


def pak_index(iso):
    e = iso.find('pak0.pak', 'valve')
    if e is None or e.is_dir:
        raise GraphicsError('Missing valve/pak0.pak')
    base = iso.base + e.sector * SECTOR
    iso._f.seek(base)
    header = iso._f.read(12)
    if len(header) != 12 or header[:4] != b'PACK':
        raise GraphicsError('Not a PACK archive')
    directory_offset, size = struct.unpack_from('<II', header, 4)
    if directory_offset < 12 or size > 16 * 1024 * 1024 or size % 64 or directory_offset + size > e.size:
        raise GraphicsError('Invalid PACK index size or position')
    iso._f.seek(base + directory_offset)
    index = iso._f.read(size)
    if len(index) != size:
        raise GraphicsError('Truncated PACK index')
    entries = {}
    for i in range(0, size, 64):
        name, offset, length = struct.unpack_from('<56sII', index, i)
        name = name.split(b'\0', 1)[0].decode('latin-1').replace('\\', '/').lower()
        if not name or name.startswith('/') or '..' in name.split('/'):
            raise GraphicsError('Unsafe PACK file name')
        if offset < 12 or offset + length > directory_offset:
            raise GraphicsError('Invalid PACK entry bounds: ' + name)
        entries[name] = (base + offset, length)
    return entries


def parse_bsp_textures(data):
    """GoldSrc version-30 BSP lump 2 (miptex); no game files are rewritten."""
    if len(data) < 124 or struct.unpack_from('<i', data)[0] != 30:
        raise GraphicsError('Expected Half-Life/GoldSrc BSP version 30')
    pos, size = struct.unpack_from('<II', data, 4 + 2 * 8)
    if size < 4 or pos > len(data) or size > len(data) - pos:
        raise GraphicsError('Texture lump outside BSP')
    lump = memoryview(data)[pos:pos + size]
    count = struct.unpack_from('<i', lump)[0]
    if count < 0 or count > 16384 or 4 + count * 4 > len(lump):
        raise GraphicsError('Invalid miptex count')
    textures = []
    for i in range(count):
        offset = struct.unpack_from('<i', lump, 4 + 4 * i)[0]
        if offset == -1:
            continue
        if offset < 4 + count * 4 or offset + 40 > len(lump):
            raise GraphicsError('Miptex header outside texture lump')
        name_bytes, width, height = struct.unpack_from('<16sII', lump, offset)
        name = name_bytes.split(b'\x00', 1)[0].decode('latin-1')
        if name and 1 <= width <= 4096 and 1 <= height <= 4096:
            textures.append({'name': name, 'width': width, 'height': height})
    return textures


def map_textures(iso, short_map):
    if not re.fullmatch(r'[A-Za-z0-9_\-]{1,48}', short_map):
        raise GraphicsError('Map must be its short name (example c0a0)')
    entries = pak_index(iso)
    key = 'maps/' + short_map.lower() + '.bsp'
    if key not in entries:
        raise GraphicsError('Map not in pak0.pak: ' + key)
    pos, length = entries[key]
    if length > 64 * 1024 * 1024:
        raise GraphicsError('Map exceeds safe limit')
    iso._f.seek(pos)
    data = iso._f.read(length)
    if len(data) != length:
        raise GraphicsError('Truncated map in PACK')
    return parse_bsp_textures(data)


def classify(name):
    """Conservative starter mappings; special/light/sign textures are excluded."""
    lower = name.lower()
    if not name or name[0] in '+-!~{' or any(s in lower for s in
        ('sky', 'light', 'lght', 'origin', 'clip', 'trigger', 'black', 'poster', 'glass', 'red', 'yellow')):
        return None
    if any(s in lower for s in ('crete', 'concr', 'cement', 'stone', 'brick')):
        return 'concrete'
    if any(s in lower for s in ('metal', 'mtl', 'steel', 'iron', 'grate', 'vent')):
        return 'metal'
    return None


def tga_bytes(kind, width=128, height=128):
    """Generate a tileable original grayscale overlay (not a higher-res HL2 asset)."""
    if kind not in ('concrete', 'metal'):
        raise GraphicsError('Unknown detail preset ' + kind)
    header = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0,
                         width, height, 24, 0x20)
    pixels = bytearray(width * height * 3)
    for y in range(height):
        for x in range(width):
            a = math.sin(2 * math.pi * (x * 7 / width + y * 13 / height))
            b = math.sin(2 * math.pi * (x * 31 / width - y * 29 / height))
            c = math.cos(2 * math.pi * (x * 43 / width + y * 47 / height))
            if kind == 'concrete':
                value = 130 + 15*a + 11*b + 7*c
            else:
                line = math.cos(2 * math.pi * (y * 31 / height))
                value = 135 + 12*line + 7*a + 5*c
            value = max(0, min(255, round(value)))
            p = (y * width + x) * 3
            pixels[p:p+3] = bytes((value, value, value))
    return header + pixels


def stage(iso, short_map, output, assignments=None):
    textures = map_textures(iso, short_map)
    by_name = {tex['name'].lower(): tex for tex in textures}
    assignments = assignments or {}
    unknown = set(assignments) - set(by_name)
    if unknown:
        raise GraphicsError('Assignments refer to unknown map textures: ' + ', '.join(sorted(unknown)))
    for key, value in assignments.items():
        if value not in ('concrete', 'metal', 'skip'):
            raise GraphicsError('Unsupported assignment %s for %s' % (value, key))
    applied = {}
    for tex in textures:
        name = tex['name']
        material = assignments.get(name.lower(), classify(name))
        if material and material != 'skip':
            applied[name] = material
    dest = Path(output).resolve()
    if dest.is_file():
        raise GraphicsError('Output is not a directory')
    detail = dest / 'valve' / 'gfx' / 'detail'
    maps = dest / 'valve' / 'maps'
    detail.mkdir(parents=True, exist_ok=True)
    maps.mkdir(parents=True, exist_ok=True)
    used = set(applied.values())
    for kind in sorted(used):
        (detail / ('hl1_' + kind + '.tga')).write_bytes(tga_bytes(kind))
    # Half-LifeX gl_rmisc.c parses: textureName gfx-relativeImage scaleX scaleY
    content = ''.join('%s detail/hl1_%s.tga 4.0 4.0\n' % (name, kind)
                      for name, kind in sorted(applied.items()))
    (maps / (short_map.lower() + '_detail.txt')).write_text(content, encoding='ascii')
    report = {'map': short_map.lower(), 'source_texture_count': len(textures),
              'detail_texture_count': len(applied), 'mappings': applied,
              'note': 'Experimental detail pass; visual effect, memory and performance require Xbox runtime validation.'}
    (dest / 'remaster_report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest='command', required=True)
    for command in ('inspect', 'stage'):
        c = sub.add_parser(command)
        c.add_argument('iso')
        c.add_argument('--map', default='c0a0', help='BSP basename, default c0a0')
        if command == 'stage':
            c.add_argument('-o', '--output', required=True)
            c.add_argument('--assignments', help='Optional JSON file mapping texture names to concrete, metal, or skip')
    args = p.parse_args(argv)
    try:
        with Xiso(args.iso) as iso:
            if args.command == 'inspect':
                textures = map_textures(iso, args.map)
                report = {'map': args.map, 'textures': textures,
                          'proposed_overlays': {x['name']: classify(x['name']) for x in textures if classify(x['name'])}}
            else:
                assignments = None
                if args.assignments:
                    assignments = json.loads(Path(args.assignments).read_text(encoding='utf-8'))
                    if not isinstance(assignments, dict) or any(not isinstance(k, str) or not isinstance(v, str) for k, v in assignments.items()):
                        raise GraphicsError('Assignments JSON must map texture names to material strings')
                    assignments = {k.lower(): v for k, v in assignments.items()}
                report = stage(iso, args.map, args.output, assignments)
        print(json.dumps(report, indent=2))
        return 0
    except (OSError, GraphicsError, XisoError, ValueError) as exc:
        p.exit(2, 'Error: %s\n' % exc)


if __name__ == '__main__':
    sys.exit(main())
