/* SPDX-License-Identifier: GPL-3.0-only */
/* Real bank parser, upload handler, DSP and persistence against 1 MiB NOR. */
#include <stdint.h>
static uint8_t bank_flash[0x100000];
#define FELUCCA_KO 1
#define FELUCCA_OTA 1
#define FELUCCA_KO_STORAGE 1
#define KO_BANK_XIP(off) (bank_flash+(off))
#define SMP_USER_XIP(k) (bank_flash+0xA0000u+(k)*0x14000u)
#define UI_TEST_MAIN unused_ui_main
#include "ui_test.c"
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) { fm1_ms++; so_r=so_w; }
static uint8_t flash_ok=1,fail_write;
static uint32_t bad_flash,erases;
static int allowed(uint32_t off,uint32_t n)
{
    return off <= sizeof bank_flash && n <= sizeof bank_flash-off &&
        ((off>=0xA0000 && off+n<=0xDC000) || (off>=KO_APP_HI && off+n<=KO_APP_DATA_HI) ||
         (off>=KO_TOP_LO && off+n<=KO_TOP_HI));
}
static int st_read(uint32_t off,void *p,uint32_t n)
{ if (!allowed(off,n)) { bad_flash++; return -1; } memcpy(p,bank_flash+off,n); return 0; }
static int st_erase(uint32_t off)
{ if (!allowed(off,4096) || (off&4095)) { bad_flash++; return -1; } memset(bank_flash+off,255,4096); erases++; return 0; }
static int st_prog(uint32_t off,const void *p,uint32_t n)
{
    if (!allowed(off,n)) { bad_flash++; return -1; }
    if (fail_write) return -1;
    const uint8_t *b=p;
    for (uint32_t i=0;i<n;i++) { if ((bank_flash[off+i]&b[i])!=b[i]) return -1; bank_flash[off+i]&=b[i]; }
    return 0;
}
#include "../firmware/src/storage.c"
#include "../firmware/src/slice_store.c"
static void audio_silence(void) {}
static void fl_inval(uint32_t off,uint32_t n) { (void)off;(void)n; }
static int fl_erase4k(uint32_t off,uint32_t *took) { *took=0; return st_erase(off); }
static int fl_write(uint32_t off,const void *p,uint32_t n) { return st_prog(off,p,n); }
#include "../firmware/src/editor.c"

static uint32_t wire21(uint8_t *a,uint32_t v)
{ a[0]=v&127;a[1]=(v>>7)&127;a[2]=(v>>14)&127;return 3; }
static uint32_t packed(uint8_t *a,const uint8_t *p,uint32_t n)
{
    uint32_t at=0;
    for (uint32_t i=0;i<n;i+=7) {
        uint32_t k=n-i>7?7:n-i,mask=0;
        for (uint32_t j=0;j<k;j++) mask|=(p[i+j]>>7)<<j;
        a[at++]=(uint8_t)mask;for (uint32_t j=0;j<k;j++) a[at++]=p[i+j]&127;
    }
    return at;
}
static int request(uint32_t cmd,const uint8_t *a,uint32_t n)
{
    uint8_t f[640]={0x7D,0x46,0x4C,(uint8_t)cmd};
    memcpy(f+4,a,n); so_r=so_w; ed_handle(f,n+4);
    return ed_n>=7 ? ed_out[5] : -1;
}
static int write_page(uint32_t off,const uint8_t *p,uint32_t n)
{ uint8_t a[300];wire21(a,off);return request(92,a,3+packed(a+3,p,n)); }
static int load(const char *path,uint8_t **p,uint32_t *n)
{
    FILE *f=fopen(path,"rb");if (!f) return -1;
    fseek(f,0,SEEK_END);*n=(uint32_t)ftell(f);rewind(f);*p=malloc(*n);
    int rc=fread(*p,1,*n,f)!=*n;fclose(f);return rc;
}
static int upload(const uint8_t *p,uint32_t n)
{
    uint8_t a[3];wire21(a,n);
    if (request(91,a,3)) return -1;
    for (uint32_t off=KO_BANK_HEAD;off<n;off+=256)
        if (write_page(off,p+off,n-off>256?256:n-off)) return -2;
    for (uint32_t off=0;off<sizeof(ko_bank_hdr_t);off+=256)
        if (write_page(off,p+off,sizeof(ko_bank_hdr_t)-off>256?256:sizeof(ko_bank_hdr_t)-off)) return -3;
    return request(93,0,0);
}
static void pump(uint32_t n)
{
    int32_t out[CTL*2];
    for (uint32_t i=0;i<n;i++) { for(uint32_t v=0;v<NVOICE;v++) ko_pcm_poll();mix_block(out,CTL); }
}
int main(int argc,char **argv)
{
    if (argc!=3) return 2;
    uint8_t *mp3,*ima;uint32_t nm,ni;int bad=0;
    if (load(argv[1],&mp3,&nm)||load(argv[2],&ima,&ni)) return 2;
    memset(bank_flash,255,sizeof bank_flash); ui_power_on();song.master_q12=4096;slc_store_boot();ko.on=1;ko_setup();pump(32);
    uint8_t a[3];wire21(a,nm);
    uint32_t before=erases;
    bad+=check("bad bank begin never erases",request(91,a,2)==1&&erases==before);
    bad+=check("bank capacity negotiated without writes",request(90,0,0)==0x4B&&ed_out[6]==2&&erases==before);
    bad+=check("upload begins invalid and interrupted commit is refused",request(91,a,3)==0&&!ko_bank_active&&request(93,0,0)==2);
    bad+=check("out-of-order payload rejected",write_page(KO_BANK_HEAD+256,mp3+KO_BANK_HEAD,256)==1);
    bad+=check("full MP3 bank uploads with header last",upload(mp3,nm)==0&&ko_bank_active&&usr_nz[1]==16);
    bad+=check("one minute at 22.05k available",slc_get(1)&&slc_get(1)->len==60u*22050u);
    before=erases;uint8_t slot=0;
    bad+=check("legacy sample uploader cannot damage shared pool",request(11,&slot,1)==0&&ed_out[6]==6&&erases==before);
    ko.loaded=0;ko_prepare();
    ko_table_t table={0};table.n=16;
    for(uint32_t i=0;i<16;i++){table.chop[i].a=i*5000;table.chop[i].b=(i+1)*5000;}
    bad+=check("compressed chops accept editable sample bounds",ko_restore(&table));
    trk_note_on(&trk[0],60,100);pump(8);
    uint32_t errors=ko_pcm_errors,misses=ko_pcm_underruns;int64_t energy=0;
    for(uint32_t i=0;i<3000;i++){
        int32_t out[CTL*2];for(uint32_t v=0;v<NVOICE;v++)ko_pcm_poll();mix_block(out,CTL);
        for(uint32_t j=0;j<CTL*2;j++)energy+=out[j]<0?-(int64_t)out[j]:out[j];
    }
    bad+=check("compressed held loop produces audio without decoder errors",energy>100000&&ko_pcm_errors==errors);
    printf("  energy=%lld errors=%u engine=%u requested=%u active=%u gate=%u ph=%u state=%x\n",(long long)energy,ko_pcm_errors-errors,trk[0].engine,trk[0].eng_req,trk[0].v[0].active,trk[0].v[0].gate,trk[0].v[0].ph[0],(unsigned)trk[0].v[0].s[4]);
    printf("  cache misses during loops: %u source samples\n",ko_pcm_underruns-misses);
    bad+=check("held loop cache remains supplied",ko_pcm_underruns==misses);
    trk_all_off(&trk[0]);pump(200);
    for(uint32_t i=0;i<8;i++) table.chop[i].pitch=i&1?24:-24;
    ko_restore(&table);
    for(uint32_t i=0;i<8;i++) trk_note_on(&trk[0],60+i,100);
    misses=ko_pcm_underruns;errors=ko_pcm_errors;pump(1500);
    uint32_t active=0;for(uint32_t i=0;i<NVOICE;i++)active+=trk[0].v[i].active!=0;
    printf("  pitched voices=%u errors=%u cache misses=%u\n",active,ko_pcm_errors-errors,ko_pcm_underruns-misses);
    bad+=check("eight pitched loops share decoder without cache misses",active==8&&ko_pcm_underruns==misses&&ko_pcm_errors==errors);
    trk_all_off(&trk[0]);pump(200);
    table.chop[0].rsv=KO_NO_LOOP;table.chop[0].pitch=0;ko_restore(&table);
    trk_note_on(&trk[0],60,100);pump(500);
    active=0;for(uint32_t i=0;i<NVOICE;i++)active+=trk[0].v[i].active!=0;
    bad+=check("compressed one shot ends while key is held",!active);
    table.chop[0].rsv=KO_LATCH;ko_restore(&table);
    trk_note_on(&trk[0],60,100);trk_note_off(&trk[0],60);pump(500);
    active=0;for(uint32_t i=0;i<NVOICE;i++)active+=trk[0].v[i].active!=0;
    bad+=check("compressed latch survives release",active==1);
    trk_note_on(&trk[0],60,100);pump(500);
    active=0;for(uint32_t i=0;i<NVOICE;i++)active+=trk[0].v[i].active!=0;
    bad+=check("second press releases compressed latch",!active);
    trk_all_off(&trk[0]);pump(200);ko_select(3);ko_pitch_turn(12);ko_flag_turn(KO_LATCH,1);
    ko_prepare();bad+=check("chop edits persist outside shared payload",ko_store_sector(1)==3);
    ko_table_t saved=*ko_table();smp_user_scan(0);ko.loaded=0;ko_prepare();
    bad+=check("reboot restores pitch and latch",!memcmp(&saved,ko_table(),sizeof saved));
    for(uint32_t i=0;i<16;i++)trk_note_on(&trk[1],60+i,100);pump(100);
    bad+=check("sixteen kit zones retain exact pad notes",usr_nz[1]==16&&usr_zone[1][3].lo==63&&usr_zone[1][10].lo==70);
    ko.view=0;song.sel=0;go_home();frame();
    bad+=check("waveform drawing never invokes decoder",ko_pcm_errors==errors);
    project_t q;project_capture(&q);project_store_t stored;
    bad+=check("FUNA project packing remains compatible",proj_pack(&stored,&q)&&proj_import(&q,&stored,sizeof stored));
    uint8_t page[256],wire[300];memcpy(page,bank_flash+SMP_USER_BASE+SLC_REC_OFF,256);
    before=erases;wire21(wire,256);
    bad+=check("metadata upload rejects reordered pages without erase",request(95,wire,3+packed(wire+3,page,256))==1&&erases==before);
    static uint8_t metadata[4096];memcpy(metadata,bank_flash+SMP_USER_BASE+SLC_REC_OFF,sizeof metadata);
    uint32_t meta_ok=1;
    for(uint32_t off=0;off<sizeof metadata;off+=256){
        wire21(wire,off);meta_ok &= request(95,wire,3+packed(wire+3,metadata+off,256))==0;
    }
    ko_prepare();
    bad+=check("exported metadata restores editable chops and pitch",meta_ok&&!memcmp(&saved,ko_table(),sizeof saved));
    trk_all_off(&trk[0]);trk_all_off(&trk[1]);pump(200);
    bad+=check("ADPCM bank uses same flexible pool",upload(ima,ni)==0&&ko_bank.codec==KO_CODEC_IMA&&slc_get(1)->len==60u*11025u);
    ko.loaded=0;ko_prepare();trk_note_on(&trk[0],60,100);pump(200);
    bad+=check("virtual ADPCM retains playback",trk[0].v[0].active||voices_busy());
    uint8_t original=mp3[664];mp3[664]^=1;
    trk_all_off(&trk[0]);pump(200);
    bad+=check("damaged staged header never publishes bank",upload(mp3,nm)==2&&!ko_bank_active);
    mp3[664]=original;
    bad+=check("valid bank recovers interrupted or rejected upload",upload(ima,ni)==0&&ko_bank_active);
    uint32_t avail,phys=ko_bank_phys(ko_bank.song_off,&avail);
    bank_flash[phys]^=1;smp_user_scan(0);
    bad+=check("corrupt payload fails closed at boot",!ko_bank_active&&!slc_get(1));
    wire21(a,nm);bad+=check("new begin accepted after corrupt bank",request(91,a,3)==0);
    fail_write=1;
    bad+=check("failed program is reported and never committed",write_page(KO_BANK_HEAD,mp3+KO_BANK_HEAD,256)==4&&request(93,0,0)==2);
    fail_write=0;
    bad+=check("no write reaches projects, presets, OTA or settings",bad_flash==0&&bank_flash[0x97000]==255&&
               bank_flash[0xE0000]==255&&bank_flash[0xE5000]==255&&bank_flash[0xFC000]==255);
    free(mp3);free(ima);return bad?1:0;
}
