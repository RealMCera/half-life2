# ACTUAL XENIA LOG: first display probe reached LUT timeout

The user ran the cloud-built native 360 `hl1_display_probe.xex`
in Windows Xenia Canary and supplied this exact log:

```text
HL1_DISPLAY: BOOT_BEGIN
HL1_DISPLAY: probe version 1 (single 720p frame)
HL1_DISPLAY: gpu_open = 0x00000000
display: the LUT autofill didn't finish: 0xC00000B5
HL1_DISPLAY: present_open = 0xC00000B5
HL1_DISPLAY: RESULT_DISPLAY_UNAVAILABLE
HL1_DISPLAY: Xenia likely needs a different display path
HL1_DISPLAY: BOOT_COMPLETE
exit: status 2
```

`0xC00000B5` means `STATUS_IO_TIMEOUT`, **not**
`STATUS_NOT_SUPPORTED`. Inspection of upstream
[`src/display.c`](https://github.com/xenosworks/xsdk/blob/main/src/display.c)
showed that `fill_lut()` writes a display-controller autofill
command and polls bit 1 for completion, sleeping 1 ms for at most
100 ms. In the user's emulator, the bit did not indicate done.
**GPU initialization DID succeed**; the display setup did not.
No actual rendered frame was demonstrated.

## Second diagnostic — isolated skip of one failed SDK wait

We added:
- `tools/patch_xsdk_xenia_lut_diagnostic.py`, which alters ONLY
  the *disposable cloud runner* checkout of upstream xsdk, after
  a baseline XEX has already been built.
- Fail-closed `tests/test_xenia_lut_diagnostic_patch.py`, which
  refuses to apply the patch if upstream code differs.
- A **second** CMake build of the same simple one-frame test,
  linked to the temporary patched libxsdk, uploaded as
  `hl1_display_lut_probe.xex`.
- The artifact still includes the **unchanged**
  `hl1_display_probe.xex` and `hl1_remaster_probe.xex`.

**This is a diagnostic, NOT a supported emulator fix.** It only
suppresses the specific LUT `STATUS_IO_TIMEOUT` to identify whether
the next display API works. The LUT hardware behavior is NOT actually
emulated, so the test may still display nothing, produce a GPU error,
or exit. It must NEVER be used on a real Xbox 360.
We have not tested the experimental XEX in the user's emulator yet.

### How to run it after a NEW successful GitHub build

1. Under [GitHub Actions](https://github.com/RealMCera/half-life2/actions),
   download **the newest successful** `HalfLife1-X360-XEX-StartupProbe`
   artifact that includes `hl1_display_lut_probe.xex`.
2. Extract the three XEX files. The new display experiment needs
   NO `assets` folder. Keep your existing working shaded-map
   viewer and map folder untouched.
3. Open `hl1_display_lut_probe.xex` in Windows Xenia Canary.
   A normal dashboard exit or no visible frame is still possible.
4. Inspect **its different title log**:

```powershell
Get-Content "C:\Users\FrontDesk\Downloads\xenia_canary_windows\cache\hl1_display_lut_probe.log" -Tail 40
```

5. Confirm that the log includes
   `EXPERIMENTAL_LUT_BYPASS_PROBE` and
   `HL1_DISPLAY_EXPERIMENT: ignoring LUT timeout` and report
   the new `present_open` status plus any visible frame.
   If the emulator crashes or hangs, terminate it; do not change
   emulator/system configuration to force the experiment.

No Debian/WSL installation on the user's Windows PC is needed.

---

# Xenia graphics display compatibility experiment

**Why this exists:** The user's real Windows Xenia Canary has now
produced 3 visually confirmed offline Black Mesa c0a0 images:
top-down wireframe, angled wireframe, and solid shaded surfaces.
Those are written to `cache:` by CPU rasterization. They are
**NOT** real-time graphics presented in the Xenia window.

Before implementing interactive first-person rendering, determine
whether the installed Windows Xenia Canary can accept xsdk's
standard Xbox 360 display-controller/present APIs.

## New Xbox 360 diagnostic XEX

Source: [`x360_probe/graphics_probe.c`](x360_probe/graphics_probe.c).

- Native PowerPC Xbox 360 `hl1_display_probe.xex` via
  [xsdk](https://github.com/xenosworks/xsdk) and GitHub Actions.
- No Half-Life ISO, map, textures, or assets necessary.
- Calls `xsdk_gpu_open()`, then `xsdk_present_open(720p, 2, 1, ...)`.
- If allowed, attempts **one** frame submission with solid blue-green
  background and exits immediately. **A momentary flash at most;
  do not expect an interactive game, permanent window, or map.**
- Logs all API status codes in the title log
  `cache/hl1_display_probe.log`; it also enables debugger output
  in Xenia's `xenia.log`.
- The upstream xsdk
  [GETTING_STARTED.md](https://github.com/xenosworks/xsdk/blob/main/docs/GETTING_STARTED.md)
  says `xsdk_present_open` currently returns `0xC00000BB`
  (`STATUS_NOT_SUPPORTED`) in the Xenia version tested by its
  authors. If this happens here too, **it is a documented emulator
  compatibility limitation**, not failure to compile.

## How to run

1. Download the **latest successful** cloud build from
   https://github.com/RealMCera/half-life2/actions.
2. Under Artifacts, download
   `HalfLife1-X360-XEX-StartupProbe`. The ZIP includes
   both `hl1_remaster_probe.xex` (existing geometry viewer) and
   `hl1_display_probe.xex` (new display diagnostic).
3. Extract to a convenient Windows folder. The display probe needs
   NO `assets` directory.
4. Launch **`hl1_display_probe.xex`**, not
   `hl1_remaster_probe.xex`, from Xenia Canary (File > Open).
5. The XEX may exit to dashboard. That is normal.
6. Open the display title log with PowerShell:

```powershell
Get-Content "C:\Users\FrontDesk\Downloads\xenia_canary_windows\cache\hl1_display_probe.log" -Tail 30
```

Look for:

```text
HL1_DISPLAY: BOOT_BEGIN
HL1_DISPLAY: gpu_open = 0x........
HL1_DISPLAY: present_open = 0x........
HL1_DISPLAY: RESULT_DISPLAY_UNAVAILABLE
HL1_DISPLAY: BOOT_COMPLETE
```

If the status is `0xC00000BB`, the next code milestone is finding
or implementing an **emulator-supported presentation path** or
considering a different renderer backend/SDK, rather than building
more features on top of the unsupported call.

**Do not claim live graphics support just because `present_open`
or the cloud compile succeeds**. Verify a visible frame and logs.

## Important scope

- No local Debian or WSL installation.
- Do not commit user's derived game asset files publicly.
- Desktop Xenia Canary is the immediate test; Series S Xenia UWP
  remains untested and may behave differently.
- The full GoldSrc engine, story/gameplay, NPCs, triggers and
  physics are NOT ported yet.
