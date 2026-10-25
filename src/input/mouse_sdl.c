/*
 * The SDL window's mouse into the mouse (mouse.c), on POSIX hosts.
 *
 * Like keyboard_sdl.c, the presenter's main loop hands every event here and
 * keeps no mouse logic of its own, so tests/input_keyboard_sdl drives these
 * rules with synthetic events.
 *
 * The capture state machine lives in mouse.c and may change on the game's
 * poll thread (the release key arrives through the key table, a script
 * forces it). SDL's relative mode must be set on the window's thread, so
 * this side only mirrors the answer, once per loop pass.
 */
#include "mouse_sdl.h"
#include "mouse.h"
#include "input_names.h"

static int s_relative = -1;     /* the mode last applied; -1 none yet */
/* SDL3 reports motion and the wheel in fractions (high-resolution mice,
 * trackpads); mouse.c counts whole steps. The part
 * left over carries into the next event, so slow motion still adds up. */
static float s_rem_x, s_rem_y, s_rem_wheel;

/* SDL3 event timestamps are nanoseconds; mouse.c's clock is milliseconds
 * and wraps. */
static uint32_t ms_of(Uint64 ns)
{
    return (uint32_t)(ns / 1000000u);
}

/* The whole steps in *acc + d, with the rest left in *acc. */
static int whole(float *acc, float d)
{
    int n;

    *acc += d;
    n = (int)*acc;              /* toward zero, so the rest keeps its sign */
    *acc -= (float)n;
    return n;
}

void xbox_InputSdlMouseEvent(const SDL_Event *ev)
{
    int n;

    switch (ev->type) {
    case SDL_EVENT_MOUSE_MOTION: {
        int dx, dy;
        /* A touch screen reports as a mouse too; it is not one. */
        if (ev->motion.which == SDL_TOUCH_MOUSEID)
            return;
        dx = whole(&s_rem_x, ev->motion.xrel);
        dy = whole(&s_rem_y, ev->motion.yrel);
        if (dx || dy)
            xbox_InputMouseMotion(dx, dy);
        return;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (ev->button.which == SDL_TOUCH_MOUSEID)
            return;
        switch (ev->button.button) {
        case SDL_BUTTON_LEFT:   n = XBOX_MOUSE_LEFT;   break;
        case SDL_BUTTON_RIGHT:  n = XBOX_MOUSE_RIGHT;  break;
        case SDL_BUTTON_MIDDLE: n = XBOX_MOUSE_MIDDLE; break;
        case SDL_BUTTON_X1:     n = XBOX_MOUSE_4;      break;
        case SDL_BUTTON_X2:     n = XBOX_MOUSE_5;      break;
        default:                return;
        }
        /* SDL reports no activation click: on X11 and Wayland the click
         * that focuses the window arrives as an ordinary press right after
         * the focus event, so mouse.c judges it by the two timestamps. On
         * macOS SDL drops that click itself (SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH
         * is off by default). */
        xbox_InputMouseButtonAt(n, ev->button.down, ms_of(ev->button.timestamp));
        return;
    case SDL_EVENT_MOUSE_WHEEL:
        if (ev->wheel.which == SDL_TOUCH_MOUSEID)
            return;
        /* "Natural" scrolling flips the sign SDL reports; the direction
         * field says so, and WheelUp means the wheel turned away from the
         * player on every host. */
        n = whole(&s_rem_wheel, ev->wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                                ? -ev->wheel.y : ev->wheel.y);
        if (n)
            xbox_InputMouseWheel(n);
        return;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        xbox_InputMouseFocusGainedAt(ms_of(ev->window.timestamp));
        return;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_HIDDEN:
    case SDL_EVENT_WINDOW_MINIMIZED:
        xbox_InputMouseFocus(1);
        return;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
    case SDL_EVENT_QUIT:
        xbox_InputMouseClose();
        return;
    }
}

int xbox_InputSdlMouseSync(SDL_Window *win)
{
    int want = xbox_InputMouseCaptured();

    if (want == s_relative)
        return 0;
    /* Relative mode hides the cursor, confines it and reports motion past
     * the screen edge: the capture in one call. A failure (no mouse, the
     * dummy driver) leaves motion as absolute deltas, which still work
     * until the cursor reaches an edge. */
    if (win)
        SDL_SetWindowRelativeMouseMode(win, want != 0);
    s_relative = want;
    return 1;
}
