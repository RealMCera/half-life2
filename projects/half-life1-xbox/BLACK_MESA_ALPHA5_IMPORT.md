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
