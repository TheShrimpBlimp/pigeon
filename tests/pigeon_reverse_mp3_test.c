/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_STORAGE_TEST_MAIN reverse_storage_base_main
#include "ko_storage_test.c"
int main(int argc,char **argv) {
    if(argc!=2)return 2;uint8_t *image;uint32_t size;int bad=0;if(load(argv[1],&image,&size))return 2;
    memset(bank_flash,255,sizeof bank_flash);ui_power_on();song.master_q12=4096;slc_store_boot();ko.on=1;ko_setup();pump(32);
    bad+=check("reverse MP3 fixture uploads",!upload(image,size));free(image);pump(32);song.sel=0;go_home();
    ko_table_t tab={0};tab.n=2;tab.chop[0].a=1000;tab.chop[0].b=10000;tab.chop[0].reverse=1;tab.chop[0].xfade=8;tab.chop[1].a=12000;tab.chop[1].b=20000;
    bad+=check("independent directions restore",ko_restore(&tab));trk_note_on(trk,60,100);
    voice_t *v=0;for(uint32_t i=0;i<NVOICE;i++)if(trk[0].v[i].active && trk[0].v[i].gate && trk[0].v[i].note==60)v=trk[0].v+i;
    bad+=check("MP3 reverse starts at chop end",v && v->ph[0]==10000 && ((uint32_t)v->s[4]&(1u<<7)));
    pump(20);bad+=check("MP3 reverse moves backwards",v && v->ph[0]<10000 && v->ph[0]>1000);
    uint32_t misses=ko_pcm_underruns,errors=ko_pcm_errors;int wrapped=0;int64_t energy=0;
    for(uint32_t i=0;i<1600;i++) {uint32_t before=v?v->ph[0]:0;int32_t out[CTL*2];for(uint32_t j=0;j<NVOICE*4u;j++)ko_pcm_poll();ko_blend_poll();mix_block(out,CTL);if(v && v->ph[0]>before)wrapped=1;for(uint32_t j=0;j<CTL*2;j++)energy+=out[j]<0?-(int64_t)out[j]:out[j];}
    bad+=check("reverse MP3 crossfade becomes ready",v && ko_blend_length(v,slc_get(ko_src()))==176);
    bad+=check("reverse MP3 produces audio and loops",energy>100000 && wrapped && v && v->active);
    printf("reverse misses=%u errors=%u\n",ko_pcm_underruns-misses,ko_pcm_errors-errors);
    bad+=check("reverse MP3 caches stay supplied",ko_pcm_underruns==misses && ko_pcm_errors==errors);
    trk_all_off(trk);pump(4);trk_note_on(trk,61,100);
    voice_t *f=0;for(uint32_t i=0;i<NVOICE;i++)if(trk[0].v[i].active && trk[0].v[i].note==61)f=trk[0].v+i;
    bad+=check("neighboring chop stays forward",f && f->ph[0]==12000 && !((uint32_t)f->s[4]&(1u<<7)));
    trk_all_off(trk);tab.chop[0].play=32768;ko_restore(&tab);trk_note_on(trk,60,100);
    v=0;for(uint32_t i=0;i<NVOICE;i++)if(trk[0].v[i].active && trk[0].v[i].gate && trk[0].v[i].note==60)v=trk[0].v+i;
    bad+=check("reverse MP3 respects separate playback point",v && v->ph[0]==5501);trk_all_off(trk);
    tab.chop[0].play=0;tab.chop[0].rsv=KO_ONE;ko_restore(&tab);trk_note_on(trk,60,100);trk_note_off(trk,60);pump(1600);
    int active=0;for(uint32_t i=0;i<NVOICE;i++)active|=trk[0].v[i].active;
    bad+=check("reverse one-shot finishes after key release",!active);
    printf("PIGEON per-chop reverse: %s\n",bad?"FAIL":"PASS");return !!bad;
}
