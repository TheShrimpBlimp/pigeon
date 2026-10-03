/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Chip voice: pulse/triangle/saw/noise, quantised amplitude and a held
 * (downsampled) output; SWEEP bends the pitch down after note-on. */
static const char *const N_CHIP[] = {"4BIT", "4B/2", "8BIT", "1BIT"};
static const char *const N_RWAVE[] = {"PLS", "TRI", "SAW", "NOIS"};
static const char *const N_RARP[] = {"OFF", "OCT", "MAJ", "MIN"};

static void lofi_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->s[0] = 0;                 /* held sample */
    v->s[1] = 0;                 /* hold counter */
    v->s[2] = 0x7FFF;            /* LFSR */
    v->s[3] = 0;                 /* sweep, 1/16 st */
    v->s[4] = 0;                 /* 1-pole lp state */
    v->s[5] = 0;                 /* arp counter */
}

static void lofi_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t chip = (uint32_t)p[P_E0], wave = (uint32_t)p[P_E1], i;
    uint32_t duty = 0x20000000u * (1u + (uint32_t)p[P_E2] / 32u);   /* 12.5 / 25 / 37.5 / 50 % */
    int32_t crush = p[P_E3];
    int32_t bits = chip == 0u ? 4 : chip == 1u ? 4 : chip == 2u ? 8 : 1;
    int32_t hold = 1 + crush / 8 + (chip == 1u ? 1 : 0);
    int32_t lpk = 3000 + (p[P_E7] << 8) + (m->cutoff > 0 ? m->cutoff : 0);
    static const int8_t ARPS[4][3] = {{0, 0, 0}, {0, 12, 0}, {0, 4, 7}, {0, 3, 7}};
    int32_t ar = p[P_E6];
    int32_t pitch, held = v->s[0], cnt = v->s[1], lp = v->s[4];   /* state in locals: out[] may alias v->s[] */
    uint32_t inc, ph0 = v->ph[0], lfsr = (uint32_t)v->s[2];
    /* sweep down, vibrato (uses the track LFO through m->pitch16 already) */
    if (v->s[3] < 4096)                                 /* past 2047 + an arp step the pitch is 0 anyway */
        v->s[3] += p[P_E4] / 8;
    v->s[5]++;
    pitch = m->pitch16 - v->s[3] + ARPS[ar & 3][(v->s[5] / 28) % 3] * 16;   /* ~50 Hz, chip-style */
    inc = PITCH_INC[clamp(pitch, 0, 2047)];
    if (p[P_E5])
        inc += (uint32_t)(((int32_t)(inc >> 12) * (((osc_sine(v->ph[1]) >> 8) * p[P_E5]) >> 4)) >> 4);   /* no overflow */
    v->ph[1] += 0x01000000u;
    if (lpk > 32767)
        lpk = 32767;
    for (i = 0; i < n; i++) {
        int32_t s;
        if (--cnt <= 0) {
            cnt = hold;
            switch (wave) {
            case 1:
                s = osc_tri(ph0);
                break;
            case 2:
                s = (int32_t)(ph0 >> 16) - 32768;
                break;
            case 3: {
                uint32_t l = lfsr;
                if ((ph0 + inc * (uint32_t)hold) < ph0 || chip == 3u)
                    l = (l >> 1) | (((l ^ (l >> 1)) & 1u) << 14);
                lfsr = l;
                s = (l & 1u) ? 32767 : -32768;
                break;
            }
            default:                                    /* minus its mean: no DC for narrow pulses */
                s = (ph0 < duty ? 32767 : -32768) - ((int32_t)(duty >> 16) - 32768);
                break;
            }
            if (bits < 16)                              /* rounded, not floored (floor = DC) */
                s = ((s + (1 << (15 - bits))) >> (16 - bits)) << (16 - bits);
            if (crush > 64)
                s = (s >> 12) << 12;
            held = s;
        }
        ph0 += inc;
        lp += mulq15(held - lp, lpk);
        out[i] += mulq15(mulq15(lp, amp_at(m, i)), VOICE_FS);
    }
    v->ph[0] = ph0;
    v->s[0] = held;
    v->s[1] = cnt;
    v->s[2] = (int32_t)lfsr;
    v->s[4] = lp;
}

static const preset_t LOFI_PRESETS[] = {
    {"PULSE LD", {0, 0, 32, 0, 0, 20, 0, 127}, {0, 60, 90, 30}, 0, 1, FX(0, 0, 40, 20), PAT(4)},
    {"WAVE BASS", {1, 1, 0, 0, 0, 0, 0, 90}, {0, 50, 70, 20}, 0, 1, FX(0, 0, 10, 0), PAT(2)},
    {"ARP 8BIT", {2, 0, 96, 0, 0, 0, 2, 110}, {0, 60, 80, 40}, 0, 0, FX(0, 0, 30, 20), ARP(1, 2, 2, 40)},
};

static const engine_t ENG_LOFI = {
    "LOFI", {"CHIP", "MOTN"},
    {
        {"CHIP", F_ENUM, 0, 3, 0, N_CHIP, 0},
        {"WAVE", F_ENUM, 0, 3, 0, N_RWAVE, 0},
        {"DUTY", F_INT, 0, 127, 64, 0, 0},
        {"CRSH", F_PCT, 0, 127, 0, 0, 0},
        {"SWP", F_PCT, 0, 127, 0, 0, 0},
        {"VIB", F_PCT, 0, 127, 0, 0, 0},
        {"ARP", F_ENUM, 0, 3, 0, N_RARP, 0},
        {"TONE", F_PCT, 0, 127, 127, 0, 0},
    },
    LOFI_PRESETS, sizeof(LOFI_PRESETS) / sizeof(LOFI_PRESETS[0]), 1, lofi_note_on, lofi_render,
    0x3F2C, {P_E1, P_E2, P_E3, P_REL},
};
