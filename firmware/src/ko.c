/* SPDX-License-Identifier: GPL-3.0-only */
/* Main-loop edits; audio reads immutable decoder checkpoints. */
static const ko_comp_t KO_COMP_DEFAULT = {12, 1, 10, 10, 0}; /* dB, ratio, ms, 10ms, dB */
static const ko_table_t *ko_table(void) { return &ko.table[ko.current]; }
static ko_table_t *ko_edit(void)
{
    ko_table_t *d = &ko.table[ko.current ^ 1u];
    *d = *ko_table(); return d;
}
static void ko_publish(int dirty)
{
    __asm__ volatile("" ::: "memory");
    ko.current ^= 1u; ko.revision++;
    if (dirty) ko.dirty = 1;
}
static void ko_auto(void)
{
    const slc_src_t *s = slc_get(1);
    ko_table_t *d = ko_edit(); memset(d, 0, sizeof *d);
    if (s) {
        d->n = s->nauto < KO_CHOPS ? s->nauto : KO_CHOPS;
        for (uint32_t j = 0; j < d->n; j++) {
            d->chop[j].a = s->apos[j];
            d->chop[j].b = j + 1u < d->n ? s->apos[j + 1u] : s->len;
            d->chop[j].st = s->ast[j];
        }
    }
    ko.selected = ko.bank = 0; ko.cursor = 0; ko_publish(0);
}
static int ko_restore(const ko_table_t *in)
{
    const slc_src_t *s = slc_get(1);
    if (!s || !in->n || in->n > KO_CHOPS) return 0;
    for (uint32_t j = 0; j < in->n; j++) {
        const ko_chop_t *c = &in->chop[j];
        if (c->a >= c->b || c->b > s->len || c->b - c->a < SLC_MIN || c->pitch < -24 || c->pitch > 24 ||
            (c->rsv & ~(KO_NO_LOOP | KO_LATCH)) || (j && c->a < in->chop[j - 1u].a)) return 0;
    }
    ko_table_t *d = ko_edit(); *d = *in;
    for (uint32_t j = 0; j < d->n; j++) d->chop[j].st = slc_state_at(s, d->chop[j].a);
    ko_publish(0); return 1;
}
static void ko_prepare(void)
{
    uint32_t sc = usr_nz[0] ? ((const smp_user_hdr_t *)smp_user_xip(0))->crc : 0;
    if (smp_ko_pair_requested()) {
        const smp_user_hdr_t *b = (const smp_user_hdr_t *)smp_user_xip(2);
        sc ^= smp_ko_pair_valid() ? b->crc * 16777619u ^ b->rsv2[1] : 0xFFFFFFFFu;
    }
    uint32_t kc = usr_nz[1] ? ((const smp_user_hdr_t *)smp_user_xip(1))->crc : 0;
#if FELUCCA_KO_STORAGE
    if (ko_bank_active) {
        sc = ko_bank_song_crc; kc = ko_bank_kit_crc;
    }
#endif
    if (ko.loaded && ko.generation == smp_user_gen) return;
    if (!ko.loaded || ko.song_crc != sc || (!usr_nz[0] && ko_table()->n)) {
        ko_auto(); ko.comp[0] = KO_COMP_DEFAULT; ko.dirty = 0;
    }
    if (!ko.loaded || ko.kit_crc != kc) ko.comp[1] = KO_COMP_DEFAULT;
    ko.song_crc = sc; ko.kit_crc = kc; ko.generation = smp_user_gen; ko.loaded = 1;
    if (ko_load_hook && !ko.dirty) ko_load_hook();
}
static void ko_select(uint32_t j)
{
    uint32_t n = ko_table()->n;
    ko.selected = (uint8_t)(n ? (j < n ? j : n - 1u) : 0);
    ko.bank = ko.selected / KO_PADS; ko.cursor = n ? ko_table()->chop[ko.selected].a : 0;
    ko.revision++;
}
static void ko_move_cursor(int32_t clicks)
{
    const slc_src_t *s = slc_get(1);
    if (!s || !ko_table()->n) return;
    uint32_t quantum = (((s->rate >> 8) * 44100u) >> 8) / 100u;
    ko.cursor = (uint32_t)clamp((int32_t)ko.cursor + clamp(clicks, -1000, 1000) * (int32_t)(quantum ? quantum : 1),
                              0, (int32_t)s->len - 1);
    ko.revision++;                                  /* scrolling never selects a different chop */
}
static void ko_chop_turn(uint32_t knob, int32_t clicks)
{
    const slc_src_t *s = slc_get(1);
    if (!s || !ko_table()->n) return;
    if (knob == 2) { ko_move_cursor(clicks); return; }
    if (knob == 3) {
        ko.zoom = (uint8_t)clamp((int32_t)ko.zoom + clamp(clicks, -6, 6), 0, 6);
        ko.revision++; return;
    }
    ko_table_t *d = ko_edit(); uint32_t j = ko.selected; ko_chop_t *c = &d->chop[j];
    int32_t quantum = (int32_t)((((s->rate >> 8) * 44100u) >> 8) / 100u);
    int32_t delta = clamp(clicks, -1000, 1000) * (quantum ? quantum : 1);
    if (knob == 0) {
        int32_t lo = j ? (int32_t)d->chop[j - 1u].a : 0, hi = (int32_t)c->b - SLC_MIN;
        if (j + 1u < d->n && hi > (int32_t)d->chop[j + 1u].a) hi = (int32_t)d->chop[j + 1u].a;
        c->a = (uint32_t)clamp((int32_t)c->a + delta, lo, hi); c->st = slc_state_at(s, c->a);
    } else if (knob == 1)
        c->b = (uint32_t)clamp((int32_t)c->b + delta, (int32_t)c->a + SLC_MIN, (int32_t)s->len);
    ko_publish(1);
}
static void ko_pitch_turn(int32_t steps)
{
    if (!ko_table()->n) return;
    ko_table_t *d = ko_edit();
    ko_chop_t *c = &d->chop[ko.selected];
    c->pitch = (int16_t)clamp(c->pitch + clamp(steps, -48, 48), -24, 24);
    ko_publish(1);
}
static void ko_flag_turn(uint32_t flag, int32_t steps)
{
    if (!ko_table()->n || !steps) return;
    ko_table_t *d = ko_edit();
    ko_chop_t *c = &d->chop[ko.selected];
    int enabled = steps > 0;
    if (flag == KO_NO_LOOP) enabled = !enabled;
    c->rsv = enabled ? c->rsv | flag : c->rsv & ~flag;
    ko_publish(1);
}
static int ko_split(void)
{
    if (!ko_table()->n || ko_table()->n == KO_CHOPS) return 0;
    ko_table_t *d = ko_edit(); uint32_t j = ko.selected; ko_chop_t *c = &d->chop[j]; uint32_t at = ko.cursor;
    if (c->b - c->a < 2u * SLC_MIN) return 0;
    if (at < c->a + SLC_MIN || at > c->b - SLC_MIN) at = c->a + (c->b - c->a) / 2u;
    if (j + 1u < d->n && at > d->chop[j + 1u].a) return 0; /* retain start order across overlaps */
    for (uint32_t k = d->n; k > j + 1u; k--) d->chop[k] = d->chop[k - 1u];
    d->chop[j + 1u] = *c; d->chop[j + 1u].a = at;
    d->chop[j + 1u].st = slc_state_at(slc_get(1), at); c->b = at; d->n++;
    ko_publish(1); ko_select(j + 1u); return 1;
}
static int ko_join(void)
{
    uint32_t j = ko.selected;
    if (!j || !ko_table()->n) return 0;
    ko_table_t *d = ko_edit();
    if (d->chop[j - 1u].b < d->chop[j].b) d->chop[j - 1u].b = d->chop[j].b;
    for (uint32_t k = j; k + 1u < d->n; k++) d->chop[k] = d->chop[k + 1u];
    d->n--; ko_publish(1); ko_select(j - 1u); return 1;
}
static int ko_comp_valid(const ko_comp_t *c)
{
    return c->threshold <= 48 && c->ratio >= 1 && c->ratio <= 8 && c->attack >= 1 && c->attack <= 100 &&
           c->release >= 1 && c->release <= 50 && c->makeup <= 12;
}
static struct { int32_t envelope, gain; } ko_cr[2];
/* log2(x), Q8, x is a positive Q8 ratio >= 1. Interpolate sixteen mantissa intervals. */
static uint32_t ko_log2(uint32_t x)
{
    static const uint16_t log[17] = {0,22,44,63,82,100,118,134,150,165,179,193,207,219,232,244,256};
    uint32_t oct = 0;
    while (x >= 512u) { x >>= 1; oct++; }
    uint32_t i = (x - 256u) >> 4, f = x & 15u;
    return oct * 256u + log[i] + ((log[i + 1u] - log[i]) * f >> 4);
}
/* Linked mid/side peak compression before pan/sends. Smooth gain and exact bypass. */
static __attribute__((noinline)) void ko_compress(track_t *t, int32_t *b, int32_t *side, uint32_t n)
{
    if (!n) return;
    uint32_t part = t == &trk[0] ? 0u : 1u;
    if (!ko_song(t) && !ko_kit(t)) return;
    ko_comp_t c = ko.comp[part];
    if (!ko_comp_valid(&c) || (c.ratio == 1 && !c.makeup)) {
        ko_cr[part].gain = 32768; ko_cr[part].envelope = 0; return;
    }
    int32_t peak = 0;
    for (uint32_t i = 0; i < n; i++) {
        int32_t x = clamp(b[i], -1048575, 1048575); x = x < 0 ? -x : x;
        if (x > peak) peak = x;
        if (side) {
            x = clamp(side[i], -1048575, 1048575); x = x < 0 ? -x : x;
            if (x > peak) peak = x;
        }
    }
    int32_t env = ko_cr[part].envelope; uint32_t rel = FS * c.release / 100u;
    if (peak > env) env = peak;
    else env -= (env - peak) * (int32_t)n / (int32_t)(rel > n ? rel : n);
    int32_t threshold = (int32_t)pow2_q16(-(int32_t)c.threshold * 32) >> 1, target = 32768;
    if (env > threshold) {
        uint32_t above = ko_log2((uint32_t)env * 256u / (uint32_t)threshold);
        int32_t attenuation = (int32_t)(above * 192u * (c.ratio - 1u) / (256u * c.ratio));
        target = (int32_t)(pow2_q16(-attenuation) >> 1); /* conventional ratio in dB */
    }
    int32_t gain = ko_cr[part].gain ? ko_cr[part].gain : 32768;
    uint32_t tau = target < gain ? FS * c.attack / 1000u : rel;
    int32_t next = gain + (target - gain) * (int32_t)n / (int32_t)(tau > n ? tau : n);
    int32_t makeup = (int32_t)(pow2_q16(c.makeup * 32) >> 4), gq = gain * 1024;
    int32_t slope = (next - gain) * 1024 / (int32_t)n; /* one divide per block */
    for (uint32_t i = 0; i < n; i++) {
        gq += slope;
        int32_t q12 = (gq >> 13) * makeup >> 12;
        b[i] = ((clamp(b[i], -1048575, 1048575) >> 4) * q12) >> 8;
        if (side) side[i] = ((clamp(side[i], -1048575, 1048575) >> 4) * q12) >> 8;
    }
    ko_cr[part].envelope = env; ko_cr[part].gain = next;
}
