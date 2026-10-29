/*
 * Half-Life 1 c0a0 on-screen GPU geometry preview for Windows Xenia Canary.
 *
 * - Reads PRIVATE user-owned H360 v1 world mesh beside this XEX.
 * - Verifies the entire format before touching GPU vertex memory.
 * - Converts GoldSrc Z-up coordinates into Xenos Y-up, normalizes bounds.
 * - Uploads each unique source vertex once, and a single GPU index list,
 *   with procedural per-material coloring using packed face normals.
 * - Draws with depth testing, a projection matrix and GPU shaders.
 * - Orbit and free-fly camera modes using a gamepad; when no gamepad is
 *   connected, automatic orbit continues so the level remains visible.
 *
 * NO Half-Life textures, gameplay, AI, animation, collisions or movement.
 * Only use with temporary Xenia emulator LUT-timeout diagnostic SDK.
 * NEVER launch this experimental XEX on real Xbox 360 hardware.
 */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hl1_world_explore_shaders.h"
#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/present.h"
#include "xsdk/shader.h"
#include "xsdk/thread.h"

enum {
    H_HEADER = 32,
    H_GROUP = 48,
    H_VERTEX = 24,
    H_MAX_BYTES = 32 * 1024 * 1024,
    H_MAX_GROUPS = 4096,
    H_MAX_INDICES = 300000,
    STRIDE_BYTES = 6 * 4,
    MAX_FRAMES = 1800,
    FENCE_TIMEOUT_MS = 2000,
    STICK_DEADZONE = 8000
};
static const float FOV = 1.0f;
static const float NEAR = 0.1f;
static const float FAR = 30.0f;
static const float DISTANCE = 3.7f;

typedef struct { float x, y, z; } Vec3;
typedef struct { float m[16]; } Matrix;
typedef struct {
    uint8_t *file;
    size_t file_bytes;
    uint32_t groups, vertices, indices, group_offset, vertex_offset, index_offset;
    float center[3], inverse_scale;
    volatile uint32_t *gpu_vertices;
    volatile uint32_t *gpu_indices;
    uint32_t draw_vertices;
    size_t gpu_bytes, index_bytes;
} World;

static uint16_t u16be(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static uint32_t u32be(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
           (uint32_t)p[2] << 8 | (uint32_t)p[3];
}
static float f32be(const uint8_t *p)
{
    const uint32_t n = u32be(p);
    float out;
    memcpy(&out, &n, sizeof out);
    return out;
}
static float clip(float n, float lo, float hi)
{
    return fminf(hi, fmaxf(lo, n));
}
static void write_float(volatile uint32_t *target, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    *target = bits;
}
static Vec3 vector_from_h360(const uint8_t *p)
{
    Vec3 v = {f32be(p), f32be(p + 8), -f32be(p + 4)};
    return v;
}
static Vec3 world_vertex(const World *w, uint32_t index)
{
    Vec3 p = vector_from_h360(w->file + w->vertex_offset + (size_t)index * H_VERTEX);
    p.x = (p.x - w->center[0]) * w->inverse_scale;
    p.y = (p.y - w->center[1]) * w->inverse_scale;
    p.z = (p.z - w->center[2]) * w->inverse_scale;
    return p;
}
static bool read_file(World *world)
{
    const char *candidates[] = {
        "game:\\assets\\hl1_c0a0_world.h360",
        "assets/hl1_c0a0_world.h360"
    };
    FILE *f = NULL;
    for (uint32_t i = 0; i < 2U && !f; i++)
        f = fopen(candidates[i], "rb");
    if (!f) {
        xsdk_log_write("HL1_WORLD: SCENE_FILE_MISSING (put assets beside this XEX)");
        return false;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    const long n = ftell(f);
    if (n < H_HEADER || n > H_MAX_BYTES || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        xsdk_log_write("HL1_WORLD: SCENE_SIZE_INVALID");
        return false;
    }
    world->file = malloc((size_t)n);
    if (!world->file) {
        fclose(f);
        xsdk_log_write("HL1_WORLD: MEMORY_ALLOC_FAIL");
        return false;
    }
    world->file_bytes = (size_t)n;
    size_t bytes = fread(world->file, 1, (size_t)n, f);
    fclose(f);
    if (bytes != (size_t)n) {
        xsdk_log_write("HL1_WORLD: SCENE_READ_FAIL");
        return false;
    }
    return true;
}
static bool validate(World *w)
{
    if (w->file_bytes < H_HEADER || memcmp(w->file, "H360", 4) != 0 ||
        u16be(w->file + 4) != 1 || u16be(w->file + 6) != 0xFEFF) return false;
    w->groups = u32be(w->file + 8);
    w->vertices = u32be(w->file + 12);
    w->indices = u32be(w->file + 16);
    w->group_offset = u32be(w->file + 20);
    w->vertex_offset = u32be(w->file + 24);
    w->index_offset = u32be(w->file + 28);
    if (!w->groups || w->groups > H_MAX_GROUPS ||
        !w->vertices || !w->indices || w->indices > H_MAX_INDICES ||
        w->indices % 3 || w->group_offset < H_HEADER ||
        (uint64_t)w->group_offset + (uint64_t)w->groups * H_GROUP > w->vertex_offset ||
        (uint64_t)w->vertex_offset + (uint64_t)w->vertices * H_VERTEX > w->index_offset ||
        (uint64_t)w->index_offset + (uint64_t)w->indices * 2 > w->file_bytes) return false;

    float lo[3] = {INFINITY, INFINITY, INFINITY};
    float hi[3] = {-INFINITY, -INFINITY, -INFINITY};
    for (uint32_t i = 0; i < w->vertices; i++) {
        Vec3 v = vector_from_h360(w->file + w->vertex_offset + (size_t)i * H_VERTEX);
        const float a[3] = {v.x, v.y, v.z};
        for (uint32_t j = 0; j < 3; j++) {
            if (!isfinite(a[j]) || fabsf(a[j]) > 1000000.f) return false;
            lo[j] = fminf(lo[j], a[j]);
            hi[j] = fmaxf(hi[j], a[j]);
        }
    }
    float largest = 0.0f;
    for (uint32_t j = 0; j < 3; j++) {
        w->center[j] = (lo[j] + hi[j]) * 0.5f;
        largest = fmaxf(largest, hi[j] - lo[j]);
    }
    if (largest < 0.00001f || !isfinite(largest)) return false;
    w->inverse_scale = 2.0f / largest;

    uint32_t counted = 0;
    for (uint32_t i = 0; i < w->groups; i++) {
        const uint8_t *g = w->file + w->group_offset + (size_t)i * H_GROUP;
        uint32_t vb = u32be(g + 32), vc = u32be(g + 36);
        uint32_t ib = u32be(g + 40), ic = u32be(g + 44);
        if (!vc || vc > 65535U || ic % 3 ||
            (uint64_t)vb + vc > w->vertices ||
            (uint64_t)ib + ic > w->indices) return false;
        for (uint32_t j = 0; j < ic; j++) {
            uint32_t local = u16be(w->file + w->index_offset + (size_t)(ib+j) * 2);
            if (local >= vc) return false;
        }
        counted += ic;
    }
    /* Each triangle index must belong to one unique group. */
    if (counted != w->indices) return false;
    w->draw_vertices = w->vertices;
    w->gpu_bytes = (size_t)w->vertices * STRIDE_BYTES;
    w->index_bytes = (size_t)((w->indices + 1U) / 2U) * 4U;
    xsdk_log_write("HL1_EXPLORE: SCENE_VALID: %u groups %u vertices %u triangles",
                   (unsigned)w->groups, (unsigned)w->vertices,
                   (unsigned)(w->indices / 3));
    return true;
}
static uint32_t group_hash(const uint8_t *name)
{
    uint32_t hash = 2166136261U;
    for (uint32_t i = 0; i < 32 && name[i]; i++)
        hash = (hash ^ name[i]) * 16777619U;
    return hash;
}
/* The packed normal is nx,ny,nz in GoldSrc's Z-up coordinates;
 * convert it with the vertex from (x,y,z) to (x,z,-y). */
static float face_shade(const uint8_t *v)
{
    const float nx = (float)(int8_t)v[20] / 127.f;
    const float ny = (float)(int8_t)v[22] / 127.f;
    const float nz = -(float)(int8_t)v[21] / 127.f;
    const float length = sqrtf(nx*nx + ny*ny + nz*nz);
    if (length < 0.01f) return 0.6f;
    return clip(0.30f + 0.68f * fabsf((0.36f*nx + 0.77f*ny - 0.53f*nz) / length),
                0.3f, 1.0f);
}
static bool upload_geometry(World *w)
{
    if (w->gpu_bytes > UINT32_MAX || w->index_bytes > UINT32_MAX) return false;
    w->gpu_vertices = xsdk_gpu_alloc((uint32_t)w->gpu_bytes);
    w->gpu_indices = xsdk_gpu_alloc((uint32_t)w->index_bytes);
    if (!w->gpu_vertices || !w->gpu_indices) return false;

    uint32_t total_indices = 0, half_pending = 0;
    for (uint32_t i = 0; i < w->groups; i++) {
        const uint8_t *g = w->file + w->group_offset + (size_t)i * H_GROUP;
        const uint32_t vb = u32be(g + 32), vc = u32be(g + 36);
        const uint32_t ib = u32be(g + 40), ic = u32be(g + 44);
        const uint32_t hash = group_hash(g);
        const float base[3] = {
            (90.f + (float)(hash & 0x7f)) / 255.f,
            (90.f + (float)((hash >> 8) & 0x7f)) / 255.f,
            (90.f + (float)((hash >> 16) & 0x7f)) / 255.f
        };
        for (uint32_t j = 0; j < vc; j++) {
            const uint32_t index = vb + j;
            const uint8_t *original = w->file + w->vertex_offset +
                                      (size_t)index * H_VERTEX;
            const Vec3 p = world_vertex(w, index);
            const float shade = face_shade(original);
            const float data[6] = {
                p.x, p.y, p.z,
                clip(base[0] * shade, 0.02f, 1.f),
                clip(base[1] * shade, 0.02f, 1.f),
                clip(base[2] * shade, 0.02f, 1.f)
            };
            for (uint32_t k = 0; k < 6; k++)
                write_float(w->gpu_vertices + (size_t)index * 6 + k, data[k]);
        }
        for (uint32_t j = 0; j < ic; j++) {
            const uint16_t local = u16be(w->file + w->index_offset +
                                         (size_t)(ib+j) * 2U);
            const uint32_t global = vb + local;
            if (global > UINT16_MAX) return false;
            if ((total_indices & 1U) == 0U)
                half_pending = global << 16;
            else
                w->gpu_indices[total_indices / 2U] = half_pending | global;
            total_indices++;
        }
    }
    if (total_indices != w->indices) return false;
    if (total_indices & 1U) w->gpu_indices[total_indices / 2U] = half_pending;
    xsdk_gpu_invalidate(xsdk_gpu_address(w->gpu_vertices), (uint32_t)w->gpu_bytes);
    xsdk_gpu_invalidate(xsdk_gpu_address(w->gpu_indices), (uint32_t)w->index_bytes);
    xsdk_log_write("HL1_EXPLORE: INDEXED_MESH_READY vertices=%u indices=%u triangles=%u vertex_bytes=%u index_bytes=%u",
                   (unsigned)w->vertices, (unsigned)w->indices, (unsigned)(w->indices / 3),
                   (unsigned)w->gpu_bytes, (unsigned)w->index_bytes);
    return true;
}
static Matrix multiply(const Matrix *a, const Matrix *b)
{
    Matrix p;
    for (uint32_t col = 0; col < 4; col++)
        for (uint32_t row = 0; row < 4; row++) {
            float sum = 0;
            for (uint32_t k = 0; k < 4; k++)
                sum += a->m[k*4 + row] * b->m[col*4 + k];
            p.m[col*4 + row] = sum;
        }
    return p;
}
static Matrix view_projection(float yaw, float pitch, float distance, float aspect)
{
    const float focal = 1.f / tanf(FOV * 0.5f);
    const float fz = FAR / (NEAR - FAR);
    const Matrix perspective = { {focal/aspect, 0, 0, 0,
                                  0, focal, 0, 0,
                                  0, 0, fz, -1,
                                  0, 0, NEAR*fz, 0} };
    const Matrix away = {{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-distance,1}};
    float cp=cosf(pitch), sp=sinf(pitch), cy=cosf(yaw), sy=sinf(yaw);
    const Matrix pitch_m = {{1,0,0,0, 0,cp,sp,0, 0,-sp,cp,0, 0,0,0,1}};
    const Matrix yaw_m = {{cy,0,-sy,0, 0,1,0,0, sy,0,cy,0, 0,0,0,1}};
    Matrix rotated = multiply(&pitch_m, &yaw_m);
    Matrix placed = multiply(&away, &rotated);
    return multiply(&perspective, &placed);
}
/* Camera-space transform: P * pitch(-pitch) * yaw(-yaw) * T(-eye).
   World+Y is upward. At yaw=0 the camera faces along world -Z. */
static Matrix fly_projection(float yaw, float pitch, Vec3 eye, float aspect)
{
    const float focal = 1.f / tanf(FOV * 0.5f);
    const float fz = FAR / (NEAR - FAR);
    const Matrix perspective = {{ focal/aspect, 0, 0, 0,
                                   0, focal, 0, 0,
                                   0, 0, fz, -1,
                                   0, 0, NEAR*fz, 0 }};
    const Matrix translation = {{ 1,0,0,0, 0,1,0,0, 0,0,1,0,
                                   -eye.x,-eye.y,-eye.z,1 }};
    const float cy=cosf(yaw), sy=sinf(yaw);
    const float cp=cosf(pitch), sp=sinf(pitch);
    const Matrix turn_y = {{ cy,0,-sy,0, 0,1,0,0, sy,0,cy,0, 0,0,0,1 }};
    const Matrix turn_x = {{ 1,0,0,0, 0,cp,-sp,0, 0,sp,cp,0, 0,0,0,1 }};
    Matrix oriented = multiply(&turn_x, &turn_y);
    Matrix view = multiply(&oriented, &translation);
    return multiply(&perspective, &view);
}
static float stick(int16_t value)
{
    return (value > STICK_DEADZONE || value < -STICK_DEADZONE)
        ? value / 32768.0f : 0.f;
}
static const XsdkVertexElement layout[] = {
    { .usage = XSDK_USAGE_POSITION, .format = XSDK_VERTEX_FLOAT3, .offset_bytes = 0 },
    { .usage = XSDK_USAGE_COLOR, .format = XSDK_VERTEX_FLOAT3, .offset_bytes = 12 }
};
int main(void)
{
    xsdk_log_write("HL1_EXPLORE: BOOT_BEGIN");
    xsdk_log_write("HL1_EXPLORE: INDEXED_GPU_WORLD_AND_FREE_FLY");
    xsdk_log_write("HL1_EXPLORE: XENIA_ONLY_EXPERIMENTAL_LUT");
    xsdk_log_write("HL1_EXPLORE: A or Start toggles orbit/fly; left move, right look, triggers up/down, B reset, Back exit");
    xsdk_log_write("HL1_INPUT: DIAGNOSTIC_MODE all 4 virtual gamepad slots tested");
    World world = {0};
    XsdkShader vertex = {0}, pixel = {0};
    uint32_t width = 0, height = 0, submitted = 0;
    int gpu_open = 0, display_open = 0, exit_code = 1;
    if (!read_file(&world) || !validate(&world)) {
        xsdk_log_write("HL1_EXPLORE: SCENE_LOAD_OR_VALIDATE_FAIL");
        goto cleanup;
    }
    NTSTATUS status = xsdk_gpu_open();
    xsdk_log_write("HL1_EXPLORE: gpu_open = 0x%08X", (unsigned)status);
    if (!NT_SUCCESS(status)) goto cleanup;
    gpu_open = 1;
    status = xsdk_shader_load(&vertex, hl1_world_vert.container, hl1_world_vert.container_bytes);
    if (NT_SUCCESS(status))
        status = xsdk_shader_load(&pixel, hl1_world_frag.container, hl1_world_frag.container_bytes);
    if (NT_SUCCESS(status))
        status = xsdk_shader_set_vertex_layout(&vertex, layout, 2,
                                                (const uint32_t[]){STRIDE_BYTES});
    xsdk_log_write("HL1_EXPLORE: shaders_load = 0x%08X", (unsigned)status);
    if (!NT_SUCCESS(status)) goto cleanup;
    if (!upload_geometry(&world)) {
        xsdk_log_write("HL1_EXPLORE: GPU_MESH_UPLOAD_FAIL");
        goto cleanup;
    }

    const XsdkDepthStencil depth = {
        .depth_test = true, .depth_write = true,
        .depth_compare = XSDK_COMPARE_LESS_EQUAL
    };
    XsdkDepthStencilState depth_state;
    xsdk_gpu_depth_stencil_state(&depth_state, &depth);
    status = xsdk_present_open(XSDK_DISPLAY_720P, 2, 1, &width, &height);
    xsdk_log_write("HL1_EXPLORE: present_open = 0x%08X (%ux%u)",
                   (unsigned)status, (unsigned)width, (unsigned)height);
    if (!NT_SUCCESS(status)) goto cleanup;
    display_open = 1;

    float orbit_yaw = 0.70f, orbit_pitch = 0.64f, distance = DISTANCE;
    float fly_yaw = 0.f, fly_pitch = 0.f;
    Vec3 eye = {0.f, 0.f, 2.7f};
    bool flying = false;
    bool last_toggle = false;
    uint32_t previous_slot = UINT32_MAX;
    uint32_t previous_buttons = UINT32_MAX;
    uint32_t input_success_frames = 0;
    uint32_t input_active_frames = 0;
    exit_code = 0;
    for (uint32_t frame = 0; frame < MAX_FRAMES; frame++) {
        INPUT_STATE input = {0};
        bool controller = false;
        uint32_t controller_slot = UINT32_MAX;
        uint32_t first_connected_slot = UINT32_MAX;
        INPUT_STATE first_connected_input = {0};
        /* Xenia may map a keyboard or a USB controller to a
           different emulated player slot. Probe all four slots,
           prefer an ACTIVE one, and report the raw kernel result. */
        for (uint32_t slot = 0; slot < 4U; slot++) {
            INPUT_STATE candidate = {0};
            const uint32_t result = XamInputGetState(slot, 0, &candidate);
            if (frame == 0U || frame == 240U || frame == 720U ||
                frame == 1440U)
                xsdk_log_write("HL1_INPUT: SLOT=%u frame=%u status=0x%08X buttons=0x%04X",
                               (unsigned)slot, (unsigned)frame,
                               (unsigned)result,
                               (unsigned)(result == ERROR_SUCCESS ? candidate.gamepad.buttons : 0U));
            if (result != ERROR_SUCCESS) continue;
            if (first_connected_slot == UINT32_MAX) {
                first_connected_slot = slot;
                first_connected_input = candidate;
            }
            const bool active = candidate.gamepad.buttons ||
                candidate.gamepad.left_trigger || candidate.gamepad.right_trigger ||
                stick(candidate.gamepad.thumb_lx) != 0.f ||
                stick(candidate.gamepad.thumb_ly) != 0.f ||
                stick(candidate.gamepad.thumb_rx) != 0.f ||
                stick(candidate.gamepad.thumb_ry) != 0.f;
            if (active) {
                input = candidate;
                controller = true;
                controller_slot = slot;
                input_active_frames++;
                break;
            }
        }
        if (!controller && first_connected_slot != UINT32_MAX) {
            controller = true;
            input = first_connected_input;
            controller_slot = first_connected_slot;
        }
        if (controller) input_success_frames++;
        if (controller_slot != previous_slot) {
            xsdk_log_write("HL1_INPUT: CONNECTED_SLOT=%d frame=%u",
                           controller ? (int)controller_slot : -1, (unsigned)frame);
            previous_slot = controller_slot;
            previous_buttons = UINT32_MAX;
        }
        if (controller && (input.gamepad.buttons != previous_buttons ||
                           frame == 240U || frame == 720U || frame == 1440U)) {
            xsdk_log_write("HL1_INPUT: RAW slot=%u frame=%u buttons=0x%04X LX=%d LY=%d RX=%d RY=%d",
                           (unsigned)controller_slot, (unsigned)frame,
                           (unsigned)input.gamepad.buttons,
                           (int)input.gamepad.thumb_lx,
                           (int)input.gamepad.thumb_ly,
                           (int)input.gamepad.thumb_rx,
                           (int)input.gamepad.thumb_ry);
            previous_buttons = input.gamepad.buttons;
        }
        if (controller && (input.gamepad.buttons & INPUT_GAMEPAD_BACK)) break;
        const bool toggle = controller &&
            (input.gamepad.buttons & (INPUT_GAMEPAD_A | INPUT_GAMEPAD_START));
        if (toggle && !last_toggle) {
            flying = !flying;
            xsdk_log_write("HL1_EXPLORE: MODE=%s frame=%u player_slot=%u",
                           flying ? "FREE_FLY" : "ORBIT", (unsigned)frame,
                           (unsigned)controller_slot);
        }
        last_toggle = toggle;
        if (flying && controller) {
            const float turn_speed = 0.055f;
            fly_yaw += turn_speed * stick(input.gamepad.thumb_rx);
            fly_pitch = clip(fly_pitch + turn_speed * stick(input.gamepad.thumb_ry),
                             -1.35f, 1.35f);
            const float forward = stick(input.gamepad.thumb_ly);
            const float strafe = stick(input.gamepad.thumb_lx);
            const float speed = (input.gamepad.buttons & INPUT_GAMEPAD_LEFT_SHOULDER)
                ? 0.075f : 0.025f;
            eye.x += speed * (sinf(fly_yaw) * forward + cosf(fly_yaw) * strafe);
            eye.z += speed * (-cosf(fly_yaw) * forward + sinf(fly_yaw) * strafe);
            eye.y += 0.026f * ((float)input.gamepad.right_trigger -
                                (float)input.gamepad.left_trigger) / 255.f;
            eye.x = clip(eye.x, -6.f, 6.f);
            eye.y = clip(eye.y, -6.f, 6.f);
            eye.z = clip(eye.z, -6.f, 6.f);
            if (input.gamepad.buttons & INPUT_GAMEPAD_B) {
                eye = (Vec3){0.f, 0.f, 2.7f};
                fly_yaw = 0.f;
                fly_pitch = 0.f;
            }
        } else {
            if (controller) {
                orbit_yaw += 0.028f * stick(input.gamepad.thumb_lx);
                orbit_pitch = clip(orbit_pitch - 0.028f * stick(input.gamepad.thumb_ly),
                                   -1.15f, 1.15f);
                distance = clip(distance + 0.05f *
                    ((float)input.gamepad.left_trigger - (float)input.gamepad.right_trigger) /
                    255.f, 2.f, 10.f);
            } else {
                orbit_yaw += 0.007f;
            }
        }

        uint32_t target = 0;
        status = xsdk_present_begin(&target, 2000);
        if (!NT_SUCCESS(status)) {
            xsdk_log_write("HL1_EXPLORE: FRAME_BEGIN_FAIL frame=%u status=0x%08X",
                           (unsigned)frame, (unsigned)status);
            exit_code = 3;
            break;
        }
        Matrix matrix = flying
            ? fly_projection(fly_yaw, fly_pitch, eye, (float)width / height)
            : view_projection(orbit_yaw, orbit_pitch, distance, (float)width / height);
        xsdk_gpu_set_target(width, height);
        xsdk_gpu_set_depth_stencil(&depth_state);
        xsdk_gpu_set_shaders(&vertex, &pixel);
        xsdk_built_shader_set_literals(&hl1_world_vert);
        xsdk_built_shader_set_literals(&hl1_world_frag);
        xsdk_gpu_set_vertex_constants(HL1_WORLD_VERT_PUSH, matrix.m, HL1_WORLD_VERT_PUSH_REGISTERS);
        xsdk_gpu_set_vertices(0, xsdk_gpu_address(world.gpu_vertices), (uint32_t)world.gpu_bytes);
        xsdk_gpu_draw_indexed(XSDK_PRIMITIVE_TRIANGLE_LIST,
                              xsdk_gpu_address(world.gpu_indices), world.indices, false);
        xsdk_gpu_resolve(target, width, height, 0xFF161E2CU);
        xsdk_present_end();
        submitted++;
        if (frame == 1 || frame == 60 || frame == 180 || frame == MAX_FRAMES-1)
            xsdk_log_write("HL1_EXPLORE: GPU_MAP_FRAME_SUBMITTED frame=%u triangles=%u",
                           (unsigned)frame, (unsigned)(world.indices / 3));
    }
    XsdkPresentStats stats = {0};
    xsdk_present_stats(&stats);
    xsdk_log_write("HL1_EXPLORE: FRAMES=%u VBLANKS=%u FLIPS=%u MISSED=%u",
                   (unsigned)submitted, (unsigned)stats.vblanks,
                   (unsigned)stats.flips, (unsigned)stats.missed);
    xsdk_log_write("HL1_INPUT: SUMMARY success_frames=%u active_frames=%u",
                   (unsigned)input_success_frames, (unsigned)input_active_frames);
    xsdk_log_write("HL1_INPUT: SUMMARY A_OR_START_SWITCH=%s",
                   flying ? "CURRENTLY_FREE_FLY" : "CURRENTLY_ORBIT");
    xsdk_log_write("HL1_EXPLORE: MAP_VISIBILITY_REQUIRES_USER_CONFIRMATION");
    if (submitted) (void)xsdk_thread_sleep(1500);

cleanup:
    if (display_open) xsdk_present_close();
    if (world.gpu_indices) xsdk_gpu_free(world.gpu_indices);
    if (world.gpu_vertices) xsdk_gpu_free(world.gpu_vertices);
    xsdk_shader_free(&pixel);
    xsdk_shader_free(&vertex);
    if (gpu_open) {
        const NTSTATUS close_status = xsdk_gpu_close();
        xsdk_log_write("HL1_EXPLORE: gpu_close = 0x%08X", (unsigned)close_status);
    }
    free(world.file);
    xsdk_log_write("HL1_EXPLORE: BOOT_COMPLETE");
    return exit_code;
}
