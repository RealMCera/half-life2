/*
 * mod.h - the enhancements layer's mods: game code built into the title's
 * executable that reads and steers the guest through one small API
 * (opt-in, XBOXRECOMP_ENHANCE=ON).
 *
 * A mod is a struct xbox_mod. The title lists its mods in a NULL-terminated
 * table and hands it over before xbox_enhance_init:
 *
 *   static const struct xbox_mod *const mods[] = { &free_camera, NULL };
 *   xbox_mod_register_table(mods);
 *   xbox_enhance_init(".", NULL);
 *
 * A call and not a self-registering macro: a static library drops an object
 * nothing names, a linker section is spelled differently for PE, Mach-O and
 * ELF, and the table gives the order. A title that calls nothing has no
 * mods and stays stock.
 *
 * xbox_enhance_init runs each mod's init, in table order, as its last step
 * (after the config files and the [input] keys, so the getters see every
 * file). The guest has not started then and its memory is not mapped yet:
 * a mod registers what it wants and touches the guest from its on_frame
 * callbacks on. A non-zero init leaves that mod out: what it registered is
 * dropped, its capture released and its shutdown never called. Shutdown
 * runs in reverse order, once, at exit (atexit, and recomp_exit_hook for
 * the Win32 windows' ExitProcess) or on xbox_mod_shutdown.
 *
 * Shutdown can run while the guest and the render thread still do: a
 * frame or a filter call may be in flight, or start just before the filter
 * is removed. A mod's shutdown therefore must not free or invalidate
 * anything its on_frame or filter callbacks use; restore guest state, give
 * input back, write files, and leave the memory to the process exit.
 *
 * The title's glue calls two entries: xbox_mod_frame once per guest frame,
 * from the guest's thread, and xbox_mod_input_captured from its pad read,
 * where a captured pad reads as released to the title.
 *
 * With no mods: no lines, no callbacks, and the draw paths' one branch
 * (nv2a_batch_skip) is never taken. Docs: docs/runtime/config.md, "Mods".
 */
#ifndef XBOX_ENHANCE_MOD_H
#define XBOX_ENHANCE_MOD_H

#include <stddef.h>
#include <stdint.h>

#include "enhance_input.h"
#include "nv2a_backend_common.h"
#include "recomp_env.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XBOX_MOD_API_VERSION 1u

struct xbox_mod_api;

struct xbox_mod {
    const char *name;           /* the log lines' [MOD name] */
    /* 0 to run; anything else leaves the mod out. May be NULL. */
    int (*init)(const struct xbox_mod_api *api);
    void (*shutdown)(void);     /* may be NULL */
};

typedef void (*xbox_mod_frame_fn)(void *user);
/* Non-zero skips the batch (nv2a_backend_common.h, "Batch filter"). */
typedef int (*xbox_mod_batch_fn)(const struct nv2a_batch_info *b, void *user);

struct xbox_mod_api {
    uint32_t version;           /* XBOX_MOD_API_VERSION */
    uint32_t size;              /* sizeof(struct xbox_mod_api): fields are only added */

    /* The guest's memory: guest address va is host address mem + va. Set
     * before the first on_frame or filter call; 0 during init. */
    uintptr_t mem;

    /* The layer's config getters (enhance_cfg.h): env over the files over
     * the default. A mod's keys sit under its own table ([mods.<name>] by
     * convention) and count as used, so they are not reported. */
    const char *(*cfg_string)(const char *key, const char *def);
    long long (*cfg_int)(const char *key, long long def);
    double (*cfg_float)(const char *key, double def);
    int (*cfg_bool)(const char *key, int def);
    int (*cfg_choice)(const char *key, const char *const *choices, int def);
    int (*cfg_bind_env)(const char *key, recomp_env_id id);

    /* One stderr line, "[MOD <m->name>] ..."; the newline is added. */
    void (*log)(const struct xbox_mod *m, const char *fmt, ...);

    /* Init only. fn(user) runs once per guest frame from xbox_mod_frame, on
     * the guest's thread, mods in table order and each mod's in the order
     * it asked. Returns 0 when the table is full (XBOX_MOD_MAX_FRAME). */
    int (*on_frame)(xbox_mod_frame_fn fn, void *user);

    /* Port 0 and the mouse as enhance_input_state gives them: the title's
     * last pad read (a mod never polls: a poll moves the keyboard's stick
     * ramp) and, while captured, the mouse's raw motion. Any thread. */
    void (*input_state)(struct enhance_input_state *out);
    /* Takes port 0 from the title (1) or gives it back (0); see
     * enhance_input_capture. The last call wins across mods. */
    void (*input_capture)(int on);

    /* Init only. fn(b, user) is asked before each GPU draw, on the render
     * (ack) thread; any filter's non-zero skips the batch. Returns 0 when
     * the table is full (XBOX_MOD_MAX_FILTER). */
    int (*batch_filter)(xbox_mod_batch_fn fn, void *user);
};

#define XBOX_MOD_MAX_FRAME   16
#define XBOX_MOD_MAX_FILTER  8

/* Guest memory through the API: XBOX_MOD_MEM32(api, 0x0035C2A0) = 1. The
 * guest is little-endian and so is every host the toolkit runs on. */
#define XBOX_MOD_MEM32(api, va) (*(volatile uint32_t *)((api)->mem + (uint32_t)(va)))
#define XBOX_MOD_MEMF(api, va)  (*(volatile float *)((api)->mem + (uint32_t)(va)))

/* The title's table, NULL-terminated and kept (not copied): before
 * xbox_enhance_init. A second call replaces the first; NULL clears it. */
void xbox_mod_register_table(const struct xbox_mod *const *mods);

/* Called by xbox_enhance_init. Runs the inits; returns the mods running. */
int xbox_mod_init(void);

/* The title's glue, once per guest frame on the guest's thread. */
void xbox_mod_frame(void);

/* The title's pad glue: whether a mod has port 0 (enhance_input_captured). */
int xbox_mod_input_captured(void);

/* Removes the filter, stops the frame dispatch, then the mods' shutdowns,
 * in reverse order, and the capture released. Runs once; the exit paths
 * call it. A later xbox_mod_init starts over (tests). */
void xbox_mod_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* XBOX_ENHANCE_MOD_H */
