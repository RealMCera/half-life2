"""
Self-check for the loop-head preemption point.

Run: py -3 tools/recomp/test_loop_yield.py

With the guest CPU lock (RECOMP_GUEST_LOCK=1) a guest thread runs guest code
until its next kernel call. A thread that polls a flag another guest thread
sets, with no kernel call in the loop, would therefore hold the CPU forever;
the console's scheduler preempts it on its quantum. The lifter marks every
loop head -- a block some later block jumps back to -- with
RECOMP_LOOP_YIELD(), a per-thread countdown that lets the guest CPU go every
RECOMP_LOOP_QUANTUM iterations (kernel_bridge.c recomp_loop_yield).
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from tools.recomp import config  # noqa: E402
from tools.recomp.translator import FunctionTranslator  # noqa: E402

BASE = 0x00010000
LOOP = BASE                 # mov ecx, 3 / L: dec ecx / jnz L / ret
HEAD = BASE + 5
STRAIGHT = BASE + 0x20      # mov eax, 1 / test eax, eax / jz +1 / inc eax / ret


def _translator():
    image = bytearray(b"\xCC" * 0x100)

    def put(va, data):
        image[va - BASE:va - BASE + len(data)] = data

    put(LOOP, bytes.fromhex("b903000000" "49" "75fd" "c3"))
    put(STRAIGHT, bytes.fromhex("b801000000" "85c0" "7401" "40" "c3"))
    config._install(
        [config.Section(".text", BASE, len(image), 0x0000, len(image), True)],
        entry_point=BASE, kernel_thunk_addr=BASE, origin="loop-yield-test")
    func_db = {
        LOOP: {"start": f"0x{LOOP:08X}", "end": LOOP + 9, "_addr": LOOP, "size": 9},
        STRAIGHT: {"start": f"0x{STRAIGHT:08X}", "end": STRAIGHT + 12,
                   "_addr": STRAIGHT, "size": 12},
    }
    return FunctionTranslator(bytes(image), func_db)


def test_loop_head_gets_one_yield():
    tr = _translator()
    code = tr.translate_function(LOOP, tr.func_db[LOOP])
    assert code.count("RECOMP_LOOP_YIELD();") == 1, code
    head = code.index(f"loc_{HEAD:08X}:")
    assert code.index("RECOMP_LOOP_YIELD();") > head, code
    print("ok  loop_head_gets_one_yield")


def test_forward_branches_get_none():
    tr = _translator()
    code = tr.translate_function(STRAIGHT, tr.func_db[STRAIGHT])
    assert "RECOMP_LOOP_YIELD" not in code, code
    print("ok  forward_branches_get_none")


if __name__ == "__main__":
    test_loop_head_gets_one_yield()
    test_forward_branches_get_none()
