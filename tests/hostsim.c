/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host-side render of the FELUCCA DSP (engines, voices, FX, sequencer) to a WAV,
 * for debugging sound without hardware. Same sources as the firmware.
 *   tests/run_tests.sh builds it into build/host/;
 *   build/host/hostsim ENGINE PRESET MONO OUT.wav [CHORUS]
 * env: SECS=n renders n s (the 2 s note pattern repeats), CHORD[=k] holds k keys
 * (default 4, up to 8), DRUMS=1 adds GM drum hits, BENCH=1 prints the render time,
 * DIST=d, LEVEL=l, SENDS=c,d,r, SWEEP=1, NOTE=k, OCT=o, PSET=id:v,..., VSWEEP=1 (see below).
 * TRACKS=DIR: the 4-track test (tracks_demo below): a bass / pad / lead / drums pattern
 * with live recording into DIR (mix + solos), checks, and the cost against one track. */
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define __attribute__(x)
#define memset felucca_memset
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "felucca_tables.h"
#include "../firmware/src/libc.c"
#undef memset
#undef memcpy
#undef memcmp
static struct { volatile uint32_t notes, buttons; } fm1_in;
#include "../firmware/src/core.h"
#include "../firmware/src/engines.c"
#include "../firmware/src/drums.c"
#include "../firmware/src/params.c"
#include "../firmware/src/voice.c"
#include "../firmware/src/fx.c"
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#include "../firmware/src/usb.c"
#include "../firmware/src/seq.c"
#define inst (trk[0])                   /* the single-part renders below: part 1 */

static void wav_hdr(FILE *f, uint32_t frames)
{
    uint32_t v;
    fwrite("RIFF", 1, 4, f); v = 36 + frames * 4; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    uint16_t a = 1, ch = 2, ba = 4, bits = 16; uint32_t sr = FS, br = FS * 4;
    fwrite(&a, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f);
    fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); v = frames * 4; fwrite(&v, 4, 1, f);
}

static void wav_put(FILE *f, int32_t l, int32_t r)
{
    int16_t s[2] = {(int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l),
                    (int16_t)(r > 32767 ? 32767 : r < -32768 ? -32768 : r)};
    fwrite(s, 2, 2, f);
}

/* ---------------------------------------------------------------- TRACKS --- */
static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

static void host_tracks_init(void)                /* as felucca_init: defaults, empty patterns */
{
    uint32_t i, k;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_E0; i++)
            trk[k].p[i] = TP[i].def;
        for (i = 0; i < NSTEP; i++)
            trk[k].step[i].time = ST_REST;
    }
    song.master_q12 = 4096;
}

/* the factory preset as ui.c apply_preset_to sets it (sound, sends, arp; not the pattern, not the mix) */
static void host_preset(track_t *t, uint32_t e, uint32_t pi)
{
    static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
    const preset_t *p = &ENGINES[e]->presets[pi % ENGINES[e]->npresets];
    uint32_t i;
    t->eng_req = t->engine = (uint8_t)e;
    t->preset = (uint8_t)(pi % ENGINES[e]->npresets);
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = p->e[i];
    t->p[P_ATK] = p->env[0];
    t->p[P_DEC] = p->env[1];
    t->p[P_SUS] = p->env[2];
    t->p[P_REL] = p->env[3];
    t->p[P_ED_FLT] = p->fenv;
    t->p[P_VOICE] = p->mono ? V_LEGATO : V_POLY;
    for (i = 0; i < 4u; i++) {
        t->p[P_DIST + i] = (int16_t)(p->fx[i] ? p->fx[i] - 1 : FX_DEF[i]);
        t->p[P_AMODE + i] = (int16_t)(p->arp[i] ? p->arp[i] - 1 : TP[P_AMODE + i].def);
    }
}

static void put_step(track_t *t, uint32_t i, uint32_t n, const uint8_t *notes, uint32_t time, uint32_t flags)
{
    step_t *s = &t->step[i];
    uint32_t k;
    s->n = (uint8_t)n;
    for (k = 0; k < 4u; k++)
        s->note[k] = k < n ? notes[k] : 0;
    s->time = (uint8_t)time;
    s->flags = (uint8_t)flags;
    s->vel = n ? 100 : 0;
}

static uint32_t busy_now(void)                   /* sounding voices of the parts (none fading after a block) */
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].active;
    return n;
}

/* the demo song: T1 ANALOG ACID (16 steps), T2 DIGITAL PAD (32 steps, tied chords), T3 LOFI PULSE LD
 * (12 steps: 3 against 4), T4 drums (16 steps, up to 3 notes a step); 120 BPM, 8 bars. Bars 5..6:
 * live recording: a clap into the drums just before step 4 (quantised onto it, not triggered twice),
 * MIDI ch 3 into the lead, a two-key chord on the keys into an empty pad step. solo: 0 = the mix,
 * 1..4 = that track only. Writes DIR/NAME; returns the number of failed checks. */
static int tracks_demo(const char *dir, const char *name, uint32_t solo)
{
    static const uint8_t ACID[16] = {45, 45, 57, 45, 0, 48, 45, 55, 45, 0, 57, 52, 45, 48, 0, 50};
    static const uint8_t ACIDF[16] = {1, 0, 2, 0, 0, 0, 1, 2, 0, 0, 1, 0, 0, 2, 0, 1};
    static const uint8_t AM[4] = {57, 60, 64, 67}, FM[4] = {53, 57, 60, 64};
    static const uint8_t LEAD[12] = {76, 0, 0, 79, 0, 0, 81, 0, 79, 0, 76, 0};
    const uint32_t frames = 16u * FS, bar = 2u * FS;
    char path[512];
    FILE *w;
    uint32_t f, i, bmax = 0, clips = 0, fail = 0, clap_done = 0, lead_done = 0, keys_t = 0, keys_on = 0;
    uint32_t clap_age = 0, clap_hits = 0xFFFFu, clap_pass = 0, kills0;
    track_t *t1 = &trk[0], *t2 = &trk[1], *t3 = &trk[2], *td = TDRUM;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    if (!(w = fopen(path, "wb"))) {
        fprintf(stderr, "tracks: cannot write %s\n", path);
        return 1;
    }
    host_tracks_init();
    song.g[G_BPM] = 120;
    host_preset(t1, 0, 4);
    host_preset(t2, 1, 5);
    host_preset(t3, 3, 0);
    for (i = 0; i < 16u; i++) {
        uint8_t n = ACID[i];
        put_step(t1, i, n ? 1u : 0u, &n, n ? ST_NOTE : ST_REST, ACIDF[i]);
    }
    t2->p[P_SLEN] = 32;
    t2->p[P_SGATE] = 120;
    for (i = 0; i < 32u; i++)
        put_step(t2, i, i % 16u == 0u ? 4u : 0u, i < 16u ? AM : FM, i % 16u == 0u ? ST_NOTE : i % 16u < 14u ? ST_TIE : ST_REST, 0);
    t3->p[P_SLEN] = 12;
    for (i = 0; i < 12u; i++) {
        uint8_t n = LEAD[i];
        put_step(t3, i, n ? 1u : 0u, &n, n ? ST_NOTE : ST_REST, i == 0u ? SF_ACCENT : 0u);
    }
    for (i = 0; i < 16u; i++) {                    /* kick 4 on the floor, snare 4 / 12, hats on the 8ths */
        uint8_t n[4];
        uint32_t k = 0;
        if (i % 4u == 0u)
            n[k++] = 36;
        if (i == 4u || i == 12u)
            n[k++] = 38;
        if (i % 2u == 0u)
            n[k++] = i == 14u ? 46 : 42;
        put_step(td, i, k, n, k ? ST_NOTE : ST_REST, i % 4u == 0u ? SF_ACCENT : 0u);
    }
    if (solo) {                                    /* the other tracks silent */
        for (i = 0; i < NPART; i++)
            if (i + 1u != solo)
                trk[i].p[P_LEVEL] = 0;
        if (solo != 4u)
            song.g[G_DRLVL] = 0;
    }
    kills0 = voice_kills;
    transport_req = 1;
    wav_hdr(w, frames);
    for (f = 0; f < frames; f += CTL) {
        int32_t o[2 * CTL];
        uint32_t barn = f / bar, period = div_samples(2);
        if (barn == 4u && !song.rec)
            song.rec = 0x0Eu;                      /* bars 5..6: tracks 2, 3, 4 armed */
        if (barn == 6u)
            song.rec = 0;
        /* (a) a clap into the drums, late in step 3: lands on step 4, sounds now, step 4 does not repeat it */
        if (song.rec && !clap_done && td->seq_idx == 3u && td->seq_pos > period * 3u / 4u) {
            input_on(td, 39, 110);
            clap_done = 1;
            clap_age = drums.age;
            clap_pass = 1;
        } else if (clap_pass == 1u && td->seq_idx == 4u) {
            clap_hits = drums.age - clap_age;      /* what step 4 triggered right after (36, 38, 42; not 39) */
            clap_pass = 2;
        }
        /* (b) MIDI ch 3 into the lead, early in its step 1 */
        if (song.rec && !lead_done && t3->seq_idx == 1u && t3->seq_pos < period / 4u) {
            midi_in_q[mi_w++ % MQ] = 0x09u | 0x92u << 8 | 84u << 16 | 100u << 24;
            lead_done = 1;
        } else if (lead_done == 1u && t3->seq_idx == 2u) {
            midi_in_q[mi_w++ % MQ] = 0x08u | 0x82u << 8 | 84u << 16;
            lead_done = 2;
        }
        /* (c) the keys, track 2 selected: two keys at once into the empty pad step 14 */
        if (song.rec && !keys_on && !keys_t && t2->seq_idx == 14u && t2->seq_pos < period / 4u) {
            song.sel = 1;
            fm1_in.notes = (1u << 7) | (1u << 11);
            keys_on = 1;
            keys_t = f;
        } else if (keys_on && f - keys_t > period / 2u) {
            fm1_in.notes = 0;
            keys_on = 0;
        }
        mix_block(o, CTL);
        if (busy_now() > bmax)
            bmax = busy_now();
        for (i = 0; i < CTL; i++) {
            int32_t l = o[2 * i];
            if (l >= 32700 || l <= -32700)
                clips++;
            wav_put(w, o[2 * i], o[2 * i + 1]);
        }
    }
    fclose(w);
    if (solo)
        return bmax > NVOICE;
    {
        const step_t *s4 = &td->step[4], *l1 = &t3->step[1], *p14 = &t2->step[14];
        int ok_clap = s4->n == 4u && s4->note[3] == 39u && clap_hits == 3u;
        int ok_lead = l1->n == 1u && l1->note[0] == 84u && l1->time == ST_NOTE;
        int ok_keys = p14->n == 2u && p14->time == ST_NOTE && p14->note[0] == 60u && p14->note[1] == 64u;
        printf("tracks: recording: drums step 4 = %u notes (kick snare hat + clap 39: %s), hits when step 4 played right "
               "after: %u (want 3: the clap is not triggered twice) %s\n", s4->n, s4->note[3] == 39u ? "yes" : "no", clap_hits,
               ok_clap ? "ok" : "FAIL");
        printf("tracks: recording: MIDI ch 3 -> lead step 1 = %u (want 84) %s; keys -> pad step 14 = %u notes %u %u %s\n",
               l1->note[0], ok_lead ? "ok" : "FAIL", p14->n, p14->note[0], p14->note[1], ok_keys ? "ok" : "FAIL");
        printf("tracks: voices sounding at most %u (budget %u) %s; given up to another part %u; %u samples near full "
               "scale\n", bmax, NVOICE, bmax <= NVOICE ? "ok" : "FAIL", voice_kills - kills0, clips);
        fail = !ok_clap + !ok_lead + !ok_keys + (bmax > NVOICE) + (clips > 0);
    }
    return (int)fail;
}

/* ns per 44.1 kHz sample of the whole mix (events, parts, drums on 16ths, buses, master) with notes
 * held: parts[k] = {engine, preset, notes} for part k, 0 notes = idle. 2 s to settle, then the
 * fastest of four 2 s stretches (the host's other load only ever adds). Runs in a child process
 * (a clean state each time); the result comes back through a pipe. */
static double tracks_cost(const uint8_t parts[NPART][3], uint32_t *busy_max)
{
    int fd[2];
    pid_t pid;
    double r[2] = {-1, 0};
    if (pipe(fd))
        return -1;
    fflush(stdout);
    if (!(pid = fork())) {
        static const uint8_t NOTES[8] = {48, 52, 55, 59, 60, 64, 67, 71};
        uint64_t ns = 0, best = ~0ull, k, nblk = 8u * FS / CTL, seg = nblk / 4u;   /* the fastest of 4 x 2 s: */
        uint32_t p, i, bm = 0;
        int32_t o[2 * CTL];
        host_tracks_init();
        for (p = 0; p < NPART; p++) {
            host_preset(&trk[p], parts[p][0], parts[p][1]);
            trk[p].p[P_VOICE] = V_POLY;
            trk[p].p[P_SUS] = 127;                  /* held notes keep sounding */
            trk[p].p[P_AMODE] = 0;
        }
        for (p = 0; p < NPART; p++)
            for (i = 0; i < parts[p][2]; i++)
                trk_note_on(&trk[p], NOTES[i] + 12u * p, 100);
        for (k = 0; k < 2u * FS / CTL + nblk; k++) {   /* 2 s to settle (attacks), then measure */
            uint64_t t0;
            if ((k * CTL) % (FS / 8u) < CTL)          /* 16ths at 120 BPM */
                drum_on((k * CTL) % (FS / 2u) < CTL ? 36u : ((k * CTL) / (FS / 8u)) % 4u == 2u ? 38u : 42u, 100u);
            t0 = now_ns();
            mix_block(o, CTL);
            if (k >= 2u * FS / CTL) {                  /* (the host's own noise only ever adds) */
                ns += now_ns() - t0;
                if ((k - 2u * FS / CTL) % seg == seg - 1u) {
                    best = ns < best ? ns : best;
                    ns = 0;
                }
            }
            if (busy_now() > bm)
                bm = busy_now();
        }
        r[0] = (double)best / (double)(seg * CTL);
        r[1] = bm;
        if (write(fd[1], r, sizeof r) != sizeof r)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], r, sizeof r) != sizeof r)
        r[0] = -1;
    close(fd[0]);
    waitpid(pid, 0, 0);
    if (busy_max)
        *busy_max = (uint32_t)r[1];
    return r[0];
}

/* stealing across parts must not click: three parts of plain sines (smooth: the largest sample
 * step is set by the pitches), 7 + 1 notes fill the budget, then notes on parts 2 and 3 take held
 * voices of part 1 (and part 2). Each taken voice fades over one block. Compares the largest sample
 * step around each take with the largest one in the 0.25 s before it; a hard cut would be several
 * times larger. Writes DIR/steal.wav. */
static int steal_test(const char *dir)
{
    static const uint8_t N1[7] = {48, 52, 55, 59, 62, 65, 69};
    const uint32_t frames = 4u * FS;
    char path[512];
    FILE *w;
    int32_t prev = 0, *L = calloc(frames, sizeof *L);
    uint32_t f, i, p, k, events[6], nev = 0, bad = 0, kills0;
    double worst = 0;
    snprintf(path, sizeof path, "%s/steal.wav", dir);
    if (!L || !(w = fopen(path, "wb")))
        return 1;
    host_tracks_init();
    for (p = 0; p < NPART; p++) {
        track_t *t = &trk[p];
        host_preset(t, 0, 5);                      /* ANALOG SINE KEY, as a plain held sine: */
        t->p[P_E4] = 127;                          /* filter open, no resonance, no drive */
        t->p[P_E5] = t->p[P_E6] = 0;
        t->p[P_ED_FLT] = 0;
        t->p[P_ATK] = 40;                          /* slow attack, full sustain, no sends */
        t->p[P_SUS] = 127;
        t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
        t->p[P_VOICE] = V_POLY;
        t->p[P_LEVEL] = 90;
    }
    kills0 = voice_kills;
    wav_hdr(w, frames);
    for (f = 0; f < frames; f += CTL) {
        int32_t o[2 * CTL];
        uint32_t ms = f * 1000u / FS, k0 = voice_kills;
        if (f == 0)
            for (i = 0; i < 7u; i++)
                trk_note_on(&trk[0], N1[i], 90);
        if (f == FS / 2u / CTL * CTL)
            trk_note_on(&trk[1], 74, 90);          /* 8: the budget is full */
        if (ms >= 1000u && ms % 500u == 0u && f % (FS / 2u) < CTL && ms < 3500u) {
            p = ms / 500u % 2u ? 1u : 2u;          /* parts 2, 3 in turn: each takes a voice */
            trk_note_on(&trk[p], 76u + ms / 250u, 90);
        }
        mix_block(o, CTL);
        if (voice_kills != k0 && nev < 6u)
            events[nev++] = f;
        for (i = 0; i < CTL; i++) {
            L[f + i] = o[2 * i];
            wav_put(w, o[2 * i], o[2 * i + 1]);
        }
    }
    fclose(w);
    for (k = 0; k < nev; k++) {                    /* around the take vs. the 0.25 s before */
        int32_t calm = 0, at = 0;
        uint32_t e = events[k];
        for (i = e - FS / 4u; i < e - 2u * CTL; i++)
            calm = abs(L[i] - L[i - 1]) > calm ? abs(L[i] - L[i - 1]) : calm;
        for (i = e; i < e + 2u * CTL; i++)
            at = abs(L[i] - L[i - 1]) > at ? abs(L[i] - L[i - 1]) : at;
        if ((double)at / calm > worst)
            worst = (double)at / calm;
        if (at > calm * 3 / 2)
            bad++;
    }
    (void)prev;
    printf("tracks: stealing across parts: %u voices taken; largest sample step at a take / before it: at most %.2f x "
           "(a hard cut: several x) %s\n", voice_kills - kills0, worst, nev >= 4u && !bad ? "ok" : "FAIL");
    free(L);
    return nev < 4u || bad;
}

static int tracks_test(const char *dir)
{
    static const char *const SOLO[5] = {"tracks_demo.wav", "t1_bass.wav", "t2_pad.wav", "t3_lead.wav", "t4_drums.wav"};
    uint32_t e, pi, s, worst_e = 0, worst_p = 0, heavy[NENGINES] = {0}, bm = 0;
    double worst = 0, best_e[NENGINES] = {0}, four, four_full;
    int fail = 0;
    for (s = 0; s < 5u; s++) {
        pid_t pid;
        fflush(stdout);
        pid = fork();
        int st = 0;
        if (!pid)
            { int rc = tracks_demo(dir, SOLO[s], s); fflush(stdout); _exit(rc); }
        waitpid(pid, &st, 0);
        if (!WIFEXITED(st) || WEXITSTATUS(st))
            fail++;
    }
    {
        pid_t pid;
        int st = 0;
        fflush(stdout);
        pid = fork();
        if (!pid) {
            int rc = steal_test(dir);
            fflush(stdout);
            _exit(rc);
        }
        waitpid(pid, &st, 0);
        if (!WIFEXITED(st) || WEXITSTATUS(st))
            fail++;
    }
    printf("tracks: WAVs in %s: %s (mix), %s, %s, %s, %s, steal.wav\n", dir, SOLO[0], SOLO[1], SOLO[2], SOLO[3], SOLO[4]);
    /* one part: every preset with 8 held notes (the engine's cap: VOICE 4) + drums; the worst one */
    for (e = 0; e < NENGINES; e++)
        for (pi = 0; pi < ENGINES[e]->npresets; pi++) {
            uint8_t parts[NPART][3] = {{(uint8_t)e, (uint8_t)pi, 8}, {0, 0, 0}, {0, 0, 0}};
            double c = tracks_cost(parts, 0);
            if (c > best_e[e]) {
                best_e[e] = c;
                heavy[e] = pi;
            }
            if (c > worst) {
                worst = c;
                worst_e = e;
                worst_p = pi;
            }
        }
    printf("tracks: one part, 8 notes held + drums (host -O2, ns per sample, heaviest preset per engine):");
    for (e = 0; e < NENGINES; e++)
        printf(" %s %s %.1f%s", ENGINES[e]->name, ENGINES[e]->presets[heavy[e]].name, best_e[e], e + 1u < NENGINES ? "," : "\n");
    {   /* DIGITAL, PHASE, VOICE (their heaviest presets) at once: 3 + 3 + 2 notes = the budget of 8 */
        uint8_t parts[NPART][3] = {{1, (uint8_t)heavy[1], 3}, {2, (uint8_t)heavy[2], 3}, {5, (uint8_t)heavy[5], 2}};
        uint8_t full[NPART][3] = {{1, (uint8_t)heavy[1], 8}, {2, (uint8_t)heavy[2], 8}, {5, (uint8_t)heavy[5], 4}};
        uint8_t vv[NPART][3] = {{5, (uint8_t)heavy[5], 4}, {5, (uint8_t)heavy[5], 4}, {0, 0, 0}};
        uint8_t idle[NPART][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
        uint32_t bm2 = 0, bm3 = 0;
        double two_voice, none;
        four = tracks_cost(parts, &bm);
        four_full = tracks_cost(full, &bm2);
        two_voice = tracks_cost(vv, &bm3);
        none = tracks_cost(idle, 0);
        printf("tracks: worst single part: %s %s %.1f ns\n", ENGINES[worst_e]->name, ENGINES[worst_e]->presets[worst_p].name, worst);
        printf("tracks: DIGITAL + PHASE + VOICE + drums, 3 + 3 + 2 notes: %.1f ns (%.2f x the worst single part), "
               "%u voices\n", four, four / worst, bm);
        printf("tracks: the same, 8 + 8 + 4 notes asked for (budget keeps %u): %.1f ns (%.2f x)\n", bm2, four_full,
               four_full / worst);
        printf("tracks: two VOICE parts, 4 + 4 voices (the heaviest 8 the budget allows): %.1f ns (%.2f x); "
               "no notes (drums, buses, 3 idle parts): %.1f ns\n", two_voice, two_voice / worst, none);
        if (bm > NVOICE || bm2 > NVOICE || bm3 > NVOICE)
            fail++;
    }
    printf("tracks: %s\n", fail ? "FAILED" : "all checks ok");
    return fail;
}

int main(int argc, char **argv)
{
    int eng = argc > 1 ? atoi(argv[1]) : 0, preset = argc > 2 ? atoi(argv[2]) : 4;
    int mono = argc > 3 ? atoi(argv[3]) : 1;
    const char *out = argc > 4 ? argv[4] : "out.wav";
    uint32_t i, frames = FS * (getenv("SWEEP") ? 4 : 2), f, nk = 0;
    clock_t c0;
    FILE *w = fopen(out, "wb");
    if (getenv("SECS"))
        frames = FS * (uint32_t)atoi(getenv("SECS"));
    if (getenv("CHORD"))
        nk = atoi(getenv("CHORD")) > 4 ? (uint32_t)atoi(getenv("CHORD")) : 4u;
    for (i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
    for (i = 0; i < P_E0; i++) inst.p[i] = TP[i].def;
    inst.eng_req = inst.engine = (uint8_t)eng;
    for (i = 0; i < 8; i++) inst.p[P_E0 + i] = ENGINES[eng]->edit[i].def;
    if (ENGINES[eng]->npresets) {
        const preset_t *p = &ENGINES[eng]->presets[preset % ENGINES[eng]->npresets];
        for (i = 0; i < 8; i++) inst.p[P_E0 + i] = p->e[i];
        inst.p[P_ATK] = p->env[0]; inst.p[P_DEC] = p->env[1];
        inst.p[P_SUS] = p->env[2]; inst.p[P_REL] = p->env[3]; inst.p[P_ED_FLT] = p->fenv;
    }
    inst.p[P_VOICE] = (int16_t)mono;
    inst.p[P_CHOR] = argc > 5 ? atoi(argv[5]) : 24;      /* as felucca_init */
    inst.p[P_DLY] = argc > 5 ? atoi(argv[5]) : 28;
    inst.p[P_REV] = argc > 5 ? atoi(argv[5]) : 36;
    if (getenv("SENDS"))                                 /* SENDS=chorus,delay,reverb */
        sscanf(getenv("SENDS"), "%hd,%hd,%hd", &inst.p[P_CHOR], &inst.p[P_DLY], &inst.p[P_REV]);
    if (getenv("OCT"))                                   /* OCT=o: keyboard octave shift (key 7 = C4 + 12 o) */
        song.octave = (int8_t)atoi(getenv("OCT"));
    if (getenv("PSET")) {                               /* PSET=id:value,... (track parameter ids, P_E0 = 45) */
        const char *s = getenv("PSET");
        int id, val, k;
        while (sscanf(s, "%d:%d%n", &id, &val, &k) == 2) {
            if (id >= 0 && id < P_COUNT)
                inst.p[id] = (int16_t)val;
            s += k;
            if (*s == ',')
                s++;
        }
    }
    song.master_q12 = 4096;
    if (getenv("TRACKS")) {
        fclose(w);
        remove(out);
        return tracks_test(getenv("TRACKS"));
    }
    wav_hdr(w, frames);
    c0 = clock();
    for (f = 0; f < frames; f += CTL) {
        int32_t o[2 * CTL];
        uint32_t fp = f % (2u * FS);    /* position in the repeating 2 s pattern */
        {   /* C4 0.1-0.5 s, E4 0.45-0.8 s (overlap: legato/steal), G4 0.9-1.2 s */
            uint32_t n = 0, f = fp;
            if (f > FS / 10 && f < FS / 2) n |= 1u << 7;
            if (f > FS * 45 / 100 && f < FS * 8 / 10) n |= 1u << 11;
            if (f > FS * 9 / 10 && f < FS * 12 / 10) n |= 1u << 14;
            fm1_in.notes = n;
        }
        if (nk) {                       /* keys held 0.1-1.5 s (C E G B, then D F A C) */
            static const uint8_t K[8] = {7, 11, 14, 18, 9, 12, 16, 19};
            uint32_t n = 0, k;
            for (k = 0; k < nk && k < 8u; k++)
                n |= 1u << K[k];
            fm1_in.notes = (fp > FS / 10 && fp < FS * 3 / 2) ? n : 0;
        }
        if (getenv("DRUMS") && fp % (FS / 4u) < CTL)   /* kick / closed hat / snare on 8ths */
            drum_on(fp % (FS / 2u) < CTL ? 36u : (fp / (FS / 4u)) % 4u == 3u ? 38u : 42u, 100u);
        if (getenv("DIST"))
            inst.p[P_DIST] = (int16_t)atoi(getenv("DIST"));
        if (getenv("LEVEL"))
            inst.p[P_LEVEL] = (int16_t)atoi(getenv("LEVEL"));
        if (getenv("SWEEP")) {          /* cutoff and resonance sweeps while a note is held */
            uint32_t t = f * 1000 / FS;  /* ms */
            inst.p[P_E4] = (int16_t)((t / 4) % 256 < 128 ? (t / 4) % 128 : 127 - (t / 4) % 128);
            inst.p[P_E5] = (int16_t)((t / 7) % 256 < 128 ? (t / 7) % 128 : 127 - (t / 7) % 128);
            fm1_in.notes = (f > FS / 20) ? (1u << 7) : 0;
        }
        if (getenv("NOTE"))             /* NOTE=k: one key (7 = C4, see OCT) held to 0.5 s before the end */
            fm1_in.notes = (f > FS / 20 && f + FS / 2 < frames) ? 1u << atoi(getenv("NOTE")) : 0;
        if (getenv("VSWEEP"))           /* VSWEEP=1: EDIT 1 (P_E0) from 0 to 127 over the render */
            inst.p[P_E0] = (int16_t)((uint64_t)f * 128u / frames);
        mix_block(o, CTL);
        {   /* VOICES=1: report the most voices active at once (engine voice caps) */
            static uint32_t vmax, hi;
            uint32_t k, a = 0;
            for (k = 0; k < NVOICE; k++)
                if (inst.v[k].active) {
                    a++;
                    hi |= 1u << k;
                }
            if (a > vmax)
                vmax = a;
            if (getenv("VOICES") && f + CTL >= frames)
                fprintf(stderr, "voices: at most %u active, slots used 0x%02X\n", vmax, hi);
        }
        for (i = 0; i < 2 * CTL; i++) {
            int16_t s = (int16_t)(o[i] > 32767 ? 32767 : o[i] < -32768 ? -32768 : o[i]);
            fwrite(&s, 2, 1, w);
        }
    }
    if (getenv("BENCH"))
        fprintf(stderr, "%.1f s audio in %.1f ms (%.2f %% of real time)\n", (double)frames / FS,
                (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC,
                (double)(clock() - c0) * 100.0 / CLOCKS_PER_SEC / ((double)frames / FS));
    fclose(w);
    return 0;
}
