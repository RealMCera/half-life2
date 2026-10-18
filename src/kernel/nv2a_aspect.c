/* Hor+ (display.aspect): the 2D test of a program batch, apart from
 * nv2a_backend_common.c so the tests that link only the common helpers
 * do not need the vertex interpreter. */
#include "nv2a_backend_common.h"
#include "nv2a_vsh_cpu.h"
#include <math.h>
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
