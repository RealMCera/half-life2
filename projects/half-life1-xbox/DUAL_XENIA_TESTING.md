# Dual Xenia test plan — Half-Life 1 Xbox 360 graphics remaster

The user will test Xbox 360 PowerPC **XEX2** builds using BOTH:
1. **Windows PC Xenia Canary** for compiler iteration, logging and debugging.
2. **Xbox Series S Xenia UWP** (Dev Mode) for second-platform compatibility.

One compiled XEX and identical asset payload must serve both platforms.
The existing original-Xbox Half-LifeX XBE cannot launch in Xenia.

## Current verified status

The source ISO's first Half-Life map has been converted to a CPU-staging
H360 mesh with sample PBR maps. The asset packer and fixture tests run
successfully. **No genuine remaster XEX has been compiled, emulated or
shown rendering yet.** Xenia UWP custom XEX/homebrew compatibility is
unverified. This plan is a testing workflow, not proof of compatibility.

[xenosworks/xsdk getting started](https://github.com/xenosworks/xsdk/blob/main/docs/GETTING_STARTED.md)
describes WSL2/Debian compilation of Xbox 360 homebrew and a
Windows Xenia Canary XEX startup smoke check.
[Xenia's official download site](https://xenia.jp/download/) states
that its Xbox UWP port is not maintained by the official team.
[Third-party Xenia UWP listing](https://emulationrevival.github.io/xbox-dev-mode/emulators.html)
shows builds for Xbox Series S|X.

## Stage an actual built Xbox 360 executable

The actual first task is to compile and test an xsdk **hello.xex**
sample (the upstream SDK must be built first). NEVER rename a
Half-LifeX original Xbox XBE to XEX; these are different platforms.

Windows PowerShell, from this repository's checkout root:

    py -3 -m tools.half_life1_xenia_bundle stage --xex "C:\dev\xsdk\build\samples\hello\hello.xex" --out "local\xenia-tests\hello-001"

When our future remaster XEX supports loading scene assets:

    py -3 -m tools.half_life1_xenia_bundle stage --xex "C:\dev\build\hl1_remaster.xex" --scene "local\x360-remaster\c0a0" --out "local\xenia-tests\remaster-001"

Verify checksums with:

    py -3 -m tools.half_life1_xenia_bundle verify --out "local\xenia-tests\remaster-001"

The tool stages:
    local/xenia-tests/remaster-001/
      payload/default.xex
      payload/assets/hl1_c0a0_world.h360
      payload/assets/material_manifest.json
      payload/assets/x360_scene_report.json
      payload/assets/materials/*.png
      xenia_payload.zip
      manifest.json

The ZIP contains the same payload file paths. Manifest includes
SHA-256 checksums. This generated output can contain original
game-derived mesh data and must stay outside the public GitHub repo.

## Test 1: Windows Xenia Canary

    py -3 -m tools.half_life1_xenia_bundle launch-pc --xenia "C:\Tools\Xenia\xenia_canary.exe" --out "local\xenia-tests\hello-001"

Optional graphics debug flag: --gpu-readback, as described in xsdk's
Xenia notes. It is not a default performance benchmark setting.

Inspect Xenia's xenia.log plus the title log in its cache folder.
Record the exact XEX hash, emulator build, whether rendering occurred,
input behavior, and errors. Launching the process is NOT proof of
successful boot or a playable game.

## Test 2: Xbox Series S Xenia UWP

Only after the same XEX starts on desktop Xenia, transfer payload/
contents to an accessible USB/storage location for the installed
Xenia UWP app in Xbox Dev Mode. Try opening the standalone XEX only
**if the installed UWP frontend supports arbitrary .xex selection**.
This is not yet verified; some UWP frontends may only accept particular
game containers. Do not invent packaging or compatibility claims.
Record the exact UWP app version, attempted file selection,
success/failure, and any errors.

## Test matrix

- [ ] Actual xsdk sample built into XEX2.
- [ ] Windows Xenia Canary runs sample (screenshot and logs).
- [ ] Xenia UWP on Series S can select and launch the *same* XEX.
- [ ] Our native Xbox 360 renderer displays a triangle.
- [ ] H360 first-map static geometry renders with correct materials.
- [ ] Controller, camera, collision and scripted moving objects function.
- [ ] First playable Half-Life chapter completed and profiled.

RGH/JTAG physical Xbox 360 hardware is not a requirement for either
emulator path. Neither Xenia platform replaces testing real hardware
if actual-console accuracy later matters.
