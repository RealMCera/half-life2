# Xenia emulator-first test target — Half-Life 1 Xbox 360 Remaster

**User choice (October 28, 2026):** Run the Xbox 360-targeted Half-Life 1
remaster in an **Xbox 360 emulator**, not on an actual Xbox 360. Do not ask
for RGH/JTAG; that only applies to running unsigned XEX on physical consoles.

## Development and output formats

- Target binary remains an Xbox 360 PowerPC **XEX2** executable, normally
  named `default.xex`. The original Half-LifeX `default.xbe` is an
  **original Xbox executable** and cannot be launched by Xenia.
- Public [xenosworks/xsdk](https://github.com/xenosworks/xsdk) provides
  GCC PowerPC + `elf2xex.py` tools and 360 kernel/GPU services.
  Its [getting started guide](https://github.com/xenosworks/xsdk/blob/main/docs/GETTING_STARTED.md)
  explicitly says Xenia Canary can load a generated XEX for startup
  smoke testing, but Xenia has kernel/GPU accuracy differences.
- Current H360 v1 files are just original CPU-side mesh staging assets.
  They are **not executable XEX files**. We must still implement and
  build a native renderer and eventually the rest of the game.

## Preferred quick iteration: Windows PC + Xenia Canary

Use an official [Xenia Canary build](https://github.com/xenia-canary/xenia-canary)
on the Windows PC for quick iteration, even if the eventual playback
device will be an Xbox Series S running Xenia UWP.

1. Install/configure Xenia Canary from its official repository.
2. Follow xsdk's getting-started instructions to compile a sample XEX.
   The toolchain build needs Linux/WSL Debian, and its own requirements.
   The user does NOT have to own a modded 360 for this emulator-only step.
3. Put the generated XEX and its required content in the same testing
   directory and run it in PowerShell:

       & "C:\Tools\Xenia\xenia_canary.exe" "C:\HL1-X360\build\default.xex"

   Paths are examples — replace with actual Windows locations.
   When using xsdk's own hello example, use its generated hello.xex.
4. Check emulator output, Xenia's `xenia.log`, and xsdk's per-title
   log under Xenia's `cache` directory (per xsdk documentation).
   Xenia Canary can remain open after the title exits; closing the
   window is normal.
5. Keep per-test observations: launch/crash, window/render, controller,
   map load, GPU errors, screenshots, elapsed time. Do not infer
   "playable" from merely loading a .xex or passing unit tests.

## If the user means Xenia on Xbox Series S

The user has previously discussed Xenia in Xbox Series S Dev Mode.
There is a **third-party Xenia UWP** port for Series S|X; it is not
maintained by the official desktop Xenia team. A public Xbox Dev Mode
emulator catalog lists the UWP port:
https://emulationrevival.github.io/xbox-dev-mode/emulators.html

This is the SECOND test route, after PC Canary. We do **not** currently
know whether the UWP build loads our future xsdk homebrew XEX or
supports all needed API/rendering features. Its installation and
storage/UI steps are separate from the Windows command above. Do not
promise XEX compatibility without testing that exact build.

## Non-goals and unresolved work

- No user requirement for a physical 360, RGH/JTAG, hardware FTP or
  dashboard launching.
- No claim that this repo can already compile a complete playable
  Half-Life remaster. The original engine port is still unstarted,
  and no emulator-backed XEX result has been demonstrated.
- No Unreal Engine 5 or Xbox Series native binary — we're targeting
  the Xbox 360 CPU/GPU APIs, so the emulator executes a 360 program.
- No commercial game images or proprietary Xbox SDK binaries in GitHub.

## First acceptance test

**Required before any "playable game" claim:** compile a minimal
Xbox 360 XEX using xsdk, get it to boot in Windows Xenia Canary,
produce a clear rendered frame plus a diagnostic log, then attach
the actual boot/log result to this repository.

Then move toward a triangle, first H360 mesh, and finally a room
of the original Half-Life Black Mesa map with camera/controller.
