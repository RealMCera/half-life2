/*
 * The walker's event wait (nv2a_kick_wake.c).
 *
 * The NV2A ack loop sleeps between ticks until the guest's KickOff pokes it.
 * A lost wake-up there is a kick that waits for the 1 ms tick, a frame that
 * runs late, so the stress case gives the walker a 200 ms timeout and the
 * guest one poke per store, no retries: any lost wake shows as a timeout
 * that finds work pending. The other cases pin the fallback (a sleep nobody
 * ends times out), that a poke with nothing pending and a poke during a held
 * flip do not wake, that a nudge does, that no wake is left latched for the
 * next sleep, the PUT watch, and the miss limit that turns the wait off.
 *
 * Two more stress cases take the guest and the timer thread as they really
 * are. The guest's KickOff, at sleep pacing, makes one pass of its lowered
 * spin and then sleeps until the ack: a plain store of the flush bit, then
 * RECOMP_SPIN_WAIT's fence and plain read of `armed` (as recomp_types.h
 * defines it), once. Without that fence the store and the read reorder and
 * about one poke in a thousand is lost (the run prints the count of the
 * unfenced pass beside it, as a check that the case can see the bug). The
 * timer thread counts a vblank and then nudges, while the walker sleeps on
 * a held flip from the count it last saw: a vblank between that read and
 * the arm must still end the sleep.
 */
#include "nv2a_kick_wake.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define KICK_BIT 0x10000u
#define MS 1000000ull
#define US 1000ull

static int failures;
static volatile int armed;
static volatile uint32_t kick, put, done_put;
static volatile uint32_t vblanks;

static uint32_t vblank_count(void)
{
    return __atomic_load_n(&vblanks, __ATOMIC_SEQ_CST);
}

static void check(int ok, const char *what)
{
    printf("  %-62s %s\n", what, ok ? "PASS" : "FAIL");
    if (!ok) failures++;
}

static void reset(XboxKickWake *k)
{
    static XboxHostWaker *waker;
    if (!waker)
        waker = xbox_HostWakerCreate();
    /* Drain anything a case left latched. */
    while (xbox_HostWakerSleepNs(waker, 0))
        ;
    armed = 0;
    kick = 0;
    put = done_put = 0;
    vblanks = 0;
    *k = (XboxKickWake){ .armed = &armed, .waker = waker, .kick = &kick,
                         .kick_bit = KICK_BIT, .put = &put, .done_put = &done_put,
                         .vblank_count = vblank_count };
}

/* ---- stress: no lost wake-up ------------------------------------------ */

#define ROUNDS 20000

static XboxKickWake g_k;
static volatile int g_stop;

/* D3D's KickOff, minus the spin's repeated passes: raise the flush bit and
 * poke once, wait for the ack, store PUT and poke once. */
static void *guest(void *arg)
{
    (void)arg;
    for (int i = 0; i < ROUNDS && !g_stop; i++) {
        __atomic_fetch_or(&kick, KICK_BIT, __ATOMIC_SEQ_CST);
        xbox_KickWakePoke(&g_k);
        while ((__atomic_load_n(&kick, __ATOMIC_ACQUIRE) & KICK_BIT) && !g_stop)
            ;
        __atomic_store_n(&put, (uint32_t)(i + 1), __ATOMIC_SEQ_CST);
        xbox_KickWakePoke(&g_k);
    }
    return NULL;
}

static void test_stress(void)
{
    pthread_t t;
    long lost = 0, timeouts = 0;
    uint64_t t0;

    reset(&g_k);
    g_stop = 0;
    t0 = xbox_HostNowNs();
    pthread_create(&t, NULL, guest, NULL);
    while (__atomic_load_n(&done_put, __ATOMIC_RELAXED) != ROUNDS) {
        uint32_t p = __atomic_load_n(&put, __ATOMIC_ACQUIRE);
        if (p != done_put) {
            __atomic_store_n(&done_put, p, __ATOMIC_RELEASE);   /* "walked" */
            continue;
        }
        if (__atomic_load_n(&kick, __ATOMIC_ACQUIRE) & KICK_BIT) {
            __atomic_fetch_and(&kick, ~KICK_BIT, __ATOMIC_SEQ_CST);
            continue;
        }
        if (!xbox_KickWakeSleep(&g_k, 200 * MS)) {
            timeouts++;
            if (xbox_KickWakePending(&g_k))
                lost++;
        }
        if (xbox_HostNowNs() - t0 > 60000 * MS) {
            g_stop = 1;   /* a hang: fail rather than block ctest */
            break;
        }
    }
    pthread_join(t, NULL);
    printf("  %d kicks in %.0f ms: sleeps %ld poked %ld early %ld timeouts %ld\n",
           ROUNDS, (double)(xbox_HostNowNs() - t0) / MS, g_k.sleeps, g_k.poked,
           g_k.early, timeouts);
    check(!g_stop, "stress: every kick was walked (no hang)");
    check(lost == 0, "stress: no sleep timed out with work pending (no lost wake)");
    check(timeouts == 0, "stress: no sleep ran to its 200 ms timeout");
}

/* ---- stress: the guest's one fenced pass ------------------------------ */

#define PASS_ROUNDS 200000

static int g_fenced;

/* RECOMP_SPIN_WAIT's test of the walker's flag, as the generated code
 * runs it (the other flags are 0 here). */
static void guest_pass(void)
{
    if (g_fenced)
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
    if (__atomic_load_n(&armed, __ATOMIC_RELAXED))
        xbox_KickWakePoke(&g_k);
}

static void *guest_one_pass(void *arg)
{
    (void)arg;
    for (int i = 0; i < PASS_ROUNDS && !g_stop; i++) {
        __atomic_store_n(&kick, KICK_BIT, __ATOMIC_RELAXED);   /* MEM32 */
        guest_pass();
        while (__atomic_load_n(&kick, __ATOMIC_ACQUIRE) && !g_stop)
            ;                           /* asleep until the ack: no passes */
    }
    g_stop = 1;
    return NULL;
}

/* Walker sleeps of 1 ms (the tick) that found the bit set: lost pokes. */
static long one_pass_lost(int fenced)
{
    pthread_t t;
    long lost = 0;
    uint64_t t0 = xbox_HostNowNs();

    reset(&g_k);
    g_fenced = fenced;
    g_stop = 0;
    pthread_create(&t, NULL, guest_one_pass, NULL);
    /* Nothing between the ack and the sleep's arm: that is the window the
     * guest's store races (a clock read there hides most of it). */
    while (!g_stop) {
        if (__atomic_load_n(&kick, __ATOMIC_ACQUIRE)) {
            __atomic_store_n(&kick, 0, __ATOMIC_RELEASE);
            continue;
        }
        if (!xbox_KickWakeSleep(&g_k, MS) && __atomic_load_n(&kick, __ATOMIC_RELAXED)) {
            lost++;
            if (xbox_HostNowNs() - t0 > 60000 * MS)
                g_stop = 1;
        }
    }
    pthread_join(t, NULL);
    return lost;
}

static void test_one_pass(void)
{
    long unfenced = one_pass_lost(0), fenced = one_pass_lost(1);

    printf("  %d one-pass kicks: %ld lost without the macro's fence, %ld with it\n",
           PASS_ROUNDS, unfenced, fenced);
    check(fenced == 0, "one pass, fenced: no poke lost");
}

/* ---- stress: a vblank between the hold's poll and the arm -------------- */

#define VBLANKS 20000

static void *vblank_thread(void *arg)
{
    (void)arg;
    for (int i = 0; i < VBLANKS && !g_stop; i++) {
        uint64_t t0 = xbox_HostNowNs();
        while (xbox_HostNowNs() - t0 < 2 * US)
            ;
        __atomic_fetch_add(&vblanks, 1, __ATOMIC_SEQ_CST);   /* counts... */
        xbox_KickWakeNudge(&g_k);                             /* ...then nudges */
    }
    return NULL;
}

static void test_held_stress(void)
{
    pthread_t t;
    long timeouts = 0;
    uint64_t t0;

    reset(&g_k);
    kick = KICK_BIT;                    /* a held flip: the bit stays set */
    g_stop = 0;
    t0 = xbox_HostNowNs();
    pthread_create(&t, NULL, vblank_thread, NULL);
    for (;;) {
        uint32_t seen = vblank_count();
        if (seen == VBLANKS)
            break;
        if (!xbox_KickWakeSleepNudge(&g_k, 200 * MS, seen))
            timeouts++;
        if (xbox_HostNowNs() - t0 > 60000 * MS) {
            g_stop = 1;
            break;
        }
    }
    pthread_join(t, NULL);
    printf("  %d vblanks in %.0f ms: held sleeps %ld nudges %ld timeouts %ld\n",
           VBLANKS, (double)(xbox_HostNowNs() - t0) / MS, g_k.held_sleeps,
           g_k.nudges, timeouts);
    check(!g_stop, "held stress: every vblank was seen (no hang)");
    check(timeouts == 0, "held stress: no held sleep ran to its 200 ms timeout");
}

/* ---- single cases ------------------------------------------------------ */

static void *after_us(void *arg)
{
    /* arg: what to do 2 ms in. */
    uint64_t t0 = xbox_HostNowNs();
    while (xbox_HostNowNs() - t0 < 2 * MS)
        ;
    ((void (*)(void))arg)();
    return NULL;
}

static void do_nudge(void) { xbox_KickWakeNudge(&g_k); }
static void do_poke_idle(void) { xbox_KickWakePoke(&g_k); }
static void do_kick_poke(void)
{
    __atomic_fetch_or(&kick, KICK_BIT, __ATOMIC_SEQ_CST);
    xbox_KickWakePoke(&g_k);
}
static void do_put(void)
{
    /* The PUT store the watch waits for, 2 ms after the "ack". */
    __atomic_store_n(&put, 0x80, __ATOMIC_SEQ_CST);
}

static int sleep_nudge_seen0(XboxKickWake *k, uint64_t ns)
{
    return xbox_KickWakeSleepNudge(k, ns, 0);
}

static uint64_t timed_sleep(int (*fn)(XboxKickWake *, uint64_t), uint64_t ns,
                            void (*other)(void), int *ret)
{
    pthread_t t;
    uint64_t t0 = xbox_HostNowNs(), dt;
    if (other)
        pthread_create(&t, NULL, after_us, (void *)other);
    *ret = fn(&g_k, ns);
    dt = xbox_HostNowNs() - t0;
    if (other)
        pthread_join(t, NULL);
    return dt;
}

static void test_cases(void)
{
    int r;
    uint64_t dt;

    reset(&g_k);
    dt = timed_sleep(xbox_KickWakeSleep, 5 * MS, NULL, &r);
    check(r == 0 && dt >= 4 * MS && dt < 500 * MS,
          "fallback: a sleep nobody ends times out after its 5 ms");
    check(armed == 0, "fallback: the timeout takes the armed flag back");

    reset(&g_k);
    dt = timed_sleep(xbox_KickWakeSleep, 20 * MS, do_poke_idle, &r);
    check(r == 0 && dt >= 15 * MS, "a poke with nothing pending does not wake");

    reset(&g_k);
    dt = timed_sleep(xbox_KickWakeSleep, 500 * MS, do_kick_poke, &r);
    check(r == 1 && dt < 200 * MS, "a kick's poke wakes the walker");
    dt = timed_sleep(xbox_KickWakeSleep, 0, NULL, &r);
    check(r == 1 && g_k.early == 1, "pending work: the sleep returns at once");
    kick = 0;
    dt = timed_sleep(xbox_KickWakeSleep, 10 * MS, NULL, &r);
    check(r == 0 && dt >= 8 * MS, "no wake is left latched for the next sleep");

    reset(&g_k);
    dt = timed_sleep(xbox_KickWakeSleep, 500 * MS, do_nudge, &r);
    check(r == 1 && dt < 200 * MS, "a nudge (vblank) wakes the walker");

    reset(&g_k);
    kick = KICK_BIT;                        /* held flip: the bit stays set */
    dt = timed_sleep(sleep_nudge_seen0, 20 * MS, do_kick_poke, &r);
    check(r == 0 && dt >= 15 * MS, "held flip: the KickOff's poke does not wake");
    dt = timed_sleep(sleep_nudge_seen0, 500 * MS, do_nudge, &r);
    check(r == 1 && dt < 200 * MS, "held flip: the vblank's nudge wakes");
    vblanks = 1;                            /* counted after the hold's poll */
    dt = timed_sleep(sleep_nudge_seen0, 500 * MS, NULL, &r);
    check(r == 1 && dt < 50 * MS && armed == 0,
          "held flip: a vblank before the arm ends the sleep at once");
    vblanks = 0;
    dt = timed_sleep(sleep_nudge_seen0, 10 * MS, NULL, &r);
    check(r == 0 && dt >= 8 * MS, "held flip: no wake is left latched by the recheck");

    reset(&g_k);
    {
        pthread_t t;
        pthread_create(&t, NULL, after_us, (void *)do_put);
        r = xbox_KickWakeWatchPut(&g_k, 0, 200 * MS);
        pthread_join(t, NULL);
        check(r == 1 && g_k.watch_hits == 1, "put watch: sees the store after the ack");
    }
    r = xbox_KickWakeWatchPut(&g_k, 0x80, 100 * US);
    check(r == 0 && g_k.watch_misses == 1, "put watch: gives up after its 100 us");

    reset(&g_k);
    check(!xbox_KickWakeMiss(&g_k) && !xbox_KickWakeMiss(&g_k), "miss limit: two misses keep the wait");
    xbox_KickWakeHit(&g_k);
    check(!xbox_KickWakeMiss(&g_k) && !xbox_KickWakeMiss(&g_k) && !g_k.off,
          "miss limit: a poked kick resets the run");
    check(xbox_KickWakeMiss(&g_k) && g_k.off, "miss limit: the third in a row turns it off");
    check(!xbox_KickWakeMiss(&g_k), "miss limit: reported once");
}

int main(void)
{
    printf("kick_wake_test\n");
    test_cases();
    test_stress();
    test_one_pass();
    test_held_stress();
    printf("%s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
