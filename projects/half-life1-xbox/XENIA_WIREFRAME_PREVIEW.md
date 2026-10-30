# Latest PC test — cache BMP file located

User reports Windows PowerShell now finds:

```text
C:\Users\FrontDesk\Downloads\xenia_canary_windows\cache\hl1_wireframe.bmp
```

This is the **intended writable location** introduced in the cache-path
fix. The user first displayed a log from the *older* XEX, ending with
`BMP_PREVIEW_FAIL` / `exit: status 1`, and later ran a file search
that located the new BMP. The exact creation timestamp, BMP contents,
and **new version's** `BMP_PREVIEW_PASS` log line have not yet been
provided. Do not conflate the earlier failure log with the later
successful file discovery. The user should open the BMP and share
the newer log or screenshot to confirm the preview contents.

Cloud build #40758925773 completed with **success**, including
host-based synthetic renderer tests and PowerPC XEX compilation.
The actual user-side file discovery is a promising indication that
writing the BMP to Xenia's `cache:` mount works.

Next technical milestone after verification: swap the synthetic
one-triangle scene for the user's original privately exported
Half-Life 1 `c0a0` H360 mesh, and inspect its CPU wireframe
projection. This is not yet live on-screen 3D graphics.

---

# Xenia write access fix — user-confirmed FULL_MESH_PASS

The user's **actual 2026-10-28 Windows Xenia test** of the first
wireframe XEX logged:

```text
HL1_PROBE: H360 header verified: 1 groups 3 vertices 1 triangles
HL1_PROBE: ASSET_HEADER_PASS
HL1_PROBE: FULL_MESH_PASS: 1 groups 3 vertices 1 triangles
open: NtCreateFile(game:\hl1_wireframe.bmp) failed: 0xC0000022
HL1_PROBE: BMP_PREVIEW_FAIL: could not write preview
HL1_PROBE: XBOX_360_BOOT_COMPLETE
exit: status 1
```

**The mesh decoded correctly; only the BMP write failed.**
`0xC0000022` is `STATUS_ACCESS_DENIED`, because Xenia mounts
standalone XEX content as a read-only `game:` host path.
Xenia's own code mounts `cache:`, `cache0:` and `cache1:`
as writable host paths.

**New source version (needs fresh GitHub Actions XEX build, and actual
emulator test):** try the writable `cache:` first, then `cache0:`,
`cache1:`, finally `game:` for console compatibility. The CPU
renderer additionally prints an easily inspected 64×24 text preview
as `HL1_WIRE|...|` lines in the *existing Xenia title log*. Even
if all BMP writes fail, the mesh check and ASCII preview remain
successful, and it returns exit status 0.

The BMP is expected in **Xenia's cache folder**, not necessarily next
to the XEX. On the Windows PC:

```powershell
Get-ChildItem "C:\Users\FrontDesk\Downloads\xenia_canary_windows" -Recurse -File -Filter "hl1_wireframe.bmp" -ErrorAction SilentlyContinue | Select-Object FullName,Length
```

Check the exact output path and indicators in the **new** log:

```text
HL1_PROBE: FULL_MESH_PASS: 1 groups 3 vertices 1 triangles
HL1_PROBE: ASCII_PREVIEW_BEGIN: 64x24
HL1_PROBE: ASCII_PREVIEW_PASS
HL1_PROBE: BMP_PREVIEW_PASS: cache:\hl1_wireframe.bmp (640x360)
HL1_PROBE: XBOX_360_BOOT_COMPLETE
exit: status 0
```

If there is no `BMP_PREVIEW_PASS`, look for
`BMP_PREVIEW_UNAVAILABLE` instead and share the
`HL1_WIRE|` lines as the fallback. **Do not download the older
#40754365566 artifact** for this test; it always attempts to write
to `game:` and cannot fix the access error.

---

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
