# VERIFIED: native RGB-triangle shader and frame submissions in Windows Xenia Canary

The user ran the cloud-built `hl1_triangle_probe.xex` in Xenia Canary
on October 29, 2026, and shared an actual title log:

```text
HL1_TRIANGLE: BOOT_BEGIN
HL1_TRIANGLE: EMULATOR_ONLY_LUT_DIAGNOSTIC
HL1_TRIANGLE: gpu_open = 0x00000000
HL1_TRIANGLE: shaders_load = 0x00000000
HL1_DISPLAY_EXPERIMENT: ignoring LUT timeout for ONE diagnostic test
HL1_TRIANGLE: present_open = 0x00000000 (1280x720)
HL1_TRIANGLE: GPU_DRAW_SUBMITTED frame=1
HL1_TRIANGLE: GPU_DRAW_SUBMITTED frame=60
HL1_TRIANGLE: GPU_DRAW_SUBMITTED frame=179
HL1_TRIANGLE: SUBMITTED=180 VBLANKS=181 FLIPS=179 MISSED=1
HL1_TRIANGLE: VISIBILITY_REQUIRES_USER_CONFIRMATION
HL1_TRIANGLE: gpu_close = 0x00000000
HL1_TRIANGLE: BOOT_COMPLETE
exit: status 0
```

**Confirmed:** native Xenos vertex/pixel shader loading, GPU draw
command submission, 180 frame submissions and 179 flips reported
by the Xenia SDK, and successful exit. One missed flip occurred
in this test; this alone does not indicate a meaningful failure.

**Still unknown:** The user has not yet explicitly said whether the
actual RGB triangle was VISIBLE inside Xenia. Shader load and flip
numbers do **not** establish that the GPU rasterized the expected
triangle correctly. The previous *solid-color* probe was visibly
confirmed, but the triangle is a separate rendering pipeline.

**Next checkpoint:** ask the user whether they saw a red-green-blue
triangle on a dark background. If visible, proceed to a genuine
on-screen first-person H360 map prototype. If not, debug the vertex
shader, GPU rasterizer, depth/culling, or pixel shader/resolve path
before assuming we can display Black Mesa GPU geometry.

The XEX uses an experimental temporary-cloud xsdk modification
which ignores a LUT autofill timeout for emulator diagnosis.
NEVER use that workaround on physical Xbox 360 hardware.

---

# CONFIRMED: user actually SAW changing colors in Windows Xenia Canary

On October 29, 2026, the user explicitly confirmed the
180-frame experimental XEX **visibly changed colors inside Xenia
Canary**, beyond the previously captured numerical log. Combined
with the logged 180 submitted frames / 179 flips / zero missed,
this verifies actual display output for the SDK LUT-timeout-bypass
experiment on the user's Windows Xenia version.

**New cloud build run #40910943589 completed successfully** and
uploaded `HalfLife1-X360-XEX-StartupProbe`, including
`hl1_triangle_probe.xex` for testing native GPU vertex + pixel
shader rasterization. The new triangle XEX has **not** yet been
visually tested in Xenia; a successful compile does not establish
GPU triangle correctness.

Build: https://github.com/RealMCera/half-life2/actions/runs/40910943589

The user should extract the artifact, launch
`hl1_triangle_probe.xex` in Xenia Canary, observe whether an RGB
triangle appears, and inspect:

```powershell
Get-Content "C:\Users\FrontDesk\Downloads\xenia_canary_windows\cache\hl1_triangle_probe.log" -Tail 35
```

This experiment uses the disposable patched xsdk LUT diagnostic
and must NOT be used on actual Xbox 360 hardware.

---

# Verified Xenia flips and next shader-drawn triangle diagnostic

## Real user result — October 29, 2026

The user ran our **180-frame** `hl1_display_lut_probe.xex` in Windows
Xenia Canary and shared the complete title log excerpt:

```text
HL1_DISPLAY: gpu_open = 0x00000000
HL1_DISPLAY_EXPERIMENT: ignoring LUT timeout for ONE diagnostic test
display: 1280x720 shown 1:1 to the 1280x720 output
HL1_DISPLAY: present_open = 0x00000000
HL1_DISPLAY: display_dimensions = 1280x720
HL1_DISPLAY: FRAME_SUBMITTED: 0
HL1_DISPLAY: FRAME_SUBMITTED: 1
HL1_DISPLAY: FRAME_SUBMITTED: 60
HL1_DISPLAY: FRAME_SUBMITTED: 120
HL1_DISPLAY: FRAME_SUBMITTED: 179
HL1_DISPLAY: FRAMES_SUBMITTED_TOTAL = 180
HL1_DISPLAY: VBLANKS = 181 FLIPS = 179 MISSED = 0
HL1_DISPLAY: LAST_FLIP_VBLANK = 181
HL1_DISPLAY: EMULATED_FLIPS_REPORTED (visible output still requires observation)
HL1_DISPLAY: HOLDING_LAST_FRAME_1500_MS
HL1_DISPLAY: BOOT_COMPLETE
exit: status 0
```

**Confirmed:** our experimental Xenia-only Xbox 360 PowerPC title
submitted 180 frames and SDK counted 181 emulated vblanks,
179 completed flips and 0 missed flips. This is a meaningful
progress milestone beyond a one-shot frame submission.

**NOT confirmed:** the user has NOT yet said whether the Xenia
window visibly changed through blue-green, violet and amber.
Counters alone do not verify visual output. The line-counter
value in earlier logs was unusual. Always distinguish emulator
bookkeeping from actual human-visible frames.

## New diagnostic: actual Xbox 360 GPU triangle

The code at `x360_probe/triangle_probe.c` uses original GLSL
shaders in `hl1_triangle.vert` and `hl1_triangle.frag`
compiled by xsdk's shader toolchain for the Xbox 360 GPU.
It draws one triangle with interpolated red, green and blue
vertex colors against a dark background, for up to 180 frames.

Unlike the confirmed static shaded BMP output, the triangle
is meant to appear **inside Xenia's window** through the native
GPU pipeline, not in a `cache:\*.bmp` file. The shader code
is entirely original and includes no Valve game assets.

A *distinct experimental XEX*, `hl1_triangle_probe.xex`, is
built ONLY against the disposable, experimental Xenia
LUT-timeout-bypass SDK, not the unchanged SDK used by the
working static H360 preview. The bypass is an emulator-only
diagnostic, **never use it on actual Xbox 360 hardware**.

### Windows test procedure — only after a successful new cloud build

1. Open the new run at:
   https://github.com/RealMCera/half-life2/actions
2. Download `HalfLife1-X360-XEX-StartupProbe` and extract.
3. Find `hl1_triangle_probe.xex`; no `assets/` folder is needed.
4. Launch that file in Windows Xenia Canary.
5. Note whether the emulator window shows a colored RGB triangle
   against a dark background, even briefly. A normal dashboard
   exit is expected after the finite test.
6. Read the title log:

```powershell
Get-Content "C:\Users\FrontDesk\Downloads\xenia_canary_windows\cache\hl1_triangle_probe.log" -Tail 35
```

Useful lines: `shaders_load`, `GPU_DRAW_SUBMITTED`,
`SUBMITTED`, `FLIPS`, `present_begin failed`.
If the shader compiler fails in GitHub, **do not** download
or claim the new binary works; inspect CI logs and repair.

### Scope remaining

The real original Half-Life 1 `c0a0` (7,833-triangle)
world mesh has passed CPU parsing and 3 offline BMP preview tests,
but has NOT been rendered in a GPU framebuffer yet. No camera,
controller, BSP renderer, level collision or story gameplay
has been implemented. Xbox Series S Xenia UWP remains untested.
No Debian installation is needed on Windows: GitHub builds the XEX.
