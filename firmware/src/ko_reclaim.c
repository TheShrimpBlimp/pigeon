/* SPDX-License-Identifier: GPL-3.0-only */
/* Stopped-only sample maintenance. No extra sector buffer: reuse the canvas. */
#ifndef KM_READ
#define KM_READ(off,p,n) memcpy(p,KO_BANK_XIP(off),n)
#define KM_WRITE(off,p,n) fl_write(off,(const uint8_t *)(p),n)
static int ko_move_erase(uint32_t off){uint32_t took;audio_silence();return fl_erase4k(off,&took);}
#define KM_ERASE(off) ko_move_erase(off)
#endif
#define KM_BUFFER ((uint8_t *)cv_px)
#define KM_BANK_READ(off,p,n) ko_bank_read_raw(off,p,n)
static uint32_t ko_move_physical(uint32_t off){uint32_t avail;return ko_bank_phys(off,&avail);}
#define KM_BANK_PHYS(off) ko_move_physical(off)
#define KM_CRC(p,n) st_crc32(p,n)
#define KM_FEED() fm1_wdt_feed()
#include "ko_move.h"
static uint8_t ko_reclaim_failed __attribute__((section(".pool")));
static int ko_reclaim_boot(void){
 int rc=ko_move_recover();ko_reclaim_failed=rc<0;return rc;
}
static int ko_reclaim_locked(void){return ko_reclaim_failed;}
/* A failed write leaves a complete logical export available for USB backup:
 * committed prefix, verified redo sector, then unmoved original ranges. */
static uint8_t ko_reclaim_byte(uint32_t off){
 if(off>=ko_move_record.used)return 255;
 if(off<4096)return KO_BANK_XIP(KM_BASE)[off];
 uint32_t sector=off/4096;
 if(ko_move_record.stage==1 && sector==ko_move_record.next)return KO_BANK_XIP(KM_SCRATCH)[off&4095];
 if(sector<ko_move_record.next){uint32_t avail;return KO_BANK_XIP(ko_bank_phys(off,&avail))[0];}
 for(uint32_t i=0;i<ko_move_record.count;i++){
  const ko_move_range_t *p=ko_move_record.range+i;
  if(off>=p->dst && off-p->dst<p->len){uint32_t avail;return KO_BANK_XIP(ko_bank_phys(p->src+off-p->dst,&avail))[0];}
 }
 return 255;
}
static int ko_reclaim(void){
 if(transport_busy() || song.rec || voices_busy() || ko_usb.state || ko_reclaim_failed || !ko_bank_active){ui_message("STOP / RESOLVE TAKE FIRST");return 0;}
 ko_line(130,"RECLAIMING SAMPLE SPACE",T_THEME);lcd_sync();ko_move_record_t plan;memset(&plan,0,sizeof plan);
 ko_bank_read_raw(0,KM_BUFFER,4096);ko_bank_hdr_t *h=(ko_bank_hdr_t *)KM_BUFFER;
 uint32_t *offset[5]={&h->kit_off,&h->song_off,&h->wave_off,0,0};
 uint32_t length[5]={h->kit_len,h->song_len,h->wave_bins*2u,0,0};
 if(h->version==3){offset[3]=&(h+1)->song_off;offset[4]=&(h+1)->wave_off;length[3]=(h+1)->song_len;length[4]=(h+1)->wave_bins*2u;}
 uint32_t order[5]={0,1,2,3,4};
 for(uint32_t i=0;i<5;i++)for(uint32_t j=i+1;j<5;j++)if(length[order[j]] && (!length[order[i]] || *offset[order[j]]<*offset[order[i]])){uint32_t k=order[i];order[i]=order[j];order[j]=k;}
 uint32_t used=4096,crc=~0u;
 for(uint32_t i=0;i<5 && length[order[i]];i++){
  uint32_t k=order[i],src=*offset[k],n=length[k];
  plan.range[plan.count++]=(ko_move_range_t){src,used,n};*offset[k]=used;used+=n;
  for(uint32_t j=0;j<n;j++){if(!(j&4095))fm1_wdt_feed();crc=ko_crc_add(crc,ko_bank_byte(src+j));}
 }
 for(uint32_t i=0;i<(h->version==3?2u:1u);i++){if(!h[i].kit_len)h[i].kit_off=KO_BANK_HEAD;h[i].used=used;h[i].crc=~crc;h[i].hcrc=st_crc32(h+i,sizeof *h-4);}
 plan.used=used;plan.header_crc=st_crc32(KM_BUFFER,4096);
 if(used==ko_bank.used){ui_message("NO SPACE TO RECLAIM");return 1;}
 if(!ko_move_begin(&plan,KM_BUFFER)){ko_reclaim_boot();ui_message("RECLAIM RETRY / USB BACKUP");return 0;}
 if(ko_reclaim_boot()<0){ui_message("RECOVERY / USB BACKUP");return 0;}
 ko_bank_scan();smp_user_gen++;ko_prepare();ui.force=1;ui_message("SAMPLE SPACE RECLAIMED");return 1;
}

/* Append-only saves need only a header transaction; payload is already durable. */
static int ko_reclaim_commit(const uint8_t *header,uint32_t used){
 lcd_sync();memcpy(KM_BUFFER,header,3840);
 ko_bank_read_raw(3840,(uint8_t *)KM_BUFFER+3840,256);
 ko_move_record_t plan;memset(&plan,0,sizeof plan);plan.count=1;plan.used=used;
 plan.range[0]=(ko_move_range_t){4096,4096,used-4096};plan.next=(used+4095u)/4096u;
 plan.header_crc=st_crc32(KM_BUFFER,4096);
 if(!ko_move_begin(&plan,KM_BUFFER)){ko_reclaim_boot();return 0;}
 return ko_reclaim_boot()>=0;
}
