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
