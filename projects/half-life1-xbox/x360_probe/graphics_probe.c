/*
 * Xbox 360 native XEX display compatibility probe for Windows Xenia.
 *
 * Separate from the VERIFIED CPU-BMP asset renderer.  Does not require
 * any Half-Life assets, hardware, WSL, or local toolchains.
 * Its sole purpose: record exactly where an on-screen frame fails in
 * this user's Xenia Canary version before building a live 3D renderer.
 *
 * Xenos/xsdk calls modeled after xenosworks/xsdk samples/template/main.c.
 * The user's first LUT diagnostic succeeded past present_begin, but
 * a submitted frame is not proof a display flipped on-screen. This
 * variant displays three rotating solid colors for up to 180 frames,
 * records vblank/flip statistics, and then terminates. The diagnostic
 * must not be mistaken for a working game or production renderer.
 */
#include <stdint.h>
#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/present.h"
#include "xsdk/thread.h"

int main(void)
{
    xsdk_log_write("HL1_DISPLAY: BOOT_BEGIN");
#ifdef HL1_EXPERIMENTAL_LUT_PROBE
    xsdk_log_write("HL1_DISPLAY: EXPERIMENTAL_LUT_BYPASS_PROBE");
    xsdk_log_write("HL1_DISPLAY: LUT_TIMEOUT_BYPASSED_IN_SDK_DIAGNOSTIC_ONLY");
#else
    xsdk_log_write("HL1_DISPLAY: BASELINE_UNMODIFIED_SDK");
#endif
    xsdk_log_write("HL1_DISPLAY: bounded 180-frame 720p screen-visibility test");
    xsdk_log_set_dbgprint(true);

    NTSTATUS status = xsdk_gpu_open();
    xsdk_log_write("HL1_DISPLAY: gpu_open = 0x%08X", (unsigned)status);
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("HL1_DISPLAY: RESULT_GPU_UNAVAILABLE");
        xsdk_log_write("HL1_DISPLAY: BOOT_COMPLETE");
        return 1;
    }

    uint32_t width = 0, height = 0;
    status = xsdk_present_open(XSDK_DISPLAY_720P, 2U, 1U, &width, &height);
    xsdk_log_write("HL1_DISPLAY: present_open = 0x%08X", (unsigned)status);
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("HL1_DISPLAY: RESULT_DISPLAY_UNAVAILABLE");
        xsdk_log_write("HL1_DISPLAY: Xenia likely needs a different display path");
        (void)xsdk_gpu_close();
        xsdk_log_write("HL1_DISPLAY: BOOT_COMPLETE");
        return 2;
    }

    xsdk_log_write("HL1_DISPLAY: display_dimensions = %ux%u",
                   (unsigned)width, (unsigned)height);

    /* The source SDK's resolve copies the eDRAM content of the
       PREVIOUS clear, then clears eDRAM for the NEXT frame.
       Display multiple frames rather than immediately exiting so
       the user can actually see a frame if the vblank path works.
       All loops are finite, including present_begin timeouts. */
    static const uint32_t colors[3] = {
        0xFF176B8FU,  /* blue-green */
        0xFF5E2C91U,  /* violet */
        0xFFB96A19U,  /* amber */
    };
    uint32_t frames_submitted = 0;
    status = STATUS_SUCCESS;
    for (uint32_t frame = 0; frame < 180U; frame++) {
        uint32_t target = 0U;
        NTSTATUS begin_status = xsdk_present_begin(&target, 2000U);
        if (!NT_SUCCESS(begin_status)) {
            xsdk_log_write("HL1_DISPLAY: FRAME_LOOP_BLOCKED: frame=%u code=0x%08X",
                           (unsigned)frame, (unsigned)begin_status);
            status = begin_status;
            break;
        }

        /* Clear color changes at frames 0, 60, and 120. */
        xsdk_gpu_resolve(target, width, height, colors[frame / 60U]);
        xsdk_present_end();
        frames_submitted++;
        if (frame == 0U || frame == 1U || frame == 60U ||
            frame == 120U || frame == 179U) {
            xsdk_log_write("HL1_DISPLAY: FRAME_SUBMITTED: %u", (unsigned)frame);
        }
    }

    XsdkPresentStats stats;
    xsdk_present_stats(&stats);
    xsdk_log_write("HL1_DISPLAY: FRAMES_SUBMITTED_TOTAL = %u", (unsigned)frames_submitted);
    xsdk_log_write("HL1_DISPLAY: VBLANKS = %u FLIPS = %u MISSED = %u",
                   (unsigned)stats.vblanks, (unsigned)stats.flips,
                   (unsigned)stats.missed);
    xsdk_log_write("HL1_DISPLAY: LAST_FLIP_VBLANK = %u",
                   (unsigned)stats.last_flip_vblank);

    if (stats.flips > 0U) {
        xsdk_log_write("HL1_DISPLAY: EMULATED_FLIPS_REPORTED (visible output still requires observation)");
    } else {
        xsdk_log_write("HL1_DISPLAY: NO_FLIPS_REPORTED (screen visibility unverified)");
    }

    /* A bounded hold to let the user see a final frame on screen.
       Does not prevent the usual exit-to-dashboard when finished. */
    if (frames_submitted >= 2U) {
        xsdk_log_write("HL1_DISPLAY: HOLDING_LAST_FRAME_1500_MS");
        (void)xsdk_thread_sleep(1500U);
    }

    xsdk_present_close();
    (void)xsdk_gpu_close();
    xsdk_log_write("HL1_DISPLAY: BOOT_COMPLETE");
    return NT_SUCCESS(status) ? 0 : 3;
}
