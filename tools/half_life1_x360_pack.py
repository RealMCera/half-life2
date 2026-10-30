"""Package a Half-Life 1 OBJ scene into a compact Xbox 360 CPU-staging mesh.

Input: tools.half_life1_modern_scene.py's OBJ + material_manifest.json.
Output: big-endian H360 v1 file + JSON report. No game binaries included.

    py -3 -m tools.half_life1_x360_pack local/hl1-modern/c0a0

H360 is a custom asset interchange format, NOT XEX, GPU-native tiled
memory, or a finished engine. A 360 renderer must still be implemented.
"""
import argparse
from collections import OrderedDict
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

MAGIC = b'H360'
VERSION = 1
HEADER = struct.Struct('>4sHHIIIIII')      # 32 bytes, big-endian
GROUP = struct.Struct('>32sIIII')          # 48 bytes, big-endian
VERTEX = struct.Struct('>5f4b')            # 24 bytes: pos,uv,normal snorm8, pad
INDEX = struct.Struct('>H')                # group-relative uint16
MAX_GROUP_VERTICES = 65535
MAX_MAP_TRIANGLES = 120000
MAX_MAP_MESH_BYTES = 32 * 1024 * 1024


class MeshError(ValueError):
    pass


def _aligned(value, multiple=16):
    return (value + multiple - 1) // multiple * multiple


def _normal(a, b, c):
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - a[i] for i in range(3)]
    n = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
    length = math.sqrt(sum(t*t for t in n))
    if length < 1e-10:
        return (0, 127, 0)
    return tuple(max(-127, min(127, round(t / length * 127))) for t in n)


def _parse_obj(path):
    """Read the strict triangulated v/vt subset emitted by our map exporter."""
    points = []
    uvs = []
    groups = OrderedDict()
    active = None
    with Path(path).open('r', encoding='utf-8') as stream:
        for lineno, raw in enumerate(stream, 1):
            row = raw.strip().split()
            if not row or row[0].startswith('#'):
                continue
            cmd = row[0]
            if cmd == 'v':
                if len(row) != 4:
                    raise MeshError('Bad vertex on line %d' % lineno)
                p = tuple(float(x) for x in row[1:])
                if not all(math.isfinite(x) for x in p):
                    raise MeshError('Non-finite vertex')
                points.append(p)
            elif cmd == 'vt':
                if len(row) < 3:
                    raise MeshError('Bad UV on line %d' % lineno)
                t = tuple(float(x) for x in row[1:3])
                if not all(math.isfinite(x) for x in t):
                    raise MeshError('Non-finite UV')
                uvs.append(t)
            elif cmd == 'usemtl':
                if len(row) != 2:
                    raise MeshError('Bad material declaration')
                active = row[1]
                groups.setdefault(active, [])
            elif cmd == 'f':
                if active is None or len(row) != 4:
                    raise MeshError('Faces require material and triangulation (line %d)' % lineno)
                tri = []
                for token in row[1:]:
                    refs = token.split('/')
                    if len(refs) < 2 or not refs[0] or not refs[1]:
                        raise MeshError('Expected v/vt face reference')
                    pi, ti = int(refs[0]), int(refs[1])
                    if pi < 1 or pi > len(points) or ti < 1 or ti > len(uvs):
                        raise MeshError('Face reference out of bounds')
                    tri.append((points[pi-1], uvs[ti-1]))
                groups[active].append(tri)
            elif cmd in ('g', 'o', 's', 'mtllib', 'vn'):
                continue
            else:
                raise MeshError('Unsupported OBJ directive %s on line %d' % (cmd, lineno))
    if not any(groups.values()):
        raise MeshError('OBJ has no triangles')
    return groups


def _key(name):
    # Unique, deterministic material identifier in the 32-byte file field.
    digest = hashlib.sha1(name.encode('utf-8')).hexdigest()[:8]
    return name.encode('ascii', 'replace').decode('ascii')[:22] + '_' + digest


def _encode(groups):
    table = []
    vertices = bytearray()
    indices = bytearray()
    vertex_total = 0
    index_total = 0
    triangle_total = 0
    for material, tris in groups.items():
        if not tris:
            continue
        if triangle_total + len(tris) > MAX_MAP_TRIANGLES:
            raise MeshError('Map triangle budget exceeded')
        base_vertex = vertex_total
        start_index = index_total
        local = OrderedDict()
        local_index = []
        for tri in tris:
            normal = _normal(*(corner[0] for corner in tri))
            for position, uv in tri:
                # The export uses six decimal places; stable quantized keys
                # ensure adjacent faces deduplicate without baking in normals.
                signature = tuple(round(c, 6) for c in (*position, *uv)) + normal
                if signature not in local:
                    if len(local) >= MAX_GROUP_VERTICES:
                        raise MeshError('Material %s exceeds uint16 vertex limit' % material)
                    local[signature] = len(local)
                local_index.append(local[signature])
        for values in local:
            x, y, z, u, v, nx, ny, nz = values
            vertices.extend(VERTEX.pack(x, y, z, u, v, nx, ny, nz, 0))
        for idx in local_index:
            indices.extend(INDEX.pack(idx))
        table.append({
            'material': material,
            'material_key': _key(material),
            'first_vertex': base_vertex,
            'vertex_count': len(local),
            'first_index': start_index,
            'index_count': len(local_index),
            'triangles': len(tris),
        })
        vertex_total += len(local)
        index_total += len(local_index)
        triangle_total += len(tris)
    directory_offset = HEADER.size
    vertex_offset = _aligned(directory_offset + len(table) * GROUP.size)
    index_offset = _aligned(vertex_offset + len(vertices))
    final_size = index_offset + len(indices)
    if final_size > MAX_MAP_MESH_BYTES:
        raise MeshError('Map mesh size budget exceeded')
    buf = bytearray(final_size)
    HEADER.pack_into(buf, 0, MAGIC, VERSION, 0xFEFF, len(table),
                     vertex_total, index_total, directory_offset,
                     vertex_offset, index_offset)
    for i, group in enumerate(table):
        GROUP.pack_into(buf, directory_offset + i * GROUP.size,
                        group['material_key'].encode('ascii'),
                        group['first_vertex'], group['vertex_count'],
                        group['first_index'], group['index_count'])
    buf[vertex_offset:vertex_offset + len(vertices)] = vertices
    buf[index_offset:index_offset + len(indices)] = indices
    return bytes(buf), table, triangle_total


def parse_header(payload):
    if len(payload) < HEADER.size:
        raise MeshError('Truncated H360 header')
    magic, version, byte_order, groups, vertices, indices, directory, voff, ioff = HEADER.unpack_from(payload)
    if magic != MAGIC or version != VERSION or byte_order != 0xFEFF:
        raise MeshError('Invalid H360 header')
    if directory < HEADER.size or directory + groups * GROUP.size > voff:
        raise MeshError('Invalid H360 group directory')
    if voff + vertices * VERTEX.size > ioff or ioff + indices * INDEX.size > len(payload):
        raise MeshError('Truncated H360 payload')
    for i in range(groups):
        name, vfirst, vcount, ifirst, icount = GROUP.unpack_from(payload, directory + i * GROUP.size)
        if vfirst + vcount > vertices or ifirst + icount > indices or vcount > MAX_GROUP_VERTICES or icount % 3:
            raise MeshError('Invalid H360 group bounds')
        for j in range(ifirst, ifirst + icount):
            if INDEX.unpack_from(payload, ioff + j * INDEX.size)[0] >= vcount:
                raise MeshError('Group index out of range')
    return {'groups': groups, 'vertices': vertices, 'indices': indices,
            'triangles': indices // 3, 'payload_bytes': len(payload),
            'vertex_stride_bytes': VERTEX.size}


def estimate_texture_memory(manifest):
    """Conservative BC1 color + BC3 normal + BC3 packed properties with mips.

    This is a *planning estimate*, not actual tiled-Xenos GPU allocation.
    Repeated starter presets are only counted once.
    """
    unique = set()
    size = int(manifest.get('starter_pbr_resolution', 512))
    if size < 64 or size > 4096:
        raise MeshError('Invalid material resolution')
    for item in manifest.get('materials', []):
        key = item.get('preset', item.get('material_id'))
        unique.add(key)
    # 4 bits color + 8 bits normal + 8 bits properties -> 20 bits per pixel.
    # Estimated complete mip chain is 4/3 of base-level allocation.
    return {'unique_material_sets': len(unique), 'texture_edge_pixels': size,
            'estimated_bc_texture_bytes': len(unique) * (size * size * 20 // 8) * 4 // 3,
            'assumption': 'BC1 base color + BC3 normal + BC3 packed properties, mip overhead ~33%; not actual Xbox 360 tiled texture size'}


def pack_scene(scene_dir):
    root = Path(scene_dir)
    objs = sorted(root.glob('hl1_*_world.obj'))
    if len(objs) != 1:
        raise MeshError('Expected exactly one hl1_*_world.obj in scene directory')
    manifest_file = root / 'material_manifest.json'
    manifest = json.loads(manifest_file.read_text(encoding='utf-8'))
    groups = _parse_obj(objs[0])
    declared = {m['material_id'] for m in manifest.get('materials', [])}
    unknown = set(groups) - declared
    if unknown:
        raise MeshError('OBJ materials missing from manifest: %s' % ', '.join(sorted(unknown)[:3]))
    payload, table, triangles = _encode(groups)
    stats = parse_header(payload)
    if triangles != int(manifest.get('exported_triangles', -1)):
        raise MeshError('Triangle count disagrees with material manifest')
    path = root / (objs[0].stem + '.h360')
    path.write_bytes(payload)
    report = {
        'output': path.name,
        'format': 'H360 v1 big-endian CPU staging mesh',
        'sha256': hashlib.sha256(payload).hexdigest(),
        **stats,
        'mesh_budget_mib': MAX_MAP_MESH_BYTES // (1024 * 1024),
        'texture_estimate': estimate_texture_memory(manifest),
        'material_groups': table,
        'warning': 'NOT a .xex, Xbox GPU tiled texture, compiled game or validated console renderer',
    }
    (root / 'x360_scene_report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('scene_dir', help='Directory containing exported OBJ and material_manifest.json')
    args = p.parse_args(argv)
    try:
        report = pack_scene(args.scene_dir)
        print('H360:', report['output'])
        print('Mesh: %d triangles, %d vertices, %d material groups, %d bytes' %
              (report['triangles'], report['vertices'], report['groups'], report['payload_bytes']))
        print('Status: CPU asset prototype; no Xbox 360 executable produced')
    except (MeshError, OSError, KeyError, ValueError, struct.error) as exc:
        p.exit(2, 'Error: %s\n' % exc)


if __name__ == '__main__':
    sys.exit(main())
