/* SPDX-License-Identifier: GPL-3.0-only */
/* Real LCD code with deferred DMA reads and a synthetic wall clock. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define FELUCCA_KO_STORAGE 1
#define FM1_TICKS_PER_US 1u
static uint32_t ticks, polls, last_poll, max_gap, max_dma;
static const uint8_t *pending;
static uint32_t pending_n, pixels, written;
static uint8_t captured[240*240*2];
static uint32_t fm1_ticks(void) { return ticks; }
static void fm1_lcd_wait(void)
{
    if (pending) {
        if (pixels) {
            assert(written+pending_n<=sizeof captured);
            memcpy(captured+written,pending,pending_n);written+=pending_n;
        }
        ticks+=pending_n;pending=0;pending_n=0;
    }
}
static void fm1_lcd_send_cmd(uint8_t c) { assert(!pending);pixels=c==0x2c;ticks++; }
static void fm1_lcd_send_data(const void *p,uint32_t n)
{ assert(!pending);pending=p;pending_n=n;if (n>max_dma) max_dma=n; }
static void fm1_lcd_deselect(void) {}
static void fm1_lcd_hw_init(void) {}
static void fm1_lcd_baud(uint32_t b) { (void)b; }
static void fm1_lcd_backlight(uint32_t b) { (void)b; }
static void ko_pcm_poll(void)
{
    if (polls && ticks-last_poll>max_gap) max_gap=ticks-last_poll;
    last_poll=ticks;polls++;ticks+=1500; /* decoder time, not free service */
}
#include "../firmware/src/lcd.c"
int main(void)
{
    lcd_audio_enabled=1;
    lcd_fill(0,0,240,240,0x1234);lcd_sync();
    assert(written==sizeof captured && max_dma<=480 && polls>20 && max_gap<4000);
    for (uint32_t i=0;i<written;i+=2) assert(captured[i]==0x12 && captured[i+1]==0x34);
    uint16_t image[73*97];
    for (uint32_t i=0;i<73*97;i++) image[i]=(uint16_t)(i*137u);
    written=0;max_dma=0;
    lcd_blit(5,9,73,97,image);lcd_sync();
    assert(written==sizeof image && !memcmp(captured,image,sizeof image) && max_dma<=480);
    uint32_t before=polls;
    lcd_audio_enabled=0;written=0;
    lcd_fill(0,0,240,240,0);lcd_sync();assert(polls==before);
    lcd_audio_enabled=1;lcd_audio_last=UINT32_MAX-500;ticks=700;
    lcd_audio_service();assert(polls==before+1);
    puts("LCD: pixel order, deferred DMA, bounded refill gaps, disabled hook and timer wrap PASSED");
    return 0;
}
