/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define USB_SAMPLE_POOL
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define FM1_INLINE static inline __attribute__((always_inline))
#include "../firmware/src/usb_sample_rx.h"
enum {S_INDEX=14,S_RXCSR1=20,S_RXCOUNT1=22};
static uint32_t regs[32],result,stuck,enabled=1,syncs;
static uint32_t fm1_usb_sie_on(void){return enabled;}
static void fm1_usb_sie_rd_start(uint32_t r){result=regs[r];}
static void fm1_usb_sie_wr_start(uint32_t r,uint32_t v){regs[r]=v;result=v;}
static uint32_t fm1_usb_sie_done(void){return !stuck;}
static uint32_t fm1_usb_sie_data(void){return result;}
static void fm1_usb_rx_sync(void){syncs++;}
#include "../firmware/src/usb_sample_flash.h"
int main(void){
 usb_sample_reset(1);assert(usb_sample_begin());
 for(uint32_t j=0;j<200;j++){
  regs[S_RXCSR1]=1;regs[S_RXCOUNT1]=46;
  for(uint32_t i=0;i<23;i++){usb_sample_packet[i*2]=(uint8_t)i;usb_sample_packet[i*2+1]=0;}
  usb_sample_flash_poll();assert(!usb_sample.fault);
  for(uint32_t i=0;i<23;i++)assert(usb_sample_pcm[(usb_sample.read+i)&2047]==(int16_t)i);
  usb_sample.read+=23;assert(regs[S_RXCSR1]==0x11);
 }
 assert(syncs==200 && usb_sample.write==4600);
 regs[S_RXCSR1]=0;usb_sample_flash_poll();assert(usb_sample.write==4600);
 regs[S_RXCSR1]=5;usb_sample_flash_poll();assert(usb_sample.fault==4);
 assert(usb_sample_begin());stuck=1;usb_sample_flash_poll();assert(usb_sample.fault==4 && !usb_sample.armed);
 assert(usb_sample_begin());stuck=0;enabled=0;usb_sample_flash_poll();assert(!usb_sample.fault && usb_sample.write==0);
 puts("RAM USB reception: packet order, wrap, FIFO overrun, bounded SIE timeout and idle PASS");
}
