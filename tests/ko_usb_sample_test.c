/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include <stdint.h>
static uint32_t usb_text_clipped;static int usb_lint;
static void usb_text_check(const char *text,uint32_t flags){
 if(usb_lint && !(flags&4u) && (flags&3u) && !(flags&8u)){fprintf(stderr,"USB screen clipped: %s\n",text);usb_text_clipped++;}
}
#define GFX_HOOK_TEXT(x0,y0,x1,y1,s,flags) usb_text_check(s,flags)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_BLIT(x,y,r0) ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define FELUCCA_KO_SCRATCH 0
#define FELUCCA_USB_SAMPLE 1
#define USB_SAMPLE_ARM() usb_sample_begin()
#define USB_SAMPLE_POOL
#define FELUCCA_UAC 1
#define KO_STORAGE_TEST_MAIN unused_storage_main
#include "ko_storage_test.c"
#include "../firmware/src/ko_usb_sample.c"
static void feed(uint32_t packets){
 uint8_t data[46];for(uint32_t i=0;i<23;i++){int16_t n=(int16_t)((i%8)*3000-10000);data[i*2]=(uint8_t)n;data[i*2+1]=(uint8_t)(n>>8);}
 while(packets-- && ko_usb.state==1){usb_sample_take(data,sizeof data);ko_usb_poll();}
}
static void keep_take(void){ko_usb_save();ko_usb_save();}
#ifndef KO_USB_TEST_MAIN
#define KO_USB_TEST_MAIN main
#endif
int KO_USB_TEST_MAIN(void){
 int bad=0;memset(bank_flash,255,sizeof bank_flash);ui_power_on();slc_store_boot();ko.on=1;usb.config=1;usb.suspended=0;
 ko.view=33;fm1_ms=100;usb_sample.peak=32768;ko_usb_meter();
 bad+=check("input meter reports full scale and latches clipping",ko_usb.level==100 && ko_usb.clipped && !usb_sample.peak);
 fm1_ms+=100;ko_usb_meter();bad+=check("meter falls to silence while clip warning remains",!ko_usb.level && ko_usb.clipped);
 bad+=check("idle recorder reports available seconds",ko_usb_remaining()>0 && ko_usb_remaining()<60);
 song.sel=2;usb_sample_reset(1);ko_usb_start();bad+=check("empty bank cannot silently redirect Song 2 to Song 1",!ko_usb.state && !usb_sample.armed);
 song.sel=0;usb_sample_reset(1);ko_usb_start();bad+=check("USB recording arms without an existing bank",ko_usb.state==1 && usb_sample.armed);
 bad+=check("preparation skips already blank sectors",erases==0);
 bad+=check("new take clears clipping and input peak",!ko_usb.clipped && !usb_sample.peak);
 feed(1000);ko_usb_stop();while(ko_usb.state==2)ko_usb_poll();
 bad+=check("USB capture drains and prepares a bounded ADPCM take",ko_usb.state==4 && ko_usb.samples==23000 && ko_usb.written==11500);
 uint32_t ready_erases=erases;ko_usb_start();
 bad+=check("record cannot overwrite an unsaved take",ko_usb.state==4 && ko_usb.samples==23000 && ko_usb.written==11500 && erases==ready_erases);
 ko.view=33;page_tap(B_ENV);bad+=check("retry asks before discarding take",ko_usb.state==6 && ko_usb.samples==23000 && erases==ready_erases);
 page_tap(B_LFO);bad+=check("cancel retry retains ready take",ko_usb.state==4 && ko_usb.samples==23000);
 int16_t heard[256];ko_usb_audition();
 for(uint32_t frame=0;frame<512;frame+=CTL){memset(mix_l,0,sizeof mix_l);memset(mix_r,0,sizeof mix_r);ko_usb_audio(CTL);for(uint32_t i=0;i<CTL;i+=2)heard[(frame+i)/2]=(int16_t)mix_l[i];}
 bad+=check("audition leaves committed bank untouched",!ko_bank_active && ko_usb.state==4 && ko_usb.preview);
 ko_usb_audition();bad+=check("play toggles unsaved audition off",!ko_usb.preview);
 ko_usb_audition();while(ko_usb.preview)ko_usb_audio(CTL);
 bad+=check("audition ends at exact take boundary",ko_usb.preview==0 && ko_usb.preview_pred>=-32768 && ko_usb.preview_pred<=32767);
 ko_usb_save();bad+=check("first keep asks before writing header",ko_usb.state==5 && !ko_bank_active && erases==ready_erases);
 ko.view=33;page_tap(B_LFO);bad+=check("cancel confirmation retains ready take",ko_usb.state==4 && ko_usb.samples==23000);
 keep_take();bad+=check("USB take saves as a validated first song",!ko_usb.state && ko_bank_active && ko_bank.version==2 && ko_bank.samples==23000 && ko_bank.codec==KO_CODEC_IMA);
 int16_t decoded[256];const slc_src_t *source=slc_get(1);int32_t peak=0;
 if(source){slc_fill(source,decoded,0,256,0,source->grid[0]);for(uint32_t i=0;i<256;i++){int32_t x=decoded[i];if(x<0)x=-x;if(x>peak)peak=x;}}
 bad+=check("recorded ADPCM decodes to audible PCM through the real player",source && source->len==23000 && peak>1000);
 slc_dec_t reference;slc_dec_at(&reference,0,0);int audition_exact=1;
 if(source)for(uint32_t i=0;i<256;i++)audition_exact &= heard[i]==slc_dec_next(source,&reference);
 bad+=check("unsaved audition matches saved IMA decoder",source && audition_exact);
 bad+=check("saving opens recorded waveform",ko.view==0);
 uint32_t songcrc=ko_bank_song_crc;ko_bank_hdr_t original=ko_bank;
 usb_sample_reset(1);ko_usb_start();feed(200);ko_usb_discard();
 bad+=check("cancelled take leaves committed header and audio intact",ko_bank_active && !memcmp(&original,&ko_bank,sizeof original) && ko_bank_crc(ko_bank.song_off,ko_bank.song_len)==songcrc);
 ko_bank_scan();bad+=check("cancelled take survives reboot",ko_bank_active && ko_bank.version==2 && ko_bank.samples==23000);
 song.sel=2;usb_sample_reset(1);ko_usb_start();feed(300);ko_usb_stop();while(ko_usb.state==2)ko_usb_poll();keep_take();
 bad+=check("second USB take replaces only song 2",ko_bank_active && ko_bank.version==3 && ko_bank.samples==23000 && ko_bank_second.samples==6900 && ko_bank_song_crc==songcrc);
 uint32_t second_crc=ko_bank_second_crc;ko_bank_hdr_t second_header=ko_bank_second;
 song.sel=0;usb_sample_reset(1);ko_usb_start();feed(400);ko_usb_stop();while(ko_usb.state==2)ko_usb_poll();
 song.sel=2;keep_take();
 bad+=check("save uses and opens the destination captured at record start",ko_bank.samples==9200 && ko_bank_second.samples==6900 && song.sel==0 && ko.view==0);
 bad+=check("replacing Song 1 preserves Song 2 audio and range",ko_bank_active && ko_bank_second_crc==second_crc && ko_bank_second.song_off==second_header.song_off && ko_bank.song_off>ko_bank_second.wave_off);
 ko_bank_scan();bad+=check("Song 1 replacement survives bank scan",ko_bank_active && ko_bank.samples==9200 && ko_bank_second.samples==6900);
 FILE *saved=fopen("build/v05-checks/usb-selected-bank.bin","wb");if(saved){for(uint32_t i=0;i<ko_bank.used;i++)fputc(ko_bank_byte(i),saved);fclose(saved);}
 songcrc=ko_bank_song_crc;
 song.sel=1;usb_sample_reset(1);ko_usb_start();bad+=check("drum track records into the selected pad",ko_usb.state==1 && ko_usb.target==2 && ko_usb.pad==ko_kit_pad);ko_usb_discard();song.sel=0;
 usb_sample_reset(1);ko_usb_start();uint8_t data[46]={0};for(uint32_t i=0;i<100;i++)usb_sample_take(data,sizeof data);ko_usb_poll();
 bad+=check("overrun rejects take instead of saving gaps",ko_usb.state==3 && ko_bank_active);
 uint32_t failed_erases=erases;ko_usb_start();bad+=check("failed take needs explicit discard before retry",ko_usb.state==3 && erases==failed_erases);ko_usb_discard();
 usb_sample_reset(1);ko_usb_start();feed(100);usb_sample_reset(0);ko_usb_poll();bad+=check("disconnect rejects a partial take",ko_usb.state==3);ko_usb_discard();
 usb_sample_reset(1);ko_usb_start();feed(5);ko_usb_stop();while(ko_usb.state==2)ko_usb_poll();
 bad+=check("odd-length take retains exact sample count",ko_usb.samples==115 && ko_usb.written==58 && ko_usb.state==4);
 ko.view=33;page_tap(B_ENV);page_tap(B_ENV);bad+=check("confirmed retry starts a new take without committing old take",ko_usb.state==1 && ko_usb.samples==0 && ko_bank_song_crc==songcrc);ko_usb_discard();
 usb_sample_reset(1);ko_usb_start();ko_usb.capacity=256;feed(100);
 while(ko_usb.state==2)ko_usb_poll();bad+=check("capacity stops without writing past reserved audio",ko_usb.state==4 && ko_usb.samples==512 && ko_usb.written==256);ko_usb_discard();
 usb_sample_reset(1);ko_usb_start();fail_write=1;feed(100);fail_write=0;
 bad+=check("program failure rejects take and preserves original header",ko_usb.state==3 && ko_bank_active);ko_usb_discard();
 uint32_t prior_used=ko_bank.used,prior_erases=erases;ko_bank.used=KO_BANK_SIZE-8192u;
 bank_flash[ko_usb_phys(KO_BANK_SIZE-8192u)]=0;bank_flash[ko_usb_phys(KO_BANK_SIZE-4096u)+4095u]=0;
 usb_sample_reset(1);ko_usb_start();
 bad+=check("near-full bank reserves waveform without erasing past pool",ko_usb.state==1 && ko_usb.capacity==7168u && erases-prior_erases==2u);
 feed(1000);while(ko_usb.state==2)ko_usb_poll();
 bad+=check("near-full capture stops with waveform space intact",ko_usb.state==4 && ko_usb.samples==14336u && ko_usb.start+ko_usb.written+1024u<=KO_BANK_SIZE);
 ko_usb_discard();ko_bank.used=prior_used;
 ui.home=1;ui.page=0;ui.menu=0;song.sel=0;ko.view=5;page_tap(B_FX);pigeon_ui_guard();
 bad+=check("USB sampler is reachable and survives the UI guard",ko.view==33);
 page_tap(B_ENV);bad+=check("sampler envelope button starts a take",ko_usb.state==1);page_tap(B_ENV);while(ko_usb.state==2)ko_usb_poll();page_tap(B_LFO);page_tap(B_FX);
 bad+=check("sampler discard and exit return to chop playback",!ko_usb.state && ko.view==5);
 song.playing=song.rec=0;for(uint32_t k=0;k<SAMPLER_PARTS;k++)trk_all_off(&trk[k]);
 ko_setup();int32_t screen_audio[2*CTL];for(uint32_t i=0;i<32;i++)mix_block(screen_audio,CTL);ui.msg_t=0;ko.view=33;usb_lint=1;
 bad+=check("USB screen regression renders the active sampler",ko_ready());
 uint32_t repeated=0,flashing=0,heading_ok=1;
 uint16_t title_pixels[240*AF_S.h];
 for(uint32_t state=0;state<=6;state++){
  ko_usb.state=state;ko_usb.samples=1000000;ko_usb.error=1;ui.force=1;ko_ui_invalidate();ko_draw();
  host_blit_rows=0;host_fl_begin();ui.force=1;ko_draw();
  flashing+=host_fl_end(26,240);repeated+=host_blit_rows>H_HEAD;
  memcpy(title_pixels,host_screen+49*240,sizeof title_pixels);
  draw_text_line(8,49,224,&AF_S,"USB SAMPLER",T_MID,T_BG,0);
  heading_ok&=!memcmp(title_pixels,host_screen+49*240,sizeof title_pixels);
 }
 bad+=check("idle recorder refresh never overwrites its own status text",!flashing && !repeated);
 bad+=check("recorder destination never erases the heading",heading_ok);
 usb_lint=0;ko_usb_discard();bad+=check("all USB sampler states fit the device screen",!usb_text_clipped);
 bad+=check("all recording writes remain in sample pool",!bad_flash);
 uint8_t *kit_fixture=0;uint32_t kit_bytes=0;
 bad+=check("dual-song kit fixture loads",!load("build/ko-fixtures/dual.kob",&kit_fixture,&kit_bytes) && !upload(kit_fixture,kit_bytes));free(kit_fixture);
 ko_bank_hdr_t kit_header=ko_bank;uint32_t kit_crc=ko_bank_kit_crc,other_crc=ko_bank_second_crc;
 song.sel=0;usb_sample_reset(1);ko_usb_start();feed(100);ko_usb_stop();while(ko_usb.state==2)ko_usb_poll();keep_take();
 bad+=check("Song 1 recording preserves all kit zones and kit audio",ko_bank_active && ko_bank.kit_n==16 && ko_bank_kit_crc==kit_crc && !memcmp(ko_bank.zone,kit_header.zone,sizeof ko_bank.zone));
 bad+=check("Song 1 recording preserves fixture Song 2",ko_bank_active && ko_bank_second_crc==other_crc);
 saved=fopen("build/v05-checks/usb-selected-bank.bin","wb");if(saved){for(uint32_t i=0;i<ko_bank.used;i++)fputc(ko_bank_byte(i),saved);fclose(saved);}
 const uint32_t ranges[][2]={{1001,19434},{1,8193},{33,97},{0,8193},{60000,64096}};
 for(uint32_t test=0;test<5;test++){
  song.sel=0;usb_sample_reset(1);ko_usb_start();
  uint8_t packet[46];uint32_t tick=0;
  for(uint32_t j=0;j<3000;j++){
   for(uint32_t i=0;i<23;i++){int16_t v=(int16_t)(12000*sin(tick++*.07));packet[i*2]=(uint8_t)v;packet[i*2+1]=(uint8_t)(v>>8);}
   usb_sample_take(packet,46);ko_usb_poll();
  }
  ko_usb_stop();while(ko_usb.state==2)ko_usb_poll();
  uint32_t a=ranges[test][0],b=ranges[test][1],count=b-a,full_used=(ko_usb.start+ko_usb.written+255u)&~255u;
  slc_src_t raw={.len=ko_usb.samples,.nseg=1,.seg={{.off=KO_VOFF+ko_usb.start,.n=ko_usb.samples}}};
  slc_dec_t rd;slc_dec_at(&rd,0,0);int16_t *reference=malloc(count*sizeof *reference);
  for(uint32_t i=0;i<b;i++){int32_t v=slc_dec_next(&raw,&rd);if(i>=a)reference[i-a]=(int16_t)v;}
  ko_usb_trim_turn(0,5);ko_usb_trim_turn(2,1);ko_usb_trim_turn(1,-1);ko_usb_trim_turn(3,-1);
  bad+=check("fine and coarse trim controls adjust both boundaries",ko_usb.trim_a==2315 && ko_usb.trim_b==66773);
  ko_usb_trim_turn(0,100000);ko_usb_trim_turn(1,-100000);
  bad+=check("trim controls cannot cross or produce a too-short take",ko_usb.trim_b-ko_usb.trim_a==64);
  ko_usb.trim_a=a;ko_usb.trim_b=b;
  ko_usb_audition();memset(mix_l,0,sizeof mix_l);memset(mix_r,0,sizeof mix_r);ko_usb_audio(CTL);
  int match=1;for(uint32_t i=0;i<CTL && i<count*2;i++)match &= mix_l[i]==reference[i/2];
  bad+=check("trim audition starts at selected sample with correct decoder state",match);
  ko_usb_audition();uint32_t old_erases=erases;ko_usb_save();ko.view=33;page_tap(B_LFO);
  bad+=check("cancelled trim confirmation preserves complete take and bounds",ko_usb.state==4 && ko_usb.samples==69000 && ko_usb.trim_a==a && ko_usb.trim_b==b && erases==old_erases);
  keep_take();
  bad+=check("trim saves exact length and reclaims unselected tail",ko_bank_active && ko_bank.samples==count && ko_bank.song_len==(count+1u)/2u && ko_bank.used<full_used && ko_bank_second_crc==other_crc && ko_bank_kit_crc==kit_crc);
  slc_dec_t saved_dec;slc_dec_at(&saved_dec,0,0);const slc_src_t *trimmed=slc_get(1);int64_t error=0,power=0;
  if(trimmed)for(uint32_t i=0;i<count;i++){int32_t v=slc_dec_next(trimmed,&saved_dec);if(i>=32){int64_t d=v-reference[i];error+=d*d;power+=(int64_t)reference[i]*reference[i];}}
  bad+=check("trimmed ADPCM retains selected audio without missing sectors",trimmed && power && error*16<power);free(reference);
  ko_bank_scan();bad+=check("trimmed bank survives reboot with waveform and kit intact",ko_bank_active && ko_bank.wave_bins==(count+2047u)/2048u && ko_bank.kit_n==16);
 }
 song.sel=0;usb_sample_reset(1);ko_usb_start();feed(1000);ko_usb_stop();while(ko_usb.state==2)ko_usb_poll();
 ko_usb.trim_a=101;ko_usb.trim_b=20000;ko_bank_hdr_t before_failure=ko_bank;
 fail_write=1;keep_take();fail_write=0;
 bad+=check("trim write failure never commits a partial take",ko_usb.state==3 && !memcmp(&before_failure,&ko_bank,sizeof ko_bank));
 ko_usb_discard();ko_bank_scan();bad+=check("committed bank survives trim write failure",ko_bank_active && ko_bank_kit_crc==kit_crc && ko_bank_second_crc==other_crc);
 printf("USB sample persistence: %s\n",bad?"FAIL":"PASS");return !!bad;
}
