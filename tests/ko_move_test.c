/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include <assert.h>
#define KO_BANK_SIZE (40u*4096u)
static uint8_t flash[0x100000],initial[0x100000],expected[KO_BANK_SIZE],buffer[4096],new_header[4096];
static uint32_t operations,cut;static jmp_buf power;
static uint32_t crc32(const void *p,uint32_t n){const uint8_t *s=p;uint32_t c=~0u;while(n--){c^=*s++;for(uint32_t i=0;i<8;i++)c=(c>>1)^(0xEDB88320u&(0u-(c&1)));}return ~c;}
static int erase(uint32_t off){assert(!(off&4095) && off+4096<=sizeof flash);operations++;if(operations==cut){memset(flash+off,255,2048);longjmp(power,1);}memset(flash+off,255,4096);return 0;}
static int write_flash(uint32_t off,const void *data,uint32_t n){const uint8_t *p=data;assert(off+n<=sizeof flash);operations++;for(uint32_t i=0;i<n;i++){if(operations==cut && i==n/2)longjmp(power,1);assert((flash[off+i]&p[i])==p[i]);flash[off+i]&=p[i];}return 0;}
#define KM_POOL
#define KM_READ(o,p,n) memcpy(p,flash+(o),n)
#define KM_WRITE(o,p,n) write_flash(o,p,n)
#define KM_ERASE(o) erase(o)
#define KM_BANK_READ(o,p,n) memcpy(p,flash+0xA0000u+(o),n)
#define KM_BANK_PHYS(o) (0xA0000u+(o))
#define KM_BUFFER buffer
#define KM_CRC(p,n) crc32(p,n)
#define KM_FEED() ((void)0)
#include "../firmware/src/ko_move.h"
static ko_move_record_t plan;
static void run(void){if(!ko_move_begin(&plan,new_header))abort();assert(ko_move_recover()==1);}
int main(void){
 memset(initial,255,sizeof initial);memset(&plan,0,sizeof plan);plan.count=3;
 plan.range[0]=(ko_move_range_t){8192,4096,10001};
 plan.range[1]=(ko_move_range_t){24576,14097,60003};
 plan.range[2]=(ko_move_range_t){98304,74100,4101};plan.used=78201;
 for(uint32_t i=4096;i<KO_BANK_SIZE;i++)initial[0xA0000+i]=(uint8_t)((i*73u+i/173u)%251u);
 memset(expected,255,sizeof expected);for(uint32_t i=0;i<4096;i++)new_header[i]=(uint8_t)(i*19u);
 memcpy(expected,new_header,4096);for(uint32_t i=0;i<3;i++)memcpy(expected+plan.range[i].dst,initial+0xA0000+plan.range[i].src,plan.range[i].len);
 plan.header_crc=crc32(new_header,4096);memcpy(flash,initial,sizeof flash);cut=0;run();uint32_t total=operations;
 assert(!memcmp(flash+0xA0000,expected,plan.used));assert(ko_move_recover()==0);
 for(uint32_t point=1;point<=total;point++){
  memcpy(flash,initial,sizeof flash);memset(&ko_move_record,0,sizeof ko_move_record);operations=0;cut=point;
  if(!setjmp(power))run();
  cut=0;assert(ko_move_recover()>=0);
  if(ko_move_load() && ko_move_record.stage==2)assert(!memcmp(flash+0xA0000,expected,plan.used));
  else assert(!memcmp(flash+0xA0000,initial+0xA0000,KO_BANK_SIZE));
 }
 printf("Bank rewrite: %u power-cut points, torn logs, redo replay, rotation and idempotence PASS\n",total);
}
