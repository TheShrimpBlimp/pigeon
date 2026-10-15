/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SAMPLE: IMA ADPCM sample sets. */
static const char *const N_ONOFF_E[] = {"OFF", "ON"};
/* IMA ADPCM sample sets (tools/gen_samples.py), native rates, zones across
 * the keyboard, loops with the ADPCM state stored at the loop start.
 * SET is P_E0 (EDIT 1, KNOB 1). SET 4 was PERC, the GM drum kit: retired, an alias of PIANO; its sounds
 * load as the DRUM engine (core.h drum_from_perc). */
typedef struct {
    uint32_t off, n, ls, le;     /* byte offset, samples, loop start / end (sample index) */
    uint32_t rate;               /* source rate / 44100, Q16 */
    int16_t root16;              /* root pitch in 1/16 semitone */
    int16_t pred;                /* ADPCM state at the loop start */
    uint8_t idx, lo, hi, looped;
} smp_zone_t;
typedef struct {
    const char *name;
    uint16_t z0, nz;
} smp_set_t;
#include "felucca_samples.h"
#if defined(SMP_PERC_SLOT) && SMP_PERC_SLOT != SMP_SET_PERC
#error "tools/gen_samples.py PERC_SLOT != core.h SMP_SET_PERC"
#endif

static const int16_t IMA_STEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
    4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767};
static const int8_t IMA_IDX[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
/* 2^(i/192), Q16: pitch ratios in 1/16 semitones, d16 >= -3072 (16 octaves down) */
static uint32_t pow2_q16(int32_t d16)
{
    uint32_t u = (uint32_t)(d16 + 192 * 16), oct = u / 192u;     /* no loop for negative d16 */
    uint32_t r = pitch_inc(1600 + u % 192u) / (pitch_inc(1600) >> 16);   /* 2^(d/192) via the pitch table */
    return oct >= 16u ? r << (oct - 16u) : r >> (16u - oct);
}

/* ---- user sample slots (loaded from the web editor into flash, see web/EDITOR_PROTOCOL.md)
 * 3 slots of 80 KiB at flash 0xA0000.. (Felucca data region), read through the plain XIP
 * window. Slot = header (magic, count, name, data length, CRC32) + up to 16 zones in the
 * smp_zone_t layout (off relative to the slot's data at +512) + IMA ADPCM data. */
#include "../hal/fm1_xip.h"   /* relative: hostsim includes this file too */
#define SMP_USER_SLOTS 3
#define SMP_USER_BASE 0xA0000u
#define SMP_USER_SIZE 0x14000u
#define SMP_USER_DATA 512u
#define SMP_USER_MAGIC 0x504D5346u                  /* "FSMP" */
#define SMP_NALL (SMP_NSETS + SMP_USER_SLOTS)
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t nz, rsv;
    char name[8];
    uint32_t data_len, crc, rsv2[2];
    smp_zone_t zone[16];
} smp_user_hdr_t;                                   /* 32 + 16 x 28 = 480 B, data at +512 */
static smp_zone_t usr_zone[SMP_USER_SLOTS][16];     /* RAM copy, off rebased onto SMP_DATA */
static uint8_t usr_nz[SMP_USER_SLOTS];
static const char *const SMP_ALL_NAMES[SMP_NALL] = {SMP_SET_NAMES_INIT, "USR1", "USR2", "USR3"};

#ifndef SMP_USER_XIP                                /* host tests: a RAM image of the slots */
#define SMP_USER_XIP(k) fm1_xip_ptr(SMP_USER_BASE + (k) * SMP_USER_SIZE)
#endif
static const uint8_t *smp_user_xip(uint32_t k) { return SMP_USER_XIP(k); }
static uint32_t smp_user_gen;
#if FELUCCA_SLICE
static void slc_user_scan(uint32_t k, int valid);   /* eng_slice.c: SLICE's slice table of the slot */
#else
#define slc_user_scan(k, valid) ((void)0)
#endif
#if FELUCCA_KO_STORAGE
#include "ko_bank.c"
#endif
static inline uint32_t smp_byte(uint32_t off)
{
#if FELUCCA_KO_STORAGE
    if (off & KO_VOFF) return ko_bank_byte(off & ~KO_VOFF);
#endif
#ifdef FELUCCA_WEB
    /* User sample offsets address the simulated NOR, beyond the factory array. */
    return fm1_web_sample_byte((uintptr_t)SMP_DATA + off);
#else
    return SMP_DATA[off];
#endif
}
/* Paired KO song: two independently encoded chunks in USR1 / USR3.
 * Existing headers remain version 1; reserved words bind the pair. No partition moves. */
#define SMP_KO_PAIR 0x31534F4Bu /* KOS1 */
static int smp_ko_pair_requested(void)
{
#if FELUCCA_KO
    const smp_user_hdr_t *a = (const smp_user_hdr_t *)smp_user_xip(0);
    return a->magic == SMP_USER_MAGIC && a->rsv == 0x4Bu && a->rsv2[0] == SMP_KO_PAIR;
#else
    return 0;
#endif
}
static int smp_ko_pair_valid(void)
{
    const smp_user_hdr_t *a = (const smp_user_hdr_t *)smp_user_xip(0);
    const smp_user_hdr_t *b = (const smp_user_hdr_t *)smp_user_xip(2);
    return smp_ko_pair_requested() && usr_nz[0] == 1 && usr_nz[2] == 1 &&
        b->rsv == 0x4Cu && b->rsv2[0] == SMP_KO_PAIR && a->rsv2[1] == b->rsv2[1] &&
        a->data_len <= SMP_USER_SIZE - SMP_USER_DATA - 4096u &&
        usr_zone[0][0].rate == usr_zone[2][0].rate;
}

/* (re)read slot k from flash: valid header -> zones usable; call after boot and after an upload
 * (main loop: SLICE scans the slot's audio here). The zones are checked in a copy: an unusable slot leaves
 * every zone of it empty (n 0: a voice still on it ends), never a half-checked one */
static void smp_user_scan(uint32_t k)
{
#if FELUCCA_KO_STORAGE
    if (*(const uint32_t *)smp_user_xip(0) == KO_BANK_MAGIC) {
        if (!k) ko_bank_scan();
        return;
    }
    ko_bank_active = 0;
#endif
    const smp_user_hdr_t *h = (const smp_user_hdr_t *)smp_user_xip(k);
    smp_zone_t zt[16];
    uint32_t i, base, nz = 0;
    smp_user_gen++;
    usr_nz[k] = 0;
    slc_user_scan(k, 0);
    if (h->magic == SMP_USER_MAGIC && h->version == 1 && h->nz && h->nz <= 16u &&
        h->data_len <= SMP_USER_SIZE - SMP_USER_DATA) {
        base = (uint32_t)(uintptr_t)(smp_user_xip(k) + SMP_USER_DATA) - (uint32_t)(uintptr_t)SMP_DATA;
        for (nz = h->nz, i = 0; i < nz; i++) {
            smp_zone_t *z = &zt[i];
            *z = h->zone[i];
            if (z->off > h->data_len || z->n > 2u * SMP_USER_SIZE || z->off + (z->n + 1u) / 2u > h->data_len ||
                z->idx > 88u || !z->rate || z->rate > 4u << 16 || z->ls > z->le || z->le >= z->n || z->lo > z->hi) {
                nz = 0;                             /* zone outside the data or malformed: slot unusable */
                break;
            }
            z->off += base;
        }
    }
    for (i = 0; i < 16u; i++)
        usr_zone[k][i] = i < nz ? zt[i] : (smp_zone_t){0};
    if (!nz)
        return;
    usr_nz[k] = (uint8_t)nz;
    slc_user_scan(k, 1);
}

/* SET / SRC si (0..SMP_NALL - 1) has no sample data: an empty or invalid user slot, a built-in set without data */
static int smp_set_missing(uint32_t si)
{
#if FELUCCA_KO_STORAGE
    if (ko_bank_active && si == SMP_NSETS && ko_bank.codec == KO_CODEC_MP3) return 1;
#endif
    if (si < SMP_NSETS)
        return !SMP_ZONES[SMP_SETS[si].z0].n;
    return !usr_nz[(si - SMP_NSETS) % SMP_USER_SLOTS];
}

/* zone index in a voice: < 0x8000 built-in, else 0x8000 | slot << 5 | zone */
static inline const smp_zone_t *smp_zone(uint32_t zi)
{
    return zi < 0x8000u ? &SMP_ZONES[zi] : &usr_zone[(zi >> 5) & 3u][zi & 31u];
}

/* voice: ph[0] position (samples), ph[1] fraction Q16, s[0] predictor, s[1] step
 * index, s[2] previous sample, s[3] current sample, s[4] zone, s[5] step Q16 */
static inline int32_t sample_next(const smp_zone_t *z, voice_t *v, int loop)
{
    uint32_t pos = v->ph[0], b = smp_byte(z->off + (pos >> 1));
    uint32_t code = (pos & 1u) ? (b >> 4) : (b & 15u);
    int32_t step = IMA_STEP[(uint32_t)v->s[1] <= 88u ? v->s[1] : 88], vd = step >> 3;
    if (code & 4u)
        vd += step;
    if (code & 2u)
        vd += step >> 1;
    if (code & 1u)
        vd += step >> 2;
    v->s[0] = clamp(v->s[0] + ((code & 8u) ? -vd : vd), -32768, 32767);
    v->s[1] = clamp(v->s[1] + IMA_IDX[code & 7u], 0, 88);
    pos++;
    if (pos > z->le && z->looped && loop) {
        pos = z->ls;
        v->s[0] = z->pred;
        v->s[1] = z->idx;
    }
    v->ph[0] = pos;
    return v->s[0];
}

static void sample_note_on(track_t *t, voice_t *v)
{
    uint32_t si = (uint32_t)t->p[P_E0] % SMP_NALL, i, zi = 0xFFFFu, width = 128u;
    if (si < SMP_NSETS) {                           /* a built-in set: its zones split the keyboard */
        const smp_set_t *set = &SMP_SETS[si];
        for (i = 0; i < set->nz; i++)
            if (v->note >= SMP_ZONES[set->z0 + i].lo && v->note <= SMP_ZONES[set->z0 + i].hi) {
                zi = set->z0 + i;
                break;
            }
        v->s[4] = (int32_t)(zi == 0xFFFFu ? set->z0 : zi);
    } else {                                        /* user slot: silent if empty; the narrowest zone plays */
        uint32_t k = si - SMP_NSETS;
        for (i = 0; i < usr_nz[k]; i++) {
            const smp_zone_t *z = &usr_zone[k][i];
            if (v->note >= z->lo && v->note <= z->hi && z->hi - z->lo <= width) {
                zi = 0x8000u | k << 5 | i;
                width = z->hi - z->lo;
            }
        }
        v->s[4] = (int32_t)(zi == 0xFFFFu ? 0 : zi);
    }
#if FELUCCA_KO
    if(ko_kit(t) && v->note>=60 && v->note<76) {
        ko_kit_transpose[v-t->v]=(int8_t)clamp(t->p[P_TRANS],-24,24);
        ko_pad_params_t *pad=&ko_kit_params[v->note-60];
        if(pad->choke) for(uint32_t j=0;j<NVOICE;j++) {voice_t *other=&t->v[j];
            if(other!=v && other->active && other->note>=60 && other->note<76 && ko_kit_params[other->note-60].choke==pad->choke) {other->stage=4;other->gate=0;}}
        ko_kit_drop[v-t->v]=pad->decay ? (1u<<24)/(pad->decay*4u) : 0;v->env=1<<24;
    }
#endif
    v->ph[0] = 0;
    v->ph[1] = 0;
    v->s[0] = 0;
    v->s[1] = 0;
    v->s[2] = v->s[3] = 0;
    v->s[6] = zi == 0xFFFFu;     /* 1 = sample ended (one-shot); a kit key with no sound stays silent */
    v->s[7] = 0;                 /* lo-pass state */
    if (smp_set_missing(si))
        v->s[6] = 1;             /* no sample data: silent, ended */
#if FELUCCA_KO
    if (ko_kit(t)) {                                /* unloaded pads are silent */
        if (zi == 0xFFFFu || smp_set_missing(si))
            v->s[6] = 1;
        v->gate = 0;                               /* sequencer gate length does not truncate a hit */
    }
#endif
}

#if FELUCCA_KO
static int32_t sample_ko_keys(const track_t *t, uint32_t k)
{
    int32_t pad = ko_pad(k);
    return ko_kit(t) ? (pad < 0 || pad >= 16 ? 255 : 60 + pad) : -1;
}
static int32_t sample_ko_amp(track_t *t, voice_t *v, int32_t adsr)
{
    if (!ko_kit(t))
        return adsr;
    if (!v->active) return adsr;                    /* budget/overload fades must still finish */
    if (v->s[6]) { v->active = 0; return 0; }
    if(v->stage==4) return adsr;
    v->stage = 2;
    if(v->note<60 || v->note>=76) return 0;
    uint32_t drop=ko_kit_drop[v-t->v];
    if(drop) {if(v->env<=(int32_t)drop) {v->active=0;return 0;}v->env-=(int32_t)drop;}
    else v->env=1<<24;
    return (int32_t)((((v->env-1)>>9)*ko_kit_params[v->note-60].level)/127);
}
#endif

static void sample_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    const smp_zone_t *z = smp_zone((uint32_t)v->s[4]);
    uint32_t i, frac = v->ph[1];
    int32_t d16 = clamp(m->pitch16 + p[P_E1] * 16 - z->root16, -1536, 576);   /* <= 3 octaves up: bounded decode load */
#if FELUCCA_KO
    if(ko_kit(t) && v->note>=60 && v->note<76) d16=clamp(d16+(ko_kit_params[v->note-60].pitch+ko_kit_transpose[v-t->v])*16,-1536,576);
#endif
    uint32_t r = pow2_q16(d16), stepq = (r >> 8) * (z->rate >> 8);   /* Q16 samples per output */
    int32_t bits = p[P_E2], lp = 4000 + ((clamp((p[P_E4] << 8) + m->cutoff, 0, 127 << 8) * 28767) >> 15);
    int32_t drv = p[P_E6], sh = bits / 10;
    int loop = p[P_E3];
#if FELUCCA_KO
    if (ko_kit(t)) loop = 0;
#endif
    if (v->s[6] || !z->n) {
        v->active = 0;
        return;
    }
    if (v->ph[0] == 0 && v->ph[1] == 0 && v->s[3] == 0)
        v->s[3] = sample_next(z, v, loop);         /* prime the interpolator */
    for (i = 0; i < n; i++) {
        int32_t s;
        frac += stepq;
        while (frac >= 65536u) {
            frac -= 65536u;
            v->s[2] = v->s[3];
            if (v->ph[0] >= z->n) {                   /* one-shot reached its end */
                v->s[6] = 1;
                v->s[3] = 0;
                if (frac >= 65536u) {                /* past the last sample-to-zero interval */
                    v->s[2] = 0;
                    frac &= 65535u;
                }
                break;
            }
            v->s[3] = sample_next(z, v, loop);
        }
        s = v->s[2] + (((v->s[3] - v->s[2]) * (int32_t)(frac >> 1)) >> 15);
        if (sh)                                       /* BITS: 0 = clean, up to 12 bits removed */
            s = (s >> sh) << sh;
        if (drv)
            s = softclip(s + (((s >> 2) * (drv * 150)) >> 11));   /* = s * drv * 600 / 32768, no overflow */
        v->s[7] += mulq15(s - v->s[7], lp);           /* gentle tone control (CUT) */
        out[i] += voice_amp(v->s[7], m, i) << 1;
        if (v->s[6])
            break;
    }
    v->ph[1] = frac;
}

static const engine_t ENG_SAMPLE = {
    .name = "SAMPLE",
    .page_title = {"SET", "TONE"},
    .edit = {
        {"SET", F_ENUM, 0, SMP_NALL - 1, 0, SMP_ALL_NAMES, 0},
        {"TUNE", F_SEMI, -24, 24, 0, 0, 0},
        {"BITS", F_INT, 0, 127, 0, 0, 0},
        {"LOOP", F_ENUM, 0, 1, 1, N_ONOFF_E, 0},
        {"CUT", F_INT, 0, 127, 127, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
        {"DRV", F_PCT, 0, 127, 0, 0, 0},
#if FELUCCA_KO
        {"KIT", F_ENUM, 0, 1, 0, N_ONOFF_E, 0},
#else
        {"-", F_INT, 0, 0, 0, 0, 0},
#endif
    },
    .presets = SMP_PRESET_TABLE,
    .npresets = SMP_NPRESETS,
    .note_on = sample_note_on,
    .render = sample_render,
    .knob = {P_E0, P_E4, P_ATK, P_REL},
    .sampled = 1,
#if FELUCCA_KO
    .keys = sample_ko_keys,
    .amp = sample_ko_amp,
#endif
};
