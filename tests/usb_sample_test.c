/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define USB_SAMPLE_POOL
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define HALF_FRAMES 128
#define FELUCCA_OTA 0
#define FELUCCA_CDC 0
#define FELUCCA_UAC 1
#define FELUCCA_USB_SAMPLE 1
static void fm1_delay_ms(uint32_t ms){(void)ms;}
#include "../firmware/src/usb.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 CHECK(usb_sample_ack(0x4105)==0x4011); /* retain ISO, clear errors, flush consumed OUT */
 const uint8_t *p=CFG_DESC_PLAIN;uint32_t n=sizeof CFG_DESC_PLAIN,at=0,rx=0,ac=0,fmt=0;
 CHECK((p[2]|p[3]<<8)==n);CHECK(p[4]==4);
 while(at<n){uint32_t length=p[at];CHECK(length>=2 && length<=n-at);
  if(p[at+1]==5 && p[at+2]==4){CHECK(p[at+3]==9 && p[at+4]==46);rx++;}
  if(p[at+1]==0x24 && p[at+2]==1 && length==11){CHECK(p[at+7]==3 && p[at+8]==2 && p[at+9]==3 && p[at+10]==1);ac++;}
  if(p[at+1]==0x24 && p[at+2]==2 && length==11){CHECK(p[at+4]==1 && p[at+5]==2 && p[at+6]==16 && (p[at+8]|p[at+9]<<8|p[at+10]<<16)==22050);fmt++;}
  at+=length;
 }
 CHECK(at==n && rx==1 && ac==1 && fmt==1);
 uint8_t packet[46];for(uint32_t i=0;i<23;i++){packet[i*2]=(uint8_t)i;packet[i*2+1]=0;}
 CHECK(!usb_sample_begin());usb_sample_reset(1);CHECK(usb_sample_begin());
 for(uint32_t j=0;j<200;j++){usb_sample_take(packet,46);CHECK(!usb_sample.fault);for(uint32_t i=0;i<23;i++)CHECK(usb_sample_pcm[(usb_sample.read+i)&(USB_SAMPLE_N-1)]==i);usb_sample.read+=23;}
 CHECK(usb_sample.write==4600 && usb_sample.frames==4600);
 usb_sample_take(packet,3);CHECK(usb_sample.fault);CHECK(usb_sample_begin());
 for(uint32_t j=0;j<100;j++)usb_sample_take(packet,46);
 CHECK(usb_sample.fault && !usb_sample.armed && usb_sample.write-usb_sample.read<=USB_SAMPLE_N);
 CHECK(usb_sample_begin());usb_sample_reset(0);CHECK(usb_sample.fault && !usb_sample.armed);
 puts("USB sampling: descriptors, mono PCM, ring wrapping, overrun, malformed packets and disconnect PASS");return 0;
}
