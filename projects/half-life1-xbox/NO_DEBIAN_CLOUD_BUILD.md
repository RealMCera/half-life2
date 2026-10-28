# No local Debian: build the Xbox 360 XEX on GitHub

**Confirmed user preference:** Do not install Debian/WSL/Linux on the Windows PC.
The user already has Xenia Canary at:

    C:\Users\FrontDesk\Downloads\xenia_canary_windows

That is a folder location, not a verified executable filename. Find the
actual .exe before launching it.

## Cloud workflow

We added [the GitHub Actions workflow](../../.github/workflows/half-life1-x360-xex.yml).
It uses a temporary **GitHub-hosted Ubuntu 24.04** runner to install build
dependencies, compile the public **xenosworks/xsdk** Xbox 360 PowerPC
cross-compiler and runtime library, and build this repository's small C17
Half-Life 1 XEX startup probe. Linux runs on **GitHub's computer**, not
the user's PC, so the user needs to install **nothing**.

[Cloud builds on GitHub](https://github.com/RealMCera/half-life2/actions)

The workflow is intentionally constrained to a push on its preparation
branch when the workflow or native probe code changes; it also has
workflow_dispatch for manual operation once the workflow is present on
the repo's default branch. It does not automatically build the game
from the original Valve disc, and it never uploads copyrighted game
content.

The current experiment needs a working network-enabled hosted build
runner. The **upstream toolchain was documented with Debian**; Ubuntu
cloud support is reasonable but NOT YET proven. Check the run conclusion
rather than assuming success. Actions minutes and storage are subject
to GitHub's account limits.

## If the workflow succeeds

1. Open the build run's page from GitHub Actions.
2. Under **Artifacts**, download
   `HalfLife1-X360-XEX-StartupProbe`. GitHub may require sign-in.
3. Extract its ZIP on Windows. It should contain
   `hl1_remaster_probe.xex`, `SHA256SUMS.txt`,
   `xsdk-upstream-commit.txt`, and `README.txt`.
4. Locate the Xenia executable, e.g. using PowerShell:

       Get-ChildItem "C:\Users\FrontDesk\Downloads\xenia_canary_windows" -Filter "*.exe" -Recurse | Select-Object FullName

5. Launch Xenia with the actual paths, for example:

       & "C:\Users\FrontDesk\Downloads\xenia_canary_windows\xenia_canary.exe" "C:\path\to\hl1_remaster_probe.xex"

   Use the *actual* executable path from step 4; its name might differ.
6. Look in Xenia's cache for `hl1_remaster_probe.log` and
   `xenia.log`. Search for `HL1_PROBE`,
   `XBOX_360_BOOT_BEGIN`, and `XBOX_360_BOOT_COMPLETE`.
   Report exact errors if launch fails.

## What the result means

A successful cloud compilation verifies a .xex header only.
It does NOT prove the XEX boots in Xenia. A successful Xenia boot/log
does NOT prove the renderer works. At present xsdk's own guide says
Xenia does not support its current display-controller functions.
The first goal is the startup/asset-header diagnostic log, not a
visible game. Xbox Series S UWP remains a separate, unverified
emulator compatibility test.

We are not using a native Windows cross compiler, and **there is
no requirement to install Debian locally**.
