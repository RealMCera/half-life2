#define _CRT_SECURE_NO_WARNINGS
/* input_map: key names, binding parse and format, key resolution, deadzone
 * arithmetic and pad remapping. */

#include "input_map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failed;
static int g_checks;
#define CHECK(cond) do { g_checks++; if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_failed++; } } while (0)
#define NEAR(a, b, tol) (abs((int)(a) - (int)(b)) <= (tol))

static void test_key_names(void)
{
    int c;
    char buf[64];
    CHECK(xbox_InputMap_KeyFromName("a") == INPUT_KEY_A);
    CHECK(xbox_InputMap_KeyFromName("Z") == INPUT_KEY_Z);
    CHECK(xbox_InputMap_KeyFromName("7") == INPUT_KEY_7);
    CHECK(xbox_InputMap_KeyFromName("F5") == INPUT_KEY_F5);
    CHECK(xbox_InputMap_KeyFromName("f12") == INPUT_KEY_F12);
    CHECK(xbox_InputMap_KeyFromName("f13") == -1);
    CHECK(xbox_InputMap_KeyFromName("f05") == -1);
    CHECK(xbox_InputMap_KeyFromName("f0") == -1);
    CHECK(xbox_InputMap_KeyFromName("Numpad4") == INPUT_KEY_NUMPAD4);
    CHECK(xbox_InputMap_KeyFromName("numpad10") == -1);
    CHECK(xbox_InputMap_KeyFromName("SPACE") == INPUT_KEY_SPACE);
    CHECK(xbox_InputMap_KeyFromName("Esc") == INPUT_KEY_ESCAPE);
    CHECK(xbox_InputMap_KeyFromName("mouse1") == INPUT_KEY_MOUSE1);
    CHECK(xbox_InputMap_KeyFromName("Mouse3") == INPUT_KEY_MOUSE3);
    CHECK(xbox_InputMap_KeyFromName("WheelUp") == INPUT_KEY_WHEEL_UP);
    CHECK(xbox_InputMap_KeyFromName("lshift") == INPUT_KEY_LSHIFT);
    CHECK(xbox_InputMap_KeyFromName("") == -1);
    CHECK(xbox_InputMap_KeyFromName("banana") == -1);
    CHECK(xbox_InputMap_KeyFromName("ab") == -1);
    /* Every code with a name reads back as itself. */
    for (c = 0; c < INPUT_KEY_COUNT; c++) {
        const char *n = xbox_InputMap_KeyName(c);
        if (!n) continue;
        snprintf(buf, sizeof(buf), "%s", n);
        CHECK(xbox_InputMap_KeyFromName(buf) == c);
    }
}

static void test_bindings(void)
{
    InputBindings b;
    char text[128];
    int k;
    memset(&b, 0, sizeof(b));

    CHECK(xbox_InputMap_BindingsParse(&b, INPUT_A, "L, Space,  Mouse1 ") == 0);
    CHECK(b.count[INPUT_A] == 3);
    CHECK(b.key[INPUT_A][0] == INPUT_KEY_L && b.key[INPUT_A][1] == INPUT_KEY_SPACE && b.key[INPUT_A][2] == INPUT_KEY_MOUSE1);
    CHECK(!strcmp(xbox_InputMap_BindingsFormat(&b, INPUT_A, text, sizeof(text)), "L, space, mouse1"));

    /* Reading the formatted text back gives the same bindings. */
    k = xbox_InputMap_BindingsParse(&b, INPUT_B, text);
    CHECK(k == 0 && b.count[INPUT_B] == 3 && b.key[INPUT_B][1] == INPUT_KEY_SPACE);

    /* Parsing replaces; none and empty clear; duplicates are dropped. */
    CHECK(xbox_InputMap_BindingsParse(&b, INPUT_A, "none") == 0 && b.count[INPUT_A] == 0);
    CHECK(!strcmp(xbox_InputMap_BindingsFormat(&b, INPUT_A, text, sizeof(text)), "none"));
    CHECK(xbox_InputMap_BindingsParse(&b, INPUT_A, "J, j, J") == 0 && b.count[INPUT_A] == 1);
    CHECK(xbox_InputMap_BindingsParse(&b, INPUT_A, "") == 0 && b.count[INPUT_A] == 0);

    /* Unknown names are skipped and counted, known ones kept. */
    CHECK(xbox_InputMap_BindingsParse(&b, INPUT_X, "K, banana, wheeldown, 99") == 2);
    CHECK(b.count[INPUT_X] == 2 && b.key[INPUT_X][1] == INPUT_KEY_WHEEL_DOWN);

    /* Four is the most; the fifth is counted as rejected. */
    CHECK(xbox_InputMap_BindingsParse(&b, INPUT_Y, "A,B,C,D,E") == 1);
    CHECK(b.count[INPUT_Y] == 4);

    /* Control names are the setting keys. */
    CHECK(!strcmp(xbox_InputMap_ControlName(INPUT_LSTICK_UP), "left_stick_up"));
}

static void test_resolve(void)
{
    InputBindings b;
    uint8_t down[INPUT_KEY_COUNT];
    InputPad p;
    memset(&b, 0, sizeof(b));
    memset(down, 0, sizeof(down));
    xbox_InputMap_BindingsParse(&b, INPUT_LSTICK_UP, "W");
    xbox_InputMap_BindingsParse(&b, INPUT_LSTICK_DOWN, "S");
    xbox_InputMap_BindingsParse(&b, INPUT_LSTICK_LEFT, "A");
    xbox_InputMap_BindingsParse(&b, INPUT_LSTICK_RIGHT, "D");
    xbox_InputMap_BindingsParse(&b, INPUT_RSTICK_LEFT, "Left");
    xbox_InputMap_BindingsParse(&b, INPUT_A, "L, Space");
    xbox_InputMap_BindingsParse(&b, INPUT_LEFT_TRIGGER, "Q");
    xbox_InputMap_BindingsParse(&b, INPUT_START, "Enter");
    xbox_InputMap_BindingsParse(&b, INPUT_DPAD_UP, "wheelup");

    xbox_InputMap_KeysResolve(&b, down, &p);
    CHECK(p.buttons == 0 && p.lx == 0 && p.ly == 0 && p.rx == 0);

    down[INPUT_KEY_W] = 1;
    xbox_InputMap_KeysResolve(&b, down, &p);
    CHECK(p.ly == 32767 && p.lx == 0);

    down[INPUT_KEY_D] = 1;                                   /* diagonal: not faster */
    xbox_InputMap_KeysResolve(&b, down, &p);
    CHECK(p.lx == 23169 && p.ly == 23169);

    down[INPUT_KEY_S] = 1;                                   /* opposite keys cancel */
    xbox_InputMap_KeysResolve(&b, down, &p);
    CHECK(p.ly == 0 && p.lx == 32767);

    down[INPUT_KEY_LEFT] = 1;
    xbox_InputMap_KeysResolve(&b, down, &p);
    CHECK(p.rx == -32767 && p.ry == 0);

    down[INPUT_KEY_SPACE] = 1;                                  /* either bound key presses A */
    down[INPUT_KEY_Q] = 1;
    down[INPUT_KEY_ENTER] = 1;
    down[INPUT_KEY_WHEEL_UP] = 1;
    xbox_InputMap_KeysResolve(&b, down, &p);
    CHECK(p.analog[INPUT_ANALOG_A] == 255);
    CHECK(p.analog[INPUT_ANALOG_LTRIGGER] == 255);
    CHECK(p.analog[INPUT_ANALOG_B] == 0);
    CHECK(p.buttons == (INPUT_PAD_START | INPUT_PAD_DPAD_UP));
}

static void test_merge(void)
{
    InputPad a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    a.buttons = INPUT_PAD_START;
    a.analog[0] = 255;
    a.lx = 1000;
    b.buttons = INPUT_PAD_BACK;
    b.analog[0] = 10;
    b.analog[7] = 90;
    b.lx = -20000;
    b.ry = 5;
    xbox_InputMap_PadMerge(&a, &b);
    CHECK(a.buttons == (INPUT_PAD_START | INPUT_PAD_BACK));
    CHECK(a.analog[0] == 255 && a.analog[7] == 90);
    CHECK(a.lx == -20000);                           /* the larger deflection wins */
    CHECK(a.ry == 5);
}

static void test_deadzone(void)
{
    int16_t x, y;
    xbox_InputMap_ApplyStick(3000, 0, 15, &x, &y);          /* 15% is 4915 */
    CHECK(x == 0 && y == 0);
    xbox_InputMap_ApplyStick(3000, 3000, 15, &x, &y);       /* magnitude 4243: still inside */
    CHECK(x == 0 && y == 0);
    xbox_InputMap_ApplyStick(32767, 0, 15, &x, &y);         /* full stays full */
    CHECK(x == 32767 && y == 0);
    xbox_InputMap_ApplyStick(-32768, 0, 15, &x, &y);
    CHECK(x == -32767 && y == 0);
    xbox_InputMap_ApplyStick(16383, 0, 15, &x, &y);         /* (16383-4915)/(32767-4915) */
    CHECK(NEAR(x, 13491, 2) && y == 0);
    xbox_InputMap_ApplyStick(0, -16383, 15, &x, &y);
    CHECK(x == 0 && NEAR(y, -13491, 2));
    xbox_InputMap_ApplyStick(20000, 20000, 15, &x, &y);     /* direction preserved */
    CHECK(x == y && x > 0 && x < 32767);
    xbox_InputMap_ApplyStick(32767, 32767, 15, &x, &y);     /* beyond the circle clamps to it */
    CHECK(NEAR(x, 23170, 3) && NEAR(y, 23170, 3));
    xbox_InputMap_ApplyStick(500, 0, 0, &x, &y);            /* no deadzone passes through */
    CHECK(x == 500 && y == 0);
    xbox_InputMap_ApplyStick(0, 0, 15, &x, &y);
    CHECK(x == 0 && y == 0);
    xbox_InputMap_ApplyStick(32767, 0, 120, &x, &y);        /* absurd percentages are clamped */
    CHECK(x == 32767);

    CHECK(xbox_InputMap_ApplyTrigger(0, 5) == 0);
    CHECK(xbox_InputMap_ApplyTrigger(12, 5) == 0);          /* 5% of 255 is 12 */
    CHECK(xbox_InputMap_ApplyTrigger(255, 5) == 255);
    CHECK(NEAR(xbox_InputMap_ApplyTrigger(134, 5), 128, 1));
    CHECK(xbox_InputMap_ApplyTrigger(100, 0) == 100);
    CHECK(xbox_InputMap_ApplyTrigger(300, 5) == 255);
}

static void test_padmap(void)
{
    InputPadMap m;
    InputRaw r;
    InputPad p;
    int i;

    CHECK(xbox_InputMap_SourceFromName("south") == INPUT_SRC_SOUTH);
    CHECK(xbox_InputMap_SourceFromName("Left_Trigger") == INPUT_SRC_LEFT_TRIGGER);
    CHECK(xbox_InputMap_SourceFromName("a") == -1);
    for (i = 0; i < INPUT_SRC_COUNT; i++)
        CHECK(xbox_InputMap_SourceFromName(xbox_InputMap_SourceName(i)) == i);

    xbox_InputMap_PadMapDefaults(&m);
    memset(&r, 0, sizeof(r));
    xbox_InputMap_PadMap(&m, &r, &p);
    CHECK(p.buttons == 0 && p.analog[0] == 0 && p.lx == 0);

    /* Identity: the bottom face button is A, the shoulders are Black and White. */
    r.src[INPUT_SRC_SOUTH] = 32767;
    r.src[INPUT_SRC_LEFT_SHOULDER] = 32767;
    r.src[INPUT_SRC_START] = 32767;
    r.src[INPUT_SRC_DPAD_LEFT] = 32767;
    r.src[INPUT_SRC_RIGHT_TRIGGER] = 32767;
    r.src[INPUT_SRC_LEFT_TRIGGER] = 128 << 7;
    xbox_InputMap_PadMap(&m, &r, &p);
    CHECK(p.analog[INPUT_ANALOG_A] == 255 && p.analog[INPUT_ANALOG_B] == 0);
    CHECK(p.analog[INPUT_ANALOG_BLACK] == 255 && p.analog[INPUT_ANALOG_WHITE] == 0);
    CHECK(p.buttons == (INPUT_PAD_START | INPUT_PAD_DPAD_LEFT));
    CHECK(p.analog[INPUT_ANALOG_RTRIGGER] == 255);
    CHECK(NEAR(p.analog[INPUT_ANALOG_LTRIGGER], 128, 8));   /* analog, with the threshold applied */

    /* A button below the press threshold does not count; at it, does. */
    memset(&r, 0, sizeof(r));
    r.src[INPUT_SRC_EAST] = 16000;
    xbox_InputMap_PadMap(&m, &r, &p);
    CHECK(p.analog[INPUT_ANALOG_B] == 0);
    r.src[INPUT_SRC_EAST] = 16384;
    xbox_InputMap_PadMap(&m, &r, &p);
    CHECK(p.analog[INPUT_ANALOG_B] == 255);

    /* Remap: swap A and B, put Start on the touchpad, X on the left trigger. */
    m.source[INPUT_A] = INPUT_SRC_EAST;
    m.source[INPUT_B] = INPUT_SRC_SOUTH;
    m.source[INPUT_START] = INPUT_SRC_TOUCHPAD;
    m.source[INPUT_X] = INPUT_SRC_LEFT_TRIGGER;
    memset(&r, 0, sizeof(r));
    r.src[INPUT_SRC_SOUTH] = 32767;
    r.src[INPUT_SRC_TOUCHPAD] = 32767;
    r.src[INPUT_SRC_LEFT_TRIGGER] = 32767;
    xbox_InputMap_PadMap(&m, &r, &p);
    CHECK(p.analog[INPUT_ANALOG_A] == 0 && p.analog[INPUT_ANALOG_B] == 255);
    CHECK(p.buttons == INPUT_PAD_START);
    CHECK(p.analog[INPUT_ANALOG_X] == 255);

    /* None leaves a control unbound. */
    xbox_InputMap_PadMapDefaults(&m);
    m.source[INPUT_A] = INPUT_SRC_NONE;
    memset(&r, 0, sizeof(r));
    r.src[INPUT_SRC_SOUTH] = 32767;
    xbox_InputMap_PadMap(&m, &r, &p);
    CHECK(p.analog[INPUT_ANALOG_A] == 0);

    /* Sticks: pass through with the deadzone, swap, or switch off. */
    xbox_InputMap_PadMapDefaults(&m);
    memset(&r, 0, sizeof(r));
    r.lx = 32767; r.ly = 0; r.rx = 0; r.ry = -32767;
    xbox_InputMap_PadMap(&m, &r, &p);
    CHECK(p.lx == 32767 && p.ry == -32767 && p.rx == 0);
    m.left_stick = INPUT_STICK_RIGHT;
    m.right_stick = INPUT_STICK_LEFT;
    xbox_InputMap_PadMap(&m, &r, &p);
    CHECK(p.ly == -32767 && p.rx == 32767 && p.lx == 0);
    m.right_stick = INPUT_STICK_NONE;
    xbox_InputMap_PadMap(&m, &r, &p);
    CHECK(p.rx == 0 && p.ry == 0);
    /* Drift inside the deadzone is ignored. */
    xbox_InputMap_PadMapDefaults(&m);
    r.lx = 2500; r.ly = -2500;
    xbox_InputMap_PadMap(&m, &r, &p);
    CHECK(p.lx == 0 && p.ly == 0);
}


static void test_api_edges(void)
{
    InputBindings b = {0};
    InputPad a = {0}, z = {0}, out;
    InputRaw raw = {0};
    InputPadMap map;
    uint8_t down[INPUT_KEY_COUNT] = {0};
    char long_text[600], small[2] = {'!', '!'}, text[80];
    const char *saved = xbox_InputMap_KeyName(INPUT_KEY_A);
    int i;
    CHECK(xbox_InputMap_KeyFromName(NULL) == -1);
    CHECK(xbox_InputMap_KeyName(INPUT_KEY_NONE) == NULL);
    CHECK(xbox_InputMap_KeyName(-1) == NULL);
    CHECK(xbox_InputMap_KeyName(INPUT_KEY_COUNT) == NULL);
    for (i = 1; i < INPUT_KEY_COUNT; i++) {
        const char *name = xbox_InputMap_KeyName(i);
        CHECK(name != NULL);
        CHECK(xbox_InputMap_KeyFromName(name) == i);
    }
    CHECK(!strcmp(saved, "A")); /* names remain valid after other lookups */
    CHECK(xbox_InputMap_BindingsParse(NULL, INPUT_A, "A") == -1);
    CHECK(xbox_InputMap_BindingsParse(&b, -1, "A") == -1);
    CHECK(xbox_InputMap_BindingsParse(&b, INPUT_CONTROL_COUNT, "A") == -1);
    CHECK(xbox_InputMap_BindingsParse(&b, INPUT_A, NULL) == 0);
    memset(long_text, 'q', sizeof(long_text));
    long_text[sizeof(long_text)-3] = ',';
    long_text[sizeof(long_text)-2] = 'B';
    long_text[sizeof(long_text)-1] = 0;
    CHECK(xbox_InputMap_BindingsParse(&b, INPUT_A, long_text) == 1);
    CHECK(b.count[INPUT_A] == 1 && b.key[INPUT_A][0] == INPUT_KEY_B);
    xbox_InputMap_BindingsFormat(&b, INPUT_A, small, 1);
    CHECK(small[0] == 0 && small[1] == '!');
    CHECK(xbox_InputMap_BindingsFormat(&b, INPUT_A, NULL, 0) == NULL);
    /* Caller-corrupted counts/IDs never read outside bounded storage. */
    b.count[INPUT_A] = 255;
    for (i = 0; i < INPUT_BINDINGS_PER_CONTROL; i++) b.key[INPUT_A][i] = UINT16_MAX;
    xbox_InputMap_BindingsFormat(&b, INPUT_A, text, sizeof(text));
    CHECK(!strcmp(text, "?, ?, ?, ?"));
    down[INPUT_KEY_NONE] = 1;
    b.key[INPUT_A][0] = INPUT_KEY_NONE;
    xbox_InputMap_KeysResolve(&b, down, &out);
    CHECK(out.analog[INPUT_ANALOG_A] == 0);
    a.lx = INT16_MIN; a.ly = INT16_MIN;
    z.lx = INT16_MAX; z.ly = INT16_MAX;
    xbox_InputMap_PadMerge(&z, &a);
    CHECK(z.lx == INT16_MIN && z.ly == INT16_MIN);
    xbox_InputMap_PadMapDefaults(&map);
    map.source[INPUT_A] = 255;
    raw.src[INPUT_SRC_LEFT_TRIGGER] = -32768;
    xbox_InputMap_PadMap(&map, &raw, &out);
    CHECK(out.analog[INPUT_ANALOG_A] == 0 && out.analog[INPUT_ANALOG_LTRIGGER] == 0);
    map.button_threshold = 255;
    raw.src[INPUT_SRC_START] = 32767;
    xbox_InputMap_PadMap(&map, &raw, &out);
    CHECK(out.buttons == INPUT_PAD_START);
    raw.src[INPUT_SRC_START] = 32766;
    xbox_InputMap_PadMap(&map, &raw, &out);
    CHECK(out.buttons == 0);
    CHECK(xbox_InputMap_ApplyTrigger(-32768, -100) == 0);
    CHECK(xbox_InputMap_ApplyTrigger(32767, 255) == 255);
}

/* Independent range/symmetry and format-roundtrip properties. */
static void test_properties(void)
{
    int x, y, dz, i;
    for (dz = 0; dz <= 100; dz += 5)
        for (x = -32767; x <= 32767; x += 8191)
            for (y = -32767; y <= 32767; y += 8191) {
                int16_t a, b, na, nb;
                xbox_InputMap_ApplyStick(x, y, dz, &a, &b);
                xbox_InputMap_ApplyStick(-x, -y, dz, &na, &nb);
                CHECK(a == -na && b == -nb);
                CHECK((int64_t)a*a + (int64_t)b*b <= INT64_C(32769)*32769);
            }
    for (dz = -5; dz <= 100; dz += 5) {
        int last = 0;
        for (i = -1; i <= 256; i++) {
            int v = xbox_InputMap_ApplyTrigger(i, dz);
            CHECK(v >= last); last = v;
        }
        CHECK(last == 255);
    }
    for (i = 1; i < INPUT_KEY_COUNT; i++) {
        InputBindings a = {0}, b = {0};
        char text[80];
        a.count[INPUT_A] = 1; a.key[INPUT_A][0] = (uint16_t)i;
        xbox_InputMap_BindingsFormat(&a, INPUT_A, text, sizeof(text));
        CHECK(xbox_InputMap_BindingsParse(&b, INPUT_A, text) == 0);
        CHECK(b.count[INPUT_A] == 1 && b.key[INPUT_A][0] == i);
    }
}

int main(void)
{
    test_key_names();
    test_bindings();
    test_resolve();
    test_merge();
    test_deadzone();
    test_padmap();
    test_api_edges();
    test_properties();
    if (g_failed) { printf("%d check(s) failed\n", g_failed); return 1; }
    printf("input_map: %d checks passed\n", g_checks);
    return 0;
}
