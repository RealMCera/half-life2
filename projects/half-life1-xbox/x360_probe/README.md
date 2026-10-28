# First executable milestone: Windows Xenia log-only probe

We now have an **original C17 Xbox 360 PowerPC boot probe** in this folder.
It is not Half-Life gameplay and it does not display graphics.

The upstream [xenosworks/xsdk GETTING_STARTED.md](https://github.com/xenosworks/xsdk/blob/main/docs/GETTING_STARTED.md)
explains building the xsdk toolchain in Debian WSL2, `~/xsdk-gcc`
compiler, and `~/xsdk/build/libxsdk.a`.

After the upstream SDK and compiler build successfully in WSL2:

```bash
cd ~/xsdk && make -j$(nproc)
# Copy x360_probe/ to ~/hl1-x360-probe in the Debian filesystem.
cmake -S ~/hl1-x360-probe -B ~/hl1-x360-probe/build -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$HOME/xsdk/cmake/xsdk.cmake \
    -DXSDK_TOOLCHAIN=$HOME/xsdk-gcc -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build ~/hl1-x360-probe/build
```

Expected output after a successful actual cross-compile:
`~/hl1-x360-probe/build/hl1_remaster_probe.xex`.

Copy it to Windows and load it with your existing Xenia Canary:

```powershell
& "C:\path\to\xenia_canary.exe" "C:\path\to\hl1_remaster_probe.xex"
```

**Caution:** This source was verified with a local C17 syntax-only compile
using a test stub of the xsdk log API; **it has NOT yet been built with
xsdk, launched by Xenia, or run on Xbox Series S UWP**.

The probe logs `XBOX_360_BOOT_BEGIN` and `XBOX_360_BOOT_COMPLETE` and
optionally validates the first 32 bytes and bounds of `assets/hl1_c0a0_world.h360`.
The latter is the custom staging mesh created by
`tools/half_life1_x360_pack.py`; missing assets do not prevent the
first startup smoke test.

Do not expect a visual image. Upstream xsdk currently documents that
Xenia Canary does not implement certain display-controller functions,
including those used by xsdk's built-in graphics sample. We therefore
start with **boot/log and asset-header verification** before addressing
GPU display compatibility.

For the Windows-only preflight checker, use
[`tools/check_xenia_pc.ps1`](../../../tools/check_xenia_pc.ps1) or the
separate chat download.
