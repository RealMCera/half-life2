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
