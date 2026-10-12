/*
 * input_map -- what a host key, mouse button or pad control means to an Xbox
 * controller.
 *
 * Pure arithmetic and text: no Windows, no SDL, no mutable globals. An adopting host layer
 * reads the devices and hands normalized values to this; keeping
 * the rules here is what lets a test check binding resolution and deadzones
 * without a pad on the desk.
 *
 * Three pieces:
 *
 *   Keyboard and mouse bindings. Each Xbox control has up to four host inputs
 *   bound to it, written as a comma separated list of names ("L, Space,
 *   Mouse1"). Keys use stable InputKey identifiers defined here, never native virtual-key
 *   codes, SDL keycodes or scancodes. The caller translates host events.
 *
 *   Pad remapping. Each of the pad's sixteen digital and trigger controls
 *   takes its value from one named physical control (the names are SDL's
 *   positional ones: south is the bottom face button, an Xbox A, a PlayStation
 *   cross), and each stick can come from either physical stick or from none.
 *
 *   Deadzones. A radial deadzone for sticks that rescales so the stick still
 *   reaches full deflection, and a threshold for triggers.
 */
#ifndef XBOX_INPUT_MAP_H
#define XBOX_INPUT_MAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pointer contract: structs, arrays and output pointers must be non-NULL unless
 * a function explicitly documents otherwise. Independent caller-owned states
 * may be used concurrently. Coordinate access to a shared mutable state.
 * Names and InputKey values are stable library identifiers, not host events.
 */

/* The Xbox controller's inputs, in the order settings list them. The first
 * sixteen are what a pad's physical controls can be remapped onto. */
typedef enum InputControl {
    INPUT_DPAD_UP, INPUT_DPAD_DOWN, INPUT_DPAD_LEFT, INPUT_DPAD_RIGHT,
    INPUT_START, INPUT_BACK, INPUT_LEFT_THUMB, INPUT_RIGHT_THUMB,
    INPUT_A, INPUT_B, INPUT_X, INPUT_Y, INPUT_BLACK, INPUT_WHITE,
    INPUT_LEFT_TRIGGER, INPUT_RIGHT_TRIGGER,
    /* Keyboard only: the sticks as four keys each. */
    INPUT_LSTICK_UP, INPUT_LSTICK_DOWN, INPUT_LSTICK_LEFT, INPUT_LSTICK_RIGHT,
    INPUT_RSTICK_UP, INPUT_RSTICK_DOWN, INPUT_RSTICK_LEFT, INPUT_RSTICK_RIGHT,
    INPUT_CONTROL_COUNT
} InputControl;
#define INPUT_PAD_CONTROL_COUNT 16

/* Names use ASCII case-insensitive matching. The setting key for a control ("dpad_up", "left_stick_left"), or NULL. */
const char *xbox_InputMap_ControlName(int control);

/* An Xbox controller's state as the title sees it. Semantic values for constructing an Xbox report (not a packed wire struct): digital buttons in the low byte, eight analog buttons (A B X Y Black
 * White, left and right trigger), and the sticks with Y positive upwards. */
typedef struct InputPad {
    uint16_t buttons;               /* INPUT_PAD_* bits */
    uint8_t  analog[8];             /* INPUT_ANALOG_* */
    int16_t  lx, ly, rx, ry;
} InputPad;

#define INPUT_PAD_DPAD_UP     0x0001
#define INPUT_PAD_DPAD_DOWN   0x0002
#define INPUT_PAD_DPAD_LEFT   0x0004
#define INPUT_PAD_DPAD_RIGHT  0x0008
#define INPUT_PAD_START       0x0010
#define INPUT_PAD_BACK        0x0020
#define INPUT_PAD_LEFT_THUMB  0x0040
#define INPUT_PAD_RIGHT_THUMB 0x0080

#define INPUT_ANALOG_A        0
#define INPUT_ANALOG_B        1
#define INPUT_ANALOG_X        2
#define INPUT_ANALOG_Y        3
#define INPUT_ANALOG_BLACK    4
#define INPUT_ANALOG_WHITE    5
#define INPUT_ANALOG_LTRIGGER 6
#define INPUT_ANALOG_RTRIGGER 7

/* Add `src` to `dst`: buttons are ORed, analog buttons take the larger, and a
 * stick takes whichever of the two is deflected further. */
void xbox_InputMap_PadMerge(InputPad *dst, const InputPad *src);

/* ---- keyboard and mouse -------------------------------------------------- */

typedef enum InputKey {
    INPUT_KEY_NONE = 0, /* reserved, never a held key */
    INPUT_KEY_MOUSE1 = 1,
    INPUT_KEY_MOUSE2 = 2,
    INPUT_KEY_MOUSE3 = 3,
    INPUT_KEY_MOUSE4 = 4,
    INPUT_KEY_MOUSE5 = 5,
    INPUT_KEY_WHEEL_UP = 6,
    INPUT_KEY_WHEEL_DOWN = 7,
    INPUT_KEY_BACKSPACE = 8,
    INPUT_KEY_TAB = 9,
    INPUT_KEY_ENTER = 10,
    INPUT_KEY_SHIFT = 11,
    INPUT_KEY_CTRL = 12,
    INPUT_KEY_ALT = 13,
    INPUT_KEY_PAUSE = 14,
    INPUT_KEY_CAPSLOCK = 15,
    INPUT_KEY_ESCAPE = 16,
    INPUT_KEY_SPACE = 17,
    INPUT_KEY_PAGEUP = 18,
    INPUT_KEY_PAGEDOWN = 19,
    INPUT_KEY_END = 20,
    INPUT_KEY_HOME = 21,
    INPUT_KEY_LEFT = 22,
    INPUT_KEY_UP = 23,
    INPUT_KEY_RIGHT = 24,
    INPUT_KEY_DOWN = 25,
    INPUT_KEY_INSERT = 26,
    INPUT_KEY_DELETE = 27,
    INPUT_KEY_NUMPADMULTIPLY = 28,
    INPUT_KEY_NUMPADADD = 29,
    INPUT_KEY_NUMPADSUBTRACT = 30,
    INPUT_KEY_NUMPADDECIMAL = 31,
    INPUT_KEY_NUMPADDIVIDE = 32,
    INPUT_KEY_LSHIFT = 33,
    INPUT_KEY_RSHIFT = 34,
    INPUT_KEY_LCTRL = 35,
    INPUT_KEY_RCTRL = 36,
    INPUT_KEY_LALT = 37,
    INPUT_KEY_RALT = 38,
    INPUT_KEY_SEMICOLON = 39,
    INPUT_KEY_EQUALS = 40,
    INPUT_KEY_COMMA = 41,
    INPUT_KEY_MINUS = 42,
    INPUT_KEY_PERIOD = 43,
    INPUT_KEY_SLASH = 44,
    INPUT_KEY_GRAVE = 45,
    INPUT_KEY_LEFTBRACKET = 46,
    INPUT_KEY_BACKSLASH = 47,
    INPUT_KEY_RIGHTBRACKET = 48,
    INPUT_KEY_QUOTE = 49,
    INPUT_KEY_A = 50,
    INPUT_KEY_B = 51,
    INPUT_KEY_C = 52,
    INPUT_KEY_D = 53,
    INPUT_KEY_E = 54,
    INPUT_KEY_F = 55,
    INPUT_KEY_G = 56,
    INPUT_KEY_H = 57,
    INPUT_KEY_I = 58,
    INPUT_KEY_J = 59,
    INPUT_KEY_K = 60,
    INPUT_KEY_L = 61,
    INPUT_KEY_M = 62,
    INPUT_KEY_N = 63,
    INPUT_KEY_O = 64,
    INPUT_KEY_P = 65,
    INPUT_KEY_Q = 66,
    INPUT_KEY_R = 67,
    INPUT_KEY_S = 68,
    INPUT_KEY_T = 69,
    INPUT_KEY_U = 70,
    INPUT_KEY_V = 71,
    INPUT_KEY_W = 72,
    INPUT_KEY_X = 73,
    INPUT_KEY_Y = 74,
    INPUT_KEY_Z = 75,
    INPUT_KEY_0 = 76,
    INPUT_KEY_1 = 77,
    INPUT_KEY_2 = 78,
    INPUT_KEY_3 = 79,
    INPUT_KEY_4 = 80,
    INPUT_KEY_5 = 81,
    INPUT_KEY_6 = 82,
    INPUT_KEY_7 = 83,
    INPUT_KEY_8 = 84,
    INPUT_KEY_9 = 85,
    INPUT_KEY_F1 = 86,
    INPUT_KEY_F2 = 87,
    INPUT_KEY_F3 = 88,
    INPUT_KEY_F4 = 89,
    INPUT_KEY_F5 = 90,
    INPUT_KEY_F6 = 91,
    INPUT_KEY_F7 = 92,
    INPUT_KEY_F8 = 93,
    INPUT_KEY_F9 = 94,
    INPUT_KEY_F10 = 95,
    INPUT_KEY_F11 = 96,
    INPUT_KEY_F12 = 97,
    INPUT_KEY_NUMPAD0 = 98,
    INPUT_KEY_NUMPAD1 = 99,
    INPUT_KEY_NUMPAD2 = 100,
    INPUT_KEY_NUMPAD3 = 101,
    INPUT_KEY_NUMPAD4 = 102,
    INPUT_KEY_NUMPAD5 = 103,
    INPUT_KEY_NUMPAD6 = 104,
    INPUT_KEY_NUMPAD7 = 105,
    INPUT_KEY_NUMPAD8 = 106,
    INPUT_KEY_NUMPAD9 = 107,
    INPUT_KEY_COUNT = 108
} InputKey;
#define INPUT_BINDINGS_PER_CONTROL 4

typedef struct InputBindings {
    uint16_t key[INPUT_CONTROL_COUNT][INPUT_BINDINGS_PER_CONTROL];
    uint8_t  count[INPUT_CONTROL_COUNT];
} InputBindings;

/* Stable InputKey ID for a key name, case-insensitive ("a", "space", "numpad4", "mouse1",
 * "wheelup", "lshift"), or -1 if there is no such key. */
int xbox_InputMap_KeyFromName(const char *name);

/* Immutable canonical name for an InputKey ID, or NULL. Safe across threads. */
const char *xbox_InputMap_KeyName(int code);

/* Parse "L, Space" into `control`'s bindings, replacing them. "none" or an
 * empty string clears it. Names that are not keys are skipped and counted in
 * the return value; at most four keys are kept. Returns the number of names
 * rejected, or -1 for an invalid binding pointer/control. No fixed input-text limit. */
int xbox_InputMap_BindingsParse(InputBindings *b, int control, const char *text);

/* The bindings of `control` as text that xbox_InputMap_BindingsParse reads back. */
const char *xbox_InputMap_BindingsFormat(const InputBindings *b, int control,
                                  char *buf, int size);

/* The pad a set of held keys makes. Counts are bounded to four per control. `down` has INPUT_KEY_COUNT entries, non-zero
 * for held. Required pointers must be non-NULL. A stick built from keys is a full-deflection circle, so a diagonal
 * is not faster than a straight line. */
void xbox_InputMap_KeysResolve(const InputBindings *b, const uint8_t *down, InputPad *out);

/* ---- pad remapping and deadzones ------------------------------------------ */

/* The physical controls a pad has, SDL's positional names. */
typedef enum InputSource {
    INPUT_SRC_NONE,
    INPUT_SRC_SOUTH, INPUT_SRC_EAST, INPUT_SRC_WEST, INPUT_SRC_NORTH,
    INPUT_SRC_BACK, INPUT_SRC_GUIDE, INPUT_SRC_START,
    INPUT_SRC_LEFT_STICK_CLICK, INPUT_SRC_RIGHT_STICK_CLICK,
    INPUT_SRC_LEFT_SHOULDER, INPUT_SRC_RIGHT_SHOULDER,
    INPUT_SRC_DPAD_UP, INPUT_SRC_DPAD_DOWN, INPUT_SRC_DPAD_LEFT, INPUT_SRC_DPAD_RIGHT,
    INPUT_SRC_MISC1, INPUT_SRC_TOUCHPAD,
    INPUT_SRC_PADDLE1, INPUT_SRC_PADDLE2, INPUT_SRC_PADDLE3, INPUT_SRC_PADDLE4,
    INPUT_SRC_LEFT_TRIGGER, INPUT_SRC_RIGHT_TRIGGER,
    /* A stick pushed one way, 0 to full. */
    INPUT_SRC_LSTICK_UP, INPUT_SRC_LSTICK_DOWN, INPUT_SRC_LSTICK_LEFT, INPUT_SRC_LSTICK_RIGHT,
    INPUT_SRC_RSTICK_UP, INPUT_SRC_RSTICK_DOWN, INPUT_SRC_RSTICK_LEFT, INPUT_SRC_RSTICK_RIGHT,
    INPUT_SRC_COUNT
} InputSource;

const char *xbox_InputMap_SourceName(int source);
int xbox_InputMap_SourceFromName(const char *name);      /* -1 if unknown */

/* The adapter must populate directional sources explicitly if they are used
 * as bindings; stick axes are not automatically copied into src[].
 * Missing sources are zero. Negative src[] values are treated as zero.
 * Digital/face-button mappings use button_threshold (clamped to 0..100%).
 */

/* What the host read from a pad before any mapping. Every source is 0 to
 * 32767 (a button is 0 or 32767); the sticks are signed, Y positive upwards. */
typedef struct InputRaw {
    int16_t src[INPUT_SRC_COUNT];
    int16_t lx, ly, rx, ry;
} InputRaw;

enum { INPUT_STICK_LEFT, INPUT_STICK_RIGHT, INPUT_STICK_NONE };

typedef struct InputPadMap {
    uint8_t source[INPUT_PAD_CONTROL_COUNT];   /* InputSource for each control */
    uint8_t left_stick;                        /* INPUT_STICK_*: which physical stick */
    uint8_t right_stick;
    uint8_t deadzone_left;                     /* percent, 0-95 */
    uint8_t deadzone_right;
    uint8_t trigger_threshold;                 /* percent, 0-95 */
    uint8_t button_threshold;                  /* percent a trigger/stick must reach to count as a button */
} InputPadMap;

/* The identity mapping: a pad as the Xbox pad it is shaped like. Deadzones 15%,
 * trigger threshold 5%, an axis counts as a button at 50%. */
void xbox_InputMap_PadMapDefaults(InputPadMap *m);

/* Apply `m` to a raw pad. */
void xbox_InputMap_PadMap(const InputPadMap *m, const InputRaw *raw, InputPad *out);

/* A radial deadzone (percent clamped to 0..95): a stick inside `deadzone_pct` of full deflection reads as
 * centred, and the range beyond it is stretched so full deflection is still
 * full. The direction is preserved. */
void xbox_InputMap_ApplyStick(int x, int y, int deadzone_pct, int16_t *ox, int16_t *oy);

/* A trigger threshold is clamped to 0..95. A trigger value (0-255) below `threshold_pct` of the range reads 0; the rest
 * is stretched to 0-255. */
uint8_t xbox_InputMap_ApplyTrigger(int value, int threshold_pct);

#ifdef __cplusplus
}
#endif

#endif /* XBOX_INPUT_MAP_H */
