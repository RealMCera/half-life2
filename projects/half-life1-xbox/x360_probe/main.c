/*
 * Half-Life 1 Xbox 360 remaster: log-only XEX bootstrap.
 * This is ORIGINAL code, not the GoldSrc engine or a playable port.
 * xsdk/GCC C17; no display-controller usage (not supported in Xenia).
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "xsdk/log.h"
#include "wireframe.h"

#define H360_HEADER_BYTES 32U
#define H360_MAX_BYTES (32UL * 1024UL * 1024UL)

static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int report_h360(const char *name)
{
    uint8_t header[H360_HEADER_BYTES] = {0};
    FILE *file = fopen(name, "rb");
    if (file == NULL)
        return 0;  /* Optional scene: report a successful boot without it. */

    const size_t got = fread(header, 1, sizeof(header), file);
    if (got != sizeof(header) || memcmp(header, "H360", 4) != 0 ||
        read_be16(header + 4) != 1 || read_be16(header + 6) != 0xFEFFU) {
        xsdk_log_write("HL1_PROBE: invalid H360 scene header");
        fclose(file);
        return -1;
    }

    const uint32_t groups = read_be32(header + 8);
    const uint32_t vertices = read_be32(header + 12);
    const uint32_t indices = read_be32(header + 16);
    const uint32_t group_offset = read_be32(header + 20);
    const uint32_t vertex_offset = read_be32(header + 24);
    const uint32_t index_offset = read_be32(header + 28);

    if (fseek(file, 0, SEEK_END) != 0) {
        xsdk_log_write("HL1_PROBE: scene length unavailable");
        fclose(file);
        return -1;
    }
    const long length = ftell(file);
    fclose(file);
    if (length < 0 || (unsigned long)length > H360_MAX_BYTES ||
        groups == 0 || groups > 4096U || vertices == 0 ||
        indices == 0 || (indices % 3U) != 0 ||
        group_offset < H360_HEADER_BYTES ||
        (uint64_t)group_offset + (uint64_t)groups * 48U > vertex_offset ||
        (uint64_t)vertex_offset + (uint64_t)vertices * 24U > index_offset ||
        (uint64_t)index_offset + (uint64_t)indices * 2U > (uint64_t)length) {
        xsdk_log_write("HL1_PROBE: scene bounds check failed");
        return -1;
    }

    xsdk_log_write("HL1_PROBE: H360 header verified: %u groups %u vertices %u triangles",
                   (unsigned)groups, (unsigned)vertices, (unsigned)(indices / 3U));
    xsdk_log_write("HL1_PROBE: scene bytes = %u", (unsigned)length);
    return 1;
}

int main(void)
{
    xsdk_log_write("HL1_PROBE: XBOX_360_BOOT_BEGIN");
    xsdk_log_write("HL1_PROBE: original Half-Life remaster bootstrap (log only)");

    /* Xenia/retail game drives may differ; try a game:-absolute and a
       relative asset path, without touching system storage. */
    const char *scene = "game:\\assets\\hl1_c0a0_world.h360";
    int status = report_h360(scene);
    if (status == 0) {
        scene = "assets/hl1_c0a0_world.h360";
        status = report_h360(scene);
    }

    if (status == 1) {
        xsdk_log_write("HL1_PROBE: ASSET_HEADER_PASS");
        if (!hl1_preview_h360(scene))
            status = -1;
    } else if (status == 0) {
        xsdk_log_write("HL1_PROBE: NO_SCENE_ASSET (optional for first XEX test)");
    } else {
        xsdk_log_write("HL1_PROBE: ASSET_HEADER_FAIL");
    }

    xsdk_log_write("HL1_PROBE: XBOX_360_BOOT_COMPLETE");
    /* This title exports an offline BMP; it does not present frames in Xenia. */
    return status < 0 ? 1 : 0;
}
