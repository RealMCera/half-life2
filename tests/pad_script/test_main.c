/* pad_script: the step grammar (ms and f<flip>), step timing, the press, and
 * the recorder, whose lines must parse back to the presses it saw. */
#include "pad_script.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void report(uint8_t *r, uint8_t dig, uint8_t ana, int16_t lx, int16_t ly)
{
    int j;
    memset(r, 0, 20);
    r[1] = 20;
    r[2] = dig;
    for (j = 0; j < 8; j++)
        if (ana & (1u << j))
            r[4 + j] = 0xFF;
    memcpy(&r[12], &lx, 2);
    memcpy(&r[14], &ly, 2);
}

static void test_parse(void)
{
    PadStep s[16];
    int n = pad_script_parse("8000:start:300, 9000:a\n# a comment, a:b\n"
                             "f120:lup+a:30 f200:x f300:p2-b:4f 010:down",
                             s, 0, 16);
    CHECK(n == 6);
    CHECK(!s[0].by_flip && s[0].at == 8000 && s[0].hold == 300 && s[0].digital == 0x10);
    CHECK(!s[1].by_flip && s[1].at == 9000 && s[1].hold == 150 && s[1].analog == 0x01);
    CHECK(s[2].by_flip && s[2].at == 120 && s[2].hold == 30);
    CHECK(s[2].stick == 0x04 && s[2].analog == 0x01);
    CHECK(s[3].by_flip && s[3].hold == PAD_HOLD_FLIPS_DEFAULT && s[3].analog == 0x04);
    CHECK(s[4].by_flip && s[4].pad == 1 && s[4].hold == 4 && s[4].analog == 0x02);
    /* Decimal: a leading zero is not octal. */
    CHECK(!s[5].by_flip && s[5].at == 10 && s[5].digital == 0x02);
    n = pad_script_parse("f010:a:08", s, 0, 16);
    CHECK(n == 1 && s[0].at == 10 && s[0].hold == 8);
    /* Hex is not a time any more. */
    CHECK(pad_script_parse("0x10:down", s, 0, 16) == 0);

    /* Sticks at a value. */
    n = pad_script_parse("f5:lx=-12000+ry=300+a:2", s, 0, 16);
    CHECK(n == 1 && s[0].axis_set == 0x09 && s[0].axis[0] == -12000
          && s[0].axis[3] == 300 && s[0].analog == 0x01);
    n = pad_script_parse("f5:lx=40000:2", s, 0, 16);
    CHECK(n == 1 && s[0].axis_set == 0);

    /* Appends after len, stops at max. */
    n = pad_script_parse("f1:a f2:b f3:x", s, 14, 16);
    CHECK(n == 16 && s[15].analog == 0x02);
    /* An entry without a time ends the parse. */
    CHECK(pad_script_parse("f1:a, start", s, 0, 16) == 1);
    /* A sign or a blank is no time (strtoul would wrap "-1"), and a hold
     * needs its number: each ends the parse. */
    CHECK(pad_script_parse("f-1:a", s, 0, 16) == 0);
    CHECK(pad_script_parse("f1:a f +5:b", s, 0, 16) == 1);
    CHECK(pad_script_parse("-5:a", s, 0, 16) == 0);
    CHECK(pad_script_parse("f1:a f10:a: f20:b", s, 0, 16) == 1);
    CHECK(pad_script_parse("f10:a:-3", s, 0, 16) == 0);
}

static void test_timing(void)
{
    PadStep s[3];
    pad_script_parse("f100:a:10, 500:b:100, f50:x:4", s, 0, 3);
    /* A flip step ignores wall time, a ms step ignores flips. */
    CHECK(!pad_step_active(&s[0], 99999, 99));
    CHECK(pad_step_active(&s[0], 0, 100));
    CHECK(pad_step_active(&s[0], 0, 109));
    CHECK(!pad_step_active(&s[0], 0, 110) && pad_step_done(&s[0], 0, 110));
    CHECK(!pad_step_done(&s[0], 99999, 105));
    CHECK(pad_step_active(&s[1], 550, 0));
    CHECK(!pad_step_active(&s[1], 499, 100000));
    CHECK(pad_step_done(&s[1], 600, 0));
    /* A live line whose flip has passed is pressed at once, for its hold. */
    CHECK(pad_step_done(&s[2], 0, 80));
    pad_step_not_before(&s[2], 80);
    CHECK(pad_step_active(&s[2], 0, 80) && pad_step_active(&s[2], 0, 83));
    CHECK(pad_step_done(&s[2], 0, 84));
    /* One not yet due keeps its flip, and a ms step is left alone. */
    pad_step_not_before(&s[0], 20);
    CHECK(s[0].at == 100);
    pad_step_not_before(&s[1], 100000);
    CHECK(s[1].at == 500);
}

static void test_truncation(void)
{
    static PadStep s[4];
    CHECK(pad_script_parse("f1:a f2:a f3:a f4:a f5:a f6:a", s, 0, 4) == 4);
    CHECK(s[3].at == 4);
}

static void test_press(void)
{
    PadStep s[2];
    uint8_t r[20];
    int16_t lx, ly, ry;
    pad_script_parse("f1:start+lt+lleft+rdown", s, 0, 1);
    report(r, 0x01, 0, 0, 0);
    pad_step_press(&s[0], r);
    memcpy(&lx, &r[12], 2);
    memcpy(&ly, &r[14], 2);
    memcpy(&ry, &r[18], 2);
    CHECK(r[2] == 0x11);
    CHECK(r[4 + 6] == 0xFF && r[4] == 0);
    CHECK(lx == -32767 && ly == 0 && ry == -32767);

    pad_script_parse("f1:lx=-1234+ly=20000", s, 0, 1);
    report(r, 0, 0, 500, 500);
    memcpy(&r[16], &(int16_t){77}, 2);
    pad_step_press(&s[0], r);
    memcpy(&lx, &r[12], 2);
    memcpy(&ly, &r[14], 2);
    memcpy(&ry, &r[16], 2);
    CHECK(lx == -1234 && ly == 20000 && ry == 77);
}

static void test_bits(void)
{
    uint8_t r[20];
    PadBits b;
    char name[64];
    report(r, 0x10, 0, 20000, -20000);
    r[4] = 30;      /* resting on A: not pressed */
    r[4 + 7] = 31;  /* RT just past the threshold */
    b = pad_bits_from_report(r);
    CHECK(b.digital == 0x10 && b.analog == 0x80);
    CHECK(b.ax[0] == 20000 && b.ax[1] == -20000 && b.ax[2] == 0);
    pad_bits_name(b, name, sizeof name);
    CHECK(strcmp(name, "start+rt") == 0);
}

static PadBits bits(uint8_t dig, uint8_t ana, int16_t lx, int16_t ly)
{
    PadBits b;
    memset(&b, 0, sizeof b);
    b.digital = dig;
    b.analog = ana;
    b.ax[0] = lx;
    b.ax[1] = ly;
    return b;
}

static void test_recorder(void)
{
    PadRecorder rec;
    char out[512];
    memset(&rec, 0, sizeof rec);

    /* A flip's state is written when the next flip's report arrives. */
    CHECK(!pad_recorder_feed(&rec, bits(0, 0, 0, 0), 10, out, sizeof out));
    CHECK(!pad_recorder_feed(&rec, bits(0, 1, 0, 0), 20, out, sizeof out));
    CHECK(!pad_recorder_feed(&rec, bits(0, 1, 0, 0), 21, out, sizeof out));
    CHECK(!pad_recorder_feed(&rec, bits(0, 1, 0, 32767), 25, out, sizeof out));
    /* The stick moving under A does not split A's step. */
    CHECK(pad_recorder_feed(&rec, bits(0, 0, 0, 32767), 40, out, sizeof out) == 0);
    CHECK(pad_recorder_feed(&rec, bits(0, 0, 0, 0), 41, out, sizeof out) == 1);
    CHECK(strcmp(out, "f20:a:20\n") == 0);
    CHECK(pad_recorder_feed(&rec, bits(0, 0, 0, 0), 42, out, sizeof out) == 1);
    CHECK(strcmp(out, "f25:ly=32767:16\n") == 0);

    /* Reports within one flip merge: a tap is kept, the stick is the last. */
    CHECK(!pad_recorder_feed(&rec, bits(0, 1, 9000, 0), 50, out, sizeof out));
    CHECK(!pad_recorder_feed(&rec, bits(0, 0, 9100, 0), 50, out, sizeof out));
    CHECK(!pad_recorder_feed(&rec, bits(0, 0, 9200, 0), 50, out, sizeof out));
    CHECK(pad_recorder_feed(&rec, bits(0, 0, 0, 0), 51, out, sizeof out) == 0);
    CHECK(pad_recorder_feed(&rec, bits(0, 0, 0, 0), 52, out, sizeof out) == 2);
    CHECK(strcmp(out, "f50:a:1\nf50:lx=9200:1\n") == 0);

    /* Jitter inside the band is one step; reaching full throw is a new one. */
    pad_recorder_feed(&rec, bits(0, 0, 10000, 0), 60, out, sizeof out);
    pad_recorder_feed(&rec, bits(0, 0, 10100, 0), 61, out, sizeof out);
    pad_recorder_feed(&rec, bits(0, 0, 9950, 0), 62, out, sizeof out);
    CHECK(!pad_recorder_feed(&rec, bits(0, 0, 32767, 0), 63, out, sizeof out));
    CHECK(pad_recorder_feed(&rec, bits(0, 0, 32767, 0), 64, out, sizeof out) == 1);
    CHECK(strcmp(out, "f60:lx=10000:3\n") == 0);
    /* At the end, what is still held is written. */
    CHECK(pad_recorder_end(&rec, 70, out, sizeof out) == 1);
    CHECK(strcmp(out, "f63:lx=32767:7\n") == 0);
    CHECK(pad_recorder_end(&rec, 80, out, sizeof out) == 0);

    memset(&rec, 0, sizeof rec);
    rec.pad = 1;
    pad_recorder_feed(&rec, bits(0, 1, 0, 0), 60, out, sizeof out);
    pad_recorder_feed(&rec, bits(0, 0, 0, 0), 63, out, sizeof out);
    CHECK(pad_recorder_feed(&rec, bits(0, 0, 0, 0), 64, out, sizeof out) == 1);
    CHECK(strcmp(out, "f60:p2-a:3\n") == 0);
}

/* Record a pad sequence, replay the lines, and the replayed reports match:
 * buttons, a stick ramping up the way the keyboard's does, a half-throw
 * walk, and a button held across the ramp. */
static void test_round_trip(void)
{
    static const struct { uint8_t dig, ana; int16_t lx, ly; } seq[] = {
        {0, 0, 0, 0}, {0x10, 0, 0, 0}, {0x10, 0, 0, 0}, {0, 0, 0, 0},
        {0, 0x01, 0, 4096}, {0, 0x01, 0, 12288}, {0, 0x01, 0, 24576},
        {0, 0x01, 0, 32767}, {0, 0, 0, 32767},
        {0, 0x40, -16384, 0}, {0x01, 0x40, -16384, 0}, {0x01, 0, -16384, 9000},
        {0, 0, 0, 0}, {0, 0, 0, 0},
    };
    enum { N = sizeof seq / sizeof seq[0] };
    PadRecorder rec;
    PadStep steps[64];
    char text[2048] = "", out[512];
    int i, n;

    memset(&rec, 0, sizeof rec);
    for (i = 0; i < N; i++) {
        uint8_t r[20];
        report(r, seq[i].dig, seq[i].ana, seq[i].lx, seq[i].ly);
        if (pad_recorder_feed(&rec, pad_bits_from_report(r), (uint32_t)(100 + i),
                              out, sizeof out))
            strcat(text, out);
    }
    if (pad_recorder_end(&rec, 100 + N, out, sizeof out))
        strcat(text, out);
    n = pad_script_parse(text, steps, 0, 64);
    CHECK(n > 0);
    for (i = 0; i < N; i++) {
        uint8_t want[20], got[20];
        int k;
        report(want, seq[i].dig, seq[i].ana, seq[i].lx, seq[i].ly);
        report(got, 0, 0, 0, 0);
        for (k = 0; k < n; k++)
            if (pad_step_active(&steps[k], 0, (uint32_t)(100 + i)))
                pad_step_press(&steps[k], got);
        if (memcmp(want, got, 20) != 0) {
            fails++;
            fprintf(stderr, "FAIL round trip at flip %d\n%s", 100 + i, text);
        }
    }
}

int main(void)
{
    test_parse();
    test_timing();
    test_truncation();
    test_press();
    test_bits();
    test_recorder();
    test_round_trip();
    if (fails) {
        fprintf(stderr, "pad_script: %d failure(s)\n", fails);
        return 1;
    }
    printf("pad_script: all passed\n");
    return 0;
}
