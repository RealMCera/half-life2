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
 * Black Mesa Alpha 5 stable-ground walking, independently testing larger map:
 * draw batches <=65532 indices and optional Source BSP spawn sidecar.
 * Experimental stable-ground walker: checks floor support before moving,
 * scans collision faces for safer starting positions, and catches landings
 * across a single physics step. Basic walls still use segment tests.
 * NO original GoldSrc BSP collision hulls, doors, entities or complete game.
 * Only use with temporary Xenia emulator LUT-timeout diagnostic SDK.
 * NEVER launch this experimental XEX on real Xbox 360 hardware.
 */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hl1_bms_world_walk_shaders.h"
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
    MAX_FRAMES = 1200,
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
    xsdk_log_write("HL1_BMS_WALK: SCENE_VALID: %u groups %u vertices %u triangles",
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
    xsdk_log_write("HL1_BMS_WALK: INDEXED_MESH_READY vertices=%u indices=%u triangles=%u vertex_bytes=%u index_bytes=%u",
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
/* CPU collision prototype using the same H360 mesh the GPU draws.
 * NOT GoldSrc hull collision: lacks water, ladders, stairs, doors,
 * moving brush entities and point-contents BSP semantics. */
typedef struct {
    Vec3 a, b, c;
    float min_x, max_x, min_z, max_z;
    float ny_ratio;
} WalkFace;
typedef struct {
    WalkFace *faces;
    uint32_t total, floors;
    Vec3 spawn;
    float eye_height, step_height, speed, jump_speed, gravity;
} WalkScene;
typedef struct {
    Vec3 eye;
    float yaw, pitch, vy;
    bool grounded, jumping_last;
    uint32_t blocked_edges, blocked_walls, jumps, landings, recoveries;
} WalkPlayer;
static Vec3 vector_sub(Vec3 a, Vec3 b) {
    Vec3 p = {a.x-b.x, a.y-b.y, a.z-b.z};
    return p;
}
static Vec3 vector_cross(Vec3 a, Vec3 b) {
    Vec3 p = {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
    return p;
}
static float vector_dot(Vec3 a, Vec3 b) {
    return a.x*b.x+a.y*b.y+a.z*b.z;
}
/* Collision functions appear after the scene builder. */
static bool walk_ground(const WalkScene *scene, float x, float z,
                        float cap_y, float *result);
static bool walk_build(const World *world, WalkScene *scene)
{
    memset(scene, 0, sizeof *scene);
    scene->total = world->indices / 3U;
    scene->faces = calloc(scene->total, sizeof *scene->faces);
    if (!scene->faces) return false;
    scene->eye_height = clip(64.f * world->inverse_scale, 0.04f, 0.16f);
    scene->step_height = clip(18.f * world->inverse_scale, 0.012f, 0.04f);
    scene->speed = clip(200.f * world->inverse_scale / 60.f, 0.002f, 0.02f);
    scene->jump_speed = clip(225.f * world->inverse_scale / 60.f, 0.006f, 0.035f);
    scene->gravity = clip(800.f * world->inverse_scale / 3600.f, 0.0002f, 0.0014f);
    float best_score = -INFINITY;
    /* Hold the best 16 candidates until all floor triangles are decoded,
       then choose one with actual nearby floor support. */
    struct { Vec3 center; float score; } spawns[16];
    for (uint32_t k=0; k<16U; ++k) spawns[k].score=-INFINITY;
    uint32_t emitted = 0;
    for (uint32_t i = 0; i < world->groups; i++) {
        const uint8_t *g = world->file + world->group_offset + (size_t)i * H_GROUP;
        uint32_t base = u32be(g+32), first = u32be(g+40), count = u32be(g+44);
        for (uint32_t j = 0; j < count; j += 3) {
            WalkFace *t = &scene->faces[emitted++];
            uint16_t ai = u16be(world->file + world->index_offset + (size_t)(first+j) * 2);
            uint16_t bi = u16be(world->file + world->index_offset + (size_t)(first+j+1) * 2);
            uint16_t ci = u16be(world->file + world->index_offset + (size_t)(first+j+2) * 2);
            t->a = world_vertex(world, base + ai);
            t->b = world_vertex(world, base + bi);
            t->c = world_vertex(world, base + ci);
            t->min_x = fminf(t->a.x, fminf(t->b.x, t->c.x));
            t->max_x = fmaxf(t->a.x, fmaxf(t->b.x, t->c.x));
            t->min_z = fminf(t->a.z, fminf(t->b.z, t->c.z));
            t->max_z = fmaxf(t->a.z, fmaxf(t->b.z, t->c.z));
            Vec3 n = vector_cross(vector_sub(t->b,t->a), vector_sub(t->c,t->a));
            const float length = sqrtf(vector_dot(n,n));
            t->ny_ratio = length > 0.0000001f ? fabsf(n.y)/length : 0.f;
            if (t->ny_ratio > 0.65f) {
                scene->floors++;
                const float cx = (t->a.x+t->b.x+t->c.x)/3.f;
                const float cy = (t->a.y+t->b.y+t->c.y)/3.f;
                const float cz = (t->a.z+t->b.z+t->c.z)/3.f;
                const float area_xz = fabsf(
                    (t->b.x-t->a.x)*(t->c.z-t->a.z)-
                    (t->c.x-t->a.x)*(t->b.z-t->a.z));
                /* Prefer a moderately centered, sizeable walkable face.
                 * This is a diagnostic spawn, not HL1 info_player_start. */
                const float score = sqrtf(area_xz) - 0.25f*(fabsf(cx)+fabsf(cz)) -
                    0.10f*fabsf(cy);
                if (score > best_score) {
                    best_score = score;
                    scene->spawn = (Vec3){cx,cy,cz};
                }
                for (uint32_t k=0; k<16U; ++k) {
                    if (score > spawns[k].score) {
                        for (uint32_t q=15U;q>k;--q) spawns[q]=spawns[q-1U];
                        spawns[k].score=score;
                        spawns[k].center=(Vec3){cx,cy,cz};
                        break;
                    }
                }
            }
        }
    }
    if (emitted != scene->total || !scene->floors) return false;
    /* Spawn selection done after building the full array of faces.
       Eight points around the player are checked; without this,
       the largest face centroid may sit beside a missing mesh edge.
       Conservative: prefer a score-ranked candidate with support. */
    uint32_t best_neighbors=0U;
    for (uint32_t c=0;c<16U;c++) {
        if (!isfinite(spawns[c].score)) continue;
        const Vec3 p=spawns[c].center;
        const float radius=fmaxf(scene->speed*2.5f,0.04f);
        const float dx[8]={radius,-radius,0.f,0.f,radius*0.7f,radius*0.7f,-radius*0.7f,-radius*0.7f};
        const float dz[8]={0.f,0.f,radius,-radius,radius*0.7f,-radius*0.7f,radius*0.7f,-radius*0.7f};
        uint32_t neighbors=0;
        for (uint32_t i=0;i<8U;i++) {
            float floor=0.f;
            if (walk_ground(scene,p.x+dx[i],p.z+dz[i],
                            p.y+scene->step_height,&floor) &&
                fabsf(floor-p.y)<=scene->step_height)
                neighbors++;
        }
        if (neighbors>best_neighbors) {
            best_neighbors=neighbors;
            scene->spawn=p;
        }
        if (neighbors==8U) break;
    }
    xsdk_log_write("HL1_BMS_WALK: SPAWN_SUPPORT neighbors=%u/8", (unsigned)best_neighbors);
    xsdk_log_write("HL1_BMS_WALK: COLLISION_MESH triangles=%u walkable=%u eye_mm=%d step_mm=%d",
                   (unsigned)scene->total, (unsigned)scene->floors,
                   (int)(scene->eye_height*1000.f),
                   (int)(scene->step_height*1000.f));
    xsdk_log_write("HL1_BMS_WALK: SPAWN x_mm=%d floor_mm=%d z_mm=%d",
                   (int)(scene->spawn.x*1000.f), (int)(scene->spawn.y*1000.f),
                   (int)(scene->spawn.z*1000.f));
    return true;
}
/* Finds the highest nearly horizontal triangle at (x,z) below
 * a cap. This is an approximate mesh-ground query, not BSP trace. */
static bool walk_ground(const WalkScene *scene, float x, float z, float cap_y, float *result)
{
    float highest = -INFINITY;
    const float epsilon = 0.00001f;
    for (uint32_t i = 0; i < scene->total; i++) {
        const WalkFace *t = &scene->faces[i];
        if (t->ny_ratio < 0.65f || x < t->min_x-epsilon ||
            x > t->max_x+epsilon || z < t->min_z-epsilon ||
            z > t->max_z+epsilon) continue;
        const float det = (t->b.z-t->c.z)*(t->a.x-t->c.x) +
                          (t->c.x-t->b.x)*(t->a.z-t->c.z);
        if (fabsf(det) < 0.00000001f) continue;
        const float u = ((t->b.z-t->c.z)*(x-t->c.x)+
                         (t->c.x-t->b.x)*(z-t->c.z))/det;
        const float v = ((t->c.z-t->a.z)*(x-t->c.x)+
                         (t->a.x-t->c.x)*(z-t->c.z))/det;
        const float w = 1.f-u-v;
        if (u < -epsilon || v < -epsilon || w < -epsilon) continue;
        const float y = u*t->a.y+v*t->b.y+w*t->c.y;
        if (y <= cap_y+epsilon && y > highest) highest=y;
    }
    if (!isfinite(highest)) return false;
    *result=highest;
    return true;
}
/* Segment-triangle intersection with both sided surfaces.
 * Only nearly vertical polygons block horizontal movement. */
static bool walk_ray_hits(const Vec3 from, const Vec3 to, const WalkFace *t)
{
    const Vec3 direction=vector_sub(to,from);
    const Vec3 e1=vector_sub(t->b,t->a);
    const Vec3 e2=vector_sub(t->c,t->a);
    const Vec3 p=vector_cross(direction,e2);
    const float det=vector_dot(e1,p);
    if (fabsf(det)<0.00000001f) return false;
    const float inv=1.f/det;
    const Vec3 delta=vector_sub(from,t->a);
    const float u=vector_dot(delta,p)*inv;
    if (u < 0.f || u > 1.f) return false;
    const Vec3 q=vector_cross(delta,e1);
    const float v=vector_dot(direction,q)*inv;
    if (v < 0.f || u+v > 1.f) return false;
    const float distance=vector_dot(e2,q)*inv;
    return distance>=0.0001f && distance<=1.f;
}
static bool walk_wall_blocked(const WalkScene *scene, Vec3 old_eye, Vec3 new_eye)
{
    /* Check knee and upper-body horizontal segments, not a full capsule. */
    for (uint32_t i=0;i<scene->total;i++) {
        const WalkFace *t=&scene->faces[i];
        if (t->ny_ratio>0.65f) continue;
        const float minx=fminf(old_eye.x,new_eye.x);
        const float maxx=fmaxf(old_eye.x,new_eye.x);
        const float minz=fminf(old_eye.z,new_eye.z);
        const float maxz=fmaxf(old_eye.z,new_eye.z);
        if (maxx<t->min_x || minx>t->max_x ||
            maxz<t->min_z || minz>t->max_z) continue;
        for (uint32_t p=0;p<2U;p++) {
            const float offset= p ? scene->eye_height*0.68f : scene->eye_height*0.22f;
            Vec3 oldp={old_eye.x,old_eye.y-scene->eye_height+offset,old_eye.z};
            Vec3 newp={new_eye.x,oldp.y,new_eye.z};
            if (walk_ray_hits(oldp,newp,t)) return true;
        }
    }
    return false;
}
static void walk_reset(const WalkScene *scene, WalkPlayer *p)
{
    p->eye = scene->spawn;
    p->eye.y += scene->eye_height;
    p->yaw=0.f;
    p->pitch=0.f;
    p->vy=0.f;
    p->grounded=true;
    p->jumping_last=false;
}
/* A grounded step is accepted only if both midpoint and destination
 * still have a floor within a manageable step from the old floor.
 * This prevents walking off unmodeled edge gaps by accident.
 * While airborne, an actual fall is possible and intentional. */
static bool walk_supported(const WalkScene *scene,
                           Vec3 from, Vec3 to, float *floor)
{
    const float old_floor=from.y-scene->eye_height;
    float mid_floor=0.f;
    if (!walk_ground(scene,(from.x+to.x)*0.5f,
                     (from.z+to.z)*0.5f,
                     old_floor+scene->step_height,&mid_floor))
        return false;
    if (mid_floor < old_floor-scene->step_height ||
        mid_floor > old_floor+scene->step_height)
        return false;
    if (!walk_ground(scene,to.x,to.z,
                     old_floor+scene->step_height,floor))
        return false;
    return *floor >= old_floor-scene->step_height &&
           *floor <= old_floor+scene->step_height;
}
static bool walk_try_step(const WalkScene *scene, WalkPlayer *p,
                          Vec3 next)
{
    if (walk_wall_blocked(scene,p->eye,next)) {
        p->blocked_walls++;
        return false;
    }
    if (p->grounded) {
        float next_floor=0.f;
        if (!walk_supported(scene,p->eye,next,&next_floor)) {
            p->blocked_edges++;
            return false;
        }
        p->eye.x=next.x;
        p->eye.z=next.z;
        p->eye.y=next_floor+scene->eye_height;
    } else {
        p->eye.x=next.x;
        p->eye.z=next.z;
    }
    return true;
}
static void walk_tick(const WalkScene *scene, WalkPlayer *p,
                      float forward, float strafe, float turn, float look,
                      bool jump)
{
    p->yaw += 0.046f*turn;
    p->pitch=clip(p->pitch + 0.046f*look, -1.35f, 1.35f);
    const float length=sqrtf(forward*forward+strafe*strafe);
    if (length>1.f) {forward/=length; strafe/=length;}
    const bool leap=jump && !p->jumping_last && p->grounded;
    p->jumping_last=jump;
    if (leap) {
        p->vy=scene->jump_speed;
        p->grounded=false;
        p->jumps++;
        xsdk_log_write("HL1_BMS_WALK: JUMP");
    }
    const Vec3 old=p->eye;
    Vec3 wanted=old;
    wanted.x += scene->speed*(sinf(p->yaw)*forward+cosf(p->yaw)*strafe);
    wanted.z += scene->speed*(-cosf(p->yaw)*forward+sinf(p->yaw)*strafe);
    if ((wanted.x!=old.x || wanted.z!=old.z) &&
        !walk_try_step(scene,p,wanted)) {
        /* Slide along blocked surfaces or unsupported edges,
           while checking floor support for each partial move. */
        Vec3 xmove={wanted.x,p->eye.y,p->eye.z};
        (void)walk_try_step(scene,p,xmove);
        Vec3 zmove={p->eye.x,p->eye.y,wanted.z};
        (void)walk_try_step(scene,p,zmove);
    }
    if (p->grounded) {
        /* Stable: keep the feet exactly on their supported surface.
           We don't use gravity on grounded frames. */
        p->vy=0.f;
    } else {
        const float old_feet=p->eye.y-scene->eye_height;
        p->vy -= scene->gravity;
        p->eye.y += p->vy;
        const float feet=p->eye.y-scene->eye_height;
        float floor=0.f;
        /* A falling player can cross the entire step threshold
           between frames; use the old vertical position for
           a proper crossing test instead of newFeet+step. */
        if (p->vy<=0.f &&
            walk_ground(scene,p->eye.x,p->eye.z,
                        old_feet+scene->step_height,&floor) &&
            feet<=floor+scene->step_height) {
            p->eye.y=floor+scene->eye_height;
            p->vy=0.f;
            p->grounded=true;
            p->landings++;
            xsdk_log_write("HL1_BMS_WALK: LANDED ground_mm=%d",
                           (int)(floor*1000.f));
        }
    }
    if (p->eye.y < -1.5f) {
        p->recoveries++;
        xsdk_log_write("HL1_BMS_WALK: FALL_RECOVERY count=%u",
                       (unsigned)p->recoveries);
        walk_reset(scene,p);
    }
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
    xsdk_log_write("HL1_BMS_WALK: BOOT_BEGIN");
    xsdk_log_write("HL1_BMS_WALK: INDEXED_GPU_WORLD_AND_FREE_FLY");
    xsdk_log_write("HL1_BMS_WALK: XENIA_ONLY_EXPERIMENTAL_LUT");
    xsdk_log_write("HL1_BMS_WALK: BLACK_MESA_ALPHA5_WALKER_V1");
    xsdk_log_write("HL1_BMS_WALK: Start toggles ORBIT/WALK; D-pad moves; LB/RB turns");
    xsdk_log_write("HL1_BMS_WALK: A or Y jumps; B respawns; Back exits; right stick looks");
    xsdk_log_write("HL1_BMS_WALK: stable floor-support blocking, gravity and basic walls; NO BSP hulls");
    xsdk_log_write("HL1_INPUT: DIAGNOSTIC_MODE all 4 virtual gamepad slots tested");
    World world = {0};
    WalkScene scene = {0};
    WalkPlayer player = {0};
    XsdkShader vertex = {0}, pixel = {0};
    uint32_t width = 0, height = 0, submitted = 0;
    int gpu_open = 0, display_open = 0, exit_code = 1;
    if (!read_file(&world) || !validate(&world)) {
        xsdk_log_write("HL1_BMS_WALK: SCENE_LOAD_OR_VALIDATE_FAIL");
        goto cleanup;
    }
    NTSTATUS status = xsdk_gpu_open();
    xsdk_log_write("HL1_BMS_WALK: gpu_open = 0x%08X", (unsigned)status);
    if (!NT_SUCCESS(status)) goto cleanup;
    gpu_open = 1;
    status = xsdk_shader_load(&vertex, hl1_world_vert.container, hl1_world_vert.container_bytes);
    if (NT_SUCCESS(status))
        status = xsdk_shader_load(&pixel, hl1_world_frag.container, hl1_world_frag.container_bytes);
    if (NT_SUCCESS(status))
        status = xsdk_shader_set_vertex_layout(&vertex, layout, 2,
                                                (const uint32_t[]){STRIDE_BYTES});
    xsdk_log_write("HL1_BMS_WALK: shaders_load = 0x%08X", (unsigned)status);
    if (!NT_SUCCESS(status)) goto cleanup;
    if (!upload_geometry(&world)) {
        xsdk_log_write("HL1_BMS_WALK: GPU_MESH_UPLOAD_FAIL");
        goto cleanup;
    }
    xsdk_log_write("HL1_BMS_WALK: DRAW_BATCHES=%u indices=%u",
                   (unsigned)((world.indices+65531U)/65532U),
                   (unsigned)world.indices);
    if (!walk_build(&world, &scene)) {
        xsdk_log_write("HL1_BMS_WALK: COLLISION_BUILD_FAIL");
        goto cleanup;
    }
    /* Optional plain-text Source BSP player spawn: X Y Z in original
     * Source engine coordinates. No game assets are bundled in the XEX.
     * Coordinates are transformed using the same H360 normalization
     * as the GPU mesh. Missing or unsupported spawn falls back to the
     * safety-scored floor centroid. */
    {
        const char *spawn_paths[] = {
            "game:\\assets\\bms_spawn.txt",
            "assets/bms_spawn.txt"
        };
        FILE *sf=NULL;
        for (uint32_t i=0; i<2U && !sf; ++i) sf=fopen(spawn_paths[i],"rb");
        if (sf) {
            char line[256]={0};
            if (fgets(line,sizeof line,sf)) {
                char *e1=NULL, *e2=NULL, *e3=NULL;
                float source_x=strtof(line,&e1);
                float source_y=e1 ? strtof(e1,&e2) : 0.f;
                float source_z=e2 ? strtof(e2,&e3) : 0.f;
                if (e1!=line && e2!=e1 && e3!=e2 &&
                    isfinite(source_x) && isfinite(source_y) &&
                    isfinite(source_z)) {
                    Vec3 candidate={
                        (source_x-world.center[0])*world.inverse_scale,
                        (source_z-world.center[1])*world.inverse_scale,
                        (-source_y-world.center[2])*world.inverse_scale
                    };
                    float ground=0.f;
                    if (walk_ground(&scene,candidate.x,candidate.z,
                                    candidate.y+scene.eye_height,&ground) &&
                        fabsf(candidate.y-ground)<scene.eye_height*3.f) {
                        scene.spawn=(Vec3){candidate.x,ground,candidate.z};
                        xsdk_log_write("HL1_BMS_WALK: SOURCE_SPAWN_ACCEPTED x_mm=%d floor_mm=%d z_mm=%d",
                                       (int)(candidate.x*1000.f),
                                       (int)(ground*1000.f),(int)(candidate.z*1000.f));
                    } else {
                        xsdk_log_write("HL1_BMS_WALK: SOURCE_SPAWN_UNSUPPORTED using_safe_floor");
                    }
                } else {
                    xsdk_log_write("HL1_BMS_WALK: SOURCE_SPAWN_PARSE_FAIL using_safe_floor");
                }
            }
            fclose(sf);
        } else {
            xsdk_log_write("HL1_BMS_WALK: SOURCE_SPAWN_NOT_PROVIDED using_safe_floor");
        }
    }
    walk_reset(&scene, &player);
    xsdk_log_write("HL1_BMS_WALK: PLAYER_SPAWN x_mm=%d y_mm=%d z_mm=%d",
                   (int)(player.eye.x*1000.f), (int)(player.eye.y*1000.f),
                   (int)(player.eye.z*1000.f));

    const XsdkDepthStencil depth = {
        .depth_test = true, .depth_write = true,
        .depth_compare = XSDK_COMPARE_LESS_EQUAL
    };
    XsdkDepthStencilState depth_state;
    xsdk_gpu_depth_stencil_state(&depth_state, &depth);
    status = xsdk_present_open(XSDK_DISPLAY_720P, 2, 1, &width, &height);
    xsdk_log_write("HL1_BMS_WALK: present_open = 0x%08X (%ux%u)",
                   (unsigned)status, (unsigned)width, (unsigned)height);
    if (!NT_SUCCESS(status)) goto cleanup;
    display_open = 1;

    float orbit_yaw = 0.70f, orbit_pitch = 0.64f, distance = DISTANCE;
    bool walking = false;
    bool last_toggle = false;
    uint32_t previous_slot = UINT32_MAX;
    uint32_t previous_buttons = UINT32_MAX;
    uint32_t input_success_frames = 0;
    uint32_t input_active_frames = 0;
    uint16_t previous_motion_buttons = 0;
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
        const bool toggle = controller && (input.gamepad.buttons & INPUT_GAMEPAD_START);
        if (toggle && !last_toggle) {
            walking = !walking;
            if (walking) walk_reset(&scene, &player);
            xsdk_log_write("HL1_BMS_WALK: MODE=%s frame=%u player_slot=%u",
                           walking ? "WALK" : "ORBIT", (unsigned)frame,
                           (unsigned)controller_slot);
        }
        last_toggle = toggle;
        if (walking) {
            const uint16_t buttons = controller ? input.gamepad.buttons : 0U;
            const float digital_forward =
                ((buttons & INPUT_GAMEPAD_DPAD_UP) ? 1.f : 0.f) -
                ((buttons & INPUT_GAMEPAD_DPAD_DOWN) ? 1.f : 0.f);
            const float digital_strafe =
                ((buttons & INPUT_GAMEPAD_DPAD_RIGHT) ? 1.f : 0.f) -
                ((buttons & INPUT_GAMEPAD_DPAD_LEFT) ? 1.f : 0.f);
            const float turn =
                ((buttons & INPUT_GAMEPAD_RIGHT_SHOULDER) ? 1.f : 0.f) -
                ((buttons & INPUT_GAMEPAD_LEFT_SHOULDER) ? 1.f : 0.f);
            const float forward_stick=stick(input.gamepad.thumb_ly);
            const float strafe_stick=stick(input.gamepad.thumb_lx);
            const float forward=forward_stick != 0.f ? forward_stick : digital_forward;
            const float strafe=strafe_stick != 0.f ? strafe_stick : digital_strafe;
            const bool jump=(buttons & (INPUT_GAMEPAD_A|INPUT_GAMEPAD_Y)) != 0U;
            const Vec3 old=player.eye;
            const bool old_grounded=player.grounded;
            if (buttons & INPUT_GAMEPAD_B) {
                walk_reset(&scene,&player);
                xsdk_log_write("HL1_BMS_WALK: RESET frame=%u",(unsigned)frame);
            } else {
                walk_tick(&scene,&player,forward,strafe,
                          turn+stick(input.gamepad.thumb_rx),
                          stick(input.gamepad.thumb_ry),jump);
            }
            const uint16_t motion_buttons = buttons &
                (INPUT_GAMEPAD_DPAD_UP | INPUT_GAMEPAD_DPAD_DOWN |
                 INPUT_GAMEPAD_DPAD_LEFT | INPUT_GAMEPAD_DPAD_RIGHT |
                 INPUT_GAMEPAD_LEFT_SHOULDER | INPUT_GAMEPAD_RIGHT_SHOULDER |
                 INPUT_GAMEPAD_A | INPUT_GAMEPAD_Y);
            if ((motion_buttons != previous_motion_buttons || frame % 120U == 0U) &&
                (motion_buttons || old.x!=player.eye.x || old.z!=player.eye.z ||
                 old_grounded!=player.grounded)) {
                xsdk_log_write("HL1_BMS_WALK: PLAYER frame=%u x_mm=%d y_mm=%d z_mm=%d on_floor=%u",
                               (unsigned)frame, (int)(player.eye.x*1000.f),
                               (int)(player.eye.y*1000.f), (int)(player.eye.z*1000.f),
                               (unsigned)player.grounded);
            }
            previous_motion_buttons=motion_buttons;
        } else {
            previous_motion_buttons=0;
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
            xsdk_log_write("HL1_BMS_WALK: FRAME_BEGIN_FAIL frame=%u status=0x%08X",
                           (unsigned)frame, (unsigned)status);
            exit_code = 3;
            break;
        }
        Matrix matrix = walking
            ? fly_projection(player.yaw, player.pitch, player.eye, (float)width / height)
            : view_projection(orbit_yaw, orbit_pitch, distance, (float)width / height);
        xsdk_gpu_set_target(width, height);
        xsdk_gpu_set_depth_stencil(&depth_state);
        xsdk_gpu_set_shaders(&vertex, &pixel);
        xsdk_built_shader_set_literals(&hl1_world_vert);
        xsdk_built_shader_set_literals(&hl1_world_frag);
        xsdk_gpu_set_vertex_constants(HL1_WORLD_VERT_PUSH, matrix.m, HL1_WORLD_VERT_PUSH_REGISTERS);
        xsdk_gpu_set_vertices(0, xsdk_gpu_address(world.gpu_vertices), (uint32_t)world.gpu_bytes);
        /* xsdk supports at most 65535 indices per indexed draw.
         * 65532 is a triangle multiple AND 32-bit aligned for
         * packed 16-bit indices; ensure large BMS maps don't overflow. */
        for (uint32_t first=0U;first<world.indices;) {
            uint32_t count=world.indices-first;
            if (count>65532U) count=65532U;
            xsdk_gpu_draw_indexed(XSDK_PRIMITIVE_TRIANGLE_LIST,
                                  xsdk_gpu_address(world.gpu_indices)+first*2U,
                                  count,false);
            first+=count;
        }
        xsdk_gpu_resolve(target, width, height, 0xFF161E2CU);
        xsdk_present_end();
        submitted++;
        if (frame == 1 || frame == 60 || frame == 180 || frame == MAX_FRAMES-1)
            xsdk_log_write("HL1_BMS_WALK: GPU_MAP_FRAME_SUBMITTED frame=%u triangles=%u",
                           (unsigned)frame, (unsigned)(world.indices / 3));
    }
    XsdkPresentStats stats = {0};
    xsdk_present_stats(&stats);
    xsdk_log_write("HL1_BMS_WALK: FRAMES=%u VBLANKS=%u FLIPS=%u MISSED=%u",
                   (unsigned)submitted, (unsigned)stats.vblanks,
                   (unsigned)stats.flips, (unsigned)stats.missed);
    xsdk_log_write("HL1_INPUT: SUMMARY success_frames=%u active_frames=%u",
                   (unsigned)input_success_frames, (unsigned)input_active_frames);
    xsdk_log_write("HL1_BMS_WALK: SUMMARY MODE=%s",
                   walking ? "WALK" : "ORBIT");
    xsdk_log_write("HL1_BMS_WALK: PHYSICS blocked_edges=%u blocked_walls=%u jumps=%u landings=%u recoveries=%u",
                   (unsigned)player.blocked_edges, (unsigned)player.blocked_walls,
                   (unsigned)player.jumps, (unsigned)player.landings,
                   (unsigned)player.recoveries);
    xsdk_log_write("HL1_BMS_WALK: MAP_VISIBILITY_REQUIRES_USER_CONFIRMATION");
    if (submitted) (void)xsdk_thread_sleep(1500);

cleanup:
    free(scene.faces);
    if (display_open) xsdk_present_close();
    if (world.gpu_indices) xsdk_gpu_free(world.gpu_indices);
    if (world.gpu_vertices) xsdk_gpu_free(world.gpu_vertices);
    xsdk_shader_free(&pixel);
    xsdk_shader_free(&vertex);
    if (gpu_open) {
        const NTSTATUS close_status = xsdk_gpu_close();
        xsdk_log_write("HL1_BMS_WALK: gpu_close = 0x%08X", (unsigned)close_status);
    }
    free(world.file);
    xsdk_log_write("HL1_BMS_WALK: BOOT_COMPLETE");
    return exit_code;
}
