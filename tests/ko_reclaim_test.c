/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_KO_MAINT 1
#define KO_USB_TEST_MAIN original_usb_test_main
#include "ko_usb_sample_test.c"
#undef KO_USB_TEST_MAIN
static uint32_t maint_fail;
static int maint_erase(uint32_t off){
 if(off>=0xE0000u && off<0xE5000u){memset(bank_flash+off,255,4096);return 0;}
 return st_erase(off);
}
static int maint_write(uint32_t off,const uint8_t *p,uint32_t n){
 if(maint_fail && !(off>=0xE0000 && off<0xE5000))return -1;
 if(off>=0xE0000 && off+n<=0xE5000){for(uint32_t i=0;i<n;i++){if((bank_flash[off+i]&p[i])!=p[i])return -1;bank_flash[off+i]&=p[i];}return 0;}
 return st_prog(off,p,n);
}
#define KM_READ(o,p,n) memcpy(p,bank_flash+(o),n)
#define KM_WRITE(o,p,n) maint_write(o,(const uint8_t *)(p),n)
#define KM_ERASE(o) maint_erase(o)
#include "../firmware/src/ko_reclaim.c"
#include "../firmware/src/ko_collect.c"
int main(void){
 if(original_usb_test_main())return 1;int bad=0;
 uint32_t first=ko_bank_song_crc,second=ko_bank_second_crc,kit=ko_bank_kit_crc,used=ko_bank.used;
 maint_fail=1;bad+=check("reclaim write failure enters read-only recovery",!ko_reclaim() && ko_reclaim_locked());
 uint8_t *backup=malloc(KO_BANK_SIZE);ko_bank_read(0,backup,KO_BANK_SIZE);ko_bank_hdr_t *h=(ko_bank_hdr_t *)backup;
 bad+=check("interrupted reclaim still exports complete CRC-valid logical bank",ko_bank_header_valid(h) && st_crc32(backup+4096,h->used-4096)==h->crc && h->used<used);
 free(backup);maint_fail=0;bad+=check("reboot resumes interrupted compaction",ko_reclaim_boot()==1 && !ko_reclaim_locked());
 ko_bank_scan();ko_prepare();bad+=check("compaction preserves both songs and kit without re-encoding",ko_bank_active && ko_bank.used<used && ko_bank_song_crc==first && ko_bank_second_crc==second && ko_bank_kit_crc==kit);
 bad+=check("repeat reclaim is a harmless no-op",ko_reclaim() && ko_bank_active);
 /* Recording options: silence is discarded before the threshold, limits count only kept samples. */
 ko_rec_options.threshold=20;ko_rec_options.seconds=1;ko_rec_options.bars=0;song.sel=0;usb_sample_reset(1);ko_usb_start();
 uint8_t silence[46]={0};for(uint32_t i=0;i<20;i++){usb_sample_take(silence,46);ko_usb_poll();}
 bad+=check("threshold wait does not consume recording duration",ko_usb.state==1 && !ko_usb.samples && !ko_usb.triggered);
 feed(2000);while(ko_usb.state==2)ko_usb_poll();bad+=check("fixed seconds stop exactly after threshold trigger",ko_usb.state==4 && ko_usb.samples==22050);ko_usb_discard();
 ko_rec_options.threshold=0;ko_rec_options.seconds=0;ko_rec_options.bars=2;song.g[G_BPM]=120;usb_sample_reset(1);ko_usb_start();bad+=check("bar recording derives duration from tempo",ko_usb.limit==88200);ko_usb_discard();ko_rec_options.bars=0;
 /* Record to one kit pad, preserving other pads and both songs. */
 uint32_t old_pad[16];for(uint32_t i=0;i<16;i++)old_pad[i]=ko_bank_crc(ko_bank.kit_off+ko_bank.zone[i].off,(ko_bank.zone[i].n+1u)/2u);
 song.sel=1;ko_kit_pad=3;ko_kit_params[1].level=77;usb_sample_reset(1);ko_usb_start();feed(1000);ko_usb_stop();while(ko_usb.state==2)ko_usb_poll();ko_usb.trim_a=101;ko_usb.trim_b=4102;keep_take();
 int pads_ok=1;for(uint32_t i=0;i<16;i++)if(i!=3)pads_ok &= old_pad[i]==ko_bank_crc(ko_bank.kit_off+ko_bank.zone[i].off,(ko_bank.zone[i].n+1u)/2u);
 bad+=check("pad recording preserves other sounds and their settings",ko_bank_active && ko_bank.zone[3].n==4001 && pads_ok && ko_kit_params[1].level==77 && ko_bank_song_crc==first && ko_bank_second_crc==second && song.sel==1 && ko.view==16);
 bad+=check("reclaim supports a kit appended after the songs",ko_reclaim() && ko_bank_active && ko_bank_song_crc==first && ko_bank_second_crc==second);
 /* Keep overlapping chops in place; removed keys become silent slots. */
 song.sel=0;ko_prepare();ko_table_t picks={.n=3};
 picks.chop[0]=(ko_chop_t){.a=100,.b=1100,.pitch=5,.reverse=1,.gain=160,.cents=3};
 picks.chop[1]=(ko_chop_t){.a=900,.b=1900,.pitch=-2};picks.chop[2]=(ko_chop_t){.a=3000,.b=4000};ko_restore(&picks);ko.dirty=1;
 ko_keep_target=0;ko_keep_mask[0]=3;bad+=check("on-device keep merges overlapping selected audio",ko_keep_apply() && ko_bank.samples==1864 && ko_table()->n==3 && ko_table()->chop[2].pad && ko_table()->chop[0].pitch==5 && ko_table()->chop[0].reverse==1 && ko_table()->chop[0].gain==160);
 st_erase(SMP_USER_BASE+SLC_REC_OFF);ko_states[0].loaded=ko_states[0].dirty=0;ko_prepare();
 bad+=check("collection settings survive loss of the ordinary metadata copy",ko_table()->n==3 && ko_table()->chop[2].pad && ko_table()->chop[0].reverse==1 && ko_table()->chop[0].gain==160);
 song.sel=2;ko_prepare();ko_table_t incoming={.n=1,.chop={{.a=500,.b=1500,.pitch=-7,.gain=120}}};ko_restore(&incoming);ko.dirty=1;
 ko_keep_target=0;ko_keep_mask[1]=1;uint32_t source_crc=ko_bank_second_crc;
 bad+=check("cross-song collection fills empty keys and preserves the source",ko_keep_apply() && ko_bank.samples==2864 && ko_table()->n==3 && !ko_table()->chop[2].pad && ko_table()->chop[2].pitch==-7 && ko_bank_second_crc==source_crc);
 printf("Integrated sample reclaim: %s\n",bad?"FAIL":"PASS");return !!bad;
}
