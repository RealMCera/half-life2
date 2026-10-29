/*
 * Xbox 360 native XEX display compatibility probe for Windows Xenia.
 *
 * Separate from the VERIFIED CPU-BMP asset renderer.  Does not require
 * any Half-Life assets, hardware, WSL, or local toolchains.
 * Its sole purpose: record exactly where an on-screen frame fails in
 * this user's Xenia Canary version before building a live 3D renderer.
 *
 * Xenos/xsdk calls modeled after xenosworks/xsdk samples/template/main.c.
 * Upstream xsdk docs warn present_open returns STATUS_NOT_SUPPORTED
 * (0xC00000BB) in the Xenia build they tested. This probe may fail to
 * show ANYTHING. A dashboard exit is expected, not a game launch.
 */
#include <stdint.h>
#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/present.h"

int main(void)
{
    xsdk_log_write("HL1_DISPLAY: BOOT_BEGIN");
    xsdk_log_write("HL1_DISPLAY: probe version 1 (single 720p frame)");
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

    /* Single attempt, no persistent render loop; the goal is API
       compatibility, NOT a finished runtime or frame-rate benchmark. */
    uint32_t target = 0U;
    status = xsdk_present_begin(&target, 2000U);
    xsdk_log_write("HL1_DISPLAY: present_begin = 0x%08X", (unsigned)status);
    if (NT_SUCCESS(status)) {
        xsdk_gpu_resolve(target, width, height, 0xFF176B8FU);
        xsdk_present_end();
        xsdk_log_write("HL1_DISPLAY: FRAME_SUBMITTED");
    } else {
        xsdk_log_write("HL1_DISPLAY: RESULT_PRESENT_BEGIN_FAILED");
    }

    xsdk_present_close();
    (void)xsdk_gpu_close();
    xsdk_log_write("HL1_DISPLAY: BOOT_COMPLETE");
    return NT_SUCCESS(status) ? 0 : 3;
}
