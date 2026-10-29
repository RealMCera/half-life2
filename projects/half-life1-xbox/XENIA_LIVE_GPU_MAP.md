# USER VERIFIED: REAL Black Mesa map rotating onscreen (2026-10-29)

The user explicitly confirmed: **"I DID SEE IT ROTATING"**, and
also provided a screenshot of the Xenia view. The previously recorded
full GPU-map XEX log showed successful real-map upload (7,833 triangles),
360 frame submissions and 359 flips. This verifies **live onscreen
GPU-rendered real Black Mesa `c0a0` geometry in Windows Xenia Canary**.
No textures, collision, enemies or actual game/first-person player
movement have been implemented.

The previous GPU orbit XEX **`hl1_world_gpu_probe.xex` is KNOWN GOOD
and left unchanged**.

## New separate EXPERIMENTAL indexed explorer XEX

`x360_probe/world_explore.c` builds as `hl1_world_explore.xex`
using the same original GLSL material-color shaders. It converts the
group-relative H360 indices to a single global 16-bit GPU index buffer
and uploads each shared source vertex once instead of repeating
triangle corners. The source H360 data were inspected locally and
confirmed to have 10,378 vertices, 23,499 indices and a maximum global
index of 10,377 (fits 16 bits).

Estimated GPU memory for mesh representation (no other resources):
- Earlier unindexed: **563,976 bytes** of packed vertex data.
- New indexed: **249,072 bytes** vertices plus **47,000 bytes**
  packed 16-bit indices = **296,072 bytes**.
- Difference: **267,904 bytes (~47.5% less mesh buffer memory)**.
  This is theoretical from source format, NOT a measured Xenia frame
  rate improvement or guaranteed reduction in missed flips.

Camera options, if Xenia exposes a gamepad:
- No gamepad: slow auto-orbit, as on known-good XEX.
- In orbit mode: left stick rotates, triggers zoom.
- Press **A** once: toggle free-fly view; left stick walks/strafe,
  right stick looks; triggers rise/descend; hold LB to move faster,
  B resets free-fly camera, Back exits.
- Press A again to return to orbit.
- A maximum of 1,800 frames gives a bounded test; this is NOT
  complete Half-Life navigation (no collision/gravity).
- No keyboard mapping is guaranteed; input mappings depend on Xenia.

**Status:** the separate new indexed/fly XEX is unverified until its
new GitHub build succeeds and user visually tests it.
Do not overwrite or delete the known-good `hl1_world_gpu_probe.xex`
while testing.

## Windows test after successful build

Download the new successful GitHub Actions artifact
`HalfLife1-X360-XEX-StartupProbe`, extract, and place the user's
private mesh next to the new XEX:

```text
HalfLife1-X360-XEX-StartupProbe/
  hl1_world_explore.xex
  assets/
    hl1_c0a0_world.h360
```

Launch `hl1_world_explore.xex` in Xenia Canary. Inspect:

```powershell
Get-Content "C:\Users\FrontDesk\Downloads\xenia_canary_windows\cache\hl1_world_explore.log" -Tail 45
```

Look for `HL1_EXPLORE: INDEXED_MESH_READY`,
`HL1_EXPLORE: GPU_MAP_FRAME_SUBMITTED`, `MODE=FREE_FLY`,
and `HL1_EXPLORE: FRAMES=... VBLANKS=... FLIPS=... MISSED=...`.
Only visually seeing the geometry confirms that it is rendered
correctly; comparing missed flip counts requires the same emulator
configuration and comparable run conditions.

All XEXs use the temporary **Xenia-only** LUT workaround. **NEVER
use the patched diagnostic programs on physical Xbox 360 hardware.**
All copyrighted game-derived assets remain private, outside GitHub.

---

# VERIFIED XENIA RUNTIME: full c0a0 GPU mesh submissions (2026-10-29)

User tested the GPU world XEX from successful [GitHub cloud run
#40924364581](https://github.com/RealMCera/half-life2/actions/runs/40924364581)
and pasted this real Windows Xenia Canary title-log excerpt:

```text
HL1_WORLD: BOOT_BEGIN
HL1_WORLD: REAL_BLACK_MESA_GPU_ORBIT_PROTOTYPE
HL1_WORLD: SCENE_VALID: 46 groups 10378 vertices 7833 triangles
HL1_WORLD: gpu_open = 0x00000000
HL1_WORLD: shaders_load = 0x00000000
HL1_WORLD: GPU_VERTICES_READY: 23499 expanded vertices 563976 bytes
HL1_DISPLAY_EXPERIMENT: ignoring LUT timeout for ONE diagnostic test
HL1_WORLD: present_open = 0x00000000 (1280x720)
HL1_WORLD: GPU_MAP_FRAME_SUBMITTED frame=1 triangles=7833
HL1_WORLD: GPU_MAP_FRAME_SUBMITTED frame=60 triangles=7833
HL1_WORLD: GPU_MAP_FRAME_SUBMITTED frame=180 triangles=7833
HL1_WORLD: GPU_MAP_FRAME_SUBMITTED frame=359 triangles=7833
HL1_WORLD: FRAMES=360 VBLANKS=629 FLIPS=359 MISSED=268
HL1_WORLD: MAP_VISIBILITY_REQUIRES_USER_CONFIRMATION
HL1_WORLD: gpu_close = 0x00000000
HL1_WORLD: BOOT_COMPLETE
exit: status 0
```

**Confirmed:** user-private real `c0a0` H360 mesh parsed and validated;
46 material groups, 10,378 source vertices, 7,833 triangles, 23,499
expanded GPU vertices (563,976 bytes) allocated and uploaded; native
Xbox 360 GPU shaders loaded and 360 map draw/resolve/present frames were
submitted; the SDK reported 359 flips; program exited cleanly.

**NOT YET CONFIRMED:** user has not said whether the real Black Mesa
geometry was visible or looked correct *inside the Xenia window*.
Previous RGB triangle was visually confirmed; this new scene still
requires separate on-screen visual confirmation / screenshot.

**Performance observation:** 268 missed flips across 629 vblanks is a
substantial count and deserves measurement/optimization; do not
conflate raw counters with a frame-rate benchmark or claim a specific
FPS without elapsed time and capture. The experiment can be expensive:
each frame submits an unindexed list of 23,499 vertices.

**Next step:** Ask user whether Black Mesa visibly appeared and ideally
request a screenshot. If yes, work on accurate camera controls,
index-buffer draw optimization and texture/material mapping. If
no, inspect on-screen output and capture logs before asserting
the map renderer works visually.

**Safety/portability:** LUT bypass is experimental and intended
strictly for Xenia Canary. NEVER run on physical Xbox 360 hardware.
All user-derived original Half-Life assets remain private.

---

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
