/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MIDI IN on the TRS jack: PH8 -> input channel 1 -> UART1 RX, 31250 baud,
 * RX DMA into a 128-byte ring, polled from the TIMER5 ISR (no UART IRQ).
 * Built only with FELUCCA_UART=1. Channel messages go into midi_in_q next to
 * USB; realtime, system common and SysEx are dropped. */
#define UT1_CON0   (*(volatile uint32_t *)0x12100u)
#define UT1_CON1   (*(volatile uint32_t *)0x12104u)
#define UT1_BAUD   (*(volatile uint32_t *)0x12108u)
#define UT1_OTCNT  (*(volatile uint32_t *)0x12110u)
#define UT1_RXSADR (*(volatile uint32_t *)0x1211Cu)
#define UT1_RXEADR (*(volatile uint32_t *)0x12120u)
#define UT1_RXCNT  (*(volatile uint32_t *)0x12124u)
#define UT1_HRXCNT (*(volatile uint32_t *)0x12128u)
#define UM_CLK_CON1  (*(volatile uint32_t *)0x10010u)   /* [11:10] UART clock: 1 = PLL48M */
#define UM_IOMAP2    (*(volatile uint32_t *)0x51024u)   /* input ch1 source [13:8] */
#define UM_IOMAP3    (*(volatile uint32_t *)0x51028u)   /* UT1: b7 fixed IO, [6:4] RX select */
#define UM_PH_DIR    (*(volatile uint32_t *)0x501C8u)
#define UM_PH_DIE    (*(volatile uint32_t *)0x501CCu)
#define UM_PH_PU     (*(volatile uint32_t *)0x501D0u)
#define UM_PH_PD     (*(volatile uint32_t *)0x501D4u)

#define UM_RING 128u
static volatile uint8_t um_ring[UM_RING] __attribute__((aligned(16)));
static struct {
    uint32_t rd, pend, bytes, drops, msgs;
    uint8_t st, need, got, d0, sysex;
} um;

static void uart_midi_init(void)                   /* before timer5_start(): PORTH RMW */
{
    UT1_CON0 = 0x3400u;                            /* off, pendings cleared */
    UT1_CON1 = 0;
    UM_CLK_CON1 = (UM_CLK_CON1 & ~(3u << 10)) | (1u << 10);
    UM_IOMAP3 &= ~0x80u;
    UM_IOMAP3 = (UM_IOMAP3 & ~0x70u) | (5u << 4);   /* UT1 RX = input ch 1 */
    UM_IOMAP2 = (UM_IOMAP2 & ~0x3F00u) | (49u << 8);/* ch 1 = PH8 */
    UM_PH_PD &= ~(1u << 8);
    UM_PH_DIE |= 1u << 8;
    UM_PH_DIR |= 1u << 8;
    UM_PH_PU |= 1u << 8;
    UT1_RXSADR = (uint32_t)(uintptr_t)um_ring;
    UT1_RXEADR = (uint32_t)(uintptr_t)um_ring + UM_RING;
    UT1_RXCNT = UM_RING;
    UT1_BAUD = 48000000u / 31250u / 4u - 1u;       /* 383 */
    UT1_OTCNT = 60000u;
    __asm__ volatile("csync" ::: "memory");
    UT1_CON0 = 0x40u | 0x80u | 0x1000u | 0x400u;   /* RXDMA, RDC, clear R/OT; no IEs */
    __asm__ volatile("csync" ::: "memory");
    UT1_CON0 |= 1u;                                /* UTEN last */
}

static uint32_t um_len(uint32_t s)                 /* data bytes of a channel status */
{
    return (s & 0xE0u) == 0xC0u ? 1u : 2u;         /* Cx Dx: 1, else 2 */
}

static void um_byte(uint32_t b)
{
    if (b >= 0xF8u)
        return;                                    /* realtime: ignored, state kept */
    if (b & 0x80u) {
        um.sysex = b == 0xF0u;
        um.st = b < 0xF0u ? (uint8_t)b : 0;        /* system common/SysEx cancel running status */
        um.need = (uint8_t)um_len(b);
        um.got = 0;
        return;
    }
    if (um.sysex || !um.st)
        return;
    if (um.need == 2u && !um.got) {
        um.d0 = (uint8_t)b;
        um.got = 1;
        return;
    }
    {
        uint32_t d1 = um.need == 2u ? um.d0 : b, d2 = um.need == 2u ? b : 0u;
        uint32_t pkt = (um.st >> 4) | (uint32_t)um.st << 8 | d1 << 16 | d2 << 24;
        um.got = 0;
        if (mi_w - mi_r < MQ) {
            midi_in_q[mi_w % MQ] = pkt;
            RING_PUBLISH();
            mi_w++;
            um.msgs++;
        } else {
            um.drops++;
        }
    }
}

static void uart_midi_poll(void)                   /* TIMER5 ISR, same context as usb_poll */
{
    UT1_CON0 |= 0x80u;                             /* RDC: latch the count */
    __asm__ volatile("csync" ::: "memory");
    UT1_CON0 |= 0x1400u;                           /* clear RPND / OTPND */
    um.pend += UT1_HRXCNT & 0xFFFFu;
    if (um.pend > UM_RING) {
        um.drops += um.pend - UM_RING;
        um.rd = (um.rd + um.pend - UM_RING) & (UM_RING - 1u);
        um.pend = UM_RING;
    }
    while (um.pend) {
        um_byte(um_ring[um.rd]);
        um.rd = (um.rd + 1u) & (UM_RING - 1u);
        um.pend--;
        um.bytes++;
    }
}
