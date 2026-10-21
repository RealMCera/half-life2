/* The sequencer's sample-limit interrupt fires once per sequencer start.
 *
 * DirectSound programs XGSCNT = 0 and SSL = 0x800 and needs one SSLINTSTS to
 * finish its initialisation (Black). Its service routine then restarts the GP
 * and EP, stalls 10 ms at DISPATCH and programs XGSCNT and SSL again. On
 * hardware the DSP code keeps the limit from being reached; our DSPs are
 * stubs, so a limit re-armed by that restart fired every 2048 samples and the
 * restart ran 23 times a second (JSRF). This checks the tick fires once at
 * the limit, not again after the title's restart or a running mode written
 * while running, again after the sequencer is switched off and on, and never
 * with no limit or the counter off.
 *
 * mcpx_apu_ssl_tick is the frame thread's per-frame step; an ISTS write of 0
 * (acks nothing) stands in for the frame thread turning set_irq into the
 * line. */
#include "apu_state.h"
#include "apu.h"
#include "apu_regs.h"

#include <stdio.h>
#include <stdlib.h>

/* The runtime links against the title's dispatch; nothing here calls it. */
void *recomp_lookup(unsigned long address) { (void)address; abort(); }
void *recomp_lookup_manual(unsigned long address) { (void)address; abort(); }

static int failures;
#define CHECK(name, cond) \
    do { if (!(cond)) { printf("FAIL: %s\n", name); failures++; } } while (0)

/* What DirectSound writes: SSL 2048 samples, the counter running. */
#define SSL_SAMPLES 0x800u
#define SECTL_RUN   (1u << 3)

static uint32_t reg(MCPXAPUState *d, uint32_t r) { return d->regs[r]; }
static int ssl_status(MCPXAPUState *d) { return !!(reg(d, NV_PAPU_ISTS) & NV_PAPU_ISTS_SSLINTSTS); }
static int line(MCPXAPUState *d) { return !!(reg(d, NV_PAPU_ISTS) & NV_PAPU_ISTS_GINTSTS); }
static void settle(MCPXAPUState *d) { mcpx_apu_mmio_write(d, NV_PAPU_ISTS, 0, 4); }
static void ack(MCPXAPUState *d) { mcpx_apu_mmio_write(d, NV_PAPU_ISTS, 0xFFFFFFFFu, 4); }

/* The title's sequencer setup, as DirectSound runs it on every restart. */
static void program(MCPXAPUState *d)
{
    mcpx_apu_mmio_write(d, NV_PAPU_XGSCNT, 0, 4);
    mcpx_apu_mmio_write(d, NV_PAPU_SSL, SSL_SAMPLES, 4);
}

static void ticks(MCPXAPUState *d, int n)
{
    for (int i = 0; i < n; i++)
        mcpx_apu_ssl_tick(d);
    settle(d);
}

int main(void)
{
    MCPXAPUState *d = (MCPXAPUState *)calloc(1, sizeof(*d));
    if (!d) return 2;
    qemu_mutex_init(&d->lock);
    qemu_cond_init(&d->cond);
    qemu_cond_init(&d->idle_cond);
    printf("apu_ssl: running\n");

    const int frames = SSL_SAMPLES / NUM_SAMPLES_PER_FRAME;
    mcpx_apu_mmio_write(d, NV_PAPU_IEN, 0xD9, 4);
    mcpx_apu_mmio_write(d, NV_PAPU_SECTL, SECTL_RUN, 4);
    program(d);

    /* 1. One frame short of the limit: nothing. At the limit: the status
     *    latches and the line rises. */
    ticks(d, frames - 1);
    CHECK("short of the limit: no SSLINTSTS", !ssl_status(d));
    ticks(d, 1);
    CHECK("at the limit: SSLINTSTS", ssl_status(d));
    CHECK("at the limit: line asserted", line(d));
    CHECK("XGSCNT reads the samples counted",
          mcpx_apu_mmio_read(d, NV_PAPU_XGSCNT, 4) == SSL_SAMPLES);

    /* 2. The service routine acks and restarts the sequencer the way it
     *    does after every SSL interrupt. The stub DSPs never hold the limit
     *    off, so a re-armed tick would fire again here, 23 times a second. */
    ack(d);
    for (int restart = 0; restart < 4; restart++) {
        program(d);
        ticks(d, frames * 2);
    }
    CHECK("after the title's restarts: no SSLINTSTS", !ssl_status(d));
    CHECK("after the title's restarts: line low", !line(d));

    /* 2b. A running mode written while running is not a new start: only an
     *     off write re-arms. */
    mcpx_apu_mmio_write(d, NV_PAPU_SECTL, SECTL_RUN, 4);
    program(d);
    ticks(d, frames * 2);
    CHECK("running mode rewritten: no SSLINTSTS", !ssl_status(d));

    /* 3. Switching the counter off and on is a new sequencer start. */
    mcpx_apu_mmio_write(d, NV_PAPU_SECTL, 0, 4);
    mcpx_apu_mmio_write(d, NV_PAPU_SECTL, SECTL_RUN, 4);
    program(d);
    ticks(d, frames);
    CHECK("new start: SSLINTSTS again", ssl_status(d));
    ack(d);

    /* 4. Counter off: no tick however long it runs. */
    mcpx_apu_mmio_write(d, NV_PAPU_SECTL, 0, 4);
    program(d);
    ticks(d, frames * 4);
    CHECK("counter off: no SSLINTSTS", !ssl_status(d));

    /* 5. No limit programmed: no tick either. */
    mcpx_apu_mmio_write(d, NV_PAPU_SECTL, SECTL_RUN, 4);
    mcpx_apu_mmio_write(d, NV_PAPU_XGSCNT, 0, 4);
    mcpx_apu_mmio_write(d, NV_PAPU_SSL, 0, 4);
    ticks(d, frames * 4);
    CHECK("SSL 0: no SSLINTSTS", !ssl_status(d));

    if (failures) { printf("apu_ssl: %d failure(s)\n", failures); return 1; }
    printf("apu_ssl: all passed\n");
    return 0;
}
