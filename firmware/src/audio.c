/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* I2S output (ALNK0 -> external codec) and the audio ISR: each half buffer is
 * rendered in blocks of CTL samples by mix_block (fx.c: events -> each synth part
 * -> dist -> level / pan -> sends -> drums -> buses -> master), then scaled to 24-bit stereo. */
#define ALNK 0x12E00u
#define A_CON0 (*(volatile uint16_t *)(ALNK + 0x00u))
#define A_CON1 (*(volatile uint16_t *)(ALNK + 0x04u))
#define A_CON2 (*(volatile uint8_t *)(ALNK + 0x08u))
#define A_CON3 (*(volatile uint8_t *)(ALNK + 0x0Cu))
#define A_ADR3 (*(volatile uint32_t *)(ALNK + 0x1Cu))
#define A_LEN (*(volatile uint16_t *)(ALNK + 0x20u))
#define CLK_CON2 (*(volatile uint32_t *)0x10014u)
#define IOMAP_CON5 (*(volatile uint32_t *)0x51030u)
#define HALF_WORDS (HALF_FRAMES * 2u)
#define OUT_SHIFT 7               /* Q15 -> 24-bit, -6 dBFS ceiling */

static int32_t abuf[2u * HALF_WORDS] __attribute__((aligned(4)));

/* diagnostics, kept across resets and UBOOT entry: read with `fm1t memr` */
#define DBG_MAGIC 0x44424731u                       /* "DBG1" */
struct felucca_dbg {
    uint32_t magic, halves, max_us, nested, in_audio, late, timer_irqs, ui_frames;
    uint32_t last_us, cpu_q8, boots;
    uint32_t stage, page, home;           /* where the main loop is (breadcrumbs) */
    uint32_t prev_stage, prev_page, prev_home, prev_rst, prev_frames;   /* as found at boot */
} felucca_dbg __attribute__((section(".noinit")));
static volatile uint32_t audio_halves, audio_max_us;
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N];
static uint32_t scope_w;

static void pc_out(uint32_t bit, int v)
{
    uint32_t m = 1u << bit;
    FM1_PR(FM1_PC, FM1_DIE) |= m;
    if (v)
        FM1_PR(FM1_PC, FM1_OUT) |= m;
    else
        FM1_PR(FM1_PC, FM1_OUT) &= ~m;
    FM1_PR(FM1_PC, FM1_DIR) &= ~m;
}

static void audio_block(int32_t *out, uint32_t n)       /* mix (fx.c), then Q15 -> 24 bit */
{
    uint32_t i;
    mix_block(out, n);
    for (i = 0; i < n; i++) {
        if (i & 1u)
            scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)out[2u * i];
        out[2u * i] <<= OUT_SHIFT;
        out[2u * i + 1u] <<= OUT_SHIFT;
    }
}

/* overload: a half that took > 85 % of its time sheds one voice before the
 * next one, over all parts: the quietest releasing voice fades out over the next
 * block (voice_kill), else the oldest held one goes into its release (stopped by
 * a later shed if still needed). The only held voice is never touched, so a dense
 * chord on a heavy engine thins out instead of starving the CPU. */
static volatile uint8_t shed_req;
static uint32_t shed_count;

static void shed_voice(void)
{
    uint32_t p, i, ngate = 0;
    voice_t *best = 0;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[p].v[i];
            if (v->active && !v->gate && v->stage != 4u && (!best || v->env < best->env))
                best = v;
        }
    if (best) {
        voice_kill(best);
        shed_count++;
        return;
    }
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[p].v[i];
            if (v->active && v->gate) {
                ngate++;
                if (!best || v->age < best->age)
                    best = v;
            }
        }
    if (ngate > 1u) {
        best->gate = 0;
        best->stage = 3;
        shed_count++;
    }
}

void fm1_alnk0_irq(void)                       /* via isr_alnk0 (hal/fm1_isr.S) */
{
    uint8_t p = A_CON2;
    uint32_t t0 = fm1_ticks();
    if (p & 0x10u)
        A_CON2 |= 1u;
    if (p & 0x20u)
        A_CON2 |= 2u;
    if (p & 0x40u)
        A_CON2 |= 4u;
    felucca_dbg.in_audio = 1;
    if (p & 0x80u) {
        uint32_t half = ((A_CON0 >> 15) & 1u) ^ 1u, b, us;
        int32_t *o = &abuf[half * HALF_WORDS];
        if (shed_req) {
            shed_req = 0;
            shed_voice();
        }
        for (b = 0; b < HALF_FRAMES; b += CTL)
            audio_block(o + 2u * b, CTL);
        A_CON2 |= 0x08u;
        audio_halves++;
        us = (fm1_ticks() - t0) / FM1_TICKS_PER_US;
        if (us > audio_max_us)
            audio_max_us = us;
        if (us * 100u > (HALF_FRAMES * 1000000u / FS) * 85u)
            shed_req = 1;
        song.cpu_q8 = (song.cpu_q8 * 15u + (us * 256u) / (HALF_FRAMES * 1000000u / FS)) / 16u;
        if ((((A_CON0 >> 15) & 1u) ^ 1u) != half)
            felucca_dbg.late++;                         /* the DMA moved on while we rendered */
        felucca_dbg.halves++;
        felucca_dbg.last_us = us;
        if (us > felucca_dbg.max_us)
            felucca_dbg.max_us = us;
        felucca_dbg.cpu_q8 = song.cpu_q8;
    }
    felucca_dbg.in_audio = 0;
}
extern void isr_alnk0(void);

static void audio_init(void)
{
    uint32_t i;
    for (i = 0; i < 2u * HALF_WORDS; i++)
        abuf[i] = 0;
    pc_out(6, 0);
    fm1_delay_ms(5);
    A_CON0 = 0;
    A_CON1 = 0;
    A_CON2 = 0;
    A_CON3 = 0;
    IOMAP_CON5 &= ~0xC0u;
    A_CON3 |= 3u;
    pc_out(0, 1);
    A_CON0 |= 0x100u;
    pc_out(2, 1);
    pc_out(1, 1);
    A_CON0 |= 0x80u;
    A_CON0 &= ~0x40u;
    A_LEN = HALF_WORDS;
    A_CON0 &= ~0x400u;
    A_CON0 &= ~0x200u;
    CLK_CON2 &= 0xFFFFF0FFu;
    A_CON3 &= ~0x1Cu;
    A_CON3 = (uint8_t)((A_CON3 & 0x1Fu) | 0x80u);
    A_CON1 |= 1u << 12;
    A_CON1 |= 1u << 14;
    A_ADR3 = (uint32_t)(uintptr_t)abuf;
    pc_out(6, 1);
    A_CON1 &= ~(1u << 15);
    A_CON2 = 0x0Fu;
    fm1_irq_attach(FM1_IRQ_ALNK0, isr_alnk0, 3);
    A_CON0 |= 0x800u;
    fm1_delay_ms(5);
    pc_out(6, 1);
}

/* MASTER knob: SARADC ch4 on PB6, read from the UI loop */
#define ADC_CON (*(volatile uint32_t *)0x13100u)
#define ADC_RES (*(volatile uint32_t *)0x13104u)
#define WLA_CON0 (*(volatile uint32_t *)0x11900u)

/* PB6 = MASTER pot (ch4), PB1 = battery divider (ch3). Both are only ever
 * analog inputs: PB1 held low for 8 s would reset the chip (isd_config). */
static void adc_init(void)
{
    uint32_t m = (1u << 6) | (1u << 1);
    FM1_PR(FM1_PB, FM1_DIE) &= ~m;
    FM1_PR(FM1_PB, FM1_PU) &= ~m;
    FM1_PR(FM1_PB, FM1_PD) &= ~m;
    FM1_PR(FM1_PB, FM1_DIR) |= m;
}

static int32_t adc_read(uint32_t ch)
{
    uint32_t t, v;
    ADC_CON = 0;
    if (WLA_CON0 & (1u << 14))
        WLA_CON0 &= ~(1u << 14);
    ADC_CON = 0xF04Eu | ((ch & 0xFu) << 8);
    ADC_CON |= 0x10u;
    ADC_CON |= 0x40u;
    for (t = 0; t < 20000u; t++)
        if (ADC_CON & 0x80u)
            break;
    v = ADC_RES & 0x3FFu;
    ADC_CON = 0x40u;
    ADC_CON = 0;
    return t == 20000u ? -1 : (int32_t)v;
}
