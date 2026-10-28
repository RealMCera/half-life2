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
