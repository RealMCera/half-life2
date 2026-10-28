/*
 * Original, self-contained H360 v1 wireframe preview for an Xbox 360 XEX.
 * CPU-only: does not use unsupported Xenia display-controller functions.
 * A successful run writes an ordinary 640x360 Windows BMP beside the XEX.
 * This is asset verification and an offline preview, not interactive 3D.
 */
#include "wireframe.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xsdk/log.h"

#define H360_HEADER 32U
#define H360_GROUP 48U
#define H360_VERTEX 24U
#define H360_MAX_BYTES (32UL * 1024UL * 1024UL)
#define W 640
#define H 360
#define ROW_BYTES (W * 3)
#define BMP_BYTES (54U + ROW_BYTES * H)

/* BGR framebuffer. File is stored bottom-up to avoid negative BMP heights. */
static uint8_t framebuffer[H][ROW_BYTES];

static uint16_t be16(const uint8_t *s)
{
    return (uint16_t)(((uint16_t)s[0] << 8) | s[1]);
}

static uint32_t be32(const uint8_t *s)
{
    return ((uint32_t)s[0] << 24) | ((uint32_t)s[1] << 16) |
           ((uint32_t)s[2] << 8) | s[3];
}

static void le16(uint8_t *d, uint16_t n)
{
    d[0] = (uint8_t)n;
    d[1] = (uint8_t)(n >> 8);
}

static void le32(uint8_t *d, uint32_t n)
{
    d[0] = (uint8_t)n;
    d[1] = (uint8_t)(n >> 8);
    d[2] = (uint8_t)(n >> 16);
    d[3] = (uint8_t)(n >> 24);
}

static float be_float(const uint8_t *s)
{
    uint32_t bits = be32(s);
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static void dot(int x, int y, const uint8_t color[3])
{
    if (x < 0 || x >= W || y < 0 || y >= H)
        return;
    uint8_t *p = &framebuffer[y][x * 3];
    p[0] = color[0];
    p[1] = color[1];
    p[2] = color[2];
}

/* Integer Bresenham lines, with conservative clipping via dot(). */
static void line(int x0, int y0, int x1, int y1, const uint8_t color[3])
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        dot(x0, y0, color);
        if (x0 == x1 && y0 == y1)
            break;
        int error2 = 2 * err;
        if (error2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (error2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void project(const uint8_t *data, uint32_t vi, int axis_a, int axis_b,
                    const float center[3], float scale, int *px, int *py)
{
    const uint8_t *v = data + (size_t)vi * H360_VERTEX;
    float a = be_float(v + axis_a * 4);
    float b = be_float(v + axis_b * 4);
    *px = (int)((a - center[axis_a]) * scale + W * 0.5f);
    *py = (int)(H * 0.5f - (b - center[axis_b]) * scale);
}

static int save_bmp(const char *path)
{
    uint8_t header[54] = {0};
    FILE *file = fopen(path, "wb");
    if (!file)
        return 0;
    header[0] = 'B';
    header[1] = 'M';
    le32(header + 2, BMP_BYTES);
    le32(header + 10, 54);
    le32(header + 14, 40);
    le32(header + 18, W);
    le32(header + 22, H);
    le16(header + 26, 1);
    le16(header + 28, 24);
    le32(header + 34, ROW_BYTES * H);
    int okay = fwrite(header, 1, sizeof header, file) == sizeof header;
    for (int y = H - 1; y >= 0 && okay; --y) {
        okay = fwrite(framebuffer[y], 1, ROW_BYTES, file) == ROW_BYTES;
    }
    if (fclose(file) != 0)
        okay = 0;
    return okay;
}

int hl1_preview_h360(const char *path)
{
    uint8_t *buffer = NULL;
    FILE *file = fopen(path, "rb");
    if (!file) {
        xsdk_log_write("HL1_PROBE: FULL_MESH_FAIL: could not reopen scene");
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        xsdk_log_write("HL1_PROBE: FULL_MESH_FAIL: cannot seek scene");
        return 0;
    }
    long length = ftell(file);
    if (length < (long)H360_HEADER || length > (long)H360_MAX_BYTES ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        xsdk_log_write("HL1_PROBE: FULL_MESH_FAIL: scene byte count invalid");
        return 0;
    }
    buffer = (uint8_t *)malloc((size_t)length);
    if (!buffer) {
        fclose(file);
        xsdk_log_write("HL1_PROBE: FULL_MESH_FAIL: out of memory");
        return 0;
    }
    size_t got = fread(buffer, 1, (size_t)length, file);
    fclose(file);
    if (got != (size_t)length) {
        xsdk_log_write("HL1_PROBE: FULL_MESH_FAIL: truncated read");
        goto fail;
    }

    uint32_t groups = be32(buffer + 8);
    uint32_t vertices = be32(buffer + 12);
    uint32_t indices = be32(buffer + 16);
    uint32_t goff = be32(buffer + 20);
    uint32_t voff = be32(buffer + 24);
    uint32_t ioff = be32(buffer + 28);
    if (memcmp(buffer, "H360", 4) != 0 || be16(buffer + 4) != 1 ||
        be16(buffer + 6) != 0xFEFF || !groups || groups > 4096U ||
        !vertices || !indices || indices % 3U ||
        goff < H360_HEADER ||
        (uint64_t)goff + (uint64_t)groups * H360_GROUP > voff ||
        (uint64_t)voff + (uint64_t)vertices * H360_VERTEX > ioff ||
        (uint64_t)ioff + (uint64_t)indices * 2U > (uint64_t)length) {
        xsdk_log_write("HL1_PROBE: FULL_MESH_FAIL: invalid H360 bounds");
        goto fail;
    }

    float lo[3] = { INFINITY, INFINITY, INFINITY };
    float hi[3] = { -INFINITY, -INFINITY, -INFINITY };
    for (uint32_t i = 0; i < vertices; i++) {
        const uint8_t *v = buffer + voff + (size_t)i * H360_VERTEX;
        for (int axis = 0; axis < 3; axis++) {
            float position = be_float(v + axis * 4);
            if (!isfinite(position) || fabsf(position) > 1000000.0f) {
                xsdk_log_write("HL1_PROBE: FULL_MESH_FAIL: bad vertex position");
                goto fail;
            }
            if (position < lo[axis]) lo[axis] = position;
            if (position > hi[axis]) hi[axis] = position;
        }
    }

    for (uint32_t g = 0; g < groups; g++) {
        const uint8_t *row = buffer + goff + (size_t)g * H360_GROUP;
        uint32_t vf = be32(row + 32);
        uint32_t vc = be32(row + 36);
        uint32_t ix = be32(row + 40);
        uint32_t ic = be32(row + 44);
        if ((uint64_t)vf + vc > vertices || !vc || vc > 65535U ||
            (uint64_t)ix + ic > indices || ic % 3U) {
            xsdk_log_write("HL1_PROBE: FULL_MESH_FAIL: group bounds invalid");
            goto fail;
        }
        for (uint32_t j = 0; j < ic; j++) {
            uint32_t local_index = be16(buffer + ioff + (size_t)(ix + j) * 2U);
            if (local_index >= vc) {
                xsdk_log_write("HL1_PROBE: FULL_MESH_FAIL: index out of group range");
                goto fail;
            }
        }
    }

    xsdk_log_write("HL1_PROBE: FULL_MESH_PASS: %u groups %u vertices %u triangles",
                   (unsigned)groups, (unsigned)vertices, (unsigned)(indices / 3U));

    /* Orthographic view: two longest spatial axes maximize map readability. */
    float span[3];
    float center[3];
    for (int a = 0; a < 3; a++) {
        span[a] = hi[a] - lo[a];
        center[a] = lo[a] + 0.5f * span[a];
    }
    int axis_a = 0;
    for (int a = 1; a < 3; a++)
        if (span[a] > span[axis_a]) axis_a = a;
    int axis_b = axis_a == 0 ? 1 : 0;
    for (int a = 0; a < 3; a++)
        if (a != axis_a && span[a] > span[axis_b]) axis_b = a;
    float scale_a = span[axis_a] < 0.001f ? 1.0f : (float)(W - 40) / span[axis_a];
    float scale_b = span[axis_b] < 0.001f ? 1.0f : (float)(H - 40) / span[axis_b];
    float scale = fminf(scale_a, scale_b);
    if (!isfinite(scale) || scale <= 0.0f)
        scale = 1.0f;

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            uint8_t *p = &framebuffer[y][3 * x];
            p[0] = 32;
            p[1] = 24;
            p[2] = 18;
        }
    }

    for (uint32_t g = 0; g < groups; g++) {
        const uint8_t *row = buffer + goff + (size_t)g * H360_GROUP;
        uint32_t vf = be32(row + 32);
        uint32_t ix = be32(row + 40);
        uint32_t ic = be32(row + 44);
        uint32_t hash = 2166136261U;
        for (int c = 0; c < 32 && row[c]; c++)
            hash = (hash ^ row[c]) * 16777619U;
        const uint8_t color[3] = {
            (uint8_t)(85 + (hash & 0x7FU)),
            (uint8_t)(90 + ((hash >> 8) & 0x7FU)),
            (uint8_t)(95 + ((hash >> 16) & 0x7FU))
        };
        for (uint32_t k = 0; k < ic; k += 3) {
            int px[3], py[3];
            for (int j = 0; j < 3; j++) {
                uint32_t local = be16(buffer + ioff + (size_t)(ix + k + (uint32_t)j) * 2U);
                project(buffer + voff, vf + local, axis_a, axis_b,
                        center, scale, &px[j], &py[j]);
            }
            line(px[0], py[0], px[1], py[1], color);
            line(px[1], py[1], px[2], py[2], color);
            line(px[2], py[2], px[0], py[0], color);
        }
    }
    free(buffer);

    /* Xenia launches a standalone XEX from a READ-ONLY game: mount.
       Unlike game:, cache: and cache0:/cache1: are writable host mounts.
       Keep an ASCII backup in the existing title log, which is known
       to work, even if every image file create is blocked. */
    xsdk_log_write("HL1_PROBE: ASCII_PREVIEW_BEGIN: 64x24");
    for (int by = 0; by < 24; by++) {
        char linebuf[65];
        for (int bx = 0; bx < 64; bx++) {
            char mark = ' ';
            for (int ry = 0; ry < 15 && mark == ' '; ry++) {
                const uint8_t *pixels = framebuffer[by * 15 + ry];
                for (int rx = 0; rx < 10; rx++) {
                    const uint8_t *p = pixels + (bx * 10 + rx) * 3;
                    if (p[0] != 32 || p[1] != 24 || p[2] != 18) {
                        mark = '#';
                        break;
                    }
                }
            }
            linebuf[bx] = mark;
        }
        linebuf[64] = '\0';
        xsdk_log_write("HL1_WIRE|%s|", linebuf);
    }
    xsdk_log_write("HL1_PROBE: ASCII_PREVIEW_PASS");

    const char *targets[] = {
        "cache:\\hl1_wireframe.bmp",
        "cache0:\\hl1_wireframe.bmp",
        "cache1:\\hl1_wireframe.bmp",
        "game:\\hl1_wireframe.bmp"
    };
    for (size_t i = 0; i < sizeof targets / sizeof targets[0]; i++) {
        if (save_bmp(targets[i])) {
            xsdk_log_write("HL1_PROBE: BMP_PREVIEW_PASS: %s (%ux%u)",
                           targets[i], (unsigned)W, (unsigned)H);
            return 1;
        }
    }
    xsdk_log_write("HL1_PROBE: BMP_PREVIEW_UNAVAILABLE: all output mounts refused writes; ASCII preview logged");
    return 1;

fail:
    free(buffer);
    return 0;
}
