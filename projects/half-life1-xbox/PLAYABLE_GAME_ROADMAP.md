# From live Black Mesa viewer to playable Half-Life: milestone gates

Updated 2026-10-29 after inspecting **user-provided private asset ZIP**
`HL1_Private_Asset_Candidates.zip`. This is a scope and integration plan,
NOT a claim that the Half-Life engine or game is already ported.

## What already works (verified by user's Xenia logs / screenshots)

- Real original HL1 `c0a0` geometry loads as H360 v1:
  **46 groups, 10,378 source vertices, 7,833 triangles**.
- Native Xbox 360 PowerPC XEX runs under Windows desktop Xenia Canary.
- Onscreen Xenos GPU RGB triangle and a live rotating, flat-colored
  Black Mesa world are **visually confirmed** by the user.
- The indexed GPU mesh `hl1_world_explore.xex` ran 1,800 frames:
  **10,378 GPU vertices, 23,499 global uint16 indices, 1,799 flips,
  165 missed** (not an FPS benchmark).
- The indexed mesh consumes **249,072 vertex bytes + 47,000 index
  bytes = 296,072 bytes**, down from 563,976 bytes of expanded
  unindexed geometry.
- `hl1_world_gpu_probe.xex` is the **known-good** orbit-view fallback.

## What is NOT yet working

- Controller-driven free flight has now been **verified by user's later
  Xenia logs**, using virtual Start to toggle modes and D-pad to move.
  Camera X/Z/Y and yaw logs change with real button input.
  Later analog right-stick samples also gave nonzero RX/RY values.
  **Visual first-person navigation and collision remain unverified**.
- No map collision or physics, interior navigation, player
  capsule, gravity, pickups, buttons, doors, triggers, AI,
  weapons, combat, sound, music, menu or save system.
- No actual GoldSrc `hl1` game engine is running; `c0a0` is a
  mesh export, not a playable BSP with active entities.
- No level loading/chapter transitions. No Blue Shift or Opposing
  Force gameplay.
- The temporary LUT-timeout diagnostic modification makes these
  XEXs **emulator-only**. Xbox Series S Xenia UWP and physical Xbox
  360 compatibility are unproven; **never run the LUT-patched builds
  on physical 360 hardware**.

## Exact inventory: user-provided private ZIP

**48 files in 2 episodic sets, 24 matched material pairs:**

| Source | VMT | VTF |
|---|---:|---:|
| HL2 Episode One | 10 | 10 |
| HL2 Episode Two | 14 | 14 |
| **Total** | **24** | **24** |

VTF headers analyzed without unpacking into the public repository:
- 19 are VTF format 13 (**DXT1**) and 5 are format 15 (**DXT5**).
- Texture dimensions span 64×64 through 512×512,
  mostly 512×512.
- Materials cover concrete floors/walls/ceilings, metal and
  bunker surfaces, plaster, tile, glass/pipes, lights,
  and a few laboratory model/monitor surfaces.
- Several `.vmt` files also reference **absent** bump maps,
  detail textures, env masks, or other Source material features.
- **No BSP/maps, model meshes, player/monster models, animations,
  sound files, entity scripts, engine code, or game executables**
  are present in this ZIP.

These are **HL2 Source Engine** materials, NOT native GoldSrc HL1
textures. They are useful as a visual style starter, not as a complete
HL1 replacement asset pack.

### Texture integration constraints

1. Build local-only conversion for VTF 7.x DXT1/DXT5, mip layouts
   and optional transparency into the target Xbox/Xenos-friendly
   texture upload layout. The XEX **cannot use VMT/VTF** just by
   dropping the ZIP alongside it.
2. Adapt GPU `hl1_world.vert` for H360 UV coordinates
   (source vertex layout includes position, UVs and normals),
   and pixel shader for sampler fetch.
3. Assign each of the H360 `c0a0` 46 material groups to an
   appropriate HL2 material or intentional fallback using a
   **separate mapping manifest**. Existing material IDs do NOT
   automatically match HL2 texture paths.
4. Begin with **opaque DXT1 concrete/tile/metal** for a simple
   first pass; defer DXT5 transparency, normal maps, animated
   monitor screens, cubemaps, fancy `VMT` proxies and detail
   textures to later passes.
5. All game-derived assets stay local/private. Do not push
   Valve-owned textures/meshes to the public GitHub repository
   or bundle them for redistribution without appropriate rights.

## Practical milestone sequence

| Gate | Minimum proof | Status |
|---|---|---|
| A. Visible Xbox GPU world | Original `c0a0` onscreen in Xenia | **CONFIRMED** |
| B. Reliable player input | Start and D-pad mapped; actual camera positions change in Xenia logs | **VERIFIED, digital path** |
| C. Traversable vertical slice | Floor collision, basic wall blocking, gravity, jump and first-person view | **CODED EXPERIMENT; runtime unverified** |
| D. Gameplay loop | Trigger/button + door, a pickup, health, one working weapon, one target/enemy and damage | **NOT STARTED** |
| E. Basic Source texture bridge | Private VTF converted; H360 material-to-texture mapping; UVs/sampler on GPU | **NOT STARTED** |
| F. Playable HL1 chapter | BSP brush entities, props and transitions, AI, audio, HUD, checkpoints/saves | **NOT STARTED** |
| G. Entire HL1 campaign and expansions | Many maps, original game rules and content, validation and performance | **MUCH LARGER PROJECT** |
| H. Target-port hardening | Distinguish desktop Xenia, Series S UWP, and actual Xbox 360; fix LUT without emulation-only shortcut | **UNPROVEN** |

**Definition of first real playable milestone:** the player can enter
a small section of `c0a0`, move/look, collide with walls and floor,
interact with a door or trigger, and complete a simple objective.
This is possible without all the original textures or full combat.

**Definition of full Half-Life:** the actual campaign must have
accurate game systems, scripts/brush logic, maps, characters,
audio, combat, persistence and testing. A working renderer alone
does not meet that definition. Exact time-to-complete is
unquantified; do not advertise an invented completion date.

## Next actionable build/test

The user's later actual `hl1_world_explore.log` confirmed Start
switching free-flight and D-pad X/Z camera movement, with occasional
right-stick input. [Cloud build #49](https://github.com/RealMCera/half-life2/actions/runs/40950772992)
adds the **separate `hl1_world_walk.xex`** with initial mesh-based
floor, wall, gravity and jump experiments. After **successful** compilation,
put the private `assets/hl1_c0a0_world.h360` beside the walk XEX,
launch in desktop Xenia, press Start for WALK and test D-pad motion,
then examine `cache/hl1_world_walk.log`. The walking collision code
has **not yet been tested** in Xenia and does not implement GoldSrc
BSP player hulls, so any success will be only a first physics prototype.
