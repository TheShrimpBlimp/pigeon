/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SAMPLE's PIANO (1.2) on the Mac (same sources as the firmware, through hostsim.c; run_tests.sh):
 *   build/host/sample_test PIANO_HD DEMO_DIR
 * 1. PIANO (SET 0; 1 and 4 its aliases) is PIANO_LO (tools/gen_samples.py): two one-shot zones at 11,025 Hz (roots
 *    48 and 72, split at 60), 1.6 s each; SAMPLE plays the right zone, at the note's pitch (YIN on the render) over
 *    the keyboard, for 1.6 s at a root; GRAIN's SRC 0 plays it. Renders into DEMO_DIR.
 * 2. SLICE's PIANO keeps the middle C zone of 1.0.4 (a copy of its own: PIANO_LO has none).
 * 3. PIANO HD (PIANO_HD.hdr / .bin: gen_samples.py --user-slot PIANO "PIANO HD"), the 5-zone PIANO of 1.0 .. 1.1.5 as
 *    a user slot: valid in USR1, its middle C zone the same ADPCM as SLICE's PIANO (1.0.4's), at the note's pitch.
 *    (gen_samples.py --user-slot checks every zone against the set as built from its own files; with that set built
 *    in, a USR1 of it rendered bit for bit as SET 0.) */
#include <stdint.h>
static uint32_t host_slots[3u * 0x14000u / 4u];          /* USR1..3, as the flash at 0xA0000 */
#define SMP_USER_XIP(k) ((const uint8_t *)host_slots + (k) * SMP_USER_SIZE)
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(const char *what, int ok)
{
    printf("sample: %-88s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

static voice_t *voice_of(track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].note == note)
            return &t->v[i];
    return 0;
}

#define SECS 2u
static int32_t buf[FS * SECS];
/* track 1 on engine eng, SET / SRC src (the engine's preset 0 otherwise, sends off), note held SECS s -> buf;
 * *ended_at the first sample after which the voice had stopped (one-shot end), 0 = still on; *zone the voice's zone */
static void render(uint32_t eng, int16_t src, uint32_t note, uint32_t *ended_at, int32_t *zone)
{
    track_t *t = &trk[0];
    voice_t *v;
    uint32_t i, k, n = 0;
    host_tracks_init();
    host_preset(t, eng, 0);
    t->p[P_E0] = src;
    for (i = 0; i < 4u; i++)
        t->p[P_DIST + i] = 0;
    t->p[P_ATK] = 0;
    t->p[P_DEC] = 127;
    t->p[P_SUS] = 127;
    t->p[P_REL] = 10;
    trk_note_on(t, note, 100);
    v = voice_of(t, note);
    if (zone)
        *zone = v ? v->s[eng == ENGI_SAMPLE ? 4 : 0] : -1;
    if (ended_at)
        *ended_at = 0;
    for (k = 0; k < FS * SECS / CTL; k++) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            buf[n++] = o[2 * i];
        if (ended_at && !*ended_at && v && (!v->active || v->s[6]))
            *ended_at = n;
    }
    trk_note_off(t, note);
    for (k = 0; k < FS / 2u / CTL; k++) {                 /* the release: nothing left sounding */
        int32_t o[2 * CTL];
        mix_block(o, CTL);
    }
}

/* YIN (cumulative mean normalised difference, threshold 0.15, parabolic) on n samples of buf from a */
static double yin_hz(uint32_t a, uint32_t n, double lo_hz, double hi_hz)
{
    static double d[4096];
    uint32_t tmin = (uint32_t)(FS / hi_hz), tmax = (uint32_t)(FS / lo_hz), t, i, best = 0;
    double sum = 0.0;
    if (tmax >= 4096u)
        tmax = 4095u;
    d[0] = 1.0;
    for (t = 1; t <= tmax; t++) {
        double s = 0.0;
        for (i = 0; i < n; i++) {
            double x = (double)buf[a + i] - (double)buf[a + i + t];
            s += x * x;
        }
        sum += s;
        d[t] = sum > 0.0 ? s * t / sum : 1.0;
    }
    for (t = tmin; t < tmax; t++)
        if (d[t] < 0.15) {
            while (t + 1u < tmax && d[t + 1u] < d[t])
                t++;
            best = t;
            break;
        }
    if (!best) {
        double m = 1e9;
        for (t = tmin; t < tmax; t++)
            if (d[t] < m)
                m = d[t], best = t;
    }
    if (best > 1u && best + 1u < 4096u) {
        double y0 = d[best - 1u], y1 = d[best], y2 = d[best + 1u], den = y0 - 2.0 * y1 + y2;
        return FS / (best + (den != 0.0 ? 0.5 * (y0 - y2) / den : 0.0));
    }
    return 0.0;
}

static double rms(uint32_t a, uint32_t b)
{
    double s = 0.0;
    uint32_t i;
    for (i = a; i < b; i++)
        s += (double)buf[i] * buf[i];
    return b > a ? sqrt(s / (b - a)) : 0.0;
}

static void wav_out(const char *dir, const char *name)
{
    char path[512];
    FILE *f;
    uint32_t i;
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    if (!(f = fopen(path, "wb")))
        return;
    wav_hdr(f, FS * SECS);
    for (i = 0; i < FS * SECS; i++)
        wav_put(f, buf[i], buf[i]);
    fclose(f);
}

static long load(const char *path, void *dst, long max)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f)
        return -1;
    n = (long)fread(dst, 1, (size_t)max, f);
    fclose(f);
    return n;
}

/* the note's pitch from buf (0.2 s on), within 15 cents */
static int pitch_ok(const char *what, uint32_t note)
{
    char m[160];
    double want = 440.0 * pow(2.0, ((double)note - 69.0) / 12.0), hz = yin_hz(FS / 5u, 4096u, want / 1.6, want * 1.6);
    int ok = hz > 0.0 && fabs(1200.0 * log2(hz / want)) < 15.0;
    snprintf(m, sizeof m, "%s: note %u at its pitch (%.1f Hz, want %.1f, %+.0f ct)", what, note, hz, want,
             hz > 0.0 ? 1200.0 * log2(hz / want) : 0.0);
    check(m, ok);
    return ok;
}

int main(int argc, char **argv)
{
    static const uint8_t NOTES[] = {36, 45, 48, 55, 59, 60, 64, 69, 72, 79, 84, 96};
    const char *dir = argc > 2 ? argv[2] : "build/sample_demo";
    char path[512], m[160];
    uint32_t i, ended;
    int32_t zone, zone2;
    if (argc < 2) {
        printf("usage: sample_test PIANO_HD_PREFIX [DEMO_DIR]\n");
        return 2;
    }

    /* 1: PIANO = PIANO_LO */
    {
        const smp_set_t *s = &SMP_SETS[0];
        const smp_zone_t *z = &SMP_ZONES[s->z0];
        uint32_t bytes = (z[0].n + 1u) / 2u + (z[1].n + 1u) / 2u;
        check("PIANO: SET 0, its aliases 1 and 4 the same zones; FLUTE 2, SAX 3, USR1..3 5..7; 4 presets",
              SMP_NSETS == 5u && str_eq(SMP_SETS[0].name, "PIANO") && SMP_SETS[1].z0 == s->z0 && SMP_SETS[4].z0 == s->z0 &&
              SMP_SETS[1].nz == s->nz && SMP_SETS[4].nz == s->nz && str_eq(SMP_SETS[2].name, "FLUTE") &&
              str_eq(SMP_SETS[3].name, "SAX") && str_eq(SMP_ALL_NAMES[5], "USR1") && ENG_SAMPLE.edit[0].max == 7 &&
              ENG_SAMPLE.npresets == 4u);
        snprintf(m, sizeof m, "PIANO: 2 one-shot zones at 11,025 Hz, roots 48 / 72, keys 0-59 / 60-127, 1.6 s (%u B ADPCM)",
                 bytes);
        check(m, s->nz == 2u && z[0].rate == 16384u && z[1].rate == 16384u && z[0].root16 == 48 * 16 &&
                 z[1].root16 == 72 * 16 && z[0].lo == 0u && z[0].hi == 59u && z[1].lo == 60u && z[1].hi == 127u &&
                 !z[0].looped && !z[1].looped && z[0].n == 17640u && z[1].n == 17640u && bytes < 18000u);
        render(ENGI_SAMPLE, 0, 59, 0, &zone);
        render(ENGI_SAMPLE, 0, 60, 0, &zone2);
        check("PIANO: note 59 plays the zone of root 48, 60 the zone of root 72 (the split)",
              zone == (int32_t)s->z0 && zone2 == (int32_t)s->z0 + 1);
        for (i = 0; i < sizeof NOTES; i++) {
            render(ENGI_SAMPLE, 0, NOTES[i], 0, 0);
            pitch_ok("PIANO", NOTES[i]);
            snprintf(path, sizeof path, "piano_%02u", NOTES[i]);
            wav_out(dir, path);
        }
        render(ENGI_SAMPLE, 0, 48, &ended, 0);
        snprintf(m, sizeof m, "PIANO: a root plays 1.6 s, faded out (ends %.3f s; RMS %.0f at 0.3 s, %.0f in its last 50 ms)",
                 (double)ended / FS, rms(FS * 3u / 10u, FS * 35u / 100u), rms(FS * 155u / 100u, FS * 16u / 10u));
        check(m, ended > FS * 159u / 100u && ended < FS * 162u / 100u && rms(FS * 162u / 100u, FS * SECS) < 2.0 &&
                 rms(FS * 155u / 100u, FS * 16u / 10u) < 0.05 * rms(FS * 3u / 10u, FS * 35u / 100u));
        render(8u, 0, 60, 0, &zone);
        check("PIANO: GRAIN SRC 0 takes its zones (the root-72 zone for note 60, not the sine)",
              zone == 1 && rms(FS / 2u, FS) > 100.0);
    }

    /* 2: SLICE's PIANO, the 1.0.4 middle C zone, a copy of its own */
    check("SLICE PIANO: the 1.0.4 middle C zone (16537 samples at 22,050 Hz), data of its own after the sets",
          SLC_PIANO.len == 16537u && SLC_PIANO.rate == 32768u && SLC_PIANO.seg[0].off >= SMP_ZONES[7].off + SMP_ZONES[7].n / 2u);

    /* 3: PIANO HD, the 5-zone PIANO of 1.0 .. 1.1.5 as a user slot */
    snprintf(path, sizeof path, "%s.hdr", argv[1]);
    if (load(path, host_slots, SMP_USER_DATA) != (long)sizeof(smp_user_hdr_t)) {
        printf("sample: no %s (gen_samples.py --user-slot PIANO \"PIANO HD\" ...)\n", path);
        return 1;
    }
    snprintf(path, sizeof path, "%s.bin", argv[1]);
    if (load(path, (uint8_t *)host_slots + SMP_USER_DATA, SMP_USER_SIZE - SMP_USER_DATA) <= 0)
        return 1;
    smp_user_scan(0);
    {
        const smp_zone_t *z = &usr_zone[0][2];
        check("PIANO HD: USR1 holds 5 zones (roots 36 .. 84, 22,050 Hz, 0.75 s), 41 KB of a 79.5 KB slot",
              usr_nz[0] == 5u && usr_zone[0][0].root16 == 36 * 16 && usr_zone[0][4].root16 == 84 * 16 &&
              z->root16 == 60 * 16 && z->rate == 32768u && z->n == 16537u);
        check("PIANO HD: its middle C zone is the ADPCM of SLICE's PIANO (1.0.4's), byte for byte",
              z->n == SLC_PIANO.len && !memcmp(&SMP_DATA[z->off], &SMP_DATA[SLC_PIANO.seg[0].off], (z->n + 1u) / 2u));
        for (i = 0; i < sizeof NOTES; i++) {
            render(ENGI_SAMPLE, (int16_t)SMP_NSETS, NOTES[i], 0, 0);
            pitch_ok("PIANO HD (USR1)", NOTES[i]);
            snprintf(path, sizeof path, "piano_hd_%02u", NOTES[i]);
            wav_out(dir, path);
        }
    }
    printf("sample: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
