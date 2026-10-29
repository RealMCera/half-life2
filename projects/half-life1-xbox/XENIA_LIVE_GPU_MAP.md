# BUILD FIX — Shader header mismatch (2026-10-29)

GitHub Actions run [#40918892542](https://github.com/RealMCera/half-life2/actions/runs/40918892542) **failed during compilation**, before producing the new live map XEX. The exact compiler error was:

```text
fatal error: hl1_world_shaders.h: No such file or directory
```

The XSDK CMake shader helper generates `hl1_world_gpu_probe_shaders.h` for the target `hl1_world_gpu_probe`, not `hl1_world_shaders.h`. Updated the C source to include the **correct generated header**, commit `d2416d5`.

New build [#40924364581](https://github.com/RealMCera/half-life2/actions/runs/40924364581) was started to verify the fix. **Do not download an XEX until that new build is successful**; no live map rendering has been emulator tested yet.

---

# First live GPU Black Mesa `c0a0` renderer (Xenia-only experiment)

## Confirmed preceding milestone

On October 29, 2026, the user **explicitly confirmed seeing the
red/green/blue triangle** directly in the Windows Xenia Canary
emulator window, after supplying this real title log:

```text
HL1_TRIANGLE: shaders_load = 0x00000000
HL1_TRIANGLE: present_open = 0x00000000 (1280x720)
HL1_TRIANGLE: GPU_DRAW_SUBMITTED frame=179
HL1_TRIANGLE: SUBMITTED=180 VBLANKS=181 FLIPS=179 MISSED=1
exit: status 0
```

This verifies that native Xbox 360 Xenos programmable vertex/pixel
shaders, draw commands and the on-screen presentation path work
under the temporary **Xenia-only** SDK LUT-timeout diagnostic
modification. It does not establish a production-quality renderer.

## New GPU map experiment

New files:

- `x360_probe/world_gpu_probe.c` — independent emulator-only XEX;
- `x360_probe/hl1_world.vert` — 3D vertex shader with MVP matrix;
- `x360_probe/hl1_world.frag` — interpolated vertex color shader;
- CMake and GitHub cloud workflow build and package
  `hl1_world_gpu_probe.xex`.

The program reads the **user's private actual Half-Life 1 c0a0**
staging mesh in the same `assets/hl1_c0a0_world.h360` folder that
the previously confirmed CPU-BMP XEX uses.

It verifies the full H360 v1 header, group-index bounds and finite
vertex positions; converts GoldSrc Z-up to a GPU Y-up world; expands
the per-group 16-bit indices to a GPU triangle list; applies
synthetic per-material face colors and directional shading; then
draws triangles with a real native Xbox 360 **depth-tested GPU
target**, perspective MVP vertex shader and orbiting camera.

Private target map previously verified by the user's Xenia
logs: 46 material groups, 10,378 source vertices, 7,833 triangles
(23,499 expanded draw vertices) from `c0a0`.

**What to expect:** only geometrical static Black Mesa structures
in solid, pseudo-colored 3D, automatically orbiting. This is
not first-person navigation and has no game textures, moving
brushes, player collision, enemies, weapon logic or audio.

### How to test, ONLY after a successful new GitHub build

1. Download the artifact `HalfLife1-X360-XEX-StartupProbe` from
   https://github.com/RealMCera/half-life2/actions
2. Extract it to its own folder, placing your **already extracted
   private map** beside the new XEX like this:

```text
HalfLife1-X360-XEX-StartupProbe/
  hl1_world_gpu_probe.xex
  assets/
    hl1_c0a0_world.h360
```

3. In Windows Xenia Canary choose File > Open and select
   `hl1_world_gpu_probe.xex`.
4. Observe whether Black Mesa geometry actually appears in the
   emulator window and rotates. A normal exit after ~360 frames
   is expected. **Left stick** changes the orbit, **triggers**
   change camera distance, and **Back** exits early if a
   controller is connected. It automatically turns when no
   gamepad is connected. Performance is not benchmarked.
5. Copy the program's own Xenia title log:

```powershell
Get-Content "C:\Users\FrontDesk\Downloads\xenia_canary_windows\cache\hl1_world_gpu_probe.log" -Tail 40
```

Look for `HL1_WORLD: SCENE_VALID`,
`HL1_WORLD: shaders_load = 0x00000000`,
`HL1_WORLD: GPU_VERTICES_READY`,
`HL1_WORLD: GPU_MAP_FRAME_SUBMITTED` and
`HL1_WORLD: FRAMES=...`.
The log alone does **not** prove that the actual map was
visible. The user should report whether the on-screen
3D scene appeared and optionally share a screenshot.

## Status and safety

**New XEX unverified** until GitHub's *new* CI build succeeds
and the user runs it in Xenia. Do not infer GPU correctness from
the prior visible RGB triangle. This target is compiled only in
GitHub's disposable experimental LUT SDK; **NEVER run on
physical Xbox 360 hardware**.

No Debian, WSL distribution or local cross compiler is required.
No copyrighted Valve-owned game-derived mesh data are committed
to the public GitHub repository. The existing successful CPU
BMP test remains a separate XEX and is unchanged.
