/*
 * Xenia-only test of native Xbox 360 GPU rasterization.
 * Uses the temporary LUT-timeout diagnostic SDK, not real-HW-safe.
 * Produces a single interpolated RGB triangle for up to 180 frames,
 * and reports GPU draw, submit and emulated flip statistics.
 * Nothing here is a playable or navigable Half-Life game.
 */
#include <stdint.h>
#include <string.h>

#include "hl1_triangle_probe_shaders.h"
#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/present.h"
#include "xsdk/shader.h"
#include "xsdk/thread.h"

enum { VERTEX_WORDS = 15, VERTEX_BYTES = 60, MAX_FRAMES = 180 };

static const XsdkVertexElement layout[] = {
    { .usage = XSDK_USAGE_POSITION, .format = XSDK_VERTEX_FLOAT2, .offset_bytes = 0 },
    { .usage = XSDK_USAGE_COLOR, .format = XSDK_VERTEX_FLOAT3, .offset_bytes = 8 }
};

/* Three corners: x, y, red, green, blue. */
static const float data[15] = {
     0.00f,  0.70f,  1.0f, 0.0f, 0.0f,
    -0.80f, -0.70f,  0.0f, 1.0f, 0.0f,
     0.80f, -0.70f,  0.0f, 0.0f, 1.0f
};

int main(void)
{
    xsdk_log_write("HL1_TRIANGLE: BOOT_BEGIN");
    xsdk_log_write("HL1_TRIANGLE: EMULATOR_ONLY_LUT_DIAGNOSTIC");
    NTSTATUS result = xsdk_gpu_open();
    xsdk_log_write("HL1_TRIANGLE: gpu_open = 0x%08X", (unsigned)result);
    if (!NT_SUCCESS(result)) return 1;

    XsdkShader vertex = {0}, pixel = {0};
    volatile uint32_t *gpu_vertices = 0;
    int exit_code = 2;
    uint32_t submitted = 0, width = 0, height = 0;
    int opened = 0;

    result = xsdk_shader_load(&vertex, hl1_triangle_vert.container,
                              hl1_triangle_vert.container_bytes);
    if (NT_SUCCESS(result))
        result = xsdk_shader_load(&pixel, hl1_triangle_frag.container,
                                  hl1_triangle_frag.container_bytes);
    if (NT_SUCCESS(result))
        result = xsdk_shader_set_vertex_layout(&vertex, layout, 2,
                                                (const uint32_t[]){VERTEX_BYTES / 3});
    xsdk_log_write("HL1_TRIANGLE: shaders_load = 0x%08X", (unsigned)result);
    if (!NT_SUCCESS(result)) goto clean;

    gpu_vertices = xsdk_gpu_alloc(VERTEX_BYTES);
    if (!gpu_vertices) {
        xsdk_log_write("HL1_TRIANGLE: VERTEX_MEMORY_FAIL");
        goto clean;
    }
    uint32_t words[VERTEX_WORDS];
    memcpy(words, data, sizeof data);
    for (uint32_t i = 0; i < VERTEX_WORDS; ++i) gpu_vertices[i] = words[i];
    xsdk_gpu_invalidate(xsdk_gpu_address(gpu_vertices), VERTEX_BYTES);

    result = xsdk_present_open(XSDK_DISPLAY_720P, 2, 1, &width, &height);
    xsdk_log_write("HL1_TRIANGLE: present_open = 0x%08X (%ux%u)",
                   (unsigned)result, (unsigned)width, (unsigned)height);
    if (!NT_SUCCESS(result)) goto clean;
    opened = 1;
    exit_code = 0;

    for (uint32_t i = 0; i < MAX_FRAMES; ++i) {
        uint32_t address = 0;
        result = xsdk_present_begin(&address, 2000);
        if (!NT_SUCCESS(result)) {
            xsdk_log_write("HL1_TRIANGLE: present_begin failed frame=%u status=0x%08X",
                           (unsigned)i, (unsigned)result);
            exit_code = 3;
            break;
        }
        xsdk_gpu_set_target(width, height);
        xsdk_gpu_set_shaders(&vertex, &pixel);
        xsdk_built_shader_set_literals(&hl1_triangle_vert);
        xsdk_built_shader_set_literals(&hl1_triangle_frag);
        xsdk_gpu_set_vertices(0, xsdk_gpu_address(gpu_vertices), VERTEX_BYTES);
        xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_LIST, 3);
        xsdk_gpu_resolve(address, width, height, 0xFF101927U);
        xsdk_present_end();
        ++submitted;
        if (i == 1 || i == 60 || i == 179)
            xsdk_log_write("HL1_TRIANGLE: GPU_DRAW_SUBMITTED frame=%u", (unsigned)i);
    }

    XsdkPresentStats stats = {0};
    xsdk_present_stats(&stats);
    xsdk_log_write("HL1_TRIANGLE: SUBMITTED=%u VBLANKS=%u FLIPS=%u MISSED=%u",
                   (unsigned)submitted, (unsigned)stats.vblanks,
                   (unsigned)stats.flips, (unsigned)stats.missed);
    if (submitted)
        (void)xsdk_thread_sleep(1500);
    xsdk_log_write("HL1_TRIANGLE: VISIBILITY_REQUIRES_USER_CONFIRMATION");

clean:
    if (opened) xsdk_present_close();
    if (gpu_vertices) xsdk_gpu_free(gpu_vertices);
    xsdk_shader_free(&pixel);
    xsdk_shader_free(&vertex);
    result = xsdk_gpu_close();
    xsdk_log_write("HL1_TRIANGLE: gpu_close = 0x%08X", (unsigned)result);
    xsdk_log_write("HL1_TRIANGLE: BOOT_COMPLETE");
    return exit_code;
}
