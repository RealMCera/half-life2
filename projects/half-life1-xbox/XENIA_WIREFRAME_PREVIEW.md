# Next milestone: full H360 mesh parse and CPU wireframe BMP

### What the user verified (2026-10-28)

The user used the prior synthetic H360 test fixture and their Windows
Xenia Canary installation. The actual Xenia cached title log contained:

```text
HL1_PROBE: XBOX_360_BOOT_BEGIN
HL1_PROBE: H360 header verified: 1 groups 3 vertices 1 triangles
HL1_PROBE: scene bytes = 166
HL1_PROBE: ASSET_HEADER_PASS
HL1_PROBE: XBOX_360_BOOT_COMPLETE
exit: status 0
```

**This is a confirmed full success for the PREVIOUS header/IO smoke test.**
It does not prove rendered graphics, full mesh decoding or actual
Black Mesa level loading.

### New CPU preview implemented in this branch

Code: `x360_probe/wireframe.c` and `wireframe.h`.

The new version reads the **ENTIRE H360 v1 file**, verifies its mesh
table bounds, positions and every group-local index, then projects
triangle edges onto their two largest spatial axes. It uses a pure
**CPU rasterizer** to write a 640×360 **Windows BMP**:

`game:\hl1_wireframe.bmp`

This is a workaround for xsdk's reported Xenia display-controller
limitation. **It is an OFFLINE IMAGE FILE, not a visible Xenia frame
or playable level.** The preview does not draw inside the emulator.
The renderer colors edges by material group.

Source validation includes the portable, original, synthetic
`tests/test_x360_wireframe_host.py`. GitHub Actions runs this
host-GCC test before attempting the PowerPC XEX cross-compilation,
so obvious mesh decoding and bitmap errors fail early.

### How to test the new executable on Windows

1. Open [GitHub Actions](https://github.com/RealMCera/half-life2/actions)
   and choose a **successful cloud workflow run with this code**.
   The prior successful cloud run `40745594688` is the old,
   header-only XEX and **will not write a BMP**.
2. Download and extract the
   `HalfLife1-X360-XEX-StartupProbe` artifact.
3. Replace the OLD `hl1_remaster_probe.xex` with this newly compiled
   one, or place it in a new folder.
4. Keep the user's previously downloaded synthetic
   `assets/hl1_c0a0_world.h360` beside the new XEX:

```text
XeniaTest/
  hl1_remaster_probe.xex
  assets/
    hl1_c0a0_world.h360
```

5. Run the new XEX in Windows Xenia Canary; the expected window still
   exits normally. Check its cached `hl1_remaster_probe.log`.
6. Look beside the XEX for `hl1_wireframe.bmp` and open it in
   Windows Paint or Photos. If missing, check whether Xenia created
   the file elsewhere and inspect the title log before assuming success.

**Expected new log entries (not yet confirmed on the emulator):**

```text
HL1_PROBE: ASSET_HEADER_PASS
HL1_PROBE: FULL_MESH_PASS: 1 groups 3 vertices 1 triangles
HL1_PROBE: BMP_PREVIEW_PASS: game:\hl1_wireframe.bmp (640x360)
HL1_PROBE: XBOX_360_BOOT_COMPLETE
exit: status 0
```

**Only a real emulator test can confirm those lines.** The CPU
preview has no connection to the on-screen GPU display. A successful
new workflow build proves the source compiles, not that rendering
or file writing works at runtime.

### After this

If the full-mesh preview works with the synthetic triangle, use the
same XEX with the user's **privately extracted** original
`c0a0` Half-Life H360 asset from the prior GoldSrc BSP converter.
This should yield an offline wireframe layout of part of Black Mesa.
Never upload proprietary original game-derived data to a public
GitHub repository.

For interactive graphics in Xenia, separately investigate a
compatible on-screen presentation path; do not conflate that
with CPU BMP output. Xbox Series S Xenia UWP remains untested.
