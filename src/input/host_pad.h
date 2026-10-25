/**
 * xbox_input -- the host pad as the host sees it, before any merge, and the
 * filter seam the enhancements layer's host actions use (opt-in,
 * XBOXRECOMP_ENHANCE=ON; docs/runtime/config.md, "[input.actions]").
 *
 * A host pad has buttons the Duke never had: Guide, the paddles, the
 * touchpad and the share button (MISC1). The guest map leaves them out, so
 * they reach the host side only, as the `extra` bits below. A pad filter
 * sees the host pad and the state the title is about to get, and may clear
 * buttons in the latter (a chord the host consumed); input_core.c calls it
 * at the end of xbox_InputGetState, the title's glue calls it from its own
 * pad read (cat's pad_input.c). With no filter set nothing changes.
 */
#ifndef XBOX_INPUT_HOST_PAD_H
#define XBOX_INPUT_HOST_PAD_H

#include <stdint.h>
#include "xinput_xbox.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The host-only buttons (struct xbox_host_pad.extra). */
#define XBOX_HOST_GUIDE     0x01u
#define XBOX_HOST_MISC1     0x02u   /* share, capture, mic */
#define XBOX_HOST_PADDLE1   0x04u   /* upper right (Elite P1, Deck R4) */
#define XBOX_HOST_PADDLE2   0x08u   /* upper left (P3, L4) */
#define XBOX_HOST_PADDLE3   0x10u   /* lower right (P2, R5) */
#define XBOX_HOST_PADDLE4   0x20u   /* lower left (P4, L5) */
#define XBOX_HOST_TOUCHPAD  0x40u
#define XBOX_HOST_EXTRA_MASK 0x7Fu

/* A read the backend could not make now (its lock was busy): the caller
 * keeps what it had. */
#define XBOX_INPUT_BUSY 170u        /* ERROR_BUSY */

struct xbox_host_pad {
    XBOX_INPUT_STATE pad;   /* the host pad mapped to the Duke, no keyboard */
    uint32_t extra;         /* XBOX_HOST_* */
    int connected;          /* 0: pad and extra are zero */
    /* The backend slot behind the title's port (xbox_InputGetPadState's), or
     * -1 for none: the flip tick reads this slot when the title has not
     * polled for a while. input_core.c's ports are their own slots; a title
     * that maps pads to ports itself (cat's pad_slots) says which. */
    int slot;
};

/* Called with the title's port, the host pad behind it (connected = 0 when
 * there is none: the keyboard alone, a script) and the state the title gets,
 * which it may change. */
typedef void (*xbox_input_pad_filter_fn)(DWORD port, const struct xbox_host_pad *host,
                                         XBOX_INPUT_STATE *st);

/* Install or remove (NULL) the filter xbox_InputGetState calls. Any thread;
 * a poll in flight finishes with the filter it loaded. */
void xbox_InputSetPadFilter(xbox_input_pad_filter_fn fn);

/* Nonzero while a filter is installed (the host actions are armed). A
 * backend reads the host-only buttons only then, so a stock run polls the
 * way it did before them. */
int xbox_InputPadFilterSet(void);

/* The host-only buttons the last poll of slot saw (0 when none). */
uint32_t xbox_InputHostExtra(DWORD slot);

/* Read slot's host pad off the poll thread (the flip tick): the guest map
 * plus extra. ERROR_SUCCESS, ERROR_DEVICE_NOT_CONNECTED (host zeroed), or
 * XBOX_INPUT_BUSY (host untouched). Publishes the extra bits, and clears
 * them when no pad is open, so a modifier held as the pad went away does
 * not stay held. */
DWORD xbox_input_backend_read_host(DWORD slot, struct xbox_host_pad *host);

/* The backend's clock (a test's fake one), in milliseconds. */
uint64_t xbox_InputNowMs(void);

#ifdef __cplusplus
}
#endif

#endif /* XBOX_INPUT_HOST_PAD_H */
