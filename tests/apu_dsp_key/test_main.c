/* RECOMP_APU_DSP, the APU DSP's run-time key (recomp_env.c, apu_dsp_resolve):
 * unset and 0 keep the stub with its ack and fold untouched; 1 in a build
 * with the DSP and its device forces apu_dsp_ack to 0 and drops
 * apu_mixdown_all, with one line only when the user had set either; a build
 * without the DSP, or with it but without the device yet, ignores it with a
 * line. Its name must not collide with RECOMP_APU_DSP_ACK or the
 * apu_dsp_ack list key.
 *
 * Built three ways by CMakeLists.txt: no XBOXRECOMP_APU_DSP, it alone, and
 * it with XBOXRECOMP_APU_DSP_DEVICE.
 * recomp_env.c is included rather than linked, to reach the held notes. */
#include "recomp_env.c"

static int s_checks, s_fail;

#define CHECK(c) do { s_checks++; if (!(c)) { s_fail++; \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void set_env(const char *k, const char *v)
{
#ifdef _WIN32
    char buf[512];
    snprintf(buf, sizeof buf, "%s=%s", k, v ? v : "");
    _putenv(buf);
#else
    if (v) setenv(k, v, 1); else unsetenv(k);
#endif
}

static int streq(const char *a, const char *b)
{
    return a && b && !strcmp(a, b);
}

/* Reload from these variables (NULL: unset), held notes cleared first. */
static void load_env(const char *apu_dsp, const char *debug, const char *legacy_ack)
{
    s_notes_len = 0;
    if (s_notes)
        s_notes[0] = 0;
    set_env("RECOMP_TRACE", NULL);
    set_env("RECOMP_APU_DSP", apu_dsp);
    set_env("RECOMP_DEBUG", debug);
    set_env("RECOMP_APU_DSP_ACK", legacy_ack);
    set_env("RECOMP_APU_MIXDOWN_ALL", NULL);
    recomp_env_reload();
}

/* Lines of the held notes about RECOMP_APU_DSP (not the deprecation line
 * of RECOMP_APU_DSP_ACK). */
static int apu_dsp_lines(void)
{
    const char *p = s_notes;
    int n = 0;
    while (p && (p = strstr(p, "[ENV] RECOMP_APU_DSP=")) != NULL) {
        n++;
        p++;
    }
    return n;
}

static int noted(const char *text)
{
    return s_notes && strstr(s_notes, text) != NULL;
}

/* Unset, empty and 0 are the stub: every APU key as it was. */
static void test_stub(void)
{
    const char *const off[] = { NULL, "", "0" };
    size_t i;

    for (i = 0; i < sizeof off / sizeof off[0]; i++) {
        load_env(off[i], NULL, NULL);
        CHECK(recomp_env(RENV_APU_DSP) == NULL);
        CHECK(streq(recomp_env(RENV_APU_DSP_ACK), "auto"));
        CHECK(recomp_env(RENV_APU_MIXDOWN_ALL) == NULL);
        CHECK(apu_dsp_lines() == 0);

        load_env(off[i], "apu_dsp_ack=0x80A20810,apu_mixdown_all=2", NULL);
        CHECK(recomp_env(RENV_APU_DSP) == NULL);
        CHECK(streq(recomp_env(RENV_APU_DSP_ACK), "0x80A20810"));
        CHECK(streq(recomp_env(RENV_APU_MIXDOWN_ALL), "2"));
        CHECK(apu_dsp_lines() == 0);
    }
}

/* The names differ only by a suffix: neither spelling of apu_dsp_ack sets
 * RECOMP_APU_DSP, and apu_dsp is no list key (it is a variable). */
static void test_no_collision(void)
{
    load_env(NULL, NULL, "0x80A20810");
    CHECK(recomp_env(RENV_APU_DSP) == NULL);
    CHECK(streq(recomp_env(RENV_APU_DSP_ACK), "0x80A20810"));

    load_env(NULL, "apu_dsp_ack=0", NULL);
    CHECK(recomp_env(RENV_APU_DSP) == NULL);
    CHECK(streq(recomp_env(RENV_APU_DSP_ACK), "0"));

    load_env(NULL, "apu_dsp=1", NULL);
    CHECK(recomp_env(RENV_APU_DSP) == NULL);
    CHECK(noted("unknown key 'apu_dsp'"));
    CHECK(streq(recomp_env(RENV_APU_DSP_ACK), "auto"));
}

/* Neither 0 nor 1: the stub, with a line. */
static void test_bad_value(void)
{
    load_env("yes", NULL, NULL);
    CHECK(recomp_env(RENV_APU_DSP) == NULL);
    CHECK(streq(recomp_env(RENV_APU_DSP_ACK), "auto"));
    CHECK(noted("RECOMP_APU_DSP=yes: not 0 or 1"));
    CHECK(apu_dsp_lines() == 1);
}

#if defined(XBOXRECOMP_APU_DSP) && defined(XBOXRECOMP_APU_DSP_DEVICE)
static void test_dsp_on(void)
{
    /* Nothing else set: the ack goes off, quietly. */
    load_env("1", NULL, NULL);
    CHECK(streq(recomp_env(RENV_APU_DSP), "1"));
    CHECK(streq(recomp_env(RENV_APU_DSP_ACK), "0"));
    CHECK(recomp_env(RENV_APU_MIXDOWN_ALL) == NULL);
    CHECK(apu_dsp_lines() == 0);

    /* Both set: both overridden, in one line. */
    load_env("1", "apu_dsp_ack=auto,apu_mixdown_all=2", NULL);
    CHECK(streq(recomp_env(RENV_APU_DSP_ACK), "0"));
    CHECK(recomp_env(RENV_APU_MIXDOWN_ALL) == NULL);
    CHECK(apu_dsp_lines() == 1);
    CHECK(noted("apu_dsp_ack (now 0) and apu_mixdown_all (ignored)"));

    /* One set, through its old variable: still the user's, still one line. */
    load_env("1", NULL, "0x80A20810");
    CHECK(streq(recomp_env(RENV_APU_DSP_ACK), "0"));
    CHECK(apu_dsp_lines() == 1);
    CHECK(noted("overrides apu_dsp_ack (now 0):"));

    load_env("1", "apu_mixdown_all=0", NULL);
    CHECK(recomp_env(RENV_APU_MIXDOWN_ALL) == NULL);
    CHECK(apu_dsp_lines() == 1);
    CHECK(noted("overrides apu_mixdown_all (ignored):"));
}
#else
static void test_stub_kept(void)
{
    /* A build without the DSP, or without its device yet, keeps the stub,
     * ack and fold included: with the ack off and nothing to answer the
     * doorbell, audio init would wait for ever. */
    load_env("1", "apu_mixdown_all=2", "0x80A20810");
    CHECK(recomp_env(RENV_APU_DSP) == NULL);
    CHECK(streq(recomp_env(RENV_APU_DSP_ACK), "0x80A20810"));
    CHECK(streq(recomp_env(RENV_APU_MIXDOWN_ALL), "2"));
#ifdef XBOXRECOMP_APU_DSP
    CHECK(noted("RECOMP_APU_DSP=1: the APU DSP device is not wired yet; the stub"));
#else
    CHECK(noted("RECOMP_APU_DSP=1: this build has no APU DSP"));
#endif
    CHECK(apu_dsp_lines() == 1);

    load_env("1", NULL, NULL);
    CHECK(streq(recomp_env(RENV_APU_DSP_ACK), "auto"));
    CHECK(apu_dsp_lines() == 1);
}
#endif

int main(void)
{
    test_stub();
    test_no_collision();
    test_bad_value();
#if defined(XBOXRECOMP_APU_DSP) && defined(XBOXRECOMP_APU_DSP_DEVICE)
    test_dsp_on();
#else
    test_stub_kept();
#endif
    load_env(NULL, NULL, NULL);
    printf("apu_dsp_key (%s): %d checks, %d failed\n",
#if defined(XBOXRECOMP_APU_DSP_DEVICE)
           "DSP and device",
#elif defined(XBOXRECOMP_APU_DSP)
           "DSP, no device",
#else
           "no DSP",
#endif
           s_checks, s_fail);
    return s_fail != 0;
}
