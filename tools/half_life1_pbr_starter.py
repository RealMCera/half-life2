"""Build an original, procedural PBR starter set for a Half-Life 1 OBJ export.

Not the game's original art, photogrammetry, or a final visual remaster.
Requires Pillow and NumPy: py -3 -m pip install pillow numpy

    py -3 -m tools.half_life1_pbr_starter local/black-mesa

Reads material_manifest.json; outputs albedo, normal, roughness and metallic
textures plus an updated OBJ .mtl to preview diffuse maps in most 3D editors.
"""

import argparse
import json
from pathlib import Path
import re
import sys

try:
    import numpy as np
    from PIL import Image
except ImportError as exc:
    raise SystemExit('Install materials dependencies: py -3 -m pip install pillow numpy') from exc

PRESETS = {
    'concrete': {'base': (133, 139, 141), 'roughness': .87, 'metallic': 0},
    'brushed_metal': {'base': (102, 112, 121), 'roughness': .32, 'metallic': 1},
    'painted_panel': {'base': (96, 122, 116), 'roughness': .46, 'metallic': .05},
    'ceramic': {'base': (177, 186, 179), 'roughness': .31, 'metallic': 0},
    'rubber': {'base': (42, 44, 49), 'roughness': .9, 'metallic': 0},
    'neutral': {'base': (122, 116, 108), 'roughness': .65, 'metallic': 0},
}


def choose_preset(texture_name):
    n = texture_name.lower()
    if any(s in n for s in ('metal', 'mtl', 'steel', 'iron', 'grate', 'vent')):
        return 'brushed_metal'
    if any(s in n for s in ('concr', 'crete', 'cement', 'brick', 'stone')):
        return 'concrete'
    if any(s in n for s in ('tile', 'til_', 'bath', 'clean')):
        return 'ceramic'
    if any(s in n for s in ('wall', 'panel', 'pnl', 'door', 'hatch')):
        return 'painted_panel'
    if any(s in n for s in ('rubber', 'dark', 'black', 'hose')):
        return 'rubber'
    return 'neutral'


def _soft_noise(rng, n, coarse):
    grid = np.uint8(rng.uniform(0, 255, size=(coarse, coarse)))
    im = Image.fromarray(grid, mode='L').resize((n, n), Image.Resampling.BICUBIC)
    return (np.asarray(im, dtype=np.float32) - 127.5) / 127.5


def generate_preset(name, n=512):
    if name not in PRESETS or not (64 <= n <= 2048 and n % 2 == 0):
        raise ValueError('Unsupported preset or size')
    preset = PRESETS[name]
    rng = np.random.default_rng(700 + list(PRESETS).index(name) * 11)
    y, x = np.mgrid[:n, :n]
    noise = (_soft_noise(rng, n, 9) * 0.48 +
             _soft_noise(rng, n, 37) * 0.30 +
             rng.uniform(-1, 1, (n, n)).astype(np.float32) * 0.22)
    height = noise.copy()
    mask = np.ones((n, n), dtype=np.float32)
    if name == 'brushed_metal':
        line = np.sin(y * 2 * np.pi * 43 / n) * 0.28
        height = height * 0.32 + line
    elif name == 'ceramic':
        tile = max(n // 4, 1)
        seam = ((x % tile < 3) | (y % tile < 3))
        height = height * 0.3 - seam.astype(np.float32) * 0.65
        mask = 1 - seam.astype(np.float32) * 0.55
    elif name == 'painted_panel':
        seam = ((x % max(n // 2, 1) < 2) | (y % max(n // 2, 1) < 2))
        height = height * 0.4 - seam.astype(np.float32) * 0.4
        mask = 1 - seam.astype(np.float32) * 0.28
    elif name == 'rubber':
        height = height * .52 + 0.14 * np.sin(x * 2 * np.pi * 23 / n)

    base = np.array(preset['base'], dtype=np.float32)[None, None, :]
    albedo = np.clip(base * mask[...,None] + 30 * height[...,None], 0, 255).astype(np.uint8)
    strength = 3.3 if name in ('concrete', 'neutral') else 2.0
    dx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * strength
    dy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * strength
    normal = np.stack((-dx, -dy, np.ones_like(dx) * 2), axis=-1)
    normal /= np.maximum(np.linalg.norm(normal, axis=2, keepdims=True), 1e-8)
    normal = np.clip(np.round((normal * .5 + .5) * 255), 0, 255).astype(np.uint8)
    rough = np.clip((preset['roughness'] + noise*.08) * 255, 0, 255).astype(np.uint8)
    metal = np.full((n, n), int(round(preset['metallic'] * 255)), dtype=np.uint8)
    return {'basecolor': albedo, 'normal': normal, 'roughness': rough, 'metallic': metal}


def build_pack(scene_dir, *, resolution=512):
    scene_dir = Path(scene_dir)
    manifest_file = scene_dir / 'material_manifest.json'
    manifest = json.loads(manifest_file.read_text(encoding='utf-8'))
    if 'materials' not in manifest or not isinstance(manifest['materials'], list):
        raise ValueError('Invalid scene material manifest')
    dest = scene_dir / 'materials'
    dest.mkdir(parents=True, exist_ok=True)
    for preset in PRESETS:
        maps = generate_preset(preset, resolution)
        for key, pixels in maps.items():
            Image.fromarray(pixels).save(dest / (preset + '_' + key + '.png'))
    for item in manifest['materials']:
        preset = choose_preset(item['source_texture'])
        item['preset'] = preset
        item['pbr'] = {key:'materials/%s_%s.png' % (preset, key)
                       for key in ('basecolor','normal','roughness','metallic')}
    obj = next(iter(sorted(scene_dir.glob('hl1_*_world.obj'))), None)
    if obj:
        mtl = obj.with_suffix('.mtl')
        with mtl.open('w', encoding='utf-8') as f:
            f.write('# Starter procedural diffuse preview; PBR maps listed in material_manifest.json\n')
            for item in manifest['materials']:
                f.write('newmtl %s\nKd 1 1 1\nmap_Kd materials/%s_basecolor.png\n\n' %
                        (item['material_id'], item['preset']))
    manifest['starter_pbr_resolution'] = resolution
    manifest['starter_pbr_caveat'] = 'Original placeholder materials. Replace with artist-authored assets; game lighting and gameplay are not implemented.'
    manifest_file.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    return len(manifest['materials'])


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('scene_dir', help='Output directory from half_life1_modern_scene')
    p.add_argument('--resolution', type=int, default=512)
    args = p.parse_args(argv)
    try:
        count=build_pack(args.scene_dir, resolution=args.resolution)
    except (ValueError, KeyError, OSError) as exc:
        p.exit(2,'Error: %s\n' % exc)
    print('Generated PBR starter pack for %d materials.' % count)


if __name__=='__main__':
    sys.exit(main())
