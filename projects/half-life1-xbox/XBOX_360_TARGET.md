# Half-Life 1 — Xbox 360 Remaster (PRIMARY TARGET)

Confirmed direction: **Xbox 360, not Unreal Engine 5 or Xbox Series X|S**.
Retain the original Half-Life 1 campaign, gameplay, enemies, weapons,
story and locations, but upgrade the art toward the maximum realistic
quality that Xbox 360 can display.

## Xbox 360 hardware envelope

The 360 has a 3.2 GHz PowerPC triple-core Xenon CPU, ATI Xenos GPU,
512 MB of shared memory and 10 MB of GPU eDRAM. Modern UE5
Nanite/Lumen and real-time path tracing are NOT console targets.

PROVISIONAL GOALS (subject to actual console profiling):
- Output at 1280 x 720, 30 FPS target (~33.3 ms/frame).
- Mostly 512/1024px textures, larger only for focal objects;
  BC1/DXT1 for opaque color, BC3/DXT5 or supported native compressed
  formats for normals. Packed property maps are preferred.
- Artist-built normal/specular materials, baked environmental lighting,
  limited dynamic lights/shadows, clear volumetric-style effects.
- Carefully managed higher-detail models for scientists, HECU and Xen.
- Budget-constrained particles, collision, map transitions and streaming.

Important: 512 MB is shared with CPU/GPU/system, not 512 MB VRAM.
Texture and memory figures below are estimates, not console allocations.
Hardware: https://ukstories.microsoft.com/features/xbox-360-launched/

## Native Xbox 360 approach

The promising public homebrew framework as of October 2026 is
https://github.com/xenosworks/xsdk . Its toolchain supports PPC C/C++,
controllers, files, audio, Xenos rendering and ELF-to-XEX packaging for
homebrew on RGH/JTAG-modded consoles. This project is early-stage and
still requires platform engineering and real-hardware validation.

Another option is libxenon (bare-metal), but it is not a drop-in
replacement for xsdk:
https://github.com/Free60Project/libxenon

An unmodified retail Xbox 360 cannot simply load arbitrary unsigned
custom XEX titles. You need RGH/JTAG hardware or an authorized
development environment for this original console game build.

The Half-LifeX executable targets ORIGINAL Xbox hardware (XBE,
x86, original Xbox graphics interfaces), not Xbox 360 (XEX, PPC,
Xenos GPU). Renaming it will not work. The upstream Half-LifeX
Xash3D/game source can inform a **port**, but graphics, input,
audio, filesystem and architecture assumptions require substantial
new work. The Xbox 360 Half-Life 2 disc is not a substitute engine
or instant visual upgrade for Half-Life 1.

## Completed CPU-side data preparation

1. Verify owned Half-LifeX disc using tools/half_life1_xbox.py.
2. Parse BSP textures / prototype detail textures using
   tools/half_life1_graphics.py.
3. Export GoldSrc BSP30 static world geometry, UVs and material
   manifest using tools/half_life1_modern_scene.py.
4. Generate original procedural starter textures (not final AAA art)
   using tools/half_life1_pbr_starter.py.
5. NEW: pack exported OBJ into a custom, compact, big-endian
   H360 v1 **CPU staging format** using tools/half_life1_x360_pack.py.

Windows PowerShell commands from this repository root (Python 3.10+):

    py -3 -m pip install pillow numpy
    py -3 -m tools.half_life1_modern_scene "C:\path\to\Half-LifeX_Test_v2.iso" --map c0a0 -o "local\x360-remaster\c0a0"
    py -3 -m tools.half_life1_pbr_starter "local\x360-remaster\c0a0" --resolution 512
    py -3 -m tools.half_life1_x360_pack "local\x360-remaster\c0a0"
    py -3 -m unittest tests.test_half_life1_x360_pack

All extracted game content stays in the ignored local/ directory.
Do NOT commit original meshes, PAKs, retail textures, XBE, ISO or SDK
binaries to GitHub.

## Observed c0a0 prototype

- 7,833 triangles, 46 material groups, 10,378 unique packed vertices.
- 23,499 triangle indices; packed static mesh 298,310 bytes.
- Five procedural starter material presets in use at this map.
- Provisional 512px BC1 color + BC3 normal + BC3 packed property
  texture memory estimate for five sets (with mips): ~4.37 MB.
- 12 additional non-world BSP brush models remain excluded, as
  they may be game-controlled doors, lifts or platforms.
- This is **NOT** a compiled XEX, ready-to-use Xenos GPU buffer,
  playable game, or completed remaster. No hardware rendering yet.

## H360 v1 format

All numeric fields are big-endian for Xbox 360 PPC CPU staging.
A native renderer must still arrange Xenos GPU buffer uploads,
supported vertex formats, shader inputs, textures and any needed
endianness/tiling conversions.

Header, 32 bytes, struct format >4sHHIIIIII:
  "H360", version 1, BOM 0xFEFF,
  group_count, vertex_count, index_count,
  directory_offset, vertex_offset, index_offset.

Group, 48 bytes, struct format >32sIIII:
  unique material key, first_vertex, vertex_count,
  first_index, index_count.

Vertex, 24 bytes, struct format >5f4b:
  float32 XYZ (Y-up, meters), float32 UV,
  signed normalized XYZ normal bytes, signed padding byte.

Index: big-endian uint16, relative to its material group's first_vertex.
Vertex/index array offsets are aligned to 16 bytes. The reader MUST
validate every offset, index and count prior to GPU upload.

Packer's conservative per-map prototype limits:
  65,535 vertices per material group;
  120,000 triangles;
  32 MiB maximum packed mesh bytes.
These are validation limits, NOT proven frame-rate limits.

## Necessary next milestones

1. Verify target RGH/JTAG Xbox 360 or authorized console dev access.
2. Bring up an xsdk sample XEX on actual hardware.
3. Implement a 360 mesh viewer, H360 reader and Xenos shader path.
4. Add properly compressed/tiled textures, baked lighting, visibility,
   culling and draw batching; benchmark console memory and FPS.
5. Port compatible Half-LifeX/Xash3D gameplay code and recreate
   moving geometry, triggers, scripted events, AI, physics, HUD,
   audio, saves and level transitions.
6. Create genuinely modern remaster art and finish the campaign.

No finished Xbox 360 game is implied by any of the above steps.
