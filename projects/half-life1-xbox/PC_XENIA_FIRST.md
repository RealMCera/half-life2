# Half-Life 1 Xbox 360 remaster — PC Xenia first

**User's current situation:** Windows PC with Xenia already installed; away from
the Xbox Series S. We can continue with all CPU-side assets, native Xbox 360
build experiments and Xenia Canary boot checks on Windows. Nothing requires
touching the Series S yet.

## First practical action on the Windows computer

Download and run the project's read-only PC readiness checker
\`Check_HalfLife_Xenia_PC.ps1\` (distributed separately as an attachment in
the ChatGPT conversation). It checks whether Windows can find Xenia,
Git, Python, WSL2/Debian, disk space, and optionally whether a given
file has an XEX2 header. It does **not** install software, build an XEX,
launch an emulator or access the Xbox.

From Windows PowerShell in the folder where you saved the script:

    powershell -NoProfile -File .\Check_HalfLife_Xenia_PC.ps1

If the Xenia executable is in a different folder:

    powershell -NoProfile -File .\Check_HalfLife_Xenia_PC.ps1 -XeniaPath "C:\path\to\xenia_canary.exe"

The report is saved as \`HL1_Xenia_PC_Check.txt\` in the current folder.
Review it and share the diagnostic text to choose the next actual build
command. It does not contain any real game files.

## SDK facts that change the emulator roadmap

The current upstream
[xenosworks/xsdk Getting Started](https://github.com/xenosworks/xsdk/blob/main/docs/GETTING_STARTED.md)
requires a WSL2/Debian cross-toolchain build, with about 10 GB of
available WSL build disk space. The meaningful order is:

1. Verify Windows Xenia Canary and Debian WSL2.
2. Build xsdk's PowerPC toolchain, glslang-xenon, xsdk libraries and samples
   following upstream docs (not included in this repo).
3. Try xsdk's unmodified \`hello.xex\` in Windows Xenia Canary and inspect
   \`xenia.log\` and Xenia's cached per-title \`hello.log\`.
4. Address and document display-control compatibility BEFORE claiming
   any Xenia-rendered graphics. The upstream guide specifically says
   Xenia lacks display-controller support and its
   \`xsdk_splash_show\` / \`xsdk_present_open\` calls return
   \`0xC00000BB\`: graphics samples can exit before displaying content.
   The initial verified result may only be a console-independent
   *startup and logging smoke test*.
5. After a real graphics path exists, load the custom H360 first-level
   static mesh, then add gameplay systems, original Half-Life triggers,
   enemies and script logic.
6. Test the identical XEX in Series S Xenia UWP **later**. Its support
   for custom homebrew XEX remains unverified.

## No false readiness claims

- Having Xenia on a PC does not mean our remaster runs yet.
- The Half-LifeX source disc's original Xbox \`.xbe\` cannot run in Xenia.
- Previous v0.3/v0.4 asset packs do **not** contain a playable XEX.
- A functioning XEX boot is not proof of graphics or gameplay.
- Both emulator platforms have different compatibility limits from
  actual 360 hardware.

## Actual build guide (once readiness confirmed)

Use upstream commands, not guessed shortcuts:
https://github.com/xenosworks/xsdk/blob/main/docs/GETTING_STARTED.md

Basic PC command for a *genuine precompiled* sample, after xsdk builds:

    & "C:\path\to\xenia_canary.exe" "C:\path\to\hello.xex"

PowerShell's path placeholders MUST be replaced with real paths.
If Xenia shows nothing but writes logs or errors, capture those logs;
don't conclude the remaster rendered. Do not rename the original
Half-LifeX \`default.xbe\` to \`.xex\`.
