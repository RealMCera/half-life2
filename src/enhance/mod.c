/*
 * mod.c - see mod.h. The tables are filled during init on the main thread,
 * before the guest and the render thread start, and only read after: the
 * frame and draw paths take no lock. Shutdown leaves them as they are: the
 * guest's thread or the render thread may be inside a callback then, and
 * the filter pointer (nv2a_batch_filter_set) is what stops new asks.
 */
#include "mod.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "enhance_cfg.h"
#include "recomp_exit_hook.h"
#include "xbox_memory_layout.h"

#define LOGP "[MOD] "

struct frame_cb { xbox_mod_frame_fn fn; void *user; int owner; };
struct filter_cb { xbox_mod_batch_fn fn; void *user; int owner; };

static const struct xbox_mod *const *s_table;
static const struct xbox_mod *s_running[64];
static int s_nrunning;
static int s_init_owner = -1;       /* the table index whose init is running */

static struct frame_cb s_frame[XBOX_MOD_MAX_FRAME];
static int s_nframe;
static struct filter_cb s_filter[XBOX_MOD_MAX_FILTER];
static int s_nfilter;

static int s_shut;                  /* 1 once shutdown has run */
static int s_exit_hooked;

static struct xbox_mod_api s_api;

static void api_log(const struct xbox_mod *m, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "[MOD %s] ", m && m->name ? m->name : "?");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static int api_on_frame(xbox_mod_frame_fn fn, void *user)
{
    if (s_init_owner < 0 || !fn || s_nframe >= XBOX_MOD_MAX_FRAME)
        return 0;
    s_frame[s_nframe++] = (struct frame_cb){ fn, user, s_init_owner };
    return 1;
}

static int api_batch_filter(xbox_mod_batch_fn fn, void *user)
{
    if (s_init_owner < 0 || !fn || s_nfilter >= XBOX_MOD_MAX_FILTER)
        return 0;
    s_filter[s_nfilter++] = (struct filter_cb){ fn, user, s_init_owner };
    return 1;
}

/* api.mem, once the guest has mapped its memory (after xbox_enhance_init,
 * before its first frame or draw). Either the guest's thread (frames) or
 * the render thread (filters) may get there first; both store the same
 * value. A mod with only a filter never sees a frame, so both ask. */
static void mod_mem(void)
{
    if (!__atomic_load_n(&s_api.mem, __ATOMIC_ACQUIRE))
        __atomic_store_n(&s_api.mem, (uintptr_t)xbox_GetMemoryOffset(), __ATOMIC_RELEASE);
}

/* The one filter the backends see: the mods' filters ORed, in order. */
static int filter_all(const struct nv2a_batch_info *b)
{
    int i;
    mod_mem();
    for (i = 0; i < s_nfilter; i++)
        if (s_filter[i].fn(b, s_filter[i].user))
            return 1;
    return 0;
}

/* A failed init leaves nothing behind. */
static void drop_owner(int owner)
{
    int i, n;

    for (i = n = 0; i < s_nframe; i++)
        if (s_frame[i].owner != owner)
            s_frame[n++] = s_frame[i];
    s_nframe = n;
    for (i = n = 0; i < s_nfilter; i++)
        if (s_filter[i].owner != owner)
            s_filter[n++] = s_filter[i];
    s_nfilter = n;
}

static void exit_hook(const char *why)
{
    (void)why;
    xbox_mod_shutdown();
}

static void at_exit(void)
{
    xbox_mod_shutdown();
}

void xbox_mod_register_table(const struct xbox_mod *const *mods)
{
    s_table = mods;
}

int xbox_mod_init(void)
{
    int i;

    s_nrunning = s_nframe = s_nfilter = 0;
    s_shut = 0;
    memset(&s_api, 0, sizeof s_api);
    if (!s_table || !s_table[0]) {
        nv2a_batch_filter_set(NULL);
        return 0;   /* no lines: a title without mods logs as before */
    }

    s_api.version = XBOX_MOD_API_VERSION;
    s_api.size = sizeof s_api;
    s_api.cfg_string = enhance_cfg_string;
    s_api.cfg_int = enhance_cfg_int;
    s_api.cfg_float = enhance_cfg_float;
    s_api.cfg_bool = enhance_cfg_bool;
    s_api.cfg_choice = enhance_cfg_choice;
    s_api.cfg_bind_env = enhance_cfg_bind_env;
    s_api.log = api_log;
    s_api.on_frame = api_on_frame;
    s_api.input_state = enhance_input_state;
    s_api.input_capture = enhance_input_capture;
    s_api.batch_filter = api_batch_filter;

    for (i = 0; s_table[i]; i++) {
        const struct xbox_mod *m = s_table[i];
        int rc = 0, held = enhance_input_captured();

        if (s_nrunning >= (int)(sizeof s_running / sizeof s_running[0])) {
            fprintf(stderr, LOGP "%s: more than %d mods; left out\n",
                    m->name ? m->name : "?", s_nrunning);
            continue;
        }
        s_init_owner = i;
        if (m->init)
            rc = m->init(&s_api);
        s_init_owner = -1;
        if (rc) {
            drop_owner(i);
            /* Back to what the earlier mods left, either way. */
            enhance_input_capture(held);
            fprintf(stderr, LOGP "%s: init returned %d; left out\n",
                    m->name ? m->name : "?", rc);
            continue;
        }
        s_running[s_nrunning++] = m;
    }

    /* Installed last, so a filter a failed init asked for is never seen. */
    nv2a_batch_filter_set(s_nfilter ? filter_all : NULL);
    if (s_nrunning && !s_exit_hooked) {
        s_exit_hooked = 1;
        atexit(at_exit);
        if (!recomp_exit_hook_add(exit_hook))
            fprintf(stderr, LOGP "no exit hook: a window's exit skips the shutdowns\n");
    }

    fprintf(stderr, LOGP "%d running:", s_nrunning);
    for (i = 0; i < s_nrunning; i++)
        fprintf(stderr, " %s", s_running[i]->name ? s_running[i]->name : "?");
    fprintf(stderr, " (%d frame, %d filter)\n", s_nframe, s_nfilter);
    return s_nrunning;
}

void xbox_mod_frame(void)
{
    int i;

    if (!s_nrunning)
        return;
    mod_mem();
    if (!s_nframe || __atomic_load_n(&s_shut, __ATOMIC_ACQUIRE))
        return;
    for (i = 0; i < s_nframe; i++)
        s_frame[i].fn(s_frame[i].user);
}

int xbox_mod_input_captured(void)
{
    return enhance_input_captured();
}

void xbox_mod_shutdown(void)
{
    int i;

    if (__atomic_exchange_n(&s_shut, 1, __ATOMIC_ACQ_REL))
        return;
    /* The render thread may still be drawing: it stops asking first. The
     * tables stay, so a callback already running reads what it read. */
    nv2a_batch_filter_set(NULL);
    for (i = s_nrunning - 1; i >= 0; i--)
        if (s_running[i]->shutdown)
            s_running[i]->shutdown();
    enhance_input_capture(0);
}
