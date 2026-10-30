# Next step after confirmed 31,964-triangle renderer: BMS WALK integration (2026-10-30)

The user asked **"OKAY SO WHAT NOW?"** after their actual BMS C0A0
`hl1_bms_world_probe.xex` log showed 578 frames, 577 flips, no
reported misses and clean exit.

**New code committed (not yet emulator-verified):**

- New **separate** `x360_probe/bms_world_walk.c` target
  `hl1_bms_world_walk.xex`: combines the BMS large-map 65,532-index
  multi-draw geometry renderer with the previously developed
  `world_walk_stable.c` ground-support approximation,
  gravity, jump and wall-segment collision.
- Mode **Start: WALK/ORBIT**, D-pad move, LB/RB yaw, right analog
  view, A/Y jump, B reset, Back exit.
- Optional private `assets/bms_spawn.txt` holding a Source-BSP
  `info_player_start` origin as three decimal X Y Z numbers
  on one line (e.g. generated from private C0A0
  `map_manifest.json`). The executable transforms Source
  coordinates into the same normalized H360 Y-up space and
  tries to place the player on a supporting static floor.
  If the floor is missing because H360 excludes
  brush-entity models/props/displacements, the program
  logs `SOURCE_SPAWN_UNSUPPORTED` and falls back to its
  best-scored floor centroid.
- Logs `HL1_BMS_WALK: DRAW_BATCHES`,
  `COLLISION_MESH`, `SPAWN_SUPPORT`,
  `SOURCE_SPAWN_ACCEPTED`/fallback,
  `PLAYER_SPAWN`, `MODE=WALK`,
  `PLAYER`, `JUMP`, `LANDED`,
  `FALL_RECOVERY`, and
  `PHYSICS blocked_edges=... blocked_walls=...
  jumps=... landings=... recoveries=...`.
- The **existing** `hl1_bms_world_probe.xex` and original
  `hl1_world_explore.xex`, `hl1_world_walk.xex`
  remain unchanged. This is an additional experiment.
- The first BMS walker is **not yet a playable game**.
  It lacks authentic Source/GoldSrc BSP hull collision,
  solid entities, player capsule, doors, NPCs, physics
  and original graphics. We must first verify that it
  compiles and stays on a floor when tested in Xenia.

Cloud build: [GitHub Actions #58](https://github.com/RealMCera/half-life2/actions/runs/41064921641),
`HalfLife1-X360-XEX-StartupProbe` artifact, download
**only after successful compilation**. The new XEX
must be tested inside desktop Windows Xenia Canary.

Separately delivered privately in-chat:
`BlackMesa_C0A0_Walking_Test_Private.zip`, which contains
`BMS_C0A0_Walking_Test/assets/hl1_c0a0_world.h360`,
optional `assets/bms_spawn.txt`, and README.
No proprietary map or Source material is committed to public GitHub.

The test directory after copying the **new built XEX**:

```text
BMS_C0A0_Walking_Test/
  hl1_bms_world_walk.xex
  assets/
    hl1_c0a0_world.h360
    bms_spawn.txt
```

After launching, read:

```powershell
Get-Content "C:\Users\FrontDesk\Downloads\xenia_canary_windows\cache\hl1_bms_world_walk.log" -Tail 100
```

**Critical technical caveat:** A static world rendering mesh
is not a complete Source BSP collision world. In particular,
the two Source player spawn origins in the C0A0 manifest may
fall on missing dynamic brush-model surfaces, so it is
expected that an authentic Source spawn can be rejected
despite correct coordinates. Do not mark it as a bug
in the coordinate parser without checking materialized
world support geometry.

As before, all XEXs rely on the temporary **Xenia-only**
LUT timeout bypass; never run on physical Xbox 360 hardware.

---

# User verified full BMS C0A0 graphics submission / presentation stats (2026-10-30)

**This replaces earlier uncertainty about whether the XEX ran.**
The user supplied the full Windows Xenia Canary title log for
`hl1_bms_world_probe.xex`:

```text
HL1_BMS: BOOT_BEGIN
HL1_BMS: MULTIDRAW_LARGE_MAP_PREVIEW 65532_INDEX_LIMIT
HL1_BMS: SCENE_VALID: 303 groups 48874 vertices 31964 triangles
HL1_BMS: gpu_open = 0x00000000
HL1_BMS: shaders_load = 0x00000000
HL1_BMS: INDEXED_MESH_READY vertices=48874 indices=95892 triangles=31964 vertex_bytes=1172976 index_bytes=191784
HL1_BMS: INDEX_DRAW_BATCHES=2 total_indices=95892 max_per_batch=65532
HL1_BMS: present_open = 0x00000000 (1280x720)
HL1_BMS: GPU_MAP_FRAME_SUBMITTED frame=1 triangles=31964
HL1_BMS: GPU_MAP_FRAME_SUBMITTED frame=60 triangles=31964
HL1_BMS: GPU_MAP_FRAME_SUBMITTED frame=180 triangles=31964
HL1_INPUT: RAW slot=0 frame=578 buttons=0x0020 LX=0 LY=0 RX=0 RY=0
HL1_BMS: FRAMES=578 VBLANKS=579 FLIPS=577 MISSED=0
HL1_INPUT: SUMMARY success_frames=579 active_frames=119
HL1_INPUT: SUMMARY A_OR_START_SWITCH=CURRENTLY_ORBIT
HL1_BMS: gpu_close = 0x00000000
HL1_BMS: BOOT_COMPLETE
exit: status 0
```

**Verified:** the XEX successfully runs and loads the private
converted Alpha 5 Source map; GPU opens, shaders load,
vertices/indices upload, two indexed draw batches submit,
present succeeds, 577 flips in 578 submitted frames, zero
SDK-reported missed flips, and normal shutdown after
virtual Back button at frame 578. Runtime retained **orbit**
camera mode; no Start/free-fly toggle observed in this test.

**Do not interpret** SDK `MISSED=0` as independently
measured FPS, nor the GPU submission logs as confirmation of
physically accurate textures/geometry/lighting, gameplay, or
reliable collision. The prior screenshot displays colored
geometry but the screenshot did not include the XEX filename.

Mesh GPU buffer storage: 1,172,976 + 191,784 =
**1,364,760 bytes**, exclusive of CPU file storage, other
allocations, shaders and framebuffers. User-private original
HL1 H360 mesh remains separately intact.

Next milestone should be a **playable map slice**: import
actual player-start entity, implement stable collision and
navigation on the Alpha 5 map, then a door/button trigger
and one test objective. The Source BSP editor/VMF has
entity/brush data that a plain H360 render mesh does not.
Textures/models are valuable but are *not* prerequisites for
the initial gameplay loop. Avoid additional unrelated
display diagnostics unless a new graphics bug emerges.

---

# VERIFIED IN USER'S XENIA: Black Mesa Alpha 5 C0A0 GPU loader and split draws (2026-10-30)

**User-provided Windows Xenia Canary title-log evidence** after testing the
separate `hl1_bms_world_probe.xex`:

```text
HL1_BMS: SCENE_VALID: 303 groups 48874 vertices 31964 triangles
HL1_BMS: INDEX_DRAW_BATCHES=2 total_indices=95892 max_per_batch=65532
HL1_BMS: GPU_MAP_FRAME_SUBMITTED frame=1 triangles=31964
HL1_BMS: GPU_MAP_FRAME_SUBMITTED frame=60 triangles=31964
HL1_BMS: GPU_MAP_FRAME_SUBMITTED frame=180 triangles=31964
HL1_BMS: BOOT_COMPLETE
```

The user also confirmed a new title log was written on
**2026-10-30 at 8:27:28 AM**, indicating that the new
`hl1_bms_world_probe.xex` actually launched in Windows Xenia.
The earlier screenshot in conversation showed colored geometric
world structures, but the screenshot itself did not show a readable
XEX filename; **don't assign that screenshot to this binary without
a separate visual confirmation**.

User's Windows folder file sizes support the intended isolation:
```text
BlackMesa_Alpha5_Private_Preview/C0A0/assets/hl1_c0a0_world.h360   1,379,336 bytes
BlackMesa_Alpha5_Private_Preview/C1A0/assets/hl1_c0a0_world.h360      67,868 bytes
HalfLife1-X360-XEX-StartupProbe/assets/hl1_c0a0_world.h360           298,310 bytes
```

**What is now verified**: H360 big-endian parser accepts the converted
BMS map, both GPU index batches are configured, draw submissions
complete through at least frame 180, and XEX reaches BOOT_COMPLETE
without the previous no-log failure.

**What is not verified yet**: the exact new map's appearance,
complete vertex colors and geometry validity on screen,
FPS/performance metrics, runtime textured Source surfaces,
props, doors, collision/physics and playable Black Mesa gameplay.
The present log is filtered to selected patterns, so do not
invent final present statistics.

**Next test**: inspect the complete log tail, screenshot the
new BMS executable, and observe camera orbit/free-fly.
Then build a *separate* BMS texture / source entity prototype
without touching the original HL1 rendering or stable-walk XEXs.
Continue keeping game-derived assets private, not in public GitHub.

---

# Fix for BMS preview not opening — oversized indexed draw (2026-10-29)

User reported that the previously supplied private Black Mesa Alpha 5
H360 preview **"IT DIDNT OPEN"**. No crash/title-log was supplied, so
the exact failure point is **not confirmed**.

**Static audit of the user's private preview archive:**

- ZIP contains ONLY `C0A0/assets/hl1_c0a0_world.h360`,
  `C1A0/assets/hl1_c0a0_world.h360`, manifests,
  `source_bsp_to_h360.py` and instructions. **There is NO XEX in
  the preview ZIP.** Users must copy an executable from a successful
  Actions XEX build into that folder.
- Both maps pass the H360 v1 structure/endianness, group-relative
  16-bit indices, 32 MiB file cap, group count and vertex count limits
  in `world_explore.c`. C0A0 is 303 groups, 48,874 vertices,
  95,892 indices / 31,964 triangles, 1,379,336-byte H360.
  C1A0 is 28 groups, 2,353 vertices, 5,010 indices /
  1,670 triangles, 67,868-byte H360.
- `xenosworks/xsdk/include/xsdk/gpu.h` explicitly documents
  `xsdk_gpu_draw_indexed` as **maximum 65,535 indices per call**.
  Our original `hl1_world_explore.xex` submitted *all 95,892*
  C0A0 indices in one call, **exceeding the SDK limit**. This is a
  real bug that may cause a blank screen, crash or no draw, but without
  the user's title log it is NOT proven as the sole cause of failure.
- The smaller C1A0 map stays below 65,535 and is a useful
  control test using the older known-good `hl1_world_explore.xex`
  to distinguish wrong folder/XEX path from oversized GPU draw.

## New dedicated experimental XEX

New independent `projects/half-life1-xbox/x360_probe/bms_world_probe.c`
is built as `hl1_bms_world_probe.xex`. It preserves the
original GoldSrc `hl1_world_explore.xex` and other known-good files.

- `xsdk_gpu_draw_indexed` is called with **up to 65,532 indices**
  per batch, always divisible by 6 (whole triangles and dword-aligned
  offset in uint16 index memory).
- C0A0's 95,892 indices are now split into TWO draws:
  **65,532 + 30,360**. Small C1A0 uses one draw.
- Logs `HL1_BMS: SCENE_VALID`,
  `HL1_BMS: INDEX_DRAW_BATCHES=2 total_indices=95892`,
  `GPU_MAP_FRAME_SUBMITTED`, and `BOOT_COMPLETE`.
- Uses the existing shader and vertex layout, index-buffer
  conversion, and orbit/free-flight input code. Limits runtime to
  600 frames for large-map diagnostics and allows early Back exit.
- It is not yet known whether GPU memory allocations, vertex cache
  capacity, Source BSP geometry, shader and performance will be
  correct in Xenia; **cloud compilation and user-visible testing
  are distinct gates**.
- [GitHub Actions run #55](https://github.com/RealMCera/half-life2/actions/runs/40962160451)
  builds and packages it, with the artifact
  `HalfLife1-X360-XEX-StartupProbe`. Download ONLY after Success.

### Safe Windows test — C0A0

Create a NEW folder and copy into it:
```text
C0A0/
  hl1_bms_world_probe.xex
  assets/
    hl1_c0a0_world.h360
```
Run the new `hl1_bms_world_probe.xex` with desktop Xenia Canary.
Read:
```powershell
Get-Content "C:\Users\FrontDesk\Downloads\xenia_canary_windows\cache\hl1_bms_world_probe.log" -Tail 60
```

If it still does not launch, inspect Xenia's `xenia.log` and
confirm exact file layout, XEX file size and log last-write-time.
Do NOT assume error cause from a previous/stale log.
The original GoldSrc demo should not be overwritten by the
BMS C0A0 asset.

The temporary SDK LUT-timeout bypass remains Xenia-only.
**Never run these binaries on real Xbox 360.** No private game
assets are committed to GitHub.

---

# Black Mesa Alpha 5 private archive — verified audit and safe import route

Updated October 29, 2026. The user supplied a privately held
`BMS.rar` file. **Do not commit the RAR, Valve/Crowbar Collective
assets, proprietary DLLs, extracted maps, textures, sounds, models,
derived H360 meshes, or a full entity dump to this public repo.**

## Archive identification

`BMS/GameInfo.txt` says:

- `game "Black Mesa"`
- `title2 "Alpha 5"`
- `developer "Crowbar Collective"`
- `SteamAppId 420` (Half-Life 2: Episode Two)
- Source Engine mount dependencies on HL2 and Episode One/Two VPKs.

This is an **early Black Mesa Alpha 5 Source mod directory**, not a
complete standalone retail Black Mesa game and not GoldSrc HL1.
Its Windows `bin/client.dll` and `bin/server.dll` are platform
binaries, **not native Xbox 360 PowerPC game libraries**.

## Validated format inventory

A header-only audit of the RAR5 archive found 11,729 entries,
about 523 MB of compressed data, ~1.28 GB uncompressed. Counts:

| Type | Files | Role |
|---|---:|---|
| `.bsp` | 19 | Compiled Source v20 levels |
| `.vmf` | 11 | Editable Source Hammer maps |
| `.vtf` | 3,027 | Valve texture image files |
| `.vmt` | 2,738 | Source material definitions |
| `.mdl` | 725 | Model headers / model assets |
| `.vtx` | 2,174 | Source model triangle strip data |
| `.vvd` | 724 | Source model vertices |
| `.wav` | 967 | Loose sound effects and voices |
| `.mp3` | 3 | Additional audio |
| `.dll` | 2 | Windows client/server binary modules |
| `.fgd` | 1 | Hammer entity definitions |
| `.ain` | 14 | Precomputed Source AI node graphs |

Archive contents include `maps/c0a0_rm8.bsp`,
`maps/bms_c1a0.bsp`, `mapsrc/bm_c0a0_8.vmf`,
and `mapsrc/bms_test_map.vmf`.

**Source-native entity data is highly useful for future systems.**

- The `c0a0_rm8.bsp` file has `VBSP` v20 header and a
  159,501-byte entity lump; among its entity class counts:
  2 `info_player_start`, 40 `path_track`,
  1 `func_tracktrain`, 2 `trigger_once`,
  1 `logic_auto`, 6 `npc_kleiner`,
  1 `npc_barney`, props and many lights.
- `bm_c0a0_8.vmf` includes **4,370 `solid`**
  declarations and **27,018 `side`** declarations
  in the editable source, plus entity classes.
- `bms_c1a0.bsp` contains a `func_door`,
  `func_button`, ladder entities and a player start.
- The explicit Source map entity origin and targetname
  conventions are **not directly interchangeable** with the
  separate original GoldSrc `c0a0` mesh/engine.

## Tested local Source BSP v20 -> H360 v1 prototype

A private local converter was successfully tested against the
user's own extracted map files and its H360 outputs were verified:

| BMS source file | H360 groups | Vertices | Triangles |
|---|---:|---:|---:|
| `maps/c0a0_rm8.bsp` | 303 | 48,874 | 31,964 |
| `maps/bms_c1a0.bsp` | 28 | 2,353 | 1,670 |

- Extracted **world model 0 static brush faces**, triangulated
  convex polygons, skipped nodraw/sky and degenerate faces.
- Converted Source BSP texture vectors into H360 vertex UVs,
  stored vertex normals, and grouped by short stable hashed
  material names; separately wrote a local JSON dictionary
  mapping names to original Source materials.
- Confirmed output satisfies the existing H360 header,
  group-local index and global indexed-XEX 16-bit
  vertex limits. **This is file-format verification,
  NOT an Xenia runtime or on-screen visual success.**
- The converter and private generated map files were supplied
  separately in the conversation as a **private download**,
  not committed to GitHub.

Important exclusions: 3D static/dynamic props, moving
brush-model faces, Source displacement meshes, Source physics
hulls, collision brushes, entities (as working behavior),
lights (as runtime lights), animated shaders, textures and sounds.

A naive exact-name search found only **24 out of 303**
converted C0A0 material-name matches in the archive's
loose `.vmt` files; the rest include generated map texture
names or materials in **external HL2/EP2 VPK dependencies**.
Texture completeness cannot be asserted just from this RAR.
Do not expect automatically textured geometry.

## How this can actually accelerate the game

1. **Original HL1 path:** retain the stable GoldSrc
   `c0a0` player prototype and investigate its authentic
   BSP collision hulls and gameplay entities. BMS does not
   silently replace HL1 entities with compatible ones.
2. **Separate remaster path:** user-private BMS Alpha 5
   `c0a0_rm8` H360 can be previewed in
   `hl1_world_explore.xex` by placing it under the
   expected generic `assets/hl1_c0a0_world.h360`
   in a *new, separate copy of the XEX folder*.
   Never overwrite the known-good original HL1 mesh.
3. **Entity bridge:** use Source BSP entity classes, origins,
   targetnames and I/O outputs for later spawn points,
   door/button/trigger/train scripting. Implement a
   Source-aware entity runtime or static manifest conversion.
4. **Materials:** build private VTF DXT1/DXT5 decoder to
   Xenos textures and map full Source material names to
   H360 groups. The 24 matched VMTs and earlier small HL2
   material pack are starting points; many dependencies are
   missing, so fallback colors are necessary.
5. **Models/AI/audio:** use assets only after engine-compatible
   model vertex/animation loaders, audio decoding and
   gameplay code exist; Windows DLLs cannot be used as
   Xbox 360 game code.
6. **Gameplay goal:** working grounded player, then
   entity-driven door/button and first objective, before
   adding full textures, NPCs or complete campaigns.

The safest strategy is **two separate maps/branches**:
continue the original GoldSrc HL1 prototype and establish
the Black Mesa Alpha 5 static remaster preview independently.
Do not merge their map coordinates or collision assumptions.

**Safety:** the existing GPU XEXs still rely on an emulator-only
LUT-timeout workaround. Run these experiments only in desktop
Xenia Canary, NEVER on physical Xbox 360. Series S/UWP
compatibility remains unverified.
