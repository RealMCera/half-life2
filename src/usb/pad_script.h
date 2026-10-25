/*
 * pad_script.h -- the scripted pad's steps, and the recorder that writes them.
 *
 * Kept apart from usb_gamepad.c so the grammar and the recorder can be tested
 * without a USB stack: this file knows a 20-byte report and text, nothing else.
 *
 * A step is <at>:<button>[+<button>...][:<hold>], numbers in decimal. <at>
 * is milliseconds from the first pad report (the original form), or f<flip>:
 * the pushbuffer executor's flip count, the "[GPU] flip N" number, with the
 * hold in flips too (a trailing "f" on the hold is allowed). A millisecond
 * script lands wherever the host's pace has got to, so a fast host reaches a
 * screen hundreds of flips before a slow one at the same wall time. A
 * flip-timed one presses at the same flip count instead. That count is the
 * walker's, not the title's frame counter: on a steady screen a replay lands
 * within about 1-2 flips, but loading screens, movies and waits on the audio
 * clock take a different number of flips per host, so hold presses that
 * wait on a screen generously. The count only moves with the LLE pushbuffer
 * walker (nv2a_pb_exec.c calls usb_gamepad_flip); on the HLE D3D8 present
 * path it stays 0 and a flip-timed step fires at boot.
 *
 * A stick may also be set to a value: lx=<v>, ly, rx, ry, -32768..32767, up
 * positive. "#" starts a comment to the end of the line, for files.
 */
#ifndef XBOX_USB_PAD_SCRIPT_H
#define XBOX_USB_PAD_SCRIPT_H

#include <stddef.h>
#include <stdint.h>

/* A recorded route of a few minutes is a few hundred button steps, but an
 * analog stick held by hand moves most flips: a long stroll can pass ten
 * thousand steps. */
#define PAD_SCRIPT_MAX 32768

/* An unheld flip-timed step lasts this many flips, the 150 ms default of the
 * millisecond form at 60 flips a second. */
#define PAD_HOLD_FLIPS_DEFAULT 9

typedef struct {
    unsigned long at, hold;  /* ms, or flips when by_flip */
    uint8_t digital;         /* report byte 2 */
    uint8_t analog;          /* bit n = report byte 4 + n */
    uint8_t pad;             /* which controller: "p2-a" is pad 1 */
    uint8_t stick;           /* full deflection: 1 L-left 2 L-right 4 L-up
                                8 L-down, then the same four for the right */
    uint8_t axis_set;        /* bit n: axis[n] (LX LY RX RY) is set */
    int16_t axis[4];
    uint8_t by_flip;
    int announced;
} PadStep;

/* Parse text (comma or newline separated) and append its steps to
 * steps[len..max). Returns the new length. Bad entries are reported on
 * stderr; parsing stops at an entry without a time, and steps past max are
 * dropped with one warning. */
int pad_script_parse(const char *text, PadStep *steps, int len, int max);

/* Whether st holds at t ms from the first report and at the given flip. */
int pad_step_active(const PadStep *st, unsigned long t_ms, uint32_t flip);

/* Has a step run its course, so a live session can drop it. */
int pad_step_done(const PadStep *st, unsigned long t_ms, uint32_t flip);

/* A flip-timed step whose flip has passed starts at flip instead, so a line
 * appended live with an old time is pressed at once, not dropped. */
void pad_step_not_before(PadStep *st, uint32_t flip);

/* Press st's buttons into a 20-byte report. */
void pad_step_press(const PadStep *st, uint8_t *report);

/* ---- the recorder: RECOMP_DEBUG=pad_record=<file> ---------------------- */

/* What a report presses, in the script's terms: analog buttons and triggers
 * past the XInput trigger threshold count as pressed (a script presses them
 * fully), and the sticks keep their values, so an analog walk, a keyboard
 * stick's ramp or a half-throw walk key replays as the title saw it. */
typedef struct {
    uint8_t digital, analog;
    int16_t ax[4];           /* LX LY RX RY */
} PadBits;

PadBits pad_bits_from_report(const uint8_t *report);

/* The button list a step would name: "start+a". Empty for no buttons. */
void pad_bits_name(PadBits b, char *out, size_t n);

/* The recorder keeps two tracks per pad, the buttons and the sticks, so a
 * stick moving under a held button does not split the button's step. The
 * reports within one flip merge: buttons pressed in any of them, the sticks
 * as the last one had them. */
typedef struct {
    PadBits cur;             /* what the open steps hold */
    uint32_t since, ax_since;
    PadBits pend;            /* the flip being gathered */
    uint32_t pend_flip;
    int pad;
    int started;
} PadRecorder;

/* Feed one report's state at a flip. When a flip's state differs from the
 * one before, the steps it ends are written to out, one per line
 * ("f<flip>:<buttons>:<flips>", "f<flip>:lx=<v>+ly=<v>:<flips>", "p2-" for
 * pads past the first). Returns the number of lines. A stick change smaller
 * than PAD_AXIS_BAND is not a new step (a resting hand's jitter, a pad's
 * off-centre rest), except onto rest and full throw. */
#define PAD_AXIS_BAND 256
int pad_recorder_feed(PadRecorder *r, PadBits b, uint32_t flip, char *out, size_t n);

/* Write the steps still held at the end of a run, as if released at flip
 * (or the flip after the last report, if that is later). */
int pad_recorder_end(PadRecorder *r, uint32_t flip, char *out, size_t n);

#endif /* XBOX_USB_PAD_SCRIPT_H */
