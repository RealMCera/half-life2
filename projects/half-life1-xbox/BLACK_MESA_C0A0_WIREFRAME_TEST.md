# VISUAL CONFIRMATION: user screenshot of real Black Mesa c0a0 map (2026-10-28)

The user opened the **actual Xenia-generated** `hl1_wireframe.bmp`
in Windows Live Photo Gallery and supplied a screenshot. The
image visibly shows a coherent, multi-part **overhead projection**
with two substantial room/complex areas connected by a long
curving corridor, plus smaller adjoining structures. Distinct
material groups are assigned different colored edges. The many
diagonal crossings are the raw face triangulation, NOT proof of
broken geometry. No obvious disconnected/misaligned vertex
explosion is visible in the screenshot.

**Confirmed:** our actual first Half-Life 1 Black Mesa map is no
longer only a successful parser log. The user can view a full
geometry diagram produced by a custom Xbox 360 XEX running under
Windows Xenia Canary. The real game-derived H360 asset is retained
privately and **not** in the public repository. The screenshot
itself is not committed here.

**Important:** This is a 2D overhead wireframe, **not** a textured,
lit, real-time 3D scene. It does not depict a playable interior
or establish that game logic, moving objects or enemies function.

## Improved next experiment

We added an **additional fixed isometric 3D-to-2D projection**
with GoldSrc's Z-up convention. The source now attempts to
write `cache:\hl1_isometric.bmp` while retaining
`cache:\hl1_wireframe.bmp`. Its second view is also
CPU-generated **offline** geometry. The new renderer is not
emulator-verified yet; only the next *successful* cloud build
containing this feature can be tested.

From Windows PowerShell after loading that new XEX with the same
`assets/hl1_c0a0_world.h360`:

```powershell
Start-Process "C:\Users\FrontDesk\Downloads\xenia_canary_windows\cache\hl1_isometric.bmp"
```

Check `hl1_remaster_probe.log` for `ISO_PREVIEW_PASS` before
expecting the new file. If the path isn't present or the new marker
doesn't appear, make sure the downloaded XEX comes from the
**latest successful run** under
https://github.com/RealMCera/half-life2/actions

---

# VERIFIED ON WINDOWS XENIA: FULL REAL BLACK MESA c0a0 LOAD & BMP EXPORT

On October 28, 2026, the user executed our Xbox 360 `hl1_remaster_probe.xex`
in Windows Xenia Canary with the **privately extracted actual Half-Life 1 c0a0
worldspawn geometry**. The following **real user-provided Xenia title log**
confirms the milestone:

```text
HL1_PROBE: XBOX_360_BOOT_BEGIN
HL1_PROBE: H360 header verified: 46 groups 10378 vertices 7833 triangles
HL1_PROBE: scene bytes = 298310
HL1_PROBE: ASSET_HEADER_PASS
HL1_PROBE: FULL_MESH_PASS: 46 groups 10378 vertices 7833 triangles
HL1_PROBE: ASCII_PREVIEW_BEGIN: 64x24
HL1_PROBE: ASCII_PREVIEW_PASS
HL1_PROBE: BMP_PREVIEW_PASS: cache:\hl1_wireframe.bmp (640x360)
HL1_PROBE: XBOX_360_BOOT_COMPLETE
exit: status 0
```

The 64×24 ASCII preview in the same log shows a complex, nontrivial
multi-area top-down mesh projection, rather than one triangle.

**VERIFIED:** the actual first-map H360 v1 geometry loads in Xenia's
PPC runtime, the vertex/group index data validates in the XEX,
the CPU wireframe export reports success, and exit is clean. The
BMP should be in:

```
C:\Users\FrontDesk\Downloads\xenia_canary_windows\cache\hl1_wireframe.bmp
```

**NEXT USER CHECK:** open that BMP and send a screenshot. This checks
the actual image appearance of the entire Black Mesa map. The BMP
was not uploaded in this user turn; **do not claim visual quality of
its layout beyond what the ASCII preview reveals**.

**REMAINING GAP:** No real-time screen display, textured GPU renderer,
player/controller/camera, physics, weapons, enemies or game logic yet.
This is an offline CPU geometry visualization produced by an Xbox
360 executable in Xenia. Xbox Series S Xenia UWP still untested.
No Linux or Debian installed on the user's PC, and proprietary
derived map mesh remains private, outside this public repo.

---

# First actual Black Mesa level wireframe test

## 2026-10-28: confirmed Xenia wireframe BMP output

The user provided a **screenshot** of `hl1_wireframe.bmp`, opened
in Windows Live Photo Gallery. It clearly displays the expected
single centered triangle outline against a dark background.

This completes the synthetic one-triangle CPU rendering milestone:

1. A GitHub-hosted Ubuntu runner compiles native Xbox 360 PPC XEX2.
2. The user loads it in Windows Xenia Canary without Debian locally.
3. The title decodes a synthetic H360 v1 mesh and CPU-rasterizes
   an offline 640x360 wireframe BMP.
4. The output is saved at the writable mount:
   `C:\Users\FrontDesk\Downloads\xenia_canary_windows\cache\hl1_wireframe.bmp`.
5. Windows Live Photo Gallery can open the BMP, as observed.

**This is NOT live GPU graphics in Xenia and NOT playable Half-Life.**

## Next user test: actual c0a0 GoldSrc world geometry

From the original user-supplied Half-LifeX ISO, we previously
exported c0a0 static worldspawn and encoded it as H360 v1.
The exact generated mesh was retained in our prior privately
delivered `HalfLife1_Xbox360_Remaster_AssetKit_v0.3.zip`.

The binary was re-read and **fully validated locally**:

| Item | Verified value |
|---|---|
| H360 magic / version | H360 / v1 |
| Bytes | 298,310 |
| Material groups | 46 |
| Vertices | 10,378 |
| Triangle indices | 23,499 |
| Triangles | 7,833 |
| SHA-256 | `0794fce768b7c0cb539a988c46c63210649f5bbbdf673124c49e047635c05e3f` |

A private, user-only ZIP was created:
`HalfLife1_BlackMesa_c0a0_H360_for_Xenia.zip`.

The ZIP contains the original game-derived static scene as
`assets/hl1_c0a0_world.h360`, `mesh_manifest.json`, and
`README_NEXT_TEST.txt`. **Never commit this ZIP or asset to the
public repository**; game-derived geometry comes from the user's
own copy of Half-Life.

### Windows test steps (no new installations)

1. Close Windows Live Photo Gallery if it has the old BMP open.
2. Extract the ZIP.
3. Replace the synthetic one-triangle
   `assets/hl1_c0a0_world.h360` beside the **working** XEX with
   the new 298,310-byte map mesh.
4. Launch the same native Xbox 360 probe in Xenia Canary.
5. The game title may exit to dashboard by design.
6. Check `cache/hl1_remaster_probe.log`. Expect
   `FULL_MESH_PASS: 46 groups 10378 vertices 7833 triangles`
   **if** the native map load succeeds. Those are *expected*
   output values; the real map has NOT yet been user-tested in Xenia.
7. Re-open `cache/hl1_wireframe.bmp`, and verify its file
   timestamp changed, rather than redisplaying the old triangle.
   The image should show the original level's complex static layout.
8. Share a screenshot/log for verification and next step.

The CPU wireframe map lacks textured materials, moving brushes,
NPCs, collisions, interactive camera, game logic and GPU output.
Next after this will be a true map visualization milestone and
research into Xenia-compatible on-screen presentation.
