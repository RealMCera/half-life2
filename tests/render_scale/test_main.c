/* The render-scale and present helpers (nv2a_backend_common.h), and the
 * guarantee they give at stock: every one is the identity at scale 1. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nv2a_backend_common.h"
#include "d3d8_hlsl.h"
#include "d3d8_msl.h"
#include "nv2a_vsh_cpu.h"

#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int rect_is(const struct nv2a_rect *r, int x, int y, int w, int h)
{
    return r->x == x && r->y == y && r->w == w && r->h == h;
}

/* A fake query pool for the tracker: every query reports s_samples. */
static uint64_t s_samples;
static int s_q;
static void *q_get(void *c) { (void)c; return &s_q; }
static void q_put(void *c, void *q) { (void)c; (void)q; }
static void q_begin(void *c, void *q) { (void)c; (void)q; }
static void q_end(void *c, void *q) { (void)c; (void)q; }
static int q_result(void *c, void *q, int f, uint64_t *n) { (void)c; (void)q; (void)f; *n = s_samples; return 1; }
static uint64_t q_now(void *c) { (void)c; return 0; }

static uint32_t occ_run(unsigned scale, uint64_t samples)
{
    static const struct nv2a_occ_ops ops = { q_get, q_put, q_begin, q_end, q_result, q_now, NULL };
    static struct nv2a_occ o;
    static uint32_t mem[64];
    nv2a_occ_init(&o, &ops, (uint8_t *)mem);
    nv2a_occ_set_scale(&o, scale);
    s_samples = samples;
    nv2a_occ_zpass(&o, 1);
    nv2a_occ_zpass(&o, 0);
    nv2a_occ_report(&o, 16, 1);
    return mem[4 + 2];       /* the report's count word */
}

/* nv2a_snap_uv (D3D8_HLSL_SNAP_UV, D3D8_MSL_SNAP_UV) in C, expression for
 * expression: the reference the shader text is checked against below. */
static void snap_uv(float uv[2], const float pos[2], const float dx[2],
                    const float dy[2], float n)
{
    const float eps = 1.0f / 64.0f;
    if (n > 1.0f && fabsf(dx[1]) <= eps && fabsf(dy[0]) <= eps
            && fabsf(fabsf(dx[0]) * n - 1.0f) <= eps && fabsf(fabsf(dy[1]) * n - 1.0f) <= eps) {
        float d0 = (floorf(pos[0] / n) + 0.5f) * n - pos[0];
        float d1 = (floorf(pos[1] / n) + 0.5f) * n - pos[1];
        uv[0] += dx[0] * d0 + dy[0] * d1;
        uv[1] += dx[1] * d0 + dy[1] * d1;
    }
}

/* The texel coordinate host pixel (px, py) gets for the affine mapping
 * uv = o + dx * px + dy * py, after the snap. */
static int snap_at(float px, float py, float ou, float ov, float dxu, float dxv,
                   float dyu, float dyv, float n, float eu, float ev)
{
    float uv[2], pos[2] = {px, py}, dx[2] = {dxu, dxv}, dy[2] = {dyu, dyv};
    uv[0] = ou + dxu * px + dyu * py;
    uv[1] = ov + dxv * px + dyv * py;
    snap_uv(uv, pos, dx, dy, n);
    return fabsf(uv[0] - eu) < 1e-3f && fabsf(uv[1] - ev) < 1e-3f;
}

/* A quad of two triangles over x0..x1, y0..y1, into xy and idx at q. */
static void quad(float (*xy)[2], uint32_t *idx, unsigned q, float x0, float x1,
                 float y0, float y1)
{
    float (*v)[2] = xy + 4u * q;
    uint32_t *i = idx + 6u * q, b = 4u * q;
    v[0][0] = x0; v[0][1] = y0; v[1][0] = x1; v[1][1] = y0;
    v[2][0] = x1; v[2][1] = y1; v[3][0] = x0; v[3][1] = y1;
    i[0] = b; i[1] = b + 1; i[2] = b + 2; i[3] = b; i[4] = b + 2; i[5] = b + 3;
}

static struct nv2a_stage stage(int mode, uint32_t color, uint32_t filter)
{
    struct nv2a_stage t;
    memset(&t, 0, sizeof t);
    t.mode = mode;
    t.color = color;
    t.filter = filter;
    t.width = 1024;
    t.height = 512;
    return t;
}

int main(void)
{
    struct nv2a_rect r;
    uint32_t hw, hh, x, y;

    /* ---- host options: zero is stock ---- */
    CHECK(nv2a_host_opts()->render_scale == 0);
    CHECK(nv2a_host_opts()->present_filter == NV2A_PRESENT_NEAREST);
    CHECK(nv2a_host_opts()->fullscreen == 0);
    CHECK(nv2a_host_render_scale() == 1);

    /* ---- host size ---- */
    CHECK(nv2a_host_size(640, 480, 0, 16384, &hw, &hh) == 1 && hw == 640 && hh == 480);
    CHECK(nv2a_host_size(640, 480, 1, 16384, &hw, &hh) == 1 && hw == 640 && hh == 480);
    CHECK(nv2a_host_size(640, 480, 2, 16384, &hw, &hh) == 2 && hw == 1280 && hh == 960);
    CHECK(nv2a_host_size(256, 256, 3, 16384, &hw, &hh) == 3 && hw == 768 && hh == 768);
    CHECK(nv2a_host_size(640, 480, 4, 16384, &hw, &hh) == 4 && hw == 2560 && hh == 1920);
    /* Clamp: a 4096 surface at 4x does not fit 8192, 2x does. */
    CHECK(nv2a_host_size(4096, 64, 4, 8192, &hw, &hh) == 2 && hw == 8192 && hh == 128);
    CHECK(nv2a_host_size(4096, 4096, 4, 4096, &hw, &hh) == 1 && hw == 4096);

    /* Hor+ padding: none at stock; at 16:9 the 4:3-shaped targets get
     * round((h*16/9 - w)/2) a side and the others nothing; the mapping
     * constants centre the guest columns in the padded width. */
    CHECK(nv2a_host_pad(640, 480) == 0);
    CHECK(nv2a_host_window_w(640, 480, 1) == 640);   /* stock window */
    CHECK(nv2a_host_window_w(640, 480, 3) == 1920);
    {
        struct nv2a_host_opts o;
        struct nv2a_pb_vsh v;
        struct nv2a_vp_consts c;
        memset(&o, 0, sizeof o);
        memset(&v, 0, sizeof v);
        nv2a_vp_consts_fill(&v, 0x128, 640, 480, 0, &c);
        CHECK(c.c[0] == 2.0f / 640.0f && c.c[4] == -1.0f && c.c[1] == -2.0f / 480.0f);
        o.aspect_w = 16; o.aspect_h = 9;
        nv2a_host_opts_set(&o);
        CHECK(nv2a_host_pad(640, 480) == 107);
        CHECK(nv2a_host_pad(320, 240) == 53);
        CHECK(nv2a_host_pad(256, 256) == 0);
        CHECK(nv2a_host_pad(512, 512) == 0);
        CHECK(nv2a_host_pad(640, 464) == 0);      /* Burnout 3's clip: not 4:3 */
        CHECK(nv2a_host_pad(0, 480) == 0);
        /* The window takes the padded target's shape, even and at scale. */
        CHECK(nv2a_host_window_w(640, 480, 1) == 854);
        CHECK(nv2a_host_window_w(640, 480, 2) == 1708);
        o.aspect_w = 16; o.aspect_h = 10;
        nv2a_host_opts_set(&o);
        CHECK(nv2a_host_pad(640, 480) == 64);
        CHECK(nv2a_host_window_w(640, 480, 1) == 768);
        o.aspect_w = 21; o.aspect_h = 9;
        nv2a_host_opts_set(&o);
        CHECK(nv2a_host_window_w(640, 480, 1) == 1120);
        o.aspect_w = 4; o.aspect_h = 3;
        nv2a_host_opts_set(&o);
        CHECK(nv2a_host_pad(640, 480) == 0);
        CHECK(nv2a_host_window_w(640, 480, 1) == 640);
        o.aspect_w = 16; o.aspect_h = 9;
        nv2a_host_opts_set(&o);
        nv2a_vp_consts_fill(&v, 0x128, 640, 480, 107, &c);
        /* guest x = 0 -> -640/854, x = 640 -> +640/854, x = 320 -> 0 */
        CHECK(c.c[0] == 2.0f / 854.0f && c.c[4] == -640.0f / 854.0f);
        CHECK(fabsf(320.0f * c.c[0] + c.c[4]) < 1e-6f);
        CHECK(c.c[1] == -2.0f / 480.0f && c.c[5] == 1.0f);
        memset(&o, 0, sizeof o);
        nv2a_host_opts_set(&o);
        CHECK(nv2a_host_pad(640, 480) == 0);
    }

    /* ---- hor+ placement (display.aspect) ---- */
    {
        struct nv2a_pb_vsh v;
        struct nv2a_vp_consts c0, c;
        static const uint32_t box[4] = { 0, 0, 640, 480 }, part[4] = { 16, 8, 320, 240 };
        uint32_t out[4], ref[4];
        int p;

        memset(&v, 0, sizeof v);
        /* pad 0 (stock): every placement leaves the constants and the box as
         * they are, so a stock run is bit-identical whatever it decided. */
        nv2a_vp_consts_fill(&v, 0x128, 640, 480, 0, &c0);
        for (p = NV2A_PLACE_CENTRE; p <= NV2A_PLACE_COVER; p++) {
            c = c0;
            nv2a_aspect_map(&c, 640, 480, 0, p);
            CHECK(memcmp(&c, &c0, sizeof c) == 0);
            nv2a_host_box(part, 640, 480, 1280, 960, ref);
            nv2a_aspect_box(part, 640, 480, 0, 2, p, 1, out);
            CHECK(memcmp(out, ref, sizeof out) == 0);
            nv2a_aspect_box(part, 640, 480, 0, 2, p, 0, out);
            CHECK(memcmp(out, ref, sizeof out) == 0);
        }

        /* 16:9, pad 107 (854 wide): guest x 0 and 640 land at these NDC. */
        nv2a_vp_consts_fill(&v, 0x128, 640, 480, 107, &c0);
        c = c0;
        nv2a_aspect_map(&c, 640, 480, 107, NV2A_PLACE_CENTRE);
        CHECK(memcmp(&c, &c0, sizeof c) == 0);
        c = c0;
        nv2a_aspect_map(&c, 640, 480, 107, NV2A_PLACE_STRETCH);
        CHECK(c.c[0] == 2.0f / 640.0f && c.c[4] == -1.0f && c.c[1] == c0.c[1] && c.c[5] == c0.c[5]);
        c = c0;
        nv2a_aspect_map(&c, 640, 480, 107, NV2A_PLACE_LEFT);
        CHECK(c.c[0] == c0.c[0] && c.c[4] == -1.0f);                     /* x 0 at the left edge */
        c = c0;
        nv2a_aspect_map(&c, 640, 480, 107, NV2A_PLACE_RIGHT);
        CHECK(fabsf(640.0f * c.c[0] + c.c[4] - 1.0f) < 1e-6f);           /* x 640 at the right edge */
        c = c0;
        nv2a_aspect_map(&c, 640, 480, 107, NV2A_PLACE_COVER);
        CHECK(c.c[0] == 2.0f / 640.0f && c.c[4] == -1.0f);
        /* y about the centre by 854/640: row 240 stays at 0, row 0 above +1 */
        CHECK(fabsf(240.0f * c.c[1] + c.c[5]) < 1e-6f);
        CHECK(fabsf(c.c[5] - 854.0f / 640.0f) < 1e-6f);

        /* Scissors at 16:9, n 2 (host 1708 x 960). */
        nv2a_aspect_box(box, 640, 480, 107, 2, NV2A_PLACE_CENTRE, 1, out);
        CHECK(out[0] == 214 && out[2] == 1494 && out[1] == 0 && out[3] == 960);
        nv2a_aspect_box(box, 640, 480, 107, 2, NV2A_PLACE_CENTRE, 0, out);
        CHECK(out[0] == 0 && out[2] == 1708);                             /* 3D fills the pads */
        nv2a_aspect_box(part, 640, 480, 107, 2, NV2A_PLACE_CENTRE, 0, out);
        CHECK(out[0] == 246 && out[2] == 854);                            /* a 3D sub-box does not */
        nv2a_aspect_box(box, 640, 480, 107, 2, NV2A_PLACE_STRETCH, 1, out);
        CHECK(out[0] == 0 && out[2] == 1708);
        nv2a_aspect_box(box, 640, 480, 107, 2, NV2A_PLACE_COVER, 1, out);
        CHECK(out[0] == 0 && out[2] == 1708 && out[3] == 960);
        nv2a_aspect_box(part, 640, 480, 107, 2, NV2A_PLACE_LEFT, 1, out);
        CHECK(out[0] == 32 && out[2] == 640);
        nv2a_aspect_box(part, 640, 480, 107, 2, NV2A_PLACE_RIGHT, 1, out);
        CHECK(out[0] == 460 && out[2] == 1068);

        /* The placement rules. */
        CHECK(nv2a_aspect_full_width(0, 640, 640) && nv2a_aspect_full_width(-1, 641, 640));
        CHECK(!nv2a_aspect_full_width(3, 640, 640) && !nv2a_aspect_full_width(0, 637, 640));
        CHECK(nv2a_aspect_place(0, 1, 1, 0, NV2A_MOVIE_FIT) == NV2A_PLACE_CENTRE);   /* 3D */
        CHECK(nv2a_aspect_place(1, 0, 0, 0, NV2A_MOVIE_FIT) == NV2A_PLACE_CENTRE);   /* a HUD piece */
        CHECK(nv2a_aspect_place(1, 1, 0, 0, NV2A_MOVIE_BLACK) == NV2A_PLACE_STRETCH); /* fade, post */
        CHECK(nv2a_aspect_place(1, 1, 1, 0, NV2A_MOVIE_BLACK) == NV2A_PLACE_CENTRE);  /* movie */
        CHECK(nv2a_aspect_place(1, 1, 1, 0, NV2A_MOVIE_FIT) == NV2A_PLACE_STRETCH);
        CHECK(nv2a_aspect_place(1, 1, 1, 0, NV2A_MOVIE_FILL) == NV2A_PLACE_COVER);
        /* a full-width composite of an unpadded target (a 256x256 blur)
         * keeps its 4:3 shape; a padded one is stretched over the pads */
        CHECK(nv2a_aspect_place(1, 1, 0, 1, NV2A_MOVIE_BLACK) == NV2A_PLACE_CENTRE);
        CHECK(nv2a_aspect_place(1, 1, 0, 1, NV2A_MOVIE_FIT) == NV2A_PLACE_CENTRE);
        CHECK(nv2a_aspect_place(0, 1, 0, 1, NV2A_MOVIE_BLACK) == NV2A_PLACE_CENTRE);
        /* The 16x16 fade texture is no picture; a 1024-wide movie frame and
         * a 256-wide backdrop are. */
        CHECK(!nv2a_aspect_picture(16, 640) && !nv2a_aspect_picture(160, 640));
        CHECK(nv2a_aspect_picture(1024, 640) && nv2a_aspect_picture(256, 640));
        /* Screen layers: w 1, the stage HUD's fixed depths 2 and 3 and the
         * title movie's 100; a world sprite's depth is no exact whole
         * number. */
        CHECK(nv2a_aspect_layer_w(1.0f) && nv2a_aspect_layer_w(2.0f) && nv2a_aspect_layer_w(3.0f)
              && nv2a_aspect_layer_w(100.0f));
        CHECK(!nv2a_aspect_layer_w(5.64599f) && !nv2a_aspect_layer_w(259.141f)
              && !nv2a_aspect_layer_w(3.0005f));
        CHECK(!nv2a_aspect_layer_w(0.5f) && !nv2a_aspect_layer_w(-1.0f) && !nv2a_aspect_layer_w(2048.0f));
        CHECK(nv2a_aspect_anchor(16, 120, 640) == NV2A_PLACE_LEFT);
        CHECK(nv2a_aspect_anchor(520, 624, 640) == NV2A_PLACE_RIGHT);
        CHECK(nv2a_aspect_anchor(260, 380, 640) == NV2A_PLACE_CENTRE);
        CHECK(nv2a_aspect_anchor(-200, -100, 640) == NV2A_PLACE_LEFT);    /* parked stays off */
        CHECK(nv2a_aspect_anchor(700, 900, 640) == NV2A_PLACE_RIGHT);
        CHECK(nv2a_aspect_anchor(0, 400, 640) == NV2A_PLACE_CENTRE);      /* wider than w/2 */
    }

    /* ---- hor+ HUD pieces (display.hud=edges) ---- */
    {
        static float xy[64][2];
        static uint32_t idx[96];
        uint8_t pl[32];
        unsigned runs, t;

        /* One quad in the left corner: one piece, one run. */
        quad(xy, idx, 0, 16, 80, 16, 48);
        CHECK(nv2a_aspect_anchor_tris((const float (*)[2])xy, idx, 2, 640, 480, 100, pl) == 1);
        CHECK(pl[0] == NV2A_PLACE_LEFT && pl[1] == NV2A_PLACE_LEFT);
        /* A string that crosses the left third's edge: glyphs 12 px apart
         * (under the 16 px gap) are one piece and move together, placed by
         * the whole string's middle (100..280, 190: the left third). */
        for (t = 0; t < 8; t++)
            quad(xy, idx, t, 100.0f + 24.0f * (float)t, 112.0f + 24.0f * (float)t, 200, 216);
        runs = nv2a_aspect_anchor_tris((const float (*)[2])xy, idx, 16, 640, 480, 200, pl);
        CHECK(runs == 1);
        for (t = 0; t < 16; t++)
            CHECK(pl[t] == NV2A_PLACE_LEFT);
        /* Left score, centred title, right timer in one batch: three runs.
         * The right piece's two glyphs 40 px apart are two pieces, both on
         * the right. */
        quad(xy, idx, 0, 16, 96, 16, 40);
        quad(xy, idx, 1, 280, 360, 16, 40);
        quad(xy, idx, 2, 540, 560, 16, 40);
        quad(xy, idx, 3, 600, 624, 16, 40);
        runs = nv2a_aspect_anchor_tris((const float (*)[2])xy, idx, 8, 640, 480, 300, pl);
        CHECK(runs == 3);
        CHECK(pl[0] == NV2A_PLACE_LEFT && pl[1] == NV2A_PLACE_LEFT);
        CHECK(pl[2] == NV2A_PLACE_CENTRE && pl[3] == NV2A_PLACE_CENTRE);
        CHECK(pl[4] == NV2A_PLACE_RIGHT && pl[7] == NV2A_PLACE_RIGHT);
        /* Pieces on other rows do not join even when their columns touch:
         * a left icon above a full-width bar. */
        quad(xy, idx, 0, 16, 64, 16, 64);
        quad(xy, idx, 1, 0, 640, 400, 440);
        runs = nv2a_aspect_anchor_tris((const float (*)[2])xy, idx, 4, 640, 480, 400, pl);
        CHECK(runs == 2 && pl[0] == NV2A_PLACE_LEFT && pl[2] == NV2A_PLACE_CENTRE);
        CHECK(nv2a_aspect_anchor_tris((const float (*)[2])xy, idx, 0, 640, 480, 500, pl) == 0);

        /* A cluster drawn in several batches: the icon (16..96) moves
         * left; the frame beside it (90..290, middle 190) is in the left
         * third too; the lights next to the frame (290..330, middle 310,
         * centre third) stay in frame 1 and join the frame's side from
         * frame 2, as the frame moved in frame 1. A piece in the right
         * half never takes a left neighbour's side, and a frame that is
         * not the next one forgets the cluster. */
        quad(xy, idx, 0, 16, 96, 16, 80);
        CHECK(nv2a_aspect_anchor_tris((const float (*)[2])xy, idx, 2, 640, 480, 1, pl) == 1
              && pl[0] == NV2A_PLACE_LEFT);
        quad(xy, idx, 0, 90, 290, 16, 80);
        CHECK(nv2a_aspect_anchor_tris((const float (*)[2])xy, idx, 2, 640, 480, 1, pl) == 1
              && pl[0] == NV2A_PLACE_LEFT);
        quad(xy, idx, 0, 290, 316, 30, 60);
        CHECK(nv2a_aspect_anchor_tris((const float (*)[2])xy, idx, 2, 640, 480, 1, pl) == 1
              && pl[0] == NV2A_PLACE_CENTRE);
        quad(xy, idx, 0, 290, 316, 30, 60);
        CHECK(nv2a_aspect_anchor_tris((const float (*)[2])xy, idx, 2, 640, 480, 2, pl) == 1
              && pl[0] == NV2A_PLACE_LEFT);
        quad(xy, idx, 0, 300, 360, 30, 60);   /* middle 330: right half */
        CHECK(nv2a_aspect_anchor_tris((const float (*)[2])xy, idx, 2, 640, 480, 3, pl) == 1
              && pl[0] == NV2A_PLACE_CENTRE);
        quad(xy, idx, 0, 290, 316, 30, 60);
        CHECK(nv2a_aspect_anchor_tris((const float (*)[2])xy, idx, 2, 640, 480, 9, pl) == 1
              && pl[0] == NV2A_PLACE_CENTRE);

        /* A 2D screen: after a full-frame picture (noted, or a piece over
         * a third of the frame) nothing moves this frame or the next. */
        nv2a_aspect_layout_note(20);
        quad(xy, idx, 0, 16, 96, 16, 80);
        CHECK(nv2a_aspect_anchor_tris((const float (*)[2])xy, idx, 2, 640, 480, 20, pl) == 1
              && pl[0] == NV2A_PLACE_CENTRE);
        CHECK(nv2a_aspect_anchor_tris((const float (*)[2])xy, idx, 2, 640, 480, 21, pl) == 1
              && pl[0] == NV2A_PLACE_CENTRE);
        CHECK(nv2a_aspect_anchor_tris((const float (*)[2])xy, idx, 2, 640, 480, 22, pl) == 1
              && pl[0] == NV2A_PLACE_LEFT);
        quad(xy, idx, 0, 40, 600, 40, 440);
        quad(xy, idx, 1, 16, 96, 16, 30);
        CHECK(nv2a_aspect_anchor_tris((const float (*)[2])xy, idx, 4, 640, 480, 23, pl) == 1
              && pl[2] == NV2A_PLACE_CENTRE);

        /* Full width by triangle: a full-screen quad is; the four corner
         * triangles of one batch, whose bounds are the screen, are not. */
        quad(xy, idx, 0, 0, 640, 0, 480);
        CHECK(nv2a_aspect_full_tris((const float (*)[2])xy, idx, 2, 640));
        xy[0][0] = 0;   xy[0][1] = 0;   xy[1][0] = 120; xy[1][1] = 0;   xy[2][0] = 0;   xy[2][1] = 90;
        xy[3][0] = 640; xy[3][1] = 0;   xy[4][0] = 520; xy[4][1] = 0;   xy[5][0] = 640; xy[5][1] = 90;
        xy[6][0] = 0;   xy[6][1] = 480; xy[7][0] = 120; xy[7][1] = 480; xy[8][0] = 0;   xy[8][1] = 390;
        xy[9][0] = 640; xy[9][1] = 480; xy[10][0] = 520; xy[10][1] = 480; xy[11][0] = 640; xy[11][1] = 390;
        for (t = 0; t < 12; t++)
            idx[t] = t;
        CHECK(!nv2a_aspect_full_tris((const float (*)[2])xy, idx, 4, 640));
        runs = nv2a_aspect_anchor_tris((const float (*)[2])xy, idx, 4, 640, 480, 600, pl);
        CHECK(runs == 4 && pl[0] == NV2A_PLACE_LEFT && pl[1] == NV2A_PLACE_RIGHT
              && pl[2] == NV2A_PLACE_LEFT && pl[3] == NV2A_PLACE_RIGHT);
    }

    /* ---- hor+ 2D test: a program's oPos.w, one screen layer ---- */
    {
        /* mov oPos, v0 (final): w comes straight from the vertex. */
        static uint32_t prog[NV2A_VSH_SLOTS][4];
        static float cst[NV2A_VSH_CONSTANTS][4];
        float vf[4][4] = { { 10, 0, 0, 1 }, { 630, 0, 0, 1 }, { 630, 480, 0, 1 }, { 10, 480, 0, 1 } };
        float hxy[4][2], lo = 0, hi = 0;
        int slot[1] = { 0 };
        struct nv2a_aspect_gate g = { -1, 0 };
        unsigned i;

        prog[0][1] = (1u << 21) | 0x1Bu;                                 /* mov, src0 v0.xyzw */
        prog[0][2] = 2u << 26;                                           /* src0 mux v */
        prog[0][3] = (0xFu << 12) | (1u << 11) | 1u;                     /* oPos.xyzw, o reg, final */
        CHECK(nv2a_aspect_scan(&g, (const uint32_t (*)[4])prog, 0, cst, &vf[0][0], 1, slot, 4,
                               NULL, &lo, &hi) == 1);
        CHECK(g.two_d == 1 && lo == 10.0f && hi == 630.0f);
        CHECK(nv2a_aspect_scan(&g, (const uint32_t (*)[4])prog, 0, cst, &vf[0][0], 1, slot, 4,
                               hxy, &lo, &hi) == 1);
        CHECK(hxy[2][0] == 630.0f && hxy[2][1] == 480.0f);
        /* A screen layer at a fixed depth (the stage HUD's w 3) is 2D too. */
        for (i = 0; i < 4; i++)
            vf[i][3] = 3.0f;
        CHECK(nv2a_aspect_scan(&g, (const uint32_t (*)[4])prog, 0, cst, &vf[0][0], 1, slot, 4,
                               NULL, &lo, &hi) == 1);
        /* Perspective (a depth per vertex, the first one whole): 3D; after
         * 16 such batches the gate checks only every 256th. */
        for (i = 0; i < 4; i++)
            vf[i][3] = 5.0f + 0.5f * (float)i;
        g.two_d = -1;
        g.n = 0;
        for (i = 0; i < 16; i++)
            CHECK(nv2a_aspect_scan(&g, (const uint32_t (*)[4])prog, 0, cst, &vf[0][0], 1, slot, 4,
                                   NULL, &lo, &hi) == 0);
        CHECK(g.two_d == 0 && g.n == 16);
        for (i = 0; i < 4; i++)
            vf[i][3] = 1.0f;
        for (i = 16; i < 256; i++)
            CHECK(nv2a_aspect_scan(&g, (const uint32_t (*)[4])prog, 0, cst, &vf[0][0], 1, slot, 4,
                                   NULL, &lo, &hi) == 0);                /* skipped */
        CHECK(nv2a_aspect_scan(&g, (const uint32_t (*)[4])prog, 0, cst, &vf[0][0], 1, slot, 4,
                               NULL, &lo, &hi) == 1);                    /* the 256th */
        CHECK(g.two_d == 1);
    }

    /* ---- present rectangle ---- */
    /* Exact fit is the whole output, every filter. */
    CHECK(nv2a_present_rect(640, 480, 640, 480, NV2A_PRESENT_NEAREST, &r) == 0 && rect_is(&r, 0, 0, 640, 480));
    CHECK(nv2a_present_rect(640, 480, 640, 480, NV2A_PRESENT_INTEGER, &r) == 1 && rect_is(&r, 0, 0, 640, 480));
    /* Fit: pillarbox in 16:9, letterbox in a tall output. */
    CHECK(nv2a_present_rect(640, 480, 1920, 1080, NV2A_PRESENT_LINEAR, &r) == 0 && rect_is(&r, 240, 0, 1440, 1080));
    CHECK(nv2a_present_rect(640, 480, 800, 1000, NV2A_PRESENT_NEAREST, &r) == 0 && rect_is(&r, 0, 200, 800, 600));
    /* Integer: 640x480 in 1920x1080 is 2x, centred. */
    CHECK(nv2a_present_rect(640, 480, 1920, 1080, NV2A_PRESENT_INTEGER, &r) == 1 && rect_is(&r, 320, 60, 1280, 960));
    /* Integer fallback: a 2x frame in a smaller output is the fit. */
    CHECK(nv2a_present_rect(1280, 960, 1024, 768, NV2A_PRESENT_INTEGER, &r) == 0 && rect_is(&r, 0, 0, 1024, 768));
    CHECK(nv2a_present_rect(1280, 960, 1000, 700, NV2A_PRESENT_INTEGER, &r) == 0 && rect_is(&r, 33, 0, 933, 700));
    /* Odd sizes round to the nearest pixel and stay inside. */
    CHECK(nv2a_present_rect(640, 480, 1001, 333, NV2A_PRESENT_NEAREST, &r) == 0 && rect_is(&r, 278, 0, 444, 333));
    /* Empty in, empty out. */
    CHECK(nv2a_present_rect(0, 480, 640, 480, NV2A_PRESENT_NEAREST, &r) == 0 && rect_is(&r, 0, 0, 0, 0));
    CHECK(nv2a_present_rect(640, 480, 0, 0, NV2A_PRESENT_INTEGER, &r) == 0 && rect_is(&r, 0, 0, 0, 0));

    /* ---- upscale / downscale ---- */
    {
        enum { W = 5, H = 3 };
        uint32_t src[W * H], up[W * 3 * H * 3], back[W * H];
        unsigned n;
        for (x = 0; x < W * H; x++)
            src[x] = 0x01020304u * (x + 1) ^ 0xA5C3F00Fu;
        for (n = 1; n <= 3; n++) {
            nv2a_upscale_nearest32((const uint8_t *)src, W * 4, W, H, n, (uint8_t *)up, W * n * 4);
            /* Every n x n block is the source pixel. */
            for (y = 0; y < H * n; y++)
                for (x = 0; x < W * n; x++)
                    CHECK(up[y * W * n + x] == src[(y / n) * W + x / n]);
            /* Round trip: box of a nearest upscale is the input. */
            nv2a_downscale_box32((const uint8_t *)up, W * n * 4, W, H, n, (uint8_t *)back, W * 4);
            for (x = 0; x < W * H; x++)
                CHECK(back[x] == src[x]);
        }
    }
    {
        /* Box average per channel, rounded: 2x2 of 0, 255, 255, 255 -> 191;
         * 3x3 of eight 0 and one 10 -> 1 (10/9 = 1.1). Bytes, so BGRA order
         * is irrelevant; a pitch wider than the row is honoured. */
        uint8_t s2[2 * 12] = {0}, d2[8] = {0};
        uint8_t s3[3 * 12] = {0}, d3[4] = {0};
        int c;
        for (c = 0; c < 4; c++) {
            s2[4 + c] = 255;          /* (1,0) */
            s2[12 + c] = 255;         /* (0,1), pitch 12 */
            s2[16 + c] = 255;         /* (1,1) */
        }
        nv2a_downscale_box32(s2, 12, 1, 1, 2, d2, 8);
        for (c = 0; c < 4; c++)
            CHECK(d2[c] == 191);
        s3[12 + 4] = 10;              /* centre pixel's first byte */
        nv2a_downscale_box32(s3, 12, 1, 1, 3, d3, 4);
        CHECK(d3[0] == 1 && d3[1] == 0);
    }

    /* ---- visibility counts ---- */
    CHECK(nv2a_occ_scale_count(0, 1) == 0);
    CHECK(nv2a_occ_scale_count(12345, 1) == 12345);
    CHECK(nv2a_occ_scale_count(12345, 0) == 12345);
    CHECK(nv2a_occ_scale_count(0, 2) == 0);
    CHECK(nv2a_occ_scale_count(1, 2) == 1);          /* visible stays visible */
    CHECK(nv2a_occ_scale_count(3, 2) == 1);
    CHECK(nv2a_occ_scale_count(4, 2) == 1);
    CHECK(nv2a_occ_scale_count(6, 2) == 2);          /* 1.5 rounds up */
    CHECK(nv2a_occ_scale_count(4000, 2) == 1000);
    CHECK(nv2a_occ_scale_count(9 * 307200ull, 3) == 307200);
    CHECK(nv2a_occ_scale_count(4, 3) == 1);          /* (4+4)/9 rounds to 0 */
    for (uint64_t n = 1; n <= 8; n++)
        CHECK(nv2a_occ_scale_count(n, 3) != 0);
    for (uint64_t n = 1; n <= 15; n++)
        CHECK(nv2a_occ_scale_count(n, 4) != 0);
    CHECK(nv2a_occ_scale_count(0x400000000ull, 1) == 0xFFFFFFFFu);
    CHECK(nv2a_occ_scale_count(0x400000000ull, 2) == 0xFFFFFFFFu);
    CHECK(nv2a_occ_scale_count(0x400000000ull, 4) == 0x40000000u);
    /* Through the tracker: the report's word is the scaled count. */
    CHECK(occ_run(1, 4000) == 4000);
    CHECK(occ_run(2, 4000) == 1000);
    CHECK(occ_run(2, 1) == 1);
    CHECK(occ_run(2, 0) == 0);

    /* ---- rect snap: which stages (tex_scale[n].z) ---- */
    {
        enum { LIN = 0x12, SWZ = 0x06, DXT = 0x0C };      /* A8R8G8B8 linear, swizzled; DXT1 */
        const uint32_t bil = 2u << 16 | 2u << 24, pnt = 1u << 16 | 1u << 24;
        const uint32_t mag_only = 1u << 16 | 2u << 24, min_only = 2u << 16 | 1u << 24;
        struct nv2a_stage t = stage(1, LIN, bil);
        CHECK(nv2a_rect_snap_scale(&t, 2) == 2);
        CHECK(nv2a_rect_snap_scale(&t, 3) == 3);
        CHECK(nv2a_rect_snap_scale(&t, 1) == 0);          /* stock: never */
        CHECK(nv2a_rect_snap_scale(&t, 0) == 0);
        t = stage(1, LIN, mag_only);
        CHECK(nv2a_rect_snap_scale(&t, 2) == 2);
        t = stage(1, LIN, min_only);
        CHECK(nv2a_rect_snap_scale(&t, 2) == 2);
        t = stage(1, LIN, pnt);                             /* one texel, no bleed */
        CHECK(nv2a_rect_snap_scale(&t, 2) == 0);
        t = stage(1, SWZ, bil);                             /* normalised coordinates */
        CHECK(nv2a_rect_snap_scale(&t, 2) == 0);
        t = stage(1, DXT, bil);
        CHECK(nv2a_rect_snap_scale(&t, 2) == 0);
        t = stage(0, LIN, bil);                             /* stage off */
        CHECK(nv2a_rect_snap_scale(&t, 2) == 0);
        t = stage(4, LIN, bil);                             /* pass-through */
        CHECK(nv2a_rect_snap_scale(&t, 2) == 0);
        t = stage(2, LIN, bil);                             /* other modes sample as 2D */
        CHECK(nv2a_rect_snap_scale(&t, 2) == 2);
    }
    {
        /* Through the fill: .z follows the host scale; a render-target
         * stage gets 1/size and .z = 0; .w stays 0. */
        static struct nv2a_pb_vsh v;
        struct nv2a_stage st[4];
        struct nv2a_ps_consts pc;
        struct nv2a_host_opts o = { .render_scale = 2, .present_filter = NV2A_PRESENT_NEAREST };
        st[0] = stage(1, 0x12, 2u << 16 | 2u << 24);
        st[1] = stage(1, 0x06, 2u << 16 | 2u << 24);
        st[2] = stage(4, 0x12, 2u << 16 | 2u << 24);
        st[3] = stage(1, 0x12, 1u << 16 | 1u << 24);
        nv2a_ps_consts_fill(&v, st, &pc);
        CHECK(pc.tex_scale[0][2] == 0.0f && pc.tex_scale[1][2] == 0.0f);   /* scale 1 */
        nv2a_host_opts_set(&o);
        nv2a_ps_consts_fill(&v, st, &pc);
        CHECK(pc.tex_scale[0][0] == 1.0f / 1024.0f && pc.tex_scale[0][1] == 1.0f / 512.0f);
        CHECK(pc.tex_scale[0][2] == 2.0f && pc.tex_scale[0][3] == 0.0f);
        CHECK(pc.tex_scale[1][2] == 0.0f && pc.tex_scale[2][2] == 0.0f && pc.tex_scale[3][2] == 0.0f);
        nv2a_ps_consts_rt(&pc, 0, &st[0], 640, 480);
        CHECK(pc.tex_scale[0][0] == 1.0f / 640.0f && pc.tex_scale[0][1] == 1.0f / 480.0f);
        CHECK(pc.tex_scale[0][2] == 0.0f);
        nv2a_ps_consts_rt(&pc, 1, &st[1], 640, 480);   /* swizzled: normalised, kept */
        CHECK(pc.tex_scale[1][0] == 1.0f && pc.tex_scale[1][2] == 0.0f);
        o.render_scale = 0;
        nv2a_host_opts_set(&o);
        CHECK(nv2a_host_render_scale() == 1);
    }

    /* ---- rect snap: the shader arithmetic ---- */
    /* The movie at scale 2: guest column 639 is host 1278.5 and 1279.5,
     * sampled at 639.25 and 639.75 before; both read 639.5, the 1x point. */
    CHECK(snap_at(1279.5f, 600.5f, 0, 0, 0.5f, 0, 0, 0.5f, 2, 639.5f, 300.5f));
    CHECK(snap_at(1278.5f, 600.5f, 0, 0, 0.5f, 0, 0, 0.5f, 2, 639.5f, 300.5f));
    CHECK(snap_at(1278.5f, 601.5f, 0, 0, 0.5f, 0, 0, 0.5f, 2, 639.5f, 300.5f));
    CHECK(snap_at(0.5f, 0.5f, 0, 0, 0.5f, 0, 0, 0.5f, 2, 0.5f, 0.5f));
    /* Scale 3: the three host pixels of guest column 639 (639.1667,
     * 639.5, 639.8333) all read 639.5. */
    CHECK(snap_at(1917.5f, 1.5f, 0, 0, 1.0f / 3, 0, 0, 1.0f / 3, 3, 639.5f, 0.5f));
    CHECK(snap_at(1918.5f, 1.5f, 0, 0, 1.0f / 3, 0, 0, 1.0f / 3, 3, 639.5f, 0.5f));
    CHECK(snap_at(1919.5f, 1.5f, 0, 0, 1.0f / 3, 0, 0, 1.0f / 3, 3, 639.5f, 0.5f));
    /* A flipped V (dy.y < 0) is still a unity blit. */
    CHECK(snap_at(10.5f, 959.5f, 0, 480.0f, 0.5f, 0, 0, -0.5f, 2, 5.5f, 0.5f));
    /* Within the 1/64 tolerance: a mapping 1/128 off unity still snaps. */
    CHECK(snap_at(1279.5f, 0.5f, 0, 0, 0.5f + 1.0f / 256, 0, 0, 0.5f, 2,
                  (0.5f + 1.0f / 256) * 1279.5f - (0.5f + 1.0f / 256) * 0.5f, 0.5f));
    /* Not a unity blit: a stretch (dx.x * N = 0.933) or a cross term of
     * 1/32 keeps the host-rate sample. */
    CHECK(snap_at(1279.5f, 600.5f, 0, 0, 0.933f / 2, 0, 0, 0.5f, 2, 0.933f / 2 * 1279.5f, 300.25f));
    CHECK(snap_at(1279.5f, 600.5f, 0, 0, 0.5f, 1.0f / 32, 0, 0.5f, 2, 639.75f, 300.25f + 1279.5f / 32));
    CHECK(snap_at(1279.5f, 600.5f, 0, 0, 0.5f, 0, 1.0f / 32, 0.5f, 2, 639.75f + 600.5f / 32, 300.25f));
    /* Magnified 2x at scale 2 (one texel per host pixel): not snapped. */
    CHECK(snap_at(1279.5f, 600.5f, 0, 0, 0.25f, 0, 0, 0.25f, 2, 319.875f, 150.125f));
    /* .z = 0 (stock, or not eligible) and .z = 1: the input as it is. */
    CHECK(snap_at(1279.5f, 600.5f, 0, 0, 0.5f, 0, 0, 0.5f, 0, 639.75f, 300.25f));
    CHECK(snap_at(639.5f, 300.5f, 0, 0, 1, 0, 0, 1, 1, 639.5f, 300.5f));
    /* The shader text is the reference's, in both languages. */
    {
        static const char *const text[2] = { D3D8_HLSL_SNAP_UV, D3D8_MSL_SNAP_UV };
        int k;
        for (k = 0; k < 2; k++) {
            CHECK(strstr(text[k], "float n = ts.z, eps = 1.0 / 64.0;"));
            CHECK(strstr(text[k], "if (n > 1.0 && abs(dx.y) <= eps && abs(dy.x) <= eps\n"
                                  "            && abs(abs(dx.x) * n - 1.0) <= eps"
                                  " && abs(abs(dy.y) * n - 1.0) <= eps) {"));
            CHECK(strstr(text[k], "float2 d = (floor(pos / n) + 0.5) * n - pos;"));
            CHECK(strstr(text[k], "uv += dx * d.x + dy * d.y;"));
        }
        CHECK(strstr(text[0], "float2 dx = ddx(uv), dy = ddy(uv);"));
        CHECK(strstr(text[1], "float2 dx = dfdx(uv), dy = dfdy(uv);"));
        /* derivatives before the branch */
        CHECK(strstr(text[0], "ddx(uv)") < strstr(text[0], "if (n > 1.0"));
        CHECK(strstr(text[1], "dfdx(uv)") < strstr(text[1], "if (n > 1.0"));
    }

    /* ---- the options round-trip; init picks the scale up ---- */
    {
        struct nv2a_host_opts o = { .render_scale = 3, .present_filter = NV2A_PRESENT_INTEGER,
                                    .fullscreen = 1 };
        nv2a_host_opts_set(&o);
        CHECK(nv2a_host_render_scale() == 3);
        CHECK(nv2a_host_opts()->present_filter == NV2A_PRESENT_INTEGER && nv2a_host_opts()->fullscreen == 1);
        {
            static const struct nv2a_occ_ops ops = { q_get, q_put, q_begin, q_end, q_result, q_now, NULL };
            struct nv2a_occ oc;
            static uint32_t mem[8];
            nv2a_occ_init(&oc, &ops, (uint8_t *)mem);
            CHECK(oc.scale == 3);
        }
    }

    printf("render_scale: all passed\n");
    return 0;
}
