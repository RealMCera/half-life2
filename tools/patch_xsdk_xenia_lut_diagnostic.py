"""Create an isolated *Xenia-only diagnostic* libxsdk display variant.

Runs ONLY on an ephemeral GitHub Actions checkout after building the
untouched baseline xsdk library and baseline display probe. This is
NOT a graphics driver and NOT a production or hardware workaround.

The current xsdk src/display.c explicitly times out waiting for LUT
autofill. Xenia Canary can leave the completion bit unset because the
emulated display controller doesn't implement that process. For one
diagnostic XEX only, continue past this specific STATUS_IO_TIMEOUT
and see what API fails next. Never mask other errors.

Usage:
  python3 tools/patch_xsdk_xenia_lut_diagnostic.py /tmp/hl1-xsdk
"""
import argparse
from pathlib import Path
import sys

ORIGINAL = '''\
\tconst NTSTATUS status = fill_lut();
\tif (!NT_SUCCESS(status)) {
\t\txsdk_log_write("display: the LUT autofill didn't finish: 0x%08X", (unsigned)status);
\t\treturn status;
\t}
'''

REPLACEMENT = '''\
\tNTSTATUS status = fill_lut();
\tif (status == STATUS_IO_TIMEOUT) {
\t\t// Xenia-only diagnostic: the emulator's LUT completion bit did not set.
\t\t// This does NOT implement LUT behavior or establish that presentation works.
\t\txsdk_log_write("HL1_DISPLAY_EXPERIMENT: ignoring LUT timeout for ONE diagnostic test");
\t\tstatus = STATUS_SUCCESS;
\t}
\tif (!NT_SUCCESS(status)) {
\t\txsdk_log_write("display: the LUT autofill didn't finish: 0x%08X", (unsigned)status);
\t\treturn status;
\t}
'''


def patch(root):
    path = Path(root) / "src" / "display.c"
    data = path.read_text(encoding="utf-8")
    count = data.count(ORIGINAL)
    if count != 1:
        raise ValueError(
            "Expected exactly one known xsdk LUT block, found %d; "
            "upstream changed; refuse to patch automatically" % count
        )
    path.write_text(data.replace(ORIGINAL, REPLACEMENT), encoding="utf-8")
    return path


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("xsdk_root")
    args = p.parse_args(argv)
    try:
        path = patch(args.xsdk_root)
        print("Xenia-only diagnostic LUT patch applied:", path)
        print("DO NOT deploy this experimental libxsdk on physical Xbox 360 hardware.")
        return 0
    except (OSError, ValueError) as err:
        p.exit(2, "Patch aborted: %s\n" % err)


if __name__ == "__main__":
    sys.exit(main())
