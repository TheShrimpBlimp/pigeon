/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* ST7789-class 240x240 panel on SPI1: PC9 CLK, PC10 DO, PC7 CS, PC8 D/C,
 * backlight PA2 active low. Polled DMA transfers; every source buffer must be
 * in RAM. A pixel transfer is left running (lcd_busy): the next LCD access, or
 * a write to its source buffer (lcd_sync), waits for it. */
#define LCD_REG(a) (*(volatile uint32_t *)(a))
#define LCD_PC_OUT LCD_REG(0x50080)
#define LCD_PC_DIR LCD_REG(0x50088)
#define LCD_IOMAP_CON1 LCD_REG(0x51020)
#define LCD_SPI_CON LCD_REG(0x11D00)
#define LCD_SPI_BAUD LCD_REG(0x11D04)
#define LCD_SPI_BUF LCD_REG(0x11D08)
#define LCD_SPI_ADR LCD_REG(0x11D0C)
#define LCD_SPI_CNT LCD_REG(0x11D10)
#define LCD_PA_OUT LCD_REG(0x50000)
#define LCD_PA_DIR LCD_REG(0x50008)
#define LCD_BL (1u << 2)
#define LCD_CS (1u << 7)
#define LCD_DC (1u << 8)
#define LCD_CLK (1u << 9)
#define LCD_DO (1u << 10)
#ifndef LCD_BAUD
#define LCD_BAUD 4u                /* lsb/(BAUD+1): 4 = 12 MHz */
#endif

static uint8_t lcd_small[64];
static uint32_t lcd_timeouts;
static uint8_t lcd_busy;           /* a lcd_data DMA may still run; CS is low */

static void lcd_spin(uint32_t n)
{
    for (volatile uint32_t i = 0; i < n; i++)
        ;
}

static void lcd_wait(void)
{
    uint32_t n;
    for (n = 0; n < 4000000u && !(LCD_SPI_CON & 0x8000u); n++)
        ;
    if (n == 4000000u)
        lcd_timeouts++;
    LCD_SPI_CON |= 0x4000u;
}

static void lcd_sync(void)          /* finish the running transfer (before touching its buffer) */
{
    if (!lcd_busy)
        return;
    lcd_wait();
    LCD_PC_OUT |= LCD_CS;
    lcd_busy = 0;
}

static void lcd_cmd(uint8_t c)
{
    lcd_sync();
    LCD_PC_OUT &= ~LCD_DC;
    LCD_PC_OUT &= ~LCD_CS;
    LCD_SPI_CON |= 0x4000u;
    LCD_SPI_BUF = c;
    lcd_wait();
    LCD_PC_OUT |= LCD_CS;
}

static void lcd_data(const void *p, uint32_t n)
{
    if (!n)
        return;                    /* SPI_CNT = 0 never completes */
    lcd_sync();
    LCD_PC_OUT |= LCD_DC;
    LCD_PC_OUT &= ~LCD_CS;
    LCD_SPI_CON |= 0x4000u;
    LCD_SPI_ADR = (uint32_t)(uintptr_t)p;
    LCD_SPI_CNT = n;
    lcd_busy = 1;                  /* completed by lcd_sync */
}

static void lcd_window(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    lcd_sync();
    lcd_small[0] = (uint8_t)(x0 >> 8);
    lcd_small[1] = (uint8_t)x0;
    lcd_small[2] = (uint8_t)(x1 >> 8);
    lcd_small[3] = (uint8_t)x1;
    lcd_cmd(0x2A);
    lcd_data(lcd_small, 4);
    lcd_small[4] = (uint8_t)(y0 >> 8);       /* own bytes: the x transfer may still read [0..3] */
    lcd_small[5] = (uint8_t)y0;
    lcd_small[6] = (uint8_t)(y1 >> 8);
    lcd_small[7] = (uint8_t)y1;
    lcd_cmd(0x2B);
    lcd_data(lcd_small + 4, 4);
    lcd_cmd(0x2C);
}

static uint16_t lcd_fillbuf[240];

static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, n, k, sw = (uint16_t)((c >> 8) | (c << 8));
    if (!w || !h || x >= 240u || y >= 240u)
        return;
    if (x + w > 240u)
        w = 240u - x;
    if (y + h > 240u)
        h = 240u - y;
    lcd_sync();
    k = 240u / w * w;                       /* whole rows per transfer (narrow fills: one DMA) */
    for (i = 0; i < k; i++)
        lcd_fillbuf[i] = (uint16_t)sw;
    lcd_window(x, y, x + w - 1u, y + h - 1u);
    for (n = w * h; n; n -= k) {
        if (k > n)
            k = n;
        lcd_data(lcd_fillbuf, k * 2u);
    }
}

static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    if (!w || !h)
        return;
    lcd_window(x, y, x + w - 1u, y + h - 1u);
    lcd_data(px, w * h * 2u);
}

/* ST7789V init: reset, then 16-bit colour, scan direction and IPS inversion;
 * voltage and gamma stay at the defaults. Format: cmd, n, n data bytes;
 * cmd 0x00 = wait (data byte: ~ms). */
static const uint8_t LCD_SEQ[] = {
    0x01, 0,                     /* SWRESET */
    0x00, 1, 150,
    0x11, 0,                     /* SLPOUT */
    0x00, 1, 120,
    0x3A, 1, 0x55,               /* COLMOD: RGB565 */
    0x36, 1, 0x00,               /* MADCTL: top-left origin, RGB order */
    0x21, 0,                     /* INVON: IPS panel */
    0x13, 0,                     /* NORON */
};

static void lcd_init(void)
{
    uint32_t r, x;
    LCD_PA_OUT &= ~LCD_BL;
    LCD_PA_DIR &= ~LCD_BL;
    LCD_IOMAP_CON1 |= 0x10u;
    LCD_PC_OUT |= LCD_CS;
    LCD_PC_OUT &= ~(LCD_DC | LCD_CLK | LCD_DO);
    LCD_PC_DIR &= ~(LCD_CS | LCD_DC | LCD_CLK | LCD_DO);
    LCD_SPI_CON = 0x4021u;
    LCD_SPI_BAUD = 4u;
    lcd_spin(2000000u);
    for (r = 0; r < sizeof LCD_SEQ; r += 2u + LCD_SEQ[r + 1u]) {
        if (LCD_SEQ[r] == 0x00u) {                       /* pseudo command: wait */
            lcd_spin(LCD_SEQ[r + 2u] * 25000u);          /* ~1 ms per unit (lcd_spin(3000000) ~ 120 ms) */
            continue;
        }
        lcd_cmd(LCD_SEQ[r]);
        for (x = 0; x < LCD_SEQ[r + 1u]; x++)
            lcd_small[x] = LCD_SEQ[r + 2u + x];
        if (LCD_SEQ[r + 1u])
            lcd_data(lcd_small, LCD_SEQ[r + 1u]);
    }
    LCD_SPI_BAUD = LCD_BAUD;
    lcd_fill(0, 0, 240, 240, 0);
    lcd_cmd(0x29);
}
