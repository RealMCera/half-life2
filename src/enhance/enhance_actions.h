/*
 * enhance_actions.h - host actions: named things the host does (open the
 * menu, toggle the stats strip, a mod's free camera) bound to keys, host-only
 * pad buttons and pad chords, without taking an input the title uses
 * (opt-in, XBOXRECOMP_ENHANCE=ON; docs/runtime/config.md, "[input.actions]").
 *
 * The toolkit and the mods register actions; [input.actions] binds them
 * (the highest file that names an action decides all of its bindings, as
 * [input.bindings] does for a control), else the action's defaults apply.
 * Bindings are read from the host side only: the pad filter
 * (xbox_enhance_pad_filter, mod.h) sees the host pad behind each poll, and
 * the flip tick reads it itself when the title has stopped polling (a
 * loading screen, a title that polls only in play). A pad the title gets
 * from a script never fires anything.
 *
 * Nothing runs unless a feature key arms the hook (menu.enabled,
 * overlay.stats, or a mod's own enable key through its API): the startup
 * line is "[ENHANCE] actions hook=off" or "[ENHANCE] actions hook=on
 * (<keys>)", and a golden run has hook=off.
 */
#ifndef ENHANCE_ACTIONS_H
#define ENHANCE_ACTIONS_H

#include <stdint.h>

#include "host_pad.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENHANCE_ACTIONS_MAX   64
#define ENHANCE_ACTION_BINDS  8     /* bindings per action */

/* Register an action. name is "<owner>.<verb>" in lowercase ASCII,
 * label what a menu shows, defaults a NULL-terminated list of bindings (or
 * NULL). Returns its id, or -1 (a duplicate name, a bad name, a full
 * table), with a line. Copies everything. After enhance_actions_start the
 * bindings are rebuilt at once. */
int enhance_action_register(const char *name, const char *label,
                            const char *const *defaults);
/* The same with an owner: the mod table index, so a mod whose init fails
 * leaves no action behind (enhance_actions_drop_owner). -1 is the toolkit. */
int enhance_action_register_owned(const char *name, const char *label,
                                  const char *const *defaults, int owner);
void enhance_actions_drop_owner(int owner);
/* Drop every mod's actions (xbox_mod_init starting over). */
void enhance_actions_drop_mods(void);

/* How many times the action fired since the last call (0 for a bad id).
 * Any thread; each fire is counted once. */
int enhance_action_pressed(int id);
/* Whether one of the action's bindings is held now, past its hold. */
int enhance_action_down(int id);
/* The id of a name, -1 when none; the name of an id, NULL. */
int enhance_action_find(const char *name);
const char *enhance_action_name(int id);

/* xbox_enhance_init: the toolkit's actions (menu.open, overlay.stats,
 * shot.window) into a cleared registry, before the mods' init. */
void enhance_actions_init(void);
/* After the mods' init: reads [input.actions] and input.chord_delay_ms,
 * builds the bindings, arms from the feature keys and logs the hook line. */
void enhance_actions_start(void);
/* Re-read [input.actions] and swap the bindings in (the menu, a reload).
 * Any thread; a poll in flight finishes on the old table. A held input
 * must be released before a new binding fires on it. */
void enhance_actions_rebuild(void);

/* Arm the hook when the config key is true: 1 when armed after the call.
 * A mod passes its enable key ("camera.free"). Before start the key is
 * kept for the hook line; after, a newly armed hook logs the line again. */
int enhance_actions_arm(const char *key);
/* The same for a mod's init (owner: its mod table index): the key goes
 * when the mod is dropped (enhance_actions_drop_owner). */
int enhance_actions_arm_owned(const char *key, int owner);
int enhance_actions_armed(void);

/* Report these guest bits (wButtons bits 0..7, analog index i as bit
 * 8 + i) released on port until each is physically released: the button
 * that closed the menu must not reach the title as a press. */
void enhance_actions_mask_until_release(unsigned port, uint32_t bits);

/* From the flip (nv2a_flip_hook_set): when a port has not been polled for
 * 100 ms, read its host pad and step the keys, host-only buttons and
 * modifier chords. Returns at once when disarmed. */
void enhance_actions_tick(void);

/* The pad filter's work (xbox_enhance_pad_filter calls it when armed). */
void enhance_actions_filter(unsigned port, const struct xbox_host_pad *host,
                            XBOX_INPUT_STATE *st);

/* Disarm, clear the registry, the bindings and the per-port state (tests). */
void enhance_actions_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* ENHANCE_ACTIONS_H */
