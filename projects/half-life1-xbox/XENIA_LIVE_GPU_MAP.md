# FIRST WALK XENIA TEST: falls through mesh edges; stable-ground V2 (2026-10-29)

The user ran real `hl1_world_walk.xex` on Windows Xenia Canary
and pasted an 1,800-frame log. **The new first-person WALK mode
has live camera coordinates and CPU floor gravity**, but repeated
unsupported edge transitions caused `on_floor=0` and
`FALL_RECOVERY` (at least nine visible in tail). It sometimes
landed on lower floors, notably `LANDED ground_mm=-552`.
Visible excerpts:

```text
HL1_WALK: PLAYER frame=260 x_mm=1 y_mm=888 z_mm=-250 on_floor=1
HL1_WALK: PLAYER frame=282 x_mm=-380 y_mm=874 z_mm=-145 on_floor=0
HL1_WALK: FALL_RECOVERY
HL1_WALK: LANDED ground_mm=-552
HL1_WALK: GPU_MAP_FRAME_SUBMITTED frame=1799 triangles=7833
HL1_WALK: FRAMES=1800 VBLANKS=2030 FLIPS=1799 MISSED=228
HL1_WALK: SUMMARY MODE=WALK
HL1_WALK: BOOT_COMPLETE
exit: status 0
```

**Confirmed:** the walking XEX booted, decoded the real
`c0a0` H360 collision/rendering mesh, moved through its
Xenia runtime and exited cleanly. The title log alone does
not certify visual collision quality or full floor/wall
correctness; user has not reported jumping successfully.

**Problem:** old walker applied horizontal movement first and
then discovered that no triangle supported the player's feet,
setting `grounded=false`. It used a downward landing cap based
on the *new* feet height, allowing a fast fall to cross a floor
in one frame and miss it.

## New experimental stable walking XEX (separate target)

`hl1_world_walk_stable.xex` built from
`x360_probe/world_walk_stable.c`:
- Preserve the original working `hl1_world_walk.xex`,
  `hl1_world_explore.xex`, and `hl1_world_gpu_probe.xex`.
- Evaluate safe walkable surfaces around 16 ranked candidate
  spawn faces; log `HL1_STABLE: SPAWN_SUPPORT neighbors=N/8`.
- Before taking a grounded horizontal step, check BOTH
  midpoint and destination have floor within the allowable
  step delta. Block unsupported ledges instead of walking
  through visible world geometry gaps.
- Keep grounded feet snapped to the validated floor while
  traversing it; use axis-separated sliding along edges/walls.
- Retain falling when actually jumping or airborne; check
  landing across previous-to-new feet height to reduce
  tunneling through floors.
- Report physics counters in
  `HL1_STABLE: PHYSICS blocked_edges=... blocked_walls=...
  jumps=... landings=... recoveries=...`.

**NOT a full GoldSrc/Source physics implementation.** No
collision-radius hulls, reliable ramps/stairs, brush entities,
trace hulls, ladders, scripted movement, or enemies. These
H360 triangles come from rendering and may not cover real
collision geometry. The safety design temporarily disallows
stepping off unsupported ledges while grounded; that is
intentional in this prototype, not final gameplay behavior.

The new experiment remains **uncompiled/unverified in Xenia**
until GitHub Actions passes and user tests.

After a successful build of the new target:

```text
HalfLife1-X360-XEX-StartupProbe/
  hl1_world_walk_stable.xex
  assets/
    hl1_c0a0_world.h360
```

Run it in Xenia. Virtual Start toggles WALK; D-pad moves,
LB/RB turns, virtual A or Y jumps, B resets, Back exits.
Then inspect:

```powershell
Get-Content "C:\Users\FrontDesk\Downloads\xenia_canary_windows\cache\hl1_world_walk_stable.log" -Tail 100
```

Check `SPAWN_SUPPORT`, `MODE=WALK`, `PLAYER`,
`PHYSICS` and any `FALL_RECOVERY`. Compare behavior
visually, not only via log counts.

No private Valve-owned map assets or textures committed
publicly. XEXs use Xenia-only LUT-timeout bypass;
**NEVER run the experimental SDK builds on physical
Xbox 360**. No local Debian/WSL installation required.

---

# CONFIRMED ACTUAL CAMERA TRANSLATION IN XENIA; walking XEX build pending (2026-10-29)

The user pasted a **real Windows Xenia Canary** log for the
separate indexed free-flight `hl1_world_explore.xex` after
the D-pad-fallback build [#45](https://github.com/RealMCera/half-life2/actions/runs/40941445055):

```text
HL1_EXPLORE: MODE=FREE_FLY frame=208 player_slot=0
HL1_INPUT: RAW slot=0 frame=671 buttons=0x0004 LX=0 LY=0 RX=0 RY=0
HL1_EXPLORE: CAMERA_MOVED frame=671 x_milli=-23 y_milli=2080 z_milli=2711 yaw_milli=-440
HL1_INPUT: RAW slot=0 frame=723 buttons=0x0002 LX=0 LY=0 RX=0 RY=0
HL1_EXPLORE: CAMERA_MOVED frame=723 x_milli=-459 y_milli=2080 z_milli=2945 yaw_milli=-440
HL1_INPUT: RAW slot=0 frame=1192 buttons=0x0001 LX=0 LY=0 RX=0 RY=32767
HL1_EXPLORE: CAMERA_MOVED frame=1192 x_milli=-624 y_milli=-52 z_milli=357 yaw_milli=274
HL1_INPUT: RAW slot=0 frame=1269 buttons=0x0004 LX=0 LY=0 RX=-32768 RY=0
HL1_EXPLORE: CAMERA_MOVED frame=1269 x_milli=-509 y_milli=-52 z_milli=-100 yaw_milli=-1265
HL1_EXPLORE: GPU_MAP_FRAME_SUBMITTED frame=1799 triangles=7833
HL1_EXPLORE: FRAMES=1800 VBLANKS=2039 FLIPS=1799 MISSED=238
HL1_INPUT: SUMMARY success_frames=1800 active_frames=865
HL1_INPUT: SUMMARY A_OR_START_SWITCH=CURRENTLY_FREE_FLY
exit: status 0
```

**Verified:** A/Start-to-free-fly control transition works via
virtual gamepad slot 0; D-pad buttons are recognized;
native camera state moves X/Z (and Y when input moves
vertically); occasional right analog stick RX/RY **also
have nonzero inputs**, disproving the earlier assumption
that analog sticks never work. 1,800 frames submitted,
1,799 flips and 238 missed, exit code zero. Do not
interpret this as a stable FPS benchmark.

**Still unverified:** This is logged camera movement,
not proof the camera stays on safe floor surfaces,
collides with walls, triggers game entities or provides
actual HL1 gameplay. The earlier image and user report
visually confirmed the rotating map; this newer
free-flight movement was confirmed **in the title log**.

## Separate CPU map collision/gravity experiment (new, unverified)

New `x360_probe/world_walk.c` target:
`hl1_world_walk.xex`, using the **same** indexed Xenos GPU
mesh and user-private H360 file. All verified earlier
`hl1_world_explore.xex` and `hl1_world_gpu_probe.xex` source
paths remain unchanged.

Features to test:
- Extract CPU triangle surfaces from the already validated
  private H360 model; identify nearly horizontal walkable
  faces, choose diagnostic spawn on one.
- First-person mode toggled using **virtual Start**.
- In WALK mode D-pad moves and LB/RB turn; analog
  sticks continue to work when Xenia reports them.
- Vertical ray/triangle barycentric test gives terrain height;
  approximate gravity and jump with **virtual A or Y**;
  B respawns, Back exits.
- Minimal two-height segment/mesh wall blocking and
  axis-separated slide; logs SPAWN, COLLISION_MESH,
  PLAYER, LANDED, JUMP, FALL_RECOVERY.
- Like the existing map XEX, bounded at 1,800 frames.
  CPU mesh collision has no acceleration structure yet:
  performance can be worse. More sophisticated BSP
  collision hulls, stairs, moving brushes, trigger
  volumes, exact eye height and map spawn are **not
  implemented**.
- This has not yet passed GitHub compilation or runtime
  testing. Please check the build result before download.

After successful [GitHub Actions build #49](https://github.com/RealMCera/half-life2/actions/runs/40950772992):

```text
HalfLife1-X360-XEX-StartupProbe/
  hl1_world_walk.xex
  assets/
    hl1_c0a0_world.h360
```

In Xenia Canary run `hl1_world_walk.xex`,
press **Start** to enter WALK mode, D-pad Up to
move, LB/RB to turn, A or Y to jump, B to reset,
Back to exit. Read:

```powershell
Get-Content "C:\Users\FrontDesk\Downloads\xenia_canary_windows\cache\hl1_world_walk.log" -Tail 90
```

Observe if the image shows a first-person location
rather than the overview and whether moving is
blocked by a wall and stays on a floor. Report log
plus visual result separately.

This walk title is still strictly **Xenia-only** due
to the temporary LUT timeout SDK patch. Do not use
on physical Xbox 360 hardware.

---

# USER VERIFIED: Start button switches free-fly; analog axes appear ZERO (2026-10-29)

Actual user Xenia title log from run #42:

```text
HL1_INPUT: SLOT=0 frame=0 status=0x00000000 buttons=0x0000
HL1_INPUT: SLOT=1 frame=0 status=0x0000048F buttons=0x0000
HL1_INPUT: SLOT=2 frame=0 status=0x0000048F buttons=0x0000
HL1_INPUT: SLOT=3 frame=0 status=0x0000048F buttons=0x0000
HL1_INPUT: CONNECTED_SLOT=0 frame=0
HL1_INPUT: RAW slot=0 frame=803 buttons=0x0010 LX=0 LY=0 RX=0 RY=0
HL1_EXPLORE: MODE=FREE_FLY frame=803 player_slot=0
HL1_INPUT: RAW slot=0 frame=872 buttons=0x0010 LX=0 LY=0 RX=0 RY=0
HL1_EXPLORE: MODE=ORBIT frame=872 player_slot=0
HL1_INPUT: RAW slot=0 frame=899 buttons=0x0010 LX=0 LY=0 RX=0 RY=0
HL1_EXPLORE: MODE=FREE_FLY frame=899 player_slot=0
HL1_INPUT: RAW slot=0 frame=1399 buttons=0x0020 LX=0 LY=0 RX=0 RY=0
HL1_EXPLORE: FRAMES=1399 VBLANKS=1400 FLIPS=1398 MISSED=0
HL1_INPUT: SUMMARY success_frames=1400 active_frames=529
HL1_INPUT: SUMMARY A_OR_START_SWITCH=CURRENTLY_FREE_FLY
HL1_EXPLORE: BOOT_COMPLETE
exit: status 0
```

The user additionally pressed digital D-pad directions (`0x0001`,
`0x0002`, `0x0004`, `0x0008`) and shoulder buttons
(`0x0100`, `0x0200`), all correctly delivered to virtual player 0.
The samples consistently had `LX=LY=RX=RY=0` even while digital
buttons changed. No `0x1000` virtual Xbox A was observed, but
`0x0010` virtual Start was delivered and **correctly switched
into and out of free-fly mode**. No user visual confirmation of
actual camera movement (translation) yet.

**Latest source fix:** the indexed XEX (but NOT the visually known-good
unindexed orbit XEX) now translates **D-pad Up/Down into forward/back**,
**D-pad Left/Right into strafe**, **LB/RB into yaw turning**.
Analog sticks still work when Xenia exposes them. Triggers and
virtual Xbox Y/X control elevation, B resets and Back exits.
`HL1_EXPLORE: CAMERA_MOVED` logs position in integer milliscale
units when a digital movement button transitions (avoiding float
printf functions Xenia may miscompile). The new code is **not yet
verified in user's Xenia**; compile and test are separate gates.

After the next successful [Actions build](https://github.com/RealMCera/half-life2/actions),
download `hl1_world_explore.xex` and keep the private
`assets/hl1_c0a0_world.h360` beside it. In Xenia Canary:
press virtual Start to enter FREE_FLY, hold D-pad Up to move
forward, D-pad Left/Right to strafe, LB/RB to turn, and Back to
exit. Then inspect:

```powershell
Get-Content "C:\Users\FrontDesk\Downloads\xenia_canary_windows\cache\hl1_world_explore.log" -Tail 85
```

Look for `MODE=FREE_FLY` and `CAMERA_MOVED ... x_milli=...`,
and report whether the scene visibly changes. These movement
controls are a no-collision, no-gravity exploration debug aid,
**not** fully playable Half-Life.

---

# INPUT FIX ATTEMPT / DIAGNOSTIC (2026-10-29)

User reported **“NOPE”** when asked whether Xbox 360 A button
switched the live Black Mesa indexed viewer to free-fly mode.
Previous `hl1_world_explore.log` had all 1,800 frames and
**no `MODE=FREE_FLY` marker**, so there is no evidence
that controller input was delivered or the toggle engaged.
The GPU rendering and mesh loading remained successful.

New source change (`x360_probe/world_explore.c`):
- Polls **all 4 virtual Xbox 360 player slots**, not just
  controller 0; if several slots return success, prefers
  one with nonzero buttons/sticks/triggers.
- Logs raw `XamInputGetState` status at frames 0, 240,
  720 and 1440 for all slots as `HL1_INPUT: SLOT=... status=...`.
- Logs detected slot, nonzero button transitions, stick values
  and `success_frames` versus `active_frames`.
- Allows **Xbox A OR Start** button to toggle the orbit/free-fly
  camera; logs `HL1_EXPLORE: MODE=FREE_FLY frame=... player_slot=...`.
- Keeps the original, visually verified
  `hl1_world_gpu_probe.xex` untouched.

This is a *diagnostic plus potential mapping fix*, not yet confirmed
to make controls work. The new code must first compile and then
be tested inside Windows Xenia Canary.

**Xenia keyboard setup** from [official Xenia Canary Options
wiki](https://github.com/xenia-canary/xenia-canary/wiki/Options):
`keyboard_mode = 1` enables keyboard-as-gamepad,
`keyboard_user_index = 0` maps keyboard to player slot 0.
These are keys in Canary's active TOML config (commonly next to
`xenia_canary.exe`). When keyboard mode is enabled, press
the keyboard key mapped to virtual Xbox A or Start, **not
necessarily the A key on the keyboard**. For physical Xbox
controllers, Xenia's `hid="any"` or `hid="xinput"` are
available; non-XInput controllers may need `hid="sdl"`.
Always close Xenia before editing config and preserve a backup.

After the new build succeeds and is installed, open
`hl1_world_explore.xex` and run:

```powershell
Get-Content "C:\Users\FrontDesk\Downloads\xenia_canary_windows\cache\hl1_world_explore.log" -Tail 75
```

Interpretation:
- `SLOT=0 status=0x00000000` indicates XamInputGetState
  was successful for slot 0.
- All four slots nonzero status implies no usable virtual
  controller mapped, and Xenia input configuration is
  the likely issue.
- `RAW ... buttons=0x1000` is virtual Xbox A,
  `buttons=0x0010` is Start; these should toggle fly.
- A `MODE=FREE_FLY` line definitively shows the camera
  transition code ran (visibility still requires observation).

The emulator-only LUT bypass remains an experiment; **never
run on physical Xbox 360 hardware**.

---

# USER VERIFIED: indexed Black Mesa world runs through all 1,800 GPU frames (2026-10-29)

User ran new `hl1_world_explore.xex` in Windows Xenia Canary and
provided the following title-log excerpt:

```text
HL1_EXPLORE: SCENE_VALID: 46 groups 10378 vertices 7833 triangles
HL1_EXPLORE: gpu_open = 0x00000000
HL1_EXPLORE: shaders_load = 0x00000000
HL1_EXPLORE: INDEXED_MESH_READY vertices=10378 indices=23499 triangles=7833 vertex_bytes=249072 index_bytes=47000
HL1_DISPLAY_EXPERIMENT: ignoring LUT timeout for ONE diagnostic test
HL1_EXPLORE: present_open = 0x00000000 (1280x720)
HL1_EXPLORE: GPU_MAP_FRAME_SUBMITTED frame=1 triangles=7833
HL1_EXPLORE: GPU_MAP_FRAME_SUBMITTED frame=60 triangles=7833
HL1_EXPLORE: GPU_MAP_FRAME_SUBMITTED frame=180 triangles=7833
HL1_EXPLORE: GPU_MAP_FRAME_SUBMITTED frame=1799 triangles=7833
HL1_EXPLORE: FRAMES=1800 VBLANKS=1967 FLIPS=1799 MISSED=165
HL1_EXPLORE: MAP_VISIBILITY_REQUIRES_USER_CONFIRMATION
HL1_EXPLORE: gpu_close = 0x00000000
HL1_EXPLORE: BOOT_COMPLETE
exit: status 0
```

**Confirmed:** indexed GPU mesh successfully loads and emits rendering
commands for all 7,833 triangles across 1,800 frames, with 1,799
SDK-reported flips and no crash. Mesh buffers total 296,072 bytes,
vs 563,976 bytes for unindexed GPU version, a 267,904-byte
(~47.5%) reduction in mesh storage. The initial *unindexed*
live map was already visually confirmed by user in Xenia. The
user has not yet explicitly confirmed that the **indexed** map
looked correct or that the new free-flight controls engaged.

**Performance:** 165 missed flips in the newer 1,800-frame indexed
test versus 268 missed in 360-frame prior unindexed test. Counts
are encouraging, but not a controlled apples-to-apples FPS
benchmark; runtime, camera, and device conditions may vary.

**Input not yet confirmed:** no `HL1_EXPLORE: MODE=FREE_FLY`
entry in this title log. This may mean the A toggle was never
pressed, the gamepad was unavailable/unmapped, or input events
were not delivered. Ask user if A toggled camera and whether
analog sticks moved the camera before claiming free-flight works.

The XEX remains Xenia-only due to experimental LUT timeout
diagnostic bypass; **never run on actual Xbox 360 hardware**.

---

# Visual check: Xenia screenshot of Black Mesa geometry (2026-10-29)

The user sent another screenshot showing a Xenia Canary **real-time
window** displaying the recognizable `c0a0` map: raised walkway
joining two structural clusters, curved/round central structure,
blue-green/purple flat procedural surfaces, a detached rectangular
piece, and the dark navy backdrop. This confirms that live GPU
geometry still visibly renders with the current test environment,
with no obvious missing geometry compared with the prior user
screenshots. No screenshot was committed to this public repo.

**Important limitation:** the screenshot itself does not identify the
XEX basename, so it cannot prove the new indexed explorer was the
program shown, nor that A-toggle, right-stick camera, or indexed
draw are functioning. Those require the user's new
`hl1_world_explore.log` and hands-on controls observation.

GitHub [build run #40929369460](https://github.com/RealMCera/half-life2/actions/runs/40929369460)
was checked and completed with **SUCCESS**, producing the
`HalfLife1-X360-XEX-StartupProbe` artifact containing
`hl1_world_explore.xex`. Compiling it is not proof of runtime
correctness; distinguish from the known-good live orbit version.

---

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
