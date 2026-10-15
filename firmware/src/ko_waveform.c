/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* PIGEON waveform cache. Decoder/envelope algorithm adapted from Felucca's
 * ui_slice.c; no legacy editor state or page navigation is included. */
static struct {
    uint8_t env_src;
    uint32_t env_a,env_b,env_len,env_gen;
    int8_t lo[KO_WAVE_COLS],hi[KO_WAVE_COLS];
} ko_wave_cache;
static void ko_wave_peaks(uint32_t a, uint32_t b)
{
    uint32_t src=ko_src(), c, e, len;
    const slc_src_t *s = slc_get(src);
    slc_dec_t d;
    len = s ? s->len : 0u;
    if (!s || (ko_wave_cache.env_a == a && ko_wave_cache.env_b == b && ko_wave_cache.env_len == len && ko_wave_cache.env_src == src &&
               ko_wave_cache.env_gen == smp_user_gen))
        return;
#if FELUCCA_KO_STORAGE
    if (ko_compressed(s)) {
        const ko_bank_hdr_t *bank=s==&slc_usr[2] ? &ko_bank_second : &ko_bank;
        for (c = 0; c < KO_WAVE_COLS; c++) {
            uint32_t start = a+(c*(b-a))/KO_WAVE_COLS, end = a+((c+1u)*(b-a))/KO_WAVE_COLS;
            uint32_t q0 = (uint32_t)(((uint64_t)start*bank->wave_bins)/len);
            uint32_t q1 = (uint32_t)(((uint64_t)end*bank->wave_bins)/len);
            int32_t lo = 127, hi = -128;
            if (q1 >= bank->wave_bins) q1 = bank->wave_bins-1;
            for (uint32_t q = q0; q <= q1; q++) {
                int32_t l = (int8_t)ko_bank_byte(bank->wave_off+q*2), h = (int8_t)ko_bank_byte(bank->wave_off+q*2+1);
                lo = l < lo ? l : lo; hi = h > hi ? h : hi;
            }
            ko_wave_cache.lo[c] = (int8_t)clamp(lo,-64,63); ko_wave_cache.hi[c] = (int8_t)clamp(hi,-64,63);
        }
    } else
#endif
    {
    slc_dec_at(&d, a, slc_state_at(s, a));
    for (c = 0; c < KO_WAVE_COLS; c++) {
        int32_t lo = 32767, hi = -32768;
        e = a + (uint32_t)((uint32_t)(c + 1u) * (b - a) / KO_WAVE_COLS);
        do {                                         /* at least one sample per column */
            int32_t x = d.pos < len ? slc_dec_next(s, &d) : 0;
            lo = x < lo ? x : lo;
            hi = x > hi ? x : hi;
        } while (d.pos < e);
        ko_wave_cache.lo[c] = (int8_t)(lo >> 9);
        ko_wave_cache.hi[c] = (int8_t)(hi >> 9);
    }
    }
    ko_wave_cache.env_a = a;
    ko_wave_cache.env_b = b;
    ko_wave_cache.env_len = len;
    ko_wave_cache.env_gen = smp_user_gen;
    ko_wave_cache.env_src = (uint8_t)src;
}

