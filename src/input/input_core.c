/**
 * xbox_input -- the public API over a host backend, with the per-port
 * connection cache.
 *
 * Connected ports are read at every call. A port last seen disconnected is
 * not asked again until XBOX_INPUT_PROBE_MS has passed since the last probe:
 * XInput's own guidance, because XInputGetState on an empty user index is
 * slow on Windows, and harmless elsewhere. xbox_InputIsConnected() answers
 * from the cache and refreshes it under the same rule, so a pad plugged in
 * later shows up within a second while the title polls the mask every frame.
 *
 * Nothing here calls the platform: the backend (input_backend.h) does, and
 * tests/pad_map swaps in a fake one to count the calls.
 */
#include "input_backend.h"
#include "host_pad.h"
#include "keyboard.h"

#include <stdlib.h>
#include <string.h>
#ifdef XBOXRECOMP_ENHANCE
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define last_yield() SwitchToThread()
#else
#include <sched.h>
#define last_yield() sched_yield()
#endif
#endif

static const xbox_input_backend *g_be = &xbox_input_host_backend;
static int      g_inited;
static BOOL     g_connected[XBOX_MAX_CONTROLLERS];
static uint64_t g_last_probe[XBOX_MAX_CONTROLLERS];
static int      g_probed[XBOX_MAX_CONTROLLERS];   /* ever probed */
static int      g_rumbling[XBOX_MAX_CONTROLLERS];

/* The host-only buttons each slot's last read saw (host_pad.h): written by
 * the poll and by the flip tick's read, read by either and by a title's
 * glue, so atomic. */
static uint32_t g_extra[XBOX_MAX_CONTROLLERS];
/* The host actions' filter (host_pad.h); atomic, NULL when disarmed. */
static xbox_input_pad_filter_fn g_filter;

static void shutdown_atexit(void) { xbox_InputShutdown(); }

static DWORD probe(DWORD port, XBOX_INPUT_STATE *st)
{
    DWORD rc = g_be->get_state(port, st);
    g_last_probe[port] = g_be->now_ms();
    g_probed[port] = 1;
    g_connected[port] = (rc == ERROR_SUCCESS);
    __atomic_store_n(&g_extra[port],
                     rc == ERROR_SUCCESS && g_be->host_extra ? g_be->host_extra(port) : 0u,
                     __ATOMIC_RELEASE);
    return rc;
}

/* Whether a probe of a port is due. */
static int due(DWORD port)
{
    return !g_probed[port] || g_connected[port]
        || g_be->now_ms() - g_last_probe[port] >= XBOX_INPUT_PROBE_MS;
}

void xbox_InputInit(void)
{
    static int atexit_done;
    XBOX_INPUT_STATE st;

    if (g_inited)
        return;
    __atomic_store_n(&g_inited, 1, __ATOMIC_RELEASE);
    if (g_be->init)
        g_be->init();
    /* The keyboard's stick ramp and the wheel pulses run on the poll's
     * clock, the backend's, so a test's fake clock drives them too. */
    xbox_InputKbmSetClock(g_be->now_ms);
    for (DWORD p = 0; p < XBOX_MAX_CONTROLLERS; p++)
        probe(p, &st);
    /* A crash or an exit while the title has the motors on must not leave a
     * pad buzzing. */
    if (!atexit_done && g_be == &xbox_input_host_backend) {
        atexit_done = 1;
        atexit(shutdown_atexit);
    }
}

void xbox_InputSetBackend(const xbox_input_backend *b)
{
    g_be = b ? b : &xbox_input_host_backend;
    g_inited = 0;
    memset(g_connected, 0, sizeof g_connected);
    memset(g_last_probe, 0, sizeof g_last_probe);
    memset(g_probed, 0, sizeof g_probed);
    memset(g_rumbling, 0, sizeof g_rumbling);
    for (int p = 0; p < XBOX_MAX_CONTROLLERS; p++)
        __atomic_store_n(&g_extra[p], 0u, __ATOMIC_RELEASE);
}

void xbox_InputSetPadFilter(xbox_input_pad_filter_fn fn)
{
    __atomic_store_n(&g_filter, fn, __ATOMIC_RELEASE);
}

int xbox_InputPadFilterSet(void)
{
    return __atomic_load_n(&g_filter, __ATOMIC_ACQUIRE) != NULL;
}

uint32_t xbox_InputHostExtra(DWORD slot)
{
    if (slot >= XBOX_MAX_CONTROLLERS)
        return 0;
    return __atomic_load_n(&g_extra[slot], __ATOMIC_ACQUIRE);
}

uint64_t xbox_InputNowMs(void)
{
    return g_be && g_be->now_ms ? g_be->now_ms() : 0;
}

DWORD xbox_input_backend_read_host(DWORD slot, struct xbox_host_pad *host)
{
    XBOX_INPUT_STATE pad;
    uint32_t extra = 0;
    DWORD rc = ERROR_DEVICE_NOT_CONNECTED;

    if (slot >= XBOX_MAX_CONTROLLERS || !host)
        return ERROR_DEVICE_NOT_CONNECTED;
    memset(&pad, 0, sizeof pad);
    /* Before the title's first poll the backend is not up, and bringing it
     * up is the poll thread's job (the SDL boot wait). */
    if (__atomic_load_n(&g_inited, __ATOMIC_ACQUIRE) && g_be->read_host)
        rc = g_be->read_host(slot, &pad, &extra);
    if (rc == XBOX_INPUT_BUSY)
        return rc;
    memset(host, 0, sizeof *host);
    host->slot = (int)slot;
    if (rc == ERROR_SUCCESS) {
        host->pad = pad;
        host->extra = extra & XBOX_HOST_EXTRA_MASK;
        host->connected = 1;
    }
    __atomic_store_n(&g_extra[slot], host->extra, __ATOMIC_RELEASE);
    return rc;
}

DWORD xbox_InputGetPadState(DWORD slot, XBOX_INPUT_STATE *pState)
{
    if (slot >= XBOX_MAX_CONTROLLERS || !pState)
        return ERROR_DEVICE_NOT_CONNECTED;
    xbox_InputInit();
    return due(slot) ? probe(slot, pState) : ERROR_DEVICE_NOT_CONNECTED;
}

void xbox_InputMergeKbm(XBOX_INPUT_STATE *pad, const XBOX_INPUT_STATE *kb)
{
    XBOX_GAMEPAD *g = &pad->Gamepad;

    g->wButtons |= kb->Gamepad.wButtons;
    for (int i = 0; i < 8; i++)
        if (kb->Gamepad.bAnalogButtons[i] > g->bAnalogButtons[i])
            g->bAnalogButtons[i] = kb->Gamepad.bAnalogButtons[i];
    if (kb->Gamepad.sThumbLX) g->sThumbLX = kb->Gamepad.sThumbLX;
    if (kb->Gamepad.sThumbLY) g->sThumbLY = kb->Gamepad.sThumbLY;
    if (kb->Gamepad.sThumbRX) g->sThumbRX = kb->Gamepad.sThumbRX;
    if (kb->Gamepad.sThumbRY) g->sThumbRY = kb->Gamepad.sThumbRY;
    /* The title's input layer records edges, so an unchanged packet
     * number reads as the same state and a key press never happens. */
    pad->dwPacketNumber = kb->dwPacketNumber;
}

#ifdef XBOXRECOMP_ENHANCE
/* Port 0 as the title's last read saw it, for a mod (xbox_InputLastState).
 * The title's poll writes it (the USB thread) and a mod reads it from the
 * guest's frame, so it sits behind a spin lock held for a copy. */
static XBOX_INPUT_STATE g_last;
static DWORD g_last_rc = ERROR_DEVICE_NOT_CONNECTED;
static unsigned char g_last_lock;   /* the type __atomic_test_and_set is defined on */

static void last_lock(void)
{
    unsigned spins = 0;

    /* Held for a 20-byte copy, so a spin is short; but the holder may be
     * preempted, and on a busy host (Proton's threads plus the guest's) a
     * waiter that never yields can keep it off the core. */
    while (__atomic_test_and_set(&g_last_lock, __ATOMIC_ACQUIRE))
        if (++spins >= 64) {
            spins = 0;
            last_yield();
        }
}

static void last_unlock(void)
{
    __atomic_clear(&g_last_lock, __ATOMIC_RELEASE);
}

static DWORD keep_last(DWORD rc, const XBOX_INPUT_STATE *st)
{
    last_lock();
    g_last_rc = rc;
    if (rc == ERROR_SUCCESS)
        g_last = *st;
    else
        memset(&g_last, 0, sizeof g_last);
    last_unlock();
    return rc;
}

DWORD xbox_InputLastState(DWORD dwPort, XBOX_INPUT_STATE *pState)
{
    DWORD rc;

    if (dwPort != 0 || !pState)
        return ERROR_DEVICE_NOT_CONNECTED;
    last_lock();
    rc = g_last_rc;
    *pState = g_last;
    last_unlock();
    return rc;
}
#define KEEP(port, rc, st) ((port) == 0 ? keep_last((rc), (st)) : (rc))
#else
#define KEEP(port, rc, st) (rc)
#endif

#ifdef XBOXRECOMP_ENHANCE
/* The host actions see every poll, a failed one too (with a released
 * state), so a key or a chord released while the pad was away is seen
 * released. */
static DWORD filtered(DWORD port, DWORD rc, XBOX_INPUT_STATE *st,
                      xbox_input_pad_filter_fn fn, const struct xbox_host_pad *host)
{
    XBOX_INPUT_STATE idle;

    if (!fn)
        return rc;
    if (rc == ERROR_SUCCESS) {
        fn(port, host, st);
    } else {
        memset(&idle, 0, sizeof idle);
        fn(port, host, &idle);
    }
    return rc;
}
#define FILTER(port, rc, st) filtered((port), (rc), (st), fn, &host)
#else
#define FILTER(port, rc, st) (rc)
#endif

DWORD xbox_InputGetState(DWORD dwPort, XBOX_INPUT_STATE *pState)
{
    XBOX_INPUT_STATE kb;
    DWORD rc;
#ifdef XBOXRECOMP_ENHANCE
    xbox_input_pad_filter_fn fn;
    struct xbox_host_pad host;
#endif

    if (dwPort >= XBOX_MAX_CONTROLLERS || !pState)
        return ERROR_DEVICE_NOT_CONNECTED;
    rc = xbox_InputGetPadState(dwPort, pState);
#ifdef XBOXRECOMP_ENHANCE
    /* The host pad before the keyboard and the script are merged: a chord is
     * the pad's, a key the keyboard's (the filter reads keys itself). */
    fn = __atomic_load_n(&g_filter, __ATOMIC_ACQUIRE);
    if (fn) {
        memset(&host, 0, sizeof host);
        host.slot = (int)dwPort;
        if (rc == ERROR_SUCCESS) {
            host.pad = *pState;
            host.extra = xbox_InputHostExtra(dwPort);
            host.connected = 1;
        }
    }
#endif

    /* The keyboard answers for port 0 with or without a pad there, and is
     * merged on top of one: a real pad keeps working, the keys only add. */
    if (dwPort == 0 && g_be->keyboard && g_be->keyboard(&kb)) {
        if (rc != ERROR_SUCCESS) {
            *pState = kb;
            return KEEP(dwPort, FILTER(dwPort, ERROR_SUCCESS, pState), pState);
        }
        xbox_InputMergeKbm(pState, &kb);
    }
    return KEEP(dwPort, FILTER(dwPort, rc, pState), pState);
}

DWORD xbox_InputSetState(DWORD dwPort, const XBOX_VIBRATION *pVibration)
{
    DWORD rc;

    if (dwPort >= XBOX_MAX_CONTROLLERS || !pVibration)
        return ERROR_DEVICE_NOT_CONNECTED;
    xbox_InputInit();
    if (!g_connected[dwPort])
        return ERROR_DEVICE_NOT_CONNECTED;
    rc = g_be->set_state(dwPort, pVibration);
    if (rc == ERROR_SUCCESS)
        g_rumbling[dwPort] = pVibration->wLeftMotorSpeed
                          || pVibration->wRightMotorSpeed;
    return rc;
}

void xbox_InputShutdown(void)
{
    static const XBOX_VIBRATION off = { 0, 0 };

    if (!g_inited)
        return;
    for (DWORD p = 0; p < XBOX_MAX_CONTROLLERS; p++)
        if (g_rumbling[p] || g_connected[p]) {
            g_be->set_state(p, &off);
            g_rumbling[p] = 0;
        }
}

BOOL xbox_InputIsConnected(DWORD dwPort)
{
    XBOX_INPUT_STATE st;

    if (dwPort >= XBOX_MAX_CONTROLLERS)
        return FALSE;
    xbox_InputInit();
    /* Connected ports are refreshed by the title's own reads; only a port
     * not read for a probe interval is asked here. */
    if (g_be->now_ms() - g_last_probe[dwPort] >= XBOX_INPUT_PROBE_MS)
        probe(dwPort, &st);
    return g_connected[dwPort];
}

DWORD xbox_InputGetCapabilities(DWORD dwPort, DWORD dwFlags, XBOX_INPUT_CAPABILITIES *pCaps)
{
    (void)dwFlags;
    if (dwPort >= XBOX_MAX_CONTROLLERS || !pCaps)
        return ERROR_DEVICE_NOT_CONNECTED;
    if (!xbox_InputIsConnected(dwPort))
        return ERROR_DEVICE_NOT_CONNECTED;
    memset(pCaps, 0, sizeof *pCaps);
    pCaps->Type    = 1;   /* XINPUT_DEVTYPE_GAMEPAD */
    pCaps->SubType = 1;   /* XINPUT_DEVSUBTYPE_GAMEPAD */
    return ERROR_SUCCESS;
}
