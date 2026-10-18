/* Hor+ (display.aspect): the 2D test of a program batch, apart from
 * nv2a_backend_common.c so the tests that link only the common helpers
 * do not need the vertex interpreter. */
#include "nv2a_backend_common.h"
#include "nv2a_vsh_cpu.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int nv2a_aspect_due(const struct nv2a_aspect_gate *gate)
{
    return gate->two_d == 1 || gate->n < 16 || (gate->n & 255u) == 0;
}

int nv2a_aspect_scan(struct nv2a_aspect_gate *gate,
                     const uint32_t prog[][4], uint32_t start, float c[][4],
                     const float *vf, unsigned nf4, const int *slot, uint32_t count,
                     float (*xy)[2], float *lo, float *hi)
{
    float in[NV2A_VSH_INPUTS][4], w0 = 1.0f;
    Nv2aVshOut out;
    uint32_t step = !xy && count > 32 ? count / 32 : 1, j;
    unsigned k;
    int check = nv2a_aspect_due(gate);

    gate->n++;
    if (!check || !count)
        return 0;
    for (j = 0; j < count; j += step) {
        float x;
        memset(in, 0, sizeof in);
        for (k = 0; k < nf4; k++)
            if (slot[k] >= 0 && slot[k] < NV2A_VSH_INPUTS)
                memcpy(in[slot[k]], vf + ((size_t)j * nf4 + k) * 4u, 16);
        nv2a_vsh_run((const uint32_t (*)[4])prog, start, (float (*)[4])c, 0,
                     (const float (*)[4])in, &out);
        if (!out.ok)
            return 0;
        x = out.o[NV2A_VSH_O_POS][0];
        if (xy) {
            xy[j][0] = x;
            xy[j][1] = out.o[NV2A_VSH_O_POS][1];
        }
        if (j == 0 ? !nv2a_aspect_layer_w(out.o[NV2A_VSH_O_POS][3])
                   : out.o[NV2A_VSH_O_POS][3] != w0) {
            if (gate->two_d < 1)
                gate->two_d = 0;
            return 0;
        }
        if (j == 0) {
            w0 = out.o[NV2A_VSH_O_POS][3];
            *lo = *hi = x;
        } else {
            if (x < *lo) *lo = x;
            if (x > *hi) *hi = x;
        }
    }
    gate->two_d = 1;
    return 1;
}

int nv2a_aspect_full_tris(const float (*xy)[2], const uint32_t *idx, uint32_t ntri,
                          uint32_t w)
{
    uint32_t t;

    for (t = 0; t < ntri; t++) {
        float a = xy[idx[3 * t]][0], b = xy[idx[3 * t + 1]][0], c = xy[idx[3 * t + 2]][0];
        if (nv2a_aspect_full_width(fminf(a, fminf(b, c)), fmaxf(a, fmaxf(b, c)), w))
            return 1;
    }
    return 0;
}

/* Union-find over triangles: parent links, path halving. */
static uint32_t uf_find(uint32_t *up, uint32_t a)
{
    while (up[a] != a) {
        up[a] = up[up[a]];
        a = up[a];
    }
    return a;
}

/* The pieces moved to a side in the current and the previous frame
 * (nv2a_aspect_anchor_tris), guest pixels; full, later pieces go unrecorded. */
enum { ANCHOR_KEEP = 128 };
struct anchor_set {
    uint32_t n;
    int      layout;   /* a 2D screen was drawn (nv2a_aspect_layout_note, the cover rules) */
    float    cover;    /* the frame's cover so far: each piece's ext over its target's area */
    float    box[ANCHOR_KEEP][4];
    uint8_t  place[ANCHOR_KEEP];
};
static struct anchor_set s_anchor[2];
static uint64_t s_anchor_frame;
static int s_aspect_trace;

void nv2a_aspect_trace_set(int on)
{
    s_aspect_trace = on;
}

static int boxes_touch(const float *a, const float *b, float gap)
{
    return !(b[1] > a[3] || a[1] > b[3] || b[0] > a[2] + gap || a[0] > b[2] + gap);
}

/* A piece nv2a_aspect_anchor left centred: the side of a piece it touches
 * that moved last frame, when it lies on that side of the middle and is
 * narrow enough to be moved at all. */
static int anchor_inherit(const float *b, uint32_t w, float gap)
{
    const struct anchor_set *prev = &s_anchor[(s_anchor_frame + 1u) & 1u];
    float mid = (b[0] + b[2]) * 0.5f, half = (float)w * 0.5f;
    uint32_t i;

    if (b[2] - b[0] > half)
        return NV2A_PLACE_CENTRE;
    for (i = 0; i < prev->n; i++) {
        int p = prev->place[i];
        if (!boxes_touch(prev->box[i], b, gap))
            continue;
        if ((p == NV2A_PLACE_LEFT && mid < half) || (p == NV2A_PLACE_RIGHT && mid > half))
            return p;
    }
    return NV2A_PLACE_CENTRE;
}

static void anchor_record(const float *b, int p)
{
    struct anchor_set *cur = &s_anchor[s_anchor_frame & 1u];
    if (p == NV2A_PLACE_CENTRE || cur->n >= ANCHOR_KEEP)
        return;
    memcpy(cur->box[cur->n], b, sizeof cur->box[0]);
    cur->place[cur->n++] = (uint8_t)p;
}

static void anchor_frame(uint64_t frame)
{
    if (frame == s_anchor_frame)
        return;
    /* last frame's set becomes prev; one from further back is stale */
    if (frame != s_anchor_frame + 1u) {
        s_anchor[(frame + 1u) & 1u].n = 0;
        s_anchor[(frame + 1u) & 1u].layout = 0;
    }
    s_anchor_frame = frame;
    s_anchor[frame & 1u].n = 0;
    s_anchor[frame & 1u].layout = 0;
    s_anchor[frame & 1u].cover = 0;
}

int nv2a_aspect_piece_cover(const float b[4], uint32_t w, uint32_t h, float *in, float *ext)
{
    float fw = (float)w, fh = (float)h, x0, x1, y0, y1;

    *in = *ext = 0.0f;
    /* Not on the frame's layer: a sprite near or behind the eye, through an
     * orthographic program, projects to thousands of pixels (BLiNX 2's 1-1
     * puts 2-triangle quads at x 1e5..1e8 every couple of seconds). Its
     * box would cover the frame many times over. */
    if (!(b[2] - b[0] <= 4.0f * fw) || !(b[3] - b[1] <= 4.0f * fh)
        || b[2] < -fw || b[0] > 2.0f * fw || b[3] < -fh || b[1] > 2.0f * fh)
        return 0;
    y0 = fmaxf(b[1], 0.0f);
    y1 = fminf(b[3], fh);
    if (y1 <= y0)
        return 1;
    x0 = fmaxf(b[0], 0.0f);
    x1 = fminf(b[2], fw);
    if (x1 > x0)
        *in = (x1 - x0) * (y1 - y0);
    x0 = fmaxf(b[0], -fw);
    x1 = fminf(b[2], 2.0f * fw);
    if (x1 > x0)
        *ext = (x1 - x0) * (y1 - y0);
    return 1;
}

void nv2a_aspect_layout_note(uint64_t frame)
{
    anchor_frame(frame);
    if (s_aspect_trace && !s_anchor[frame & 1u].layout)
        fprintf(stderr, "[aspect.layout] flip %llu: full-width picture\n",
                (unsigned long long)frame);
    s_anchor[frame & 1u].layout = 1;
}

unsigned nv2a_aspect_anchor_tris(const float (*xy)[2], const uint32_t *idx, uint32_t ntri,
                                 uint32_t w, uint32_t h, uint64_t frame, uint8_t *place)
{
    static float (*box)[4];
    static uint32_t *up, cap;
    static uint8_t *off;
    /* The gap that still joins two pieces: a word space of the HUD font
     * and the seams of a tiled panel, not the space between two corners. */
    float gap = (float)h / 30.0f;
    struct anchor_set *cur;
    float full;
    uint32_t t, u, runs;

    anchor_frame(frame);
    if (!ntri)
        return 0;
    if (ntri > NV2A_ASPECT_TRIS_MAX) {
        float lo = xy[idx[0]][0], hi = lo;
        int p;
        for (t = 0; t < ntri * 3u; t++) {
            float x = xy[idx[t]][0];
            if (x < lo) lo = x;
            if (x > hi) hi = x;
        }
        p = nv2a_aspect_anchor(lo, hi, w);
        memset(place, p, ntri);
        return 1;
    }
    if (cap < ntri) {
        float (*nb)[4] = (float (*)[4])realloc(box, (size_t)NV2A_ASPECT_TRIS_MAX * sizeof *box);
        uint32_t *nu = nb ? (uint32_t *)realloc(up, (size_t)NV2A_ASPECT_TRIS_MAX * sizeof *up)
                          : NULL;
        uint8_t *no = nu ? (uint8_t *)realloc(off, NV2A_ASPECT_TRIS_MAX) : NULL;
        if (nb)
            box = nb;
        if (nu)
            up = nu;
        if (!nb || !nu || !no) {
            memset(place, NV2A_PLACE_CENTRE, ntri);
            return 1;
        }
        off = no;
        cap = NV2A_ASPECT_TRIS_MAX;
    }
    for (t = 0; t < ntri; t++) {
        const float *a = xy[idx[3 * t]], *b = xy[idx[3 * t + 1]], *c = xy[idx[3 * t + 2]];
        box[t][0] = fminf(a[0], fminf(b[0], c[0]));
        box[t][1] = fminf(a[1], fminf(b[1], c[1]));
        box[t][2] = fmaxf(a[0], fmaxf(b[0], c[0]));
        box[t][3] = fmaxf(a[1], fmaxf(b[1], c[1]));
        up[t] = t;
        /* Off the layer on its own (a triangle crossing the eye plane
         * projects to +-1e8): joined to its neighbours it would make their
         * whole piece off-layer, and the HUD beside it would go centred. */
        {
            float in, ext;
            off[t] = !nv2a_aspect_piece_cover(box[t], w, h, &in, &ext);
        }
    }
    for (t = 0; t < ntri; t++)
        for (u = t + 1; u < ntri; u++) {
            if (off[t] || off[u])
                continue;
            /* Same rows (the y ranges overlap) and the x ranges within the
             * gap of each other. */
            if (box[u][1] > box[t][3] || box[t][1] > box[u][3])
                continue;
            if (box[u][0] > box[t][2] + gap || box[t][0] > box[u][2] + gap)
                continue;
            {
                uint32_t ra = uf_find(up, t), rb = uf_find(up, u);
                if (ra != rb)
                    up[rb] = ra;
            }
        }
    /* Each root's box grows to its piece's; then a triangle takes its
     * piece's placement. */
    for (t = 0; t < ntri; t++) {
        uint32_t r = uf_find(up, t);
        if (r != t) {
            box[r][0] = fminf(box[r][0], box[t][0]);
            box[r][1] = fminf(box[r][1], box[t][1]);
            box[r][2] = fmaxf(box[r][2], box[t][2]);
            box[r][3] = fmaxf(box[r][3], box[t][3]);
        }
    }
    /* Each piece's placement, decided at its root (up[r] == r) first, and
     * the layout rules (nv2a_aspect_layout_note): a piece covering a third
     * of the frame is a 2D screen's own picture; pieces that together
     * cover the frame, counted a frame to each side (a panel parked
     * off-screen to slide in), are a screen tiled from many batches. Half
     * the frame keeps a screen that was one last frame, so its panels
     * sliding out stay together to the end. A busy HUD covers about half
     * at most (BLiNX 2's mission list, 52%), so the mission list drawn
     * right after a screen stays centred with it. The cover is a sum of
     * fractions, each piece's area over its own target's, since one frame
     * draws into the 640x480 frame and its 320x240 post buffers alike. */
    cur = &s_anchor[frame & 1u];
    full = s_anchor[(frame + 1u) & 1u].layout ? 0.5f : 1.0f;
    for (t = 0; t < ntri; t++)
        if (up[t] == t) {
            float in, ext;
            int p;
            if (off[t] || !nv2a_aspect_piece_cover(box[t], w, h, &in, &ext)) {
                place[t] = NV2A_PLACE_CENTRE;
                continue;
            }
            cur->cover += ext / ((float)w * (float)h);
            if (!cur->layout && (in >= (float)w * (float)h / 3.0f
                                 || cur->cover >= full)) {
                if (s_aspect_trace)
                    fprintf(stderr, "[aspect.layout] flip %llu: piece x %g..%g y %g..%g"
                            " (%u tris), cover %.0f%%\n", (unsigned long long)frame,
                            box[t][0], box[t][2], box[t][1], box[t][3], ntri,
                            100.0f * cur->cover);
                cur->layout = 1;
            }
            p = nv2a_aspect_anchor(box[t][0], box[t][2], w);
            if (p == NV2A_PLACE_CENTRE)
                p = anchor_inherit(box[t], w, gap);
            place[t] = (uint8_t)p;
            /* Recorded on a layout frame too, so a cluster is whole on the
             * first frame its pieces move again. */
            anchor_record(box[t], p);
        }
    if (s_anchor[0].layout || s_anchor[1].layout) {
        memset(place, NV2A_PLACE_CENTRE, ntri);
        return 1;
    }
    runs = 0;
    for (t = 0; t < ntri; t++) {
        place[t] = place[uf_find(up, t)];
        if (!t || place[t] != place[t - 1])
            runs++;
    }
    return runs;
}
