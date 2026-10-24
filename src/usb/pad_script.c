/*
 * pad_script.c -- the scripted pad's grammar and the pad recorder.
 * See pad_script.h.
 */
#include "pad_script.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Stick directions, for screens and games that read the stick and ignore
 * the d-pad: Burnout 3 steers and picks map locations with it. */
static const struct { const char *name; uint8_t bit; } s_sticks[] = {
    {"lleft", 0x01}, {"lright", 0x02}, {"lup", 0x04}, {"ldown", 0x08},
    {"rleft", 0x10}, {"rright", 0x20}, {"rup", 0x40}, {"rdown", 0x80},
};

static const struct { const char *name; uint8_t dig, ana; } s_buttons[] = {
    {"up", 0x01, 0}, {"down", 0x02, 0}, {"left", 0x04, 0},
    {"right", 0x08, 0}, {"start", 0x10, 0}, {"back", 0x20, 0},
    {"lthumb", 0x40, 0}, {"rthumb", 0x80, 0},
    {"a", 0, 0x01}, {"b", 0, 0x02}, {"x", 0, 0x04}, {"y", 0, 0x08},
    {"black", 0, 0x10}, {"white", 0, 0x20}, {"lt", 0, 0x40}, {"rt", 0, 0x80},
};

#define COUNT(a) (sizeof (a) / sizeof (a)[0])

static int same_word(const char *a, const char *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
            return 0;
    return 1;
}

static const char *const s_axes[4] = {"lx", "ly", "rx", "ry"};

/* "lx=-12000": a stick axis at a value. */
static int axis_value(const char *name, size_t n, PadStep *st)
{
    char num[8], *end;
    long v;
    int i;

    if (n < 4 || n - 3 >= sizeof num || name[2] != '=')
        return 0;
    for (i = 0; i < 4; i++)
        if (same_word(s_axes[i], name, 2))
            break;
    if (i == 4)
        return 0;
    memcpy(num, name + 3, n - 3);
    num[n - 3] = 0;
    v = strtol(num, &end, 10);
    if (*end || end == num || v < -32768 || v > 32767)
        return 0;
    st->axis[i] = (int16_t)v;
    st->axis_set |= (uint8_t)(1u << i);
    return 1;
}

static int button_bits(const char *name, size_t n, PadStep *st)
{
    size_t i;

    if (axis_value(name, n, st))
        return 1;
    for (i = 0; i < COUNT(s_sticks); i++) {
        if (strlen(s_sticks[i].name) == n && same_word(s_sticks[i].name, name, n)) {
            st->stick |= s_sticks[i].bit;
            return 1;
        }
    }
    for (i = 0; i < COUNT(s_buttons); i++) {
        if (strlen(s_buttons[i].name) == n && same_word(s_buttons[i].name, name, n)) {
            st->digital |= s_buttons[i].dig;
            st->analog |= s_buttons[i].ana;
            return 1;
        }
    }
    return 0;
}

int pad_script_parse(const char *text, PadStep *steps, int len, int max)
{
    const char *p = text;

    while (*p) {
        PadStep st;
        char *end;
        const char *b;

        while (*p == ',' || *p == ' ' || *p == '\r' || *p == '\n' || *p == '\t')
            p++;
        if (*p == '#') {
            while (*p && *p != '\n')
                p++;
            continue;
        }
        if (!*p)
            break;
        if (len >= max) {
            fprintf(stderr, "  PAD: script longer than %d steps; the rest is"
                    " dropped\n", max);
            break;
        }
        memset(&st, 0, sizeof st);
        if (*p == 'f' || *p == 'F') {
            st.by_flip = 1;
            p++;
        }
        st.hold = st.by_flip ? PAD_HOLD_FLIPS_DEFAULT : 150;
        /* strtoul alone would take a sign or blanks, and "-1" would wrap
         * to a flip that never comes. */
        st.at = isdigit((unsigned char)*p) ? strtoul(p, &end, 10) : 0;
        if (!isdigit((unsigned char)*p) || *end != ':') {
            fprintf(stderr, "  PAD: script entry without <ms> or f<flip>: at \"%.20s\"\n", p);
            break;
        }
        b = end + 1;
        /* "p2-" .. "p4-": the step is for that controller, not the first. */
        if ((b[0] == 'p' || b[0] == 'P') && b[1] >= '1' && b[1] <= '4'
            && b[2] == '-') {
            st.pad = (uint8_t)(b[1] - '1');
            b += 3;
        }
        for (;;) {
            size_t n = strcspn(b, "+:,\r\n #");
            if (!button_bits(b, n, &st))
                fprintf(stderr, "  PAD: unknown button \"%.*s\"\n", (int)n, b);
            b += n;
            if (*b != '+')
                break;
            b++;
        }
        if (*b == ':') {
            if (!isdigit((unsigned char)b[1])) {
                /* "f10:a:" would be a hold of 0, a step that never presses. */
                fprintf(stderr, "  PAD: script hold without a number: at \"%.20s\"\n", b);
                break;
            }
            st.hold = strtoul(b + 1, &end, 10);
            b = end;
            if (*b == 'f' || *b == 'F') {
                if (!st.by_flip)
                    fprintf(stderr, "  PAD: hold in flips on a step timed in ms;"
                            " read as ms\n");
                b++;
            }
        }
        steps[len++] = st;
        p = b;
    }
    return len;
}

int pad_step_active(const PadStep *st, unsigned long t_ms, uint32_t flip)
{
    unsigned long now = st->by_flip ? (unsigned long)flip : t_ms;
    return now >= st->at && now < st->at + st->hold;
}

int pad_step_done(const PadStep *st, unsigned long t_ms, uint32_t flip)
{
    unsigned long now = st->by_flip ? (unsigned long)flip : t_ms;
    return now >= st->at + st->hold;
}

void pad_step_not_before(PadStep *st, uint32_t flip)
{
    if (st->by_flip && st->at < (unsigned long)flip)
        st->at = flip;
}

void pad_step_press(const PadStep *st, uint8_t *out)
{
    int j;

    out[2] |= st->digital;
    for (j = 0; j < 8; j++)
        if (st->analog & (1u << j))
            out[4 + j] = 0xFF;
    if (st->stick) {
        /* Axes at 12..19: LX LY RX RY, signed 16-bit, up positive. */
        static const int16_t full = 32767;
        int16_t ax[4];
        memcpy(ax, &out[12], sizeof ax);
        if (st->stick & 0x01) ax[0] = (int16_t)-full;
        if (st->stick & 0x02) ax[0] = full;
        if (st->stick & 0x04) ax[1] = full;
        if (st->stick & 0x08) ax[1] = (int16_t)-full;
        if (st->stick & 0x10) ax[2] = (int16_t)-full;
        if (st->stick & 0x20) ax[2] = full;
        if (st->stick & 0x40) ax[3] = full;
        if (st->stick & 0x80) ax[3] = (int16_t)-full;
        memcpy(&out[12], ax, sizeof ax);
    }
    if (st->axis_set) {
        int16_t ax[4];
        memcpy(ax, &out[12], sizeof ax);
        for (j = 0; j < 4; j++)
            if (st->axis_set & (1u << j))
                ax[j] = st->axis[j];
        memcpy(&out[12], ax, sizeof ax);
    }
}

/* ---- the recorder ------------------------------------------------------ */

/* XINPUT_GAMEPAD_TRIGGER_THRESHOLD: below it a trigger or analog button is
 * resting under a finger, not pressed. */
#define ANALOG_PRESSED 30

PadBits pad_bits_from_report(const uint8_t *r)
{
    PadBits b;
    int j;

    memset(&b, 0, sizeof b);
    b.digital = r[2];
    for (j = 0; j < 8; j++)
        if (r[4 + j] > ANALOG_PRESSED)
            b.analog |= (uint8_t)(1u << j);
    memcpy(b.ax, &r[12], sizeof b.ax);
    return b;
}

static void append(char *out, size_t n, size_t *used, const char *name)
{
    int w = snprintf(out + *used, n - *used, "%s%s", *used ? "+" : "", name);
    if (w > 0 && (size_t)w < n - *used)
        *used += (size_t)w;
}

void pad_bits_name(PadBits b, char *out, size_t n)
{
    size_t i, used = 0;

    if (n)
        out[0] = 0;
    for (i = 0; i < COUNT(s_buttons); i++)
        if ((s_buttons[i].dig & b.digital) || (s_buttons[i].ana & b.analog))
            append(out, n, &used, s_buttons[i].name);
}

static void axes_name(const int16_t *ax, char *out, size_t n)
{
    size_t used = 0;
    int i;

    if (n)
        out[0] = 0;
    for (i = 0; i < 4; i++) {
        char one[16];
        if (!ax[i])
            continue;
        snprintf(one, sizeof one, "%s=%d", s_axes[i], ax[i]);
        append(out, n, &used, one);
    }
}

static int buttons_any(PadBits b)
{
    return b.digital || b.analog;
}

static int axes_any(const int16_t *ax)
{
    return ax[0] || ax[1] || ax[2] || ax[3];
}

/* A new stick step: past the band, or onto rest or full throw, which a
 * band would otherwise round away. Leaving rest takes the band too: a pad
 * at rest reads a few hundred off centre, inside any title's dead zone. */
static int axes_moved(const int16_t *cur, const int16_t *now)
{
    int i;
    for (i = 0; i < 4; i++) {
        int d = (int)now[i] - (int)cur[i];
        if (!d)
            continue;
        if (d >= PAD_AXIS_BAND || d <= -PAD_AXIS_BAND || !now[i]
            || now[i] >= 32767 || now[i] <= -32767)
            return 1;
    }
    return 0;
}

static int emit(char *out, size_t n, size_t *used, int pad, uint32_t at,
                const char *names, uint32_t hold)
{
    int w;
    if (pad > 0)
        w = snprintf(out + *used, n - *used, "f%u:p%d-%s:%u\n", at, pad + 1,
                     names, hold);
    else
        w = snprintf(out + *used, n - *used, "f%u:%s:%u\n", at, names, hold);
    if (w > 0 && (size_t)w < n - *used) {
        *used += (size_t)w;
        return 1;
    }
    return 0;
}

/* The state s holds from flip on: end what changed. */
static int commit(PadRecorder *r, PadBits s, uint32_t flip, char *out, size_t n,
                  size_t *used)
{
    char names[128];
    int lines = 0;

    if (s.digital != r->cur.digital || s.analog != r->cur.analog) {
        if (buttons_any(r->cur)) {
            pad_bits_name(r->cur, names, sizeof names);
            lines += emit(out, n, used, r->pad, r->since, names, flip - r->since);
        }
        r->cur.digital = s.digital;
        r->cur.analog = s.analog;
        r->since = flip;
    }
    if (axes_moved(r->cur.ax, s.ax)) {
        if (axes_any(r->cur.ax)) {
            axes_name(r->cur.ax, names, sizeof names);
            lines += emit(out, n, used, r->pad, r->ax_since, names,
                          flip - r->ax_since);
        }
        memcpy(r->cur.ax, s.ax, sizeof s.ax);
        r->ax_since = flip;
    }
    return lines;
}

int pad_recorder_feed(PadRecorder *r, PadBits b, uint32_t flip, char *out, size_t n)
{
    size_t used = 0;
    int lines = 0;

    if (n)
        out[0] = 0;
    if (!r->started) {
        r->started = 1;
        memset(&r->cur, 0, sizeof r->cur);
        r->pend = b;
        r->pend_flip = flip;
        return 0;
    }
    if (flip == r->pend_flip) {
        r->pend.digital |= b.digital;
        r->pend.analog |= b.analog;
        memcpy(r->pend.ax, b.ax, sizeof b.ax);
        return 0;
    }
    lines = commit(r, r->pend, r->pend_flip, out, n, &used);
    r->pend = b;
    r->pend_flip = flip;
    return lines;
}

int pad_recorder_end(PadRecorder *r, uint32_t flip, char *out, size_t n)
{
    PadBits none;
    size_t used = 0;
    int lines;

    if (n)
        out[0] = 0;
    if (!r->started)
        return 0;
    memset(&none, 0, sizeof none);
    lines = commit(r, r->pend, r->pend_flip, out, n, &used);
    if (flip <= r->pend_flip)
        flip = r->pend_flip + 1;
    lines += commit(r, none, flip, out, n, &used);
    r->started = 0;
    return lines;
}
