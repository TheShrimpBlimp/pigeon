/* SPDX-License-Identifier: GPL-3.0-only */
/* Real bank parser, upload handler, DSP and persistence against 1 MiB NOR. */
#include <stdint.h>
static uint8_t bank_flash[0x100000];
static uint32_t diagnostic_ticks;
#define KO_DIAG_NOW() diagnostic_ticks
#define KO_DIAG_US(ticks) (ticks)
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
    for (uint32_t off=0;off<sizeof(ko_bank_hdr_t)*(p[4]==3?2u:1u);off+=256)
        if (write_page(off,p+off,sizeof(ko_bank_hdr_t)*(p[4]==3?2u:1u)-off>256?256:sizeof(ko_bank_hdr_t)*(p[4]==3?2u:1u)-off)) return -3;
    return request(93,0,0);
}
static void pump(uint32_t n)
{
    int32_t out[CTL*2];
    for (uint32_t i=0;i<n;i++) { for(uint32_t v=0;v<NVOICE*4u;v++) ko_pcm_poll();mix_block(out,CTL); }
}
int main(int argc,char **argv)
{
    if (argc!=(FELUCCA_KO_DUAL ? 4 : 3)) return 2;
    uint8_t *mp3,*ima;uint32_t nm,ni;int bad=0;
    if (load(argv[1],&mp3,&nm)||load(argv[2],&ima,&ni)) return 2;
    memset(bank_flash,255,sizeof bank_flash); ui_power_on();song.master_q12=4096;slc_store_boot();ko.on=1;ko_setup();pump(32);
    uint8_t reset=1, invalid=2;
    ko_diag.decode_calls=123;ko_pcm_underruns=99;
    uint32_t diag_erases=erases;
    bad+=check("diagnostic snapshot uses versioned 32-bit fields",
              request(97,0,0)==0 && ed_n==58 && ed_out[6]==1 && ed_out[7]==99 && ed_out[22]==123);
    bad+=check("invalid diagnostics do not reset counters",
              request(97,&invalid,1)==1 && ko_diag.decode_calls==123);
    bad+=check("diagnostics reset RAM counters without erasing flash",
              request(97,&reset,1)==0 && !ko_diag.decode_calls && !ko_pcm_underruns && erases==diag_erases);
    /* Synthetic clock checks are independent of host decoder speed. */
    trk[0].v[0].active=1;diagnostic_ticks=100000;ko_pcm_poll();diagnostic_ticks=107000;ko_pcm_poll();
    bad+=check("diagnostics capture active refill gaps",ko_diag.poll_gap_max_us==7000);
    trk[0].v[0].active=0;ko_pcm_poll();diagnostic_ticks=1000000;ko_pcm_poll();
    bad+=check("idle time does not inflate active refill gap",ko_diag.poll_gap_max_us==7000);
    ko_diag_reset();
    uint8_t a[3];wire21(a,nm);
    uint32_t before=erases;
    bad+=check("bad bank begin never erases",request(91,a,2)==1&&erases==before);
    bad+=check("bank capacity negotiated without writes",request(90,0,0)==0x4B&&ed_out[6]==(FELUCCA_KO_DUAL?3:2)&&erases==before);
    bad+=check("upload begins invalid and interrupted commit is refused",request(91,a,3)==0&&!ko_bank_active&&request(93,0,0)==2);
    bad+=check("out-of-order payload rejected",write_page(KO_BANK_HEAD+256,mp3+KO_BANK_HEAD,256)==1);
    bad+=check("full MP3 bank uploads with header last",upload(mp3,nm)==0&&ko_bank_active&&usr_nz[1]==16);
    int reads_ok=1;
    for(uint32_t off=0;off<nm;off+=257) {
        uint8_t bytes[257];uint32_t n=nm-off>sizeof bytes ? sizeof bytes : nm-off;
        ko_bank_read(off,bytes,n);reads_ok &= !memcmp(bytes,mp3+off,n);
    }
    bad+=check("bulk flash reads match upload across every storage extent",reads_ok);
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
        int32_t out[CTL*2];for(uint32_t v=0;v<NVOICE*4u;v++)ko_pcm_poll();mix_block(out,CTL);
        for(uint32_t j=0;j<CTL*2;j++)energy+=out[j]<0?-(int64_t)out[j]:out[j];
    }
    bad+=check("compressed held loop produces audio without decoder errors",energy>100000&&ko_pcm_errors==errors);
    printf("  energy=%lld errors=%u engine=%u requested=%u active=%u gate=%u ph=%u state=%x\n",(long long)energy,ko_pcm_errors-errors,trk[0].engine,trk[0].eng_req,trk[0].v[0].active,trk[0].v[0].gate,trk[0].v[0].ph[0],(unsigned)trk[0].v[0].s[4]);
    printf("  cache misses during loops: %u source samples\n",ko_pcm_underruns-misses);
    bad+=check("held loop cache remains supplied",ko_pcm_underruns==misses);
    trk_all_off(&trk[0]);pump(200);
    /* Reproduce the old display-paced service: one decode job per 15 ms
     * cannot supply two independent song voices. Then service the decoder
     * during the UI wait, with only one opportunity per audio block. */
    trk_note_on(&trk[0],60,100);trk_note_on(&trk[0],61,100);pump(8);
    misses=ko_pcm_underruns;
    for(uint32_t i=0;i<3000;i++) {
        int32_t out[CTL*2];
        if(i % ((44100u*15u/1000u+CTL-1u)/CTL)==0) ko_pcm_poll();
        mix_block(out,CTL);
    }
    bad+=check("display-paced decoder reproduces two-note starvation",ko_pcm_underruns>misses);
    trk_all_off(&trk[0]);pump(200);
    trk_note_on(&trk[0],60,100);trk_note_on(&trk[0],61,100);pump(8);
    misses=ko_pcm_underruns;errors=ko_pcm_errors;
    for(uint32_t i=0;i<3000;i++) {
        int32_t out[CTL*2];ko_pcm_poll();mix_block(out,CTL);
    }
    bad+=check("two notes remain supplied with decoder service during UI wait",
              ko_pcm_underruns==misses&&ko_pcm_errors==errors);
    trk_all_off(&trk[0]);pump(200);
    ko_pcm_reset();trk_note_on(&trk[0],62,100);
    { int32_t out[CTL*2];mix_block(out,CTL); }
    ko_pcm_poll();
    bad+=check("cold attack starts a cooperative decode job",ko_pcm_job.active);
    uint32_t pending_voice=ko_pcm_job.voice;
    ko_pcm_poll();
    bad+=check("partial cold seek does not publish incomplete PCM",
              ko_pcm_job.active&&ko_pcm[pending_voice].frame[0]==~0u&&ko_pcm[pending_voice].frame[1]==~0u);
    ko_pcm_reset();ko_pcm_poll();
    bad+=check("bank reset cancels an in-flight seek before publication",
              !ko_pcm_job.active&&ko_pcm[pending_voice].frame[0]==~0u&&ko_pcm[pending_voice].frame[1]==~0u);
    trk_all_off(&trk[0]);pump(200);
    ko_table_t overlap=table;overlap.chop[1]=overlap.chop[0];ko_restore(&overlap);
    uint32_t reused=ko_pcm_reused;
    misses=ko_pcm_underruns;errors=ko_pcm_errors;
    trk_note_on(&trk[0],60,100);trk_note_on(&trk[0],61,100);pump(500);
    bad+=check("overlapping triggers reuse decoded PCM without missing audio",
              ko_pcm_reused>reused&&ko_pcm_underruns==misses&&ko_pcm_errors==errors);
    trk_all_off(&trk[0]);pump(200);ko_restore(&table);
    ko_pcm_reset();trk_note_on(&trk[0],60,100);pump(16);
    while(ko_pcm_job.active)ko_pcm_poll();
    uint32_t running=0;
    for(uint32_t v=0;v<NVOICE;v++)if(trk[0].v[v].active&&trk[0].v[v].note==60)running=v;
    trk_note_on(&trk[0],61,100);
    { int32_t out[CTL*2];mix_block(out,CTL); }
    ko_pcm[running].frame[0]=ko_pcm[running].frame[1]=~0u;
    ko_pcm_poll();
    bad+=check("running voice refill takes priority over a cold trigger",
              ko_pcm_job.active&&ko_pcm_job.voice==running);
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
    /* Real MP3 priming, complete-chop sequencing, and persistent timing preferences. */
    trk_all_off(&trk[0]);pump(200);step_t old_steps[16];memcpy(old_steps,trk[0].step,sizeof old_steps);
    for(uint32_t i=0;i<16;i++)step_clear(&trk[0].step[i]);
    for(uint32_t i=0;i<2;i++){trk[0].step[i].time=ST_NOTE;trk[0].step[i].n=1;trk[0].step[i].note[0]=60+i;trk[0].step[i].vel=100;}
    ko_seq[0].enabled=ko_seq[0].end_mode=1;ko_pcm_reset();seq_start();ko_end_tick(&trk[0],CTL);
    for(uint32_t i=0;i<10;i++)ko_end_tick(&trk[0],CTL);
    bad+=check("sample-end MP3 waits through decoder priming",trk[0].seq_idx==0);
    uint32_t elapsed=0;while(trk[0].seq_idx==0 && elapsed<2000){pump(1);elapsed++;}
    bad+=check("sample-end MP3 advances after complete chop",trk[0].seq_idx==1 && elapsed>10 && elapsed<2000);
    seq_stop();trk_all_off(&trk[0]);pump(200);memcpy(trk[0].step,old_steps,sizeof old_steps);ko_seq[0].enabled=0;
    ko_seq[1].end_mode=1;ko.dirty=1;ko.revision++;bad+=check("sample-end timing preferences save using unchanged record",ko_store_sector(1)!=2);
    ko_seq[0].end_mode=ko_seq[1].end_mode=0;ko_store_load();
    bad+=check("song and kit timing preferences restore independently",ko_seq[0].end_mode && ko_seq[1].end_mode);
    trk_all_off(&trk[0]);pump(200);ko_select(3);ko_pitch_turn(12);ko_flag_turn(KO_LATCH,1);
    ko_table_t moved=*ko_table();moved.chop[3].a=moved.chop[7].a;moved.chop[3].b=moved.chop[7].b;
    bad+=check("compressed chops accept out-of-order pad positions",ko_restore(&moved));
    ko_prepare();bad+=check("chop edits persist outside shared payload",ko_store_sector(1)==3);
    ko_table_t saved=*ko_table();smp_user_scan(0);ko.loaded=0;ko_prepare();
    bad+=check("reboot restores pitch and latch",!memcmp(&saved,ko_table(),sizeof saved));
    for(uint32_t i=0;i<16;i++)trk_note_on(&trk[1],60+i,100);pump(100);
    bad+=check("sixteen kit zones retain exact pad notes",usr_nz[1]==16&&usr_zone[1][3].lo==63&&usr_zone[1][10].lo==70);
    ko.view=0;song.sel=0;go_home();frame();
    bad+=check("waveform drawing never invokes decoder",ko_pcm_errors==errors);
    project_t q;project_capture(&q);project_store_t stored;
    bad+=check("FUNA project packing remains compatible",proj_pack(&stored,&q)&&proj_import(&q,&stored,sizeof stored));
    /* Exercise next-bar launches against the real MP3 decoder/refill pipeline. */
    for(uint32_t p=0;p<SAMPLER_PARTS;p++) trk_all_off(&trk[p]);pump(300);
    trk[0].step[0].time=ST_NOTE;trk[0].step[0].n=1;trk[0].step[0].note[0]=60;trk[0].step[0].vel=100;
    trk[0].p[P_SLEN]=16;trk[0].p[P_SDIV]=2;project_save(0);
    trk[0].step[0].note[0]=61;project_save(1);ko_pattern_sel=0;
    uint32_t launch_errors=ko_pcm_errors;ko_pattern_start(0);pump(100);ko_pattern_select(1);
    uint32_t launch_wait=0;while(ko_launch_pending<4 && launch_wait<6000){pump(1);launch_wait++;}
    bad+=check("real MP3 pattern launch waits for a bar and switches cached source",launch_wait>100 && launch_wait<6000 && chain.running && ko_pattern_active==1 && seq_steps(&trk[0])[0].note[0]==61);
    pump(100);bad+=check("MP3 launch keeps decoder healthy without touching sample metadata",ko_pcm_errors==launch_errors && !memcmp(&saved,ko_table(),sizeof saved));
    seq_stop();for(uint32_t p=0;p<SAMPLER_PARTS;p++) trk_all_off(&trk[p]);pump(300);ko_pattern_poll();
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
#if FELUCCA_KO_DUAL
    uint8_t *dual;uint32_t nd;
    bad+=check("dual fixture loads",load(argv[3],&dual,&nd)==0);
    bad+=check("two-song bank publishes both independent sources",upload(dual,nd)==0 && ko_bank.version==3 && slc_get(1) && slc_get(3));
    ko_states[0].loaded=ko_states[1].loaded=0;ko_prepare();song.sel=2;ko_select(0);
    ko_pitch_turn(7);ko_flag_turn(KO_LATCH,1);
    bad+=check("second song edits remain independent",ko_table()->chop[0].pitch==7 && ko_states[0].table[ko_states[0].current].chop[0].pitch!=7);
    ko_comp_t first_comp=ko_states[0].comp[0];ko.comp[0].makeup=3;
    bad+=check("third compressor independent",memcmp(&first_comp,&ko_states[0].comp[0],sizeof first_comp)==0);
    trk_note_on(&trk[0],60,100);trk_note_on(&trk[2],60,100);pump(64);
    trk_note_on(&trk[1],60,100);pump(16);
    uint32_t playing0=0,playing2=0;
    for(uint32_t i=0;i<NVOICE;i++){playing0+=trk[0].v[i].active;playing2+=trk[2].v[i].active;}
    bad+=check("both song tracks play simultaneously",playing0 && playing2 && ko_pcm_errors==0);
    while (ko_pcm_job.active) ko_pcm_poll();
    int exact=1;
    for(uint32_t track=0;track<2;track++) {
        uint32_t vi=track*NVOICE;
        for(uint32_t i=0;i<NVOICE;i++) if (ko_pcm_voice(track*NVOICE+i)->active &&
            (ko_pcm[track*NVOICE+i].frame[0]!=~0u || ko_pcm[track*NVOICE+i].frame[1]!=~0u)) { vi+=i;break; }
        uint32_t slot=ko_pcm[vi].frame[0]==~0u;
        uint32_t frame=ko_pcm[vi].frame[slot];
        const ko_bank_hdr_t *h=ko_pcm_bank(vi);
        if(frame==~0u) { printf("  cache check: track %u voice %u has no published frame\n",track,vi);exact=0;continue; }
        uint32_t first=frame>1?frame-2:0;
        uint32_t a=ko_bank_u32(h->song_off+first*4),b=ko_bank_u32(h->song_off+(frame+1)*4);
        printf("  source window: track %u voice %u frame %u first %u offsets %u..%u codec %u\n",track,vi,frame,first,a,b,h->codec);
        if(b<a || b-a>640 || frame>=h->frames) { exact=0;continue; }
        uint8_t encoded[648];int16_t expected[576];
        ko_bank_read(h->song_off+a,encoded,b-a);memset(encoded+(b-a),0,8);
        int decoded=ko_mp3_decode(encoded,b-a+8,frame-first+1,expected);
        int equal=!memcmp(expected,ko_pcm_buffer(vi,slot),sizeof expected);
        printf("  cache check: track %u voice %u frame %u decode %d equal %d\n",track,vi,frame,decoded,equal);
        exact &= !decoded && equal;
    }
    bad+=check("interleaved song caches match independent source decodes",exact);

    bad+=check("independent sources never reuse each other's PCM",ko_pcm_bank(0)!=ko_pcm_bank(NVOICE));
    for (uint32_t i=0;i<12;i++) { trk_note_on(&trk[0],60+i,100);trk_note_on(&trk[2],60+i,100);trk_note_on(&trk[1],60+i,100); }
    pump(64);bad+=check("three tracks retain the shared eight voice limit",voices_busy()<=NVOICE);
    trk_all_off(&trk[0]);trk_all_off(&trk[1]);trk_all_off(&trk[2]);pump(200);
    /* Sustained mixed sampler load after cold attacks have been serviced.
     * Host polling verifies scheduling/data integrity, not hardware deadlines. */
    ko_comp_t saved_comp[3]={ko_states[0].comp[0],ko_states[0].comp[1],ko_states[1].comp[0]};
    int16_t saved_filter[3];
    ko_states[0].comp[0]=ko_states[0].comp[1]=ko_states[1].comp[0]=(ko_comp_t){12,4,10,10,0};
    for(uint32_t part=0;part<SAMPLER_PARTS;part++) {
        uint32_t filter=part==1?P_E4:P_E7;saved_filter[part]=trk[part].p[filter];trk[part].p[filter]=80;
        trk[part].p[P_DLY]=64;trk[part].p[P_CHOR]=64;
        trk[part].p[P_REV]=64;trk[part].p[P_DIST]=32;
    }
    for(uint32_t i=0;i<3;i++) {trk_note_on(&trk[0],60+i,100);trk_note_on(&trk[2],60+i,100);}
    trk_note_on(&trk[1],60,100);trk_note_on(&trk[1],61,100);pump(64);
    uint32_t stress_errors=ko_pcm_errors,stress_misses=ko_pcm_underruns;
    pump(1024);
    bad+=check("two songs plus drums and all six FX retain bounded voices and valid PCM",voices_busy()<=NVOICE && ko_pcm_errors==stress_errors && ko_pcm_underruns==stress_misses);
    for(uint32_t part=0;part<SAMPLER_PARTS;part++) {
        trk[part].p[part==1?P_E4:P_E7]=saved_filter[part];
        trk_all_off(&trk[part]);trk[part].p[P_DLY]=trk[part].p[P_CHOR]=trk[part].p[P_REV]=trk[part].p[P_DIST]=0;
    }
    ko_states[0].comp[0]=saved_comp[0];ko_states[0].comp[1]=saved_comp[1];ko_states[1].comp[0]=saved_comp[2];
    pump(300);
    ko_table_t second_saved=*ko_table();
    bad+=check("both song records persist",ko_store_sector(1)==3);
    ko_auto();ko_store_load();
    bad+=check("second song chop pitch and latch restore",!memcmp(&second_saved,ko_table(),sizeof second_saved));
    ko_bank_scan();ko_states[0].loaded=ko_states[1].loaded=0;ko_prepare();
    bad+=check("two-song bank and edits survive reboot scan",ko_bank.version==3 && ko_table()->chop[0].pitch==7 && (ko_table()->chop[0].rsv&KO_LATCH));
    trk[2].step[0].n=1;trk[2].step[0].note[0]=60;trk[2].step[0].time=ST_NOTE;trk[2].step[0].vel=100;
    trk[2].p[P_SLEN]=16;trk[2].p[P_SDIV]=2;
    project_t project;project_store_t store;project_capture(&project);
    int roundtrip=proj_pack(&store,&project) && proj_import(&proj_scratch,&store,sizeof store) && !project_restore_runtime(&proj_scratch);
    bad+=check("third song pattern uses unchanged project format",roundtrip && trk[2].eng_req==13 && trk[2].p[P_E0]==3 && trk[2].step[0].note[0]==60 && trk[2].step[0].n==1);
    seq_start();pump(128);uint32_t sequenced=0;
    for(uint32_t i=0;i<NVOICE;i++) sequenced+=trk[2].v[i].active;
    bad+=check("sequencer triggers the third song track",sequenced>0);seq_stop();pump(200);
    uint8_t byte=dual[2*sizeof(ko_bank_hdr_t)-1];dual[2*sizeof(ko_bank_hdr_t)-1]^=1;
    bad+=check("damaged secondary header fails closed",upload(dual,nd)==2 && !ko_bank_active);
    dual[2*sizeof(ko_bank_hdr_t)-1]=byte;
    bad+=check("old bank still loads on dual firmware",upload(mp3,nm)==0 && ko_bank.version==2 && !slc_get(3));
    song.sel=0;free(dual);
#endif
    free(mp3);free(ima);return bad?1:0;
}
