/*
 * nv2a_overlay_font.c - the overlay's text: Atkinson Hyperlegible Regular
 * baked by stb_truetype into one atlas at the three text tokens' sizes
 * (R7, R8), and the metrics microui asks for.
 *
 * The atlas is coverage expanded to premultiplied white, so a glyph quad
 * takes its colour from the vertex. Rows 0..3 hold a white block whose
 * centre texel is the solid fill of every rect, so a whole layer draws
 * from one texture. Row 4 stays empty, so a glyph on the first packed row
 * sampled bilinearly (scaled text) never picks up the block's white.
 *
 * A size drawn at other than its baked size (the placement changed since
 * the bake; phase 3E rebakes on the worker) is the baked glyphs scaled,
 * which the presenters' linear sampler keeps legible.
 */
#include "nv2a_overlay_int.h"
#include "stb_truetype.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const unsigned char nv2a_ovl_font_regular[];
extern const unsigned long nv2a_ovl_font_regular_len;

/* The baked code points (R7): Basic Latin, Latin-1 and Latin Extended-A,
 * the dashes and quotes of General Punctuation, the arrows. U+FFFF is no
 * character and so maps to .notdef, the box drawn for anything else. It
 * goes first: stb_truetype packs the missing glyph once per call, for the
 * first code point that lacks one, and leaves the later ones empty. */
static const struct { int first, n; } s_ranges[] = {
    { 0xFFFF, 1 }, { 0x20, 95 }, { 0xA0, 224 }, { 0x2010, 23 }, { 0x2190, 6 },
};
#define NRANGES ((int)(sizeof s_ranges / sizeof s_ranges[0]))
#define WHITE_ROWS 4
#define GLYPH_ROW0 (WHITE_ROWS + 1)  /* one empty row of padding */

struct slot {
    int px;                                   /* the baked size */
    stbtt_packedchar *ch[NRANGES];
    float tab_adv;                            /* the widest digit, baked px */
};

static struct {
    stbtt_fontinfo info;
    float ascent;                             /* of a 1 px line */
    struct slot slot[OVL_FONT_SLOTS];
    stbtt_packedchar *store;
    uint32_t *px;
    struct nv2a_ovl_atlas atlas;
    uint64_t gen;
} s_font;

static const int *token_px(void)
{
    static int px[OVL_FONT_SLOTS];
    px[OVL_FONT_SMALL] = nv2a_ovl_theme_default.font_small;
    px[OVL_FONT_BODY] = nv2a_ovl_theme_default.font_body;
    px[OVL_FONT_TITLE] = nv2a_ovl_theme_default.font_title;
    return px;
}

void nv2a_ovl_font_free(void)
{
    free(s_font.store);
    free(s_font.px);
    s_font.store = NULL;
    s_font.px = NULL;
    memset(&s_font.atlas, 0, sizeof s_font.atlas);
    memset(s_font.slot, 0, sizeof s_font.slot);
}

static int try_pack(unsigned char *cov, int w, int h, stbtt_pack_range *r, int nr)
{
    stbtt_pack_context pc;
    int ok, i;
    memset(cov, 0, (size_t)w * h);
    if (!stbtt_PackBegin(&pc, cov + (size_t)w * GLYPH_ROW0, w, h - GLYPH_ROW0, w, 1, NULL))
        return 0;
    stbtt_PackSetOversampling(&pc, 2, 1);
    /* One call per size, so each size gets its own .notdef. */
    for (i = 0, ok = 1; i < nr && ok; i += NRANGES)
        ok = stbtt_PackFontRanges(&pc, nv2a_ovl_font_regular, 0, r + i, NRANGES);
    stbtt_PackEnd(&pc);
    return ok;
}

int nv2a_ovl_font_bake(uint32_t out_h)
{
    static const int sizes[][2] = { { 512, 1024 }, { 1024, 1024 }, { 2048, 2048 } };
    const int *tok = token_px();
    float s = nv2a_ovl_ui_scale(out_h);
    int per = 0, i, k, si, asc, desc, gap;
    stbtt_pack_range r[OVL_FONT_SLOTS * NRANGES];
    unsigned char *cov = NULL;
    int w = 0, h = 0, packed = 0;
    uint64_t t0 = nv2a_ovl_now_ns();

    nv2a_ovl_font_free();
    if (!stbtt_InitFont(&s_font.info, nv2a_ovl_font_regular,
                        stbtt_GetFontOffsetForIndex(nv2a_ovl_font_regular, 0))) {
        fprintf(stderr, OVL_LOGP "font: the embedded face does not load\n");
        return -1;
    }
    stbtt_GetFontVMetrics(&s_font.info, &asc, &desc, &gap);
    s_font.ascent = (float)asc / (float)(asc - desc);

    for (i = 0; i < NRANGES; i++)
        per += s_ranges[i].n;
    s_font.store = calloc((size_t)per * OVL_FONT_SLOTS, sizeof *s_font.store);
    if (!s_font.store)
        goto oom;
    {
        stbtt_packedchar *p = s_font.store;
        for (si = 0; si < OVL_FONT_SLOTS; si++) {
            s_font.slot[si].px = nv2a_ovl_px(s, tok[si]);
            for (i = 0; i < NRANGES; i++) {
                stbtt_pack_range *pr = &r[si * NRANGES + i];
                memset(pr, 0, sizeof *pr);
                pr->font_size = (float)s_font.slot[si].px;
                pr->first_unicode_codepoint_in_range = s_ranges[i].first;
                pr->num_chars = s_ranges[i].n;
                pr->chardata_for_range = p;
                s_font.slot[si].ch[i] = p;
                p += s_ranges[i].n;
            }
        }
    }
    for (k = 0; k < (int)(sizeof sizes / sizeof sizes[0]) && !packed; k++) {
        free(cov);
        w = sizes[k][0];
        h = sizes[k][1];
        cov = malloc((size_t)w * h);
        if (!cov)
            goto oom;
        packed = try_pack(cov, w, h, r, OVL_FONT_SLOTS * NRANGES);
    }
    if (!packed) {
        fprintf(stderr, OVL_LOGP "font: the glyphs do not fit a %dx%d atlas\n", w, h);
        free(cov);
        nv2a_ovl_font_free();
        return -1;
    }
    s_font.px = malloc((size_t)w * h * 4);
    if (!s_font.px)
        goto oom;
    for (i = 0; i < w * h; i++)
        s_font.px[i] = 0x01010101u * cov[i];
    free(cov);
    cov = NULL;
    for (i = 0; i < WHITE_ROWS; i++)
        for (k = 0; k < WHITE_ROWS; k++)
            s_font.px[i * w + k] = 0xffffffffu;

    /* Tabular digits: every digit advanced by the widest, so the strip's
     * numbers do not shuffle as they change. */
    for (si = 0; si < OVL_FONT_SLOTS; si++) {
        float m = 0;
        for (i = '0'; i <= '9'; i++)
            if (s_font.slot[si].ch[1][i - 0x20].xadvance > m)
                m = s_font.slot[si].ch[1][i - 0x20].xadvance;
        s_font.slot[si].tab_adv = m;
    }
    s_font.atlas.px = s_font.px;
    s_font.atlas.w = (uint32_t)w;
    s_font.atlas.h = (uint32_t)h;
    s_font.atlas.gen = ++s_font.gen;
    fprintf(stderr, OVL_LOGP "atlas %dx%d %.1f ms (%d glyphs at %d/%d/%d px for %u lines)\n",
            w, h, (double)(nv2a_ovl_now_ns() - t0) / 1e6, per, s_font.slot[OVL_FONT_SMALL].px,
            s_font.slot[OVL_FONT_BODY].px, s_font.slot[OVL_FONT_TITLE].px, out_h);
    return 0;
oom:
    free(cov);
    nv2a_ovl_font_free();
    fprintf(stderr, OVL_LOGP "font: out of memory\n");
    return -1;
}

const struct nv2a_ovl_atlas *nv2a_ovl_font_atlas(void)
{
    return s_font.px ? &s_font.atlas : NULL;
}

void nv2a_ovl_font_white_uv(float *u, float *v)
{
    *u = s_font.atlas.w ? 2.0f / (float)s_font.atlas.w : 0;
    *v = s_font.atlas.h ? 2.0f / (float)s_font.atlas.h : 0;
}

uint32_t nv2a_ovl_utf8_next(const char **sp, const char *end)
{
    const unsigned char *s = (const unsigned char *)*sp;
    uint32_t c;
    int n, i;
    if (s >= (const unsigned char *)end)
        return 0;
    c = *s;
    if (c < 0x80) {
        *sp += 1;
        return c;
    }
    if ((c & 0xE0) == 0xC0) { n = 1; c &= 0x1F; }
    else if ((c & 0xF0) == 0xE0) { n = 2; c &= 0x0F; }
    else if ((c & 0xF8) == 0xF0) { n = 3; c &= 0x07; }
    else { *sp += 1; return 0xFFFD; }
    for (i = 1; i <= n; i++) {
        if (s + i >= (const unsigned char *)end || (s[i] & 0xC0) != 0x80) {
            *sp += i;
            return 0xFFFD;
        }
        c = (c << 6) | (s[i] & 0x3F);
    }
    *sp += n + 1;
    return c;
}

static const stbtt_packedchar *lookup(int slot, uint32_t cp)
{
    int i;
    /* Basic Latin is whole in the face; a baked code point beyond it may
     * still be missing from it (Atkinson Hyperlegible has no arrows), and
     * its packed entry is then empty. */
    if (cp >= 0x20 && cp < 0x7F)
        return &s_font.slot[slot].ch[1][cp - 0x20];
    if (stbtt_FindGlyphIndex(&s_font.info, (int)cp))
        for (i = 1; i < NRANGES; i++)
            if (cp >= (uint32_t)s_ranges[i].first
                && cp < (uint32_t)(s_ranges[i].first + s_ranges[i].n))
                return &s_font.slot[slot].ch[i][cp - (uint32_t)s_ranges[i].first];
    return &s_font.slot[slot].ch[0][0];   /* .notdef */
}

static int slot_of(mu_Font f)
{
    int s = OVL_FONT_SLOT(f);
    return s < OVL_FONT_SLOTS ? s : OVL_FONT_BODY;
}

static int px_of(mu_Font f)
{
    int px = OVL_FONT_PX(f);
    return px ? px : s_font.slot[slot_of(f)].px;
}

static int is_digit(uint32_t cp) { return cp >= '0' && cp <= '9'; }

int nv2a_ovl_font_glyph(mu_Font font, uint32_t cp, float *x, float y,
                        struct nv2a_ovl_quad *q)
{
    int slot = slot_of(font);
    const struct slot *sl = &s_font.slot[slot];
    const stbtt_packedchar *b;
    float k, adv, off = 0;
    if (!s_font.px || !sl->px)
        return 0;
    b = lookup(slot, cp);
    k = (float)px_of(font) / (float)sl->px;
    adv = b->xadvance;
    if (OVL_FONT_TAB(font) && is_digit(cp)) {
        off = (sl->tab_adv - adv) * 0.5f;
        adv = sl->tab_adv;
    }
    q->x0 = *x + (b->xoff + off) * k;
    q->x1 = *x + (b->xoff2 + off) * k;
    q->y0 = y + b->yoff * k;
    q->y1 = y + b->yoff2 * k;
    q->u0 = (float)b->x0 / (float)s_font.atlas.w;
    q->u1 = (float)b->x1 / (float)s_font.atlas.w;
    q->v0 = (float)(b->y0 + GLYPH_ROW0) / (float)s_font.atlas.h;
    q->v1 = (float)(b->y1 + GLYPH_ROW0) / (float)s_font.atlas.h;
    *x += adv * k;
    return b->x1 > b->x0 && b->y1 > b->y0;
}

float nv2a_ovl_font_ascent(mu_Font font)
{
    return s_font.ascent * (float)px_of(font);
}

int nv2a_ovl_text_width(mu_Font font, const char *str, int len)
{
    const char *end;
    float x = 0;
    int slot = slot_of(font);
    const struct slot *sl = &s_font.slot[slot];
    if (!s_font.px || !sl->px)
        return 0;
    if (len < 0)
        len = (int)strlen(str);
    end = str + len;
    while (str < end) {
        uint32_t cp = nv2a_ovl_utf8_next(&str, end);
        x += OVL_FONT_TAB(font) && is_digit(cp) ? sl->tab_adv : lookup(slot, cp)->xadvance;
    }
    return (int)ceilf(x * (float)px_of(font) / (float)sl->px);
}

int nv2a_ovl_text_height(mu_Font font)
{
    return px_of(font);
}
