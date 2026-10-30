"""Export original Half-Life BSP30 world geometry as a modern-engine-friendly OBJ.

Reads an owned original Xbox Half-LifeX XISO with the existing XDVDFS and PACK
readers. Produces geometry, texture inventory, and a material manifest only.
No retail game data or maps belong in source control.

Usage from xboxrecomp checkout root:
  py -3 -m tools.half_life1_modern_scene Half-LifeX_Test_v2.iso --map c0a0 -o local/black-mesa

OBJ coordinates are Y-up meters (GoldSrc units converted at 0.0254 m/unit).
OBJ is a geometry reference, NOT a playable game or automatic Source conversion.
"""

import argparse
from collections import Counter, defaultdict
import json
import hashlib
import math
from pathlib import Path
import re
import struct
import sys

from tools.half_life1_graphics import GraphicsError, pak_index
from tools.xiso.xdvdfs import Xiso, XisoError

_LUMP_SIZE = 15
_FACE = struct.Struct('<Hhihh4si')  # 20 bytes
_TEXINFO = struct.Struct('<8fii')  # 40 bytes
_MODEL = struct.Struct('<9f7i')  # 64 bytes


def _lump(data, number, stride=1):
    if len(data) < 124 or struct.unpack_from('<i', data)[0] != 30:
        raise GraphicsError('Expected Half-Life / GoldSrc BSP version 30')
    offset, length = struct.unpack_from('<II', data, 4 + number * 8)
    if offset > len(data) or length > len(data) - offset or length % stride:
        raise GraphicsError('Bad BSP lump %d' % number)
    return memoryview(data)[offset:offset + length]


def _texnames(data):
    t = _lump(data, 2)
    if len(t) < 4:
        raise GraphicsError('Missing miptexture directory')
    count = struct.unpack_from('<i', t)[0]
    if count < 0 or count > 16000 or 4 + 4 * count > len(t):
        raise GraphicsError('Invalid miptexture directory')
    names = []
    for i in range(count):
        offset = struct.unpack_from('<i', t, 4 + i * 4)[0]
        if offset == -1:
            names.append(('missing_%04d' % i, 64, 64))
            continue
        if offset < 4 + 4 * count or offset + 40 > len(t):
            raise GraphicsError('Bad miptexture index %d' % i)
        raw, width, height = struct.unpack_from('<16sII', t, offset)
        name = raw.split(b'\x00', 1)[0].decode('latin1')
        if not width or not height or width > 8192 or height > 8192:
            raise GraphicsError('Invalid miptexture resolution')
        names.append((name or ('unnamed_%04d' % i), width, height))
    return names


def _axis(vector, scale=1):
    # Original Z-up -> standard Y-up; proper right-handed rotation.
    x, y, z = vector
    return (x * scale, z * scale, -y * scale)


def _clean_name(name):
    safe = re.sub(r'[^A-Za-z0-9_.-]+', '_', name).strip('._') or 'unknown'
    return safe + '_' + hashlib.sha1(name.encode('latin1', 'replace')).hexdigest()[:6]


def bsp_to_obj(bsp, obj_path, *, unit_scale=0.0254):
    """Return a report and write worldspawn static mesh + MTL.

    Models 1..N are intentionally excluded: those brushes are gameplay entities
    (doors, trains, lifts etc.) and must be recreated with their entity logic.
    """
    if not (0.00001 < unit_scale < 10):
        raise GraphicsError('Invalid unit scale')
    vertices = _lump(bsp, 3, 12)
    texinfo = _lump(bsp, 6, 40)
    faces = _lump(bsp, 7, 20)
    edges = _lump(bsp, 12, 4)
    surfedges = _lump(bsp, 13, 4)
    models = _lump(bsp, 14, 64)
    names = _texnames(bsp)
    if len(models) < _MODEL.size:
        raise GraphicsError('Worldspawn model missing')
    world = _MODEL.unpack_from(models)
    first_face, face_count = world[-2:]
    if first_face < 0 or face_count < 0 or first_face + face_count > len(faces) // 20:
        raise GraphicsError('Worldspawn face range out of bounds')
    edge_count, vertex_count = len(edges) // 4, len(vertices) // 12
    surfedge_count = len(surfedges) // 4
    texture_count = len(texinfo) // 40
    triangles = 0
    skipped = 0
    used = Counter()
    # Keep vertices distinct per BSP corner: reliable UVs and hard normals.
    groups = defaultdict(list)
    for face_id in range(first_face, first_face + face_count):
        planenum, side, firstedge, n, tindex, styles, lightofs = _FACE.unpack_from(faces, face_id * 20)
        if n < 3 or firstedge < 0 or firstedge + n > surfedge_count or tindex < 0 or tindex >= texture_count:
            skipped += 1
            continue
        ti = _TEXINFO.unpack_from(texinfo, tindex * 40)
        mindex, texflags = ti[8], ti[9]
        if mindex < 0 or mindex >= len(names):
            skipped += 1
            continue
        name, width, height = names[mindex]
        # Clip/trigger/sky are not modeled as visible surfaces in this preview.
        if name.lower().startswith(('sky', 'clip', 'trigger', 'aaatrigger')):
            skipped += 1
            continue
        corners = []
        for edge_i in range(firstedge, firstedge + n):
            directed = struct.unpack_from('<i', surfedges, edge_i * 4)[0]
            if abs(directed) >= edge_count:
                corners = []
                break
            a, b = struct.unpack_from('<HH', edges, abs(directed) * 4)
            vertex_i = a if directed >= 0 else b
            if vertex_i >= vertex_count:
                corners = []
                break
            v = struct.unpack_from('<3f', vertices, vertex_i * 12)
            u = (v[0]*ti[0] + v[1]*ti[1] + v[2]*ti[2] + ti[3]) / width
            t = (v[0]*ti[4] + v[1]*ti[5] + v[2]*ti[6] + ti[7]) / height
            corners.append((_axis(v, unit_scale), (u, -t)))
        if len(corners) != n:
            skipped += 1
            continue
        for i in range(1, n-1):
            groups[name].append((corners[0], corners[i], corners[i+1]))
            triangles += 1
        used[name] += 1

    out = Path(obj_path)
    out.parent.mkdir(parents=True, exist_ok=True)
    mtl_path = out.with_suffix('.mtl')
    with out.open('w', encoding='utf-8') as f:
        f.write('# Half-Life 1 worldspawn static geometry. Other brush models and game logic NOT included.\n')
        f.write('mtllib %s\n' % mtl_path.name)
        next_index = 1
        for name, tris in sorted(groups.items()):
            mat = 'hl1_' + _clean_name(name)
            f.write('g %s\nusemtl %s\n' % (mat, mat))
            for tri in tris:
                for pos, uv in tri:
                    f.write('v %.6f %.6f %.6f\n' % pos)
                    f.write('vt %.6f %.6f\n' % uv)
                f.write('f %d/%d %d/%d %d/%d\n' % tuple(k for j in range(next_index,next_index+3) for k in (j,j)))
                next_index += 3
    # Neutral placeholder palette; *no* original game textures included.
    with mtl_path.open('w', encoding='utf-8') as f:
        f.write('# Placeholder materials for geometry inspection; see material_manifest.json\n')
        for name in sorted(groups):
            f.write('newmtl hl1_%s\nKa 0.12 0.12 0.12\nKd 0.55 0.57 0.60\nKs 0.10 0.10 0.10\nNs 20\n\n' % _clean_name(name))
    report = {
        'input_format': 'GoldSrc BSP30',
        'static_worldspawn_only': True,
        'xbox_brush_entities_excluded': max(0, len(models) // 64 - 1),
        'worldspawn_faces': face_count,
        'skipped_faces': skipped,
        'exported_triangles': triangles,
        'material_count': len(groups),
        'unit_scale': unit_scale,
        'up_axis': '+Y',
        'forward_axis': '-Z',
        'materials': [{'source_texture': name, 'material_id': 'hl1_' + _clean_name(name),
                       'faces': used[name], 'triangles': len(groups[name]),
                       'pbr': {'base_color': None, 'normal': None,
                               'roughness': None, 'metallic': None}}
                      for name in sorted(groups)],
        'warning': 'Geometry/material planning artifact only, not a playable remaster. No GoldSrc game logic, enemies, lightmaps, collision, or Source 2 features.',
    }
    out.with_name('material_manifest.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('iso', help='Legally owned Half-LifeX original Xbox disc')
    p.add_argument('--map', default='c0a0', help='BSP short name')
    p.add_argument('-o', '--output', required=True, help='Output directory, e.g. local/black-mesa')
    p.add_argument('--scale', type=float, default=0.0254, help='meters per BSP unit')
    a = p.parse_args(argv)
    if not re.fullmatch(r'[\w-]{1,40}', a.map, re.ASCII):
        p.error('Invalid map name')
    try:
        with Xiso(a.iso) as iso:
            entries = pak_index(iso)
            key = 'maps/%s.bsp' % a.map.lower()
            if key not in entries:
                raise GraphicsError('Map not found: ' + key)
            offset, size = entries[key]
            if size > 64 * 1024 * 1024:
                raise GraphicsError('Map unexpectedly large')
            iso._f.seek(offset)
            data = iso._f.read(size)
            if len(data) != size:
                raise GraphicsError('Truncated map')
        report = bsp_to_obj(data, Path(a.output) / ('hl1_%s_world.obj' % a.map.lower()), unit_scale=a.scale)
        print('Exported %s: %d triangles; %d materials; %d skipped faces' %
              (a.map, report['exported_triangles'], report['material_count'], report['skipped_faces']))
        print('Static geometry only. See material_manifest.json for the PBR remaster plan.')
    except (OSError, XisoError, GraphicsError, ValueError) as e:
        p.exit(2, 'Error: %s\n' % e)


if __name__ == '__main__':
    sys.exit(main())
