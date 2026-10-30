/*
 * Software solid-surface isometric preview for H360 v1.
 *
 * NO Xbox GPU calls. Validated static world geometry is transformed
 * into a fixed isometric view, filled using barycentric coordinates,
 * shaded with one directional light, and depth-tested in a 640x360
 * float Z-buffer. Intended only for inspecting original game geometry
 * in a CPU-generated BMP under Windows Xenia Canary.
 *
 * Inputs are a fully validated H360 buffer from wireframe.c.
 * Does not implement GoldSrc visibility, texture sampling, collision,
 * player camera, BSP models, or real-time Xenia presentation.
 */
#include "shaded.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xsdk/log.h"

enum {
    FRAME_W = 640,
    FRAME_H = 360,
    FRAME_ROW = FRAME_W * 3,
    VERTEX_STRIDE = 24,
    GROUP_STRIDE = 48,
    BMP_HEADER = 54
};

typedef struct ProjectedVertex {
    float x, y, depth;
    float world[3];
} ProjectedVertex;

static uint8_t frame[FRAME_H][FRAME_ROW];
static float zbuffer[FRAME_H][FRAME_W];

static uint16_t big_u16(const uint8_t *b)
{
    return (uint16_t)(((uint16_t)b[0] << 8) | b[1]);
}

static uint32_t big_u32(const uint8_t *b)
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | b[3];
}

static float big_f32(const uint8_t *b)
{
    const uint32_t bits = big_u32(b);
    float f;
    memcpy(&f, &bits, sizeof f);
    return f;
}

static void little_u16(uint8_t *dest, uint16_t value)
{
    dest[0] = (uint8_t)value;
    dest[1] = (uint8_t)(value >> 8);
}

static void little_u32(uint8_t *dest, uint32_t value)
{
    dest[0] = (uint8_t)value;
    dest[1] = (uint8_t)(value >> 8);
    dest[2] = (uint8_t)(value >> 16);
    dest[3] = (uint8_t)(value >> 24);
}

static int write_bmp(const char *path)
{
    FILE *output = fopen(path, "wb");
    if (!output) return 0;

    uint8_t header[BMP_HEADER] = {0};
    header[0] = 'B';
    header[1] = 'M';
    little_u32(header + 2, BMP_HEADER + FRAME_H * FRAME_ROW);
    little_u32(header + 10, BMP_HEADER);
    little_u32(header + 14, 40);
    little_u32(header + 18, FRAME_W);
    little_u32(header + 22, FRAME_H);
    little_u16(header + 26, 1);
    little_u16(header + 28, 24);
    little_u32(header + 34, FRAME_H * FRAME_ROW);

    int okay = fwrite(header, 1, sizeof header, output) == sizeof header;
    for (int y = FRAME_H - 1; y >= 0 && okay; --y)
        okay = fwrite(frame[y], 1, FRAME_ROW, output) == FRAME_ROW;

    if (fclose(output) != 0) okay = 0;
    return okay;
}

static float edge_area(float ax, float ay, float bx, float by,
                       float px, float py)
{
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

static void render_triangle(const ProjectedVertex corners[3], uint8_t bgr[3])
{
    float area = edge_area(corners[0].x, corners[0].y,
                           corners[1].x, corners[1].y,
                           corners[2].x, corners[2].y);
    if (!isfinite(area) || fabsf(area) < 1e-5f) return;

    float left = fminf(corners[0].x, fminf(corners[1].x, corners[2].x));
    float right = fmaxf(corners[0].x, fmaxf(corners[1].x, corners[2].x));
    float top = fminf(corners[0].y, fminf(corners[1].y, corners[2].y));
    float bottom = fmaxf(corners[0].y, fmaxf(corners[1].y, corners[2].y));

    if (right < 0 || left >= FRAME_W || bottom < 0 || top >= FRAME_H)
        return;
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > FRAME_W - 1) right = FRAME_W - 1;
    if (bottom > FRAME_H - 1) bottom = FRAME_H - 1;
    int x0 = (int)floorf(left);
    int x1 = (int)ceilf(right);
    int y0 = (int)floorf(top);
    int y1 = (int)ceilf(bottom);

    /* The camera is on the positive XYZ side; larger projected depth
       is closer to the camera. */
    for (int py = y0; py <= y1; ++py) {
        for (int px = x0; px <= x1; ++px) {
            float sx = (float)px + 0.5f;
            float sy = (float)py + 0.5f;
            float w0 = edge_area(corners[1].x, corners[1].y,
                                  corners[2].x, corners[2].y, sx, sy) / area;
            float w1 = edge_area(corners[2].x, corners[2].y,
                                  corners[0].x, corners[0].y, sx, sy) / area;
            float w2 = 1.0f - w0 - w1;
            if (w0 < -1e-5f || w1 < -1e-5f || w2 < -1e-5f)
                continue;
            float depth = w0 * corners[0].depth +
                          w1 * corners[1].depth + w2 * corners[2].depth;
            if (depth <= zbuffer[py][px]) continue;
            zbuffer[py][px] = depth;
            frame[py][px * 3 + 0] = bgr[0];
            frame[py][px * 3 + 1] = bgr[1];
            frame[py][px * 3 + 2] = bgr[2];
        }
    }
}

static void material_color(uint32_t hash,
                           const ProjectedVertex tri[3], uint8_t color[3])
{
    const float ux = tri[1].world[0] - tri[0].world[0];
    const float uy = tri[1].world[1] - tri[0].world[1];
    const float uz = tri[1].world[2] - tri[0].world[2];
    const float vx = tri[2].world[0] - tri[0].world[0];
    const float vy = tri[2].world[1] - tri[0].world[1];
    const float vz = tri[2].world[2] - tri[0].world[2];
    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;
    float length = sqrtf(nx * nx + ny * ny + nz * nz);
    float intensity = 0.65f;
    if (length > 1e-9f) {
        /* Two-sided lighting since this visualizes raw BSP faces,
           not a properly culled real-time world. */
        float cosine = fabsf((nx * 0.35f - ny * 0.50f + nz * 0.79f) / length);
        intensity = 0.23f + 0.76f * cosine;
    }
    for (int c = 0; c < 3; c++) {
        unsigned val = 85U + ((hash >> (8 * c)) & 0x7FU);
        color[c] = (uint8_t)(val * intensity);
    }
}

int hl1_shaded_h360(const uint8_t *buffer,
                    uint32_t groups, uint32_t vertices,
                    uint32_t goff, uint32_t voff, uint32_t ioff)
{
    if (!buffer || !vertices || vertices > 1000000U) {
        xsdk_log_write("HL1_PROBE: SHADED_PREVIEW_FAIL: unreasonable mesh vertex count");
        return 0;
    }
    ProjectedVertex *projected =
        (ProjectedVertex *)malloc((size_t)vertices * sizeof *projected);
    if (!projected) {
        xsdk_log_write("HL1_PROBE: SHADED_PREVIEW_FAIL: vertex allocation");
        return 0;
    }

    float min_u = INFINITY, max_u = -INFINITY;
    float min_v = INFINITY, max_v = -INFINITY;
    for (uint32_t i = 0; i < vertices; i++) {
        ProjectedVertex *p = projected + i;
        const uint8_t *raw = buffer + voff + (size_t)i * VERTEX_STRIDE;
        for (int j = 0; j < 3; j++)
            p->world[j] = big_f32(raw + j * 4);
        const float x = p->world[0];
        const float y = p->world[1];
        const float z = p->world[2];
        p->x = 0.707106781f * (x - y);
        p->y = 0.408248290f * (x + y) - 0.816496581f * z;
        p->depth = 0.577350269f * (x + y + z);
        if (p->x < min_u) min_u = p->x;
        if (p->x > max_u) max_u = p->x;
        if (p->y < min_v) min_v = p->y;
        if (p->y > max_v) max_v = p->y;
    }

    float width = max_u - min_u, height = max_v - min_v;
    float scale_x = width < 0.001f ? 1.0f : (FRAME_W - 40.0f) / width;
    float scale_y = height < 0.001f ? 1.0f : (FRAME_H - 40.0f) / height;
    float scale = fminf(scale_x, scale_y);
    float center_x = min_u + width * 0.5f, center_y = min_v + height * 0.5f;
    if (scale <= 0.0f || !isfinite(scale)) {
        xsdk_log_write("HL1_PROBE: SHADED_PREVIEW_FAIL: invalid projection");
        free(projected);
        return 0;
    }

    for (uint32_t i = 0; i < vertices; i++) {
        projected[i].x = (projected[i].x - center_x) * scale + FRAME_W * 0.5f;
        projected[i].y = FRAME_H * 0.5f - (projected[i].y - center_y) * scale;
    }

    for (int y = 0; y < FRAME_H; y++) {
        for (int x = 0; x < FRAME_W; x++) {
            zbuffer[y][x] = -INFINITY;
            uint8_t *p = &frame[y][x * 3];
            p[0] = 32; p[1] = 24; p[2] = 18;
        }
    }

    uint32_t drawn = 0;
    for (uint32_t g = 0; g < groups; g++) {
        const uint8_t *group = buffer + goff + (size_t)g * GROUP_STRIDE;
        const uint32_t first_vertex = big_u32(group + 32);
        const uint32_t first_index = big_u32(group + 40);
        const uint32_t index_count = big_u32(group + 44);
        uint32_t hash = 2166136261U;
        for (int c = 0; c < 32 && group[c]; c++)
            hash = (hash ^ group[c]) * 16777619U;

        for (uint32_t k = 0; k < index_count; k += 3) {
            ProjectedVertex tri[3];
            for (uint32_t j = 0; j < 3; j++) {
                uint32_t local_index = big_u16(buffer + ioff +
                    (size_t)(first_index + k + j) * 2U);
                tri[j] = projected[first_vertex + local_index];
            }
            uint8_t color[3];
            material_color(hash, tri, color);
            render_triangle(tri, color);
            drawn++;
        }
    }
    free(projected);

    const char *paths[] = {
        "cache:\\hl1_shaded.bmp",
        "cache0:\\hl1_shaded.bmp",
        "cache1:\\hl1_shaded.bmp",
        "game:\\hl1_shaded.bmp"
    };
    for (size_t i = 0; i < sizeof paths / sizeof paths[0]; i++) {
        if (write_bmp(paths[i])) {
            xsdk_log_write("HL1_PROBE: SHADED_PREVIEW_PASS: %s (%ux%u, %u input triangles)",
                           paths[i], (unsigned)FRAME_W, (unsigned)FRAME_H,
                           (unsigned)drawn);
            return 1;
        }
    }
    xsdk_log_write("HL1_PROBE: SHADED_PREVIEW_FAIL: no writable image mount");
    return 0;
}
