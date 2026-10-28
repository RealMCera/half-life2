# Windows Xenia — native Xbox 360 startup confirmed (2026-10-28)

**CONFIRMED BY AN ACTUAL USER-RUN EMULATOR LOG:** the cloud-compiled
`hl1_remaster_probe.xex` boots in Windows **Xenia Canary** and exits
normally, without installing Debian on the user's Windows PC.

Exact relevant lines from the user's own `hl1_remaster_probe.log`:

```text
log: cache:\hl1_remaster_probe.log
log: named after \Device\Harddisk0\Partition1\hl1_remaster_probe.xex
log: no net_log.txt beside the .xex, so not over the network
HL1_PROBE: XBOX_360_BOOT_BEGIN
HL1_PROBE: original Half-Life remaster bootstrap (log only)
open: NtCreateFile(game:\assets\hl1_c0a0_world.h360) failed: 0xC000003A
open: NtCreateFile(game:\assets\hl1_c0a0_world.h360) failed: 0xC000003A
HL1_PROBE: NO_SCENE_ASSET (optional for first XEX test)
HL1_PROBE: XBOX_360_BOOT_COMPLETE
exit: status 0
```

The on-screen message **Game title requested exit to dashboard** was
the intended termination of our log-only probe. The two failed asset
lookups are simply a missing optional scene file; their NTSTATUS value
`0xC000003A` means `STATUS_OBJECT_PATH_NOT_FOUND`.

### Proven

- A **GitHub Actions Linux runner** successfully built the public
  `xenosworks/xsdk` toolchain, runtime and our original PowerPC XEX2.
- Xenia Canary loaded our test XEX and executed its C17 startup.
- Our XEX wrote its log, finished normally, and returned 0.
- Nothing required installing Debian, WSL distros, or a cross-compiler
  on the user's Windows machine.

Build: https://github.com/RealMCera/half-life2/actions/runs/40745594688

### NOT proven

- Loading real H360 scene contents.
- Drawing anything; currently upstream xsdk present/display APIs
  are incompatible with the relevant Xenia Canary behavior.
- A playable Half-Life build, dynamic brush entities, physics,
  NPCs, save/load, audio, controller logic.
- Xbox Series S Xenia UWP compatibility, pending console availability.

## Next test: original synthetic scene asset

The ChatGPT response for this milestone provides a small private,
generated test archive `HalfLife1_Xenia_H360_Asset_SmokeTest_v0.6.zip`.
Its file `assets/hl1_c0a0_world.h360` is entirely ORIGINAL
placeholder geometry: exactly **one triangle, three vertices and one
material group**, not Valve / Half-Life game content. It uses the
same H360 v1 binary layout as `tools/half_life1_x360_pack.py`.

Unzip the `assets/` directory **beside the working
`hl1_remaster_probe.xex`**:

```text
your-test-folder/
  hl1_remaster_probe.xex
  assets/
    hl1_c0a0_world.h360
```

Reopen the same XEX in Xenia Canary. If the relative / `game:`
mount works, the log should now include:

```text
HL1_PROBE: H360 header verified: 1 groups 3 vertices 1 triangles
HL1_PROBE: ASSET_HEADER_PASS
HL1_PROBE: XBOX_360_BOOT_COMPLETE
exit: status 0
```

This is **asset-header I/O smoke testing only, not rendering**.
If `NO_SCENE_ASSET` persists, first check the assets folder layout and
XEX file location. Don't assume the emulator is broken.
