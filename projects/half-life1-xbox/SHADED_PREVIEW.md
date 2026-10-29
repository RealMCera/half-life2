# VERIFIED — solid shaded c0a0 image in real Windows Xenia Canary (2026-10-29)

The user supplied a screenshot of `hl1_shaded.bmp`, opened in
Windows Live Photo Gallery. The image depicts a clean **solid, flat-colored
isometric rendering** of the original `c0a0` Black Mesa geometry, including
two room structures, a long curved elevated connector, height differences,
and a small isolated-looking rectangular solid at the bottom. The earlier
wireframe screenshots and Xenia logs agree with this geometry.

This verifies that the Xbox 360 XEX CPU renderer's shaded/depth-buffered
offline BMP output works **end to end** in Windows Xenia, not just in
host-C tests. A separate new runtime log was not provided in this turn;
the screenshot demonstrates image output, not a formal frame-timing or
render correctness benchmark.

**Current limits:** no game textures, no true per-pixel material maps,
no movable BSP entities, no first-person controller, no gameplay,
and **no GPU-presented real-time scene** in Xenia. Do not call
the static offline BMP a playable game or a live renderer.

**Next engineering gate:** first prove a simple **solid-color frame can
be displayed inside the Xenia emulator window** using an Xbox 360 XEX.
The public xsdk GETTING_STARTED docs warn that its `xsdk_present_open`
display-controller path returns `0xC00000BB` on their tested
desktop Xenia version. A diagnostic-only display-probe XEX should
record the actual kernel/GPU statuses on the user's Xenia build,
without assuming it can draw. Only after a real frame is visible should
we work on a navigable first-person mesh renderer.

---

# Black Mesa c0a0 — shaded isometric milestone

## User-verified current state

The user supplied a screenshot of the **second**, isometric, offline
image `hl1_isometric.bmp` generated under **Windows Xenia Canary** by our
custom PPC Xbox 360 XEX.

Visual inspection of the screenshot shows:

- Two substantial vertical, multi-level room structures
- A long raised connector with a curved bend between larger spaces
- Material-coded triangle wireframes, with no immediately obvious vertex
  explosion in the projection
- A small separate-looking green cluster below the main geometry in
  screen space. This is NOT proof of a geometry error; the raw world
  may contain separate vertical sections or disconnected entities, and
  the fixed projection/occlusion cannot resolve that by itself.

**This confirms the previously built isometric preview works**.
The map geometry contains 46 material groups, 10,378 vertices,
and 7,833 triangles. The offline screenshot is not a textured
gameplay view and should not be described as a rendered playable game.
The actual screenshot image is user-owned and is not copied into
this public repository.

## New source experiment: solid shaded surfaces

The branch now includes:

- `projects/half-life1-xbox/x360_probe/shaded.c` and `shaded.h`:
  fixed isometric solid triangle rendering on CPU, with a full float
  depth buffer and a simple directional light.
- A third 640x360 Windows BMP named
  `cache:\hl1_shaded.bmp`. The previous `hl1_wireframe.bmp`
  and `hl1_isometric.bmp` are retained.
- A host-GCC regression fixture that checks all three images and
  validates the new shaded image has nonbackground pixels.
- The same GitHub-hosted cross compiler: **no Debian or WSL
  installation on the user's Windows PC**.

The shaded preview uses pseudo-colors based on each H360 material
group key; it does **not** use the game's actual original textures,
PBR material maps, dynamic lighting, BSP visibility, or a working
Xbox 360 GPU. Its z-buffer removes triangle surfaces hidden from the
view. This is still a static, offline image written via Xenia's
writable cache mount, not real-time on-screen 3D.

**Status:** This new shader is currently source-code-only until a
fresh CI workflow run passes and the user actually tests the updated
XEX in Windows Xenia.

## Instructions after a NEW successful GitHub Actions build

1. Open the latest successful workflow in
   https://github.com/RealMCera/half-life2/actions
2. Under **Artifacts**, download `HalfLife1-X360-XEX-StartupProbe`.
3. Extract `hl1_remaster_probe.xex` and replace the previous XEX
   in the test folder; leave the same private
   `assets/hl1_c0a0_world.h360` file beside it.
4. Open the XEX in Xenia Canary, let it exit normally, then check
   `cache/hl1_remaster_probe.log` for
   `HL1_PROBE: SHADED_PREVIEW_PASS`.
5. Open the image from Windows PowerShell:

```powershell
Start-Process "C:\Users\FrontDesk\Downloads\xenia_canary_windows\cache\hl1_shaded.bmp"
```

6. Share a screenshot of the new image if it was created. If the
   file was not created, provide the new Xenia title log.

Do not assume that a green GitHub build proves graphics on Xenia;
each emulator output must be verified independently.
