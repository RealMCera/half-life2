# Current target: Xbox 360 game, tested in Xenia emulator

The user will run the remaster using an **Xbox 360 emulator**, not a modded
physical Xbox 360. We still need to compile a native 360 **XEX2** executable;
the original Xbox XBE cannot boot in Xenia, and H360 asset meshes are not
programs. A modded physical console is **not required** for the emulator route.

Start with [XENIA_EMULATOR_TARGET.md](XENIA_EMULATOR_TARGET.md) for PC Xenia
Canary and optional Xbox Series S Xenia UWP testing, then see
[XBOX_360_TARGET.md](XBOX_360_TARGET.md) for hardware/asset plans.

**No playable XEX exists yet and none has been emulator-tested.**

---

# Primary target changed: XBOX 360

The user confirmed the remaster is for **Xbox 360**. The modern PC/UE5
approach below is no longer the deployment target. Read the new
[XBOX_360_TARGET.md](XBOX_360_TARGET.md) for the real 360 hardware
limits, open homebrew toolchain options and the H360 model pipeline.

**This project does not yet produce a playable Xbox 360 XEX.**

---

# Half-Life 1 Graphics Remaster — original campaign, upgraded visuals

**Project intent (corrected October 28, 2026):** Preserve Half-Life 1's original maps, gameplay, NPCs, weapons and story, while modernizing textures, models, lighting and effects. Half-LifeX is our *starting version of Half-Life 1*, not a graphics-quality target. The user also owns an Xbox disc image of Half-Life 2, which is reference/possible licensed source material for compatible elements, **not a drop-in replacement engine**.

**Target:** Prior user context prefers Xbox or Xbox 360. Prioritize an original Xbox-compatible detail-texture prototype using the supplied Half-LifeX ISO, then evaluate whether the Xbox 360 requires a distinct Source-engine port. Xbox 360 is faster, but a native 360 build cannot be produced merely by inserting Half-Life 2 files into Half-LifeX.

## First concrete graphics prototype (implemented)

Original Half-LifeX source (`Engine/client/gl_rmisc.c`) includes `R_ParseDetailTextures`, which loads map-specific `maps/<map>_detail.txt` definitions, reads images from `gfx/<image>`, and uses the `r_detailtextures` setting. The source also includes the `gl_rsurf.c` detail surface render path. This appears to provide a low-risk *experimental* enhancement path; **not yet validated with this actual Beta 1.4 XBE or hardware**.

From this repository's root, on Windows with Python 3.10+:

```powershell
py -3 -m tools.half_life1_graphics inspect "C:path	oHalf-LifeX_Test_v2.iso" --map c0a0
py -3 -m tools.half_life1_graphics stage "C:path	oHalf-LifeX_Test_v2.iso" --map c0a0 -o "localhl1-remaster"
```

The stage output is a **standalone mod overlay** to experiment with against your *existing*, legally obtained game installation, not a replacement executable or ISO. Its paths are:

```text
local/hl1-remaster/
  valve/gfx/detail/hl1_concrete.tga
  valve/gfx/detail/hl1_metal.tga
  valve/maps/c0a0_detail.txt
  remaster_report.json
```

- Source code parses original BSP30 texture names from `maps/c0a0.bsp` inside `valve/pak0.pak`.
- It creates **two original, tileable, grayscale 128x128 procedural detail textures** for initial compatibility tests. These are *not* actual Half-Life 2 textures and do not create Half-Life 2-grade visuals alone.
- Conservative matching currently selects **three of the first map's 77 texture names** (`crete4_flr03`, `frostgrate`, `drkmtlt_bord12`). More assignments are deliberately manual pending artist review.
- To override or skip specific texture assignments, supply a JSON object such as `{"crete4_flr03":"concrete","frostgrate":"metal","poster14":"skip"}` through `--assignments path/to/file.json`.
- Copy the `valve/` folder contents into a **backup/test copy** of your extracted Half-LifeX game, maintaining directories, then launch your existing game with `r_detailtextures 1` if the build accepts that cvar. Verify visually; absence of visible overlays or crashes means more renderer work is needed.
- **Safety:** do not modify the source ISO, do not commit extracted assets, and do not call this a finished remaster.

## Graphics roadmap

1. **Prove first-level detail passes** and measure Xbox frame rate, texture VRAM and memory. Original Xbox 64 MB vs 128 MB limitations matter.
2. **High-resolution base textures**: audit all map texture names; author or adapt suitable higher-resolution concrete, metal, wood and lab materials. Confirm how this Xash3D Xbox build loads base texture replacements; detail textures alone are insufficient.
3. **Updated models and sprites**: update scientists, guards, creatures, hands, weapons, effects; GoldSrc MDL ≠ Source MDL, so conversion/rigging/animation must be done deliberately.
4. **Lighting and renderer**: tune lightmaps and effects; true Source-like lighting requires substantial graphics-engine changes or moving original maps/content into the Source engine.
5. **Xbox 360 feasibility**: a separate Source-engine port/rebuild may deliver more of the desired visuals than original-Xbox hardware, but compatibility, SDK and platform packaging are unproven. Stage and validate on PC first, then evaluate console targets.

## Half-Life 2 ISO relationship

Google Drive link supplied to the conversation identifies `Half-Life 2 (USA, Europe) (En,Fr,De,Es,It).xiso.iso`, 2.06 GB. Metadata was accessible but downloading the file failed, so **no Source materials from that file have been extracted or used**. Existing original Xbox Half-Life 2 content is not evidence of a working Half-Life 1 renderer or automatically compatible texture/model assets.

---

## Earlier preparation notes

# Half-Life 1 original Xbox rebuild workspace

This workspace concerns **Half-Life (1998)**, not Half-Life 2. The supplied
Half-LifeX_Test_v2.iso is an existing **Half-LifeX Beta 1.4** original-Xbox
port of Half-Life 1. Its included ReadMe.txt identifies Xash3D + FakeGL.
This work is a *verified extraction / rebuild preparation step*, not a
newly compiled playable game.

## Two paths, different outputs

**Original Xbox XBE route:** The upstream Half-LifeX source exists at
https://github.com/brentdc-nz/Half-LifeX and includes Engine/Half-LifeX.sln,
Client, Server, and MenuUI projects. The upstream available source reads as
Beta 0.90 and is *not verified* as matching the Beta 1.4 disc executable.
It documents that version as requiring a 128 MB modded original Xbox.
Rebuilding an Xbox XBE requires appropriate original-Xbox-compatible build
tools, dependency resolution and real runtime tests.

**Windows native recompilation route:** The parent repository xboxrecomp
converts original-Xbox XBE instructions into C and links Windows runtime
libraries. It does NOT create an Xbox XBE, and its own CMake target only builds
libraries. It needs a separate game-specific executable project and runtime
integration, as described in docs/GETTING_STARTED.md.

## Reproducible inspection (PowerShell)

From the checkout root, with Python 3.10+:

~~~powershell
py -3 -m tools.half_life1_xbox inspect "C:\path\to\Half-LifeX_Test_v2.iso" --report "local\hl1\inspection.json"
py -3 -m tools.half_life1_xbox prepare "C:\path\to\Half-LifeX_Test_v2.iso" -o "local\hl1\game_files" --report "local\hl1\inspection.json"
py -3 -m tools.xbe_parser "local\hl1\game_files\default.xbe" --json "local\hl1\game_files\default_analysis.json"
~~~

The preparation tool reads XDVDFS, validates the Xbox executable and PAK index,
checks for the original campaign and training starting maps, and extracts the
files to an ignored *local* directory. It does not compile or run a game.

## Results from supplied Beta 1.4 disc

- Xbox executable title: Half-LifeX; default.xbe size: 2,281,472 bytes.
- XBE SHA-256: bd7286c4eb9a6cf4fa08e25b27e6a5fcef46cb97bf4b5c8a44b459d020b35475
- Disc: 267 files.
- valve/pak0.pak: 303,829,553 bytes with 3,454 indexed assets.
- Maps: 111 BSP files, including maps/c0a0.bsp (campaign start)
  and maps/t0a0.bsp (training start).

These checks validate packaged content, not playability or memory compatibility.

## Next development milestones

1. Boot and benchmark the *existing* Beta 1.4 XBE on intended Xbox hardware
   or a suitable test environment; record specific failures.
2. Check the Half-LifeX source and its build dependencies against the
   packaged executable. Do not assume the published older source exactly
   reproduces the supplied Beta 1.4 image.
3. Choose an original-Xbox source build or PC native recompilation.
4. Implement and test missing build, graphics, audio and memory interfaces
   before calling any generated artifact a playable rebuild.

## Important separation: Half-Life 2 Drive image

A separate Google Drive file supplied on 2026-10-28 is named
"Half-Life 2 (USA, Europe) (En,Fr,De,Es,It).xiso.iso" (2,059,796,480 bytes).
It is a Half-Life **2** disc, not the Half-Life **1** source or map data.
Do not substitute this disc when rebuilding the first Half-Life.

## Never commit copyrighted game data

The ISO, default.xbe, PACK archives, model files, texture data, DLLs and
proprietary Xbox SDK components remain local. The repo root ignores local/
and game_files/, and the project .gitignore excludes common asset formats.
