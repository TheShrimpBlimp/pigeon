/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN finish_fixture_main
#include "ko_test.c"
int main(void){
 int bad=0;ui_power_on();memset(ko_states,0,sizeof ko_states);memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);fixture(2,0);ko_product_boot();render_blocks(16);go_home();
 ko_table_t tab={0};tab.n=1;tab.chop[0]=(ko_chop_t){.a=2048,.b=16384,.cents=37,.gain=512,.xfade=4};
 bad+=check("cents gain and crossfade validate",ko_restore(&tab));
 trk_all_off(trk);voice_t *v=&trk[0].v[0];memset(v,0,sizeof *v);v->active=v->gate=1;v->note=60;slice_note_on(trk,v);
 bad+=check("fine tuning and gain captured on trigger",ko_voice_fine[0][0]>32768 && ko_voice_gain[0][0]==512);
 trk[0].p[P_E6]=127;v->stage=2;bad+=check("gain leaves envelope arithmetic bounded",slice_amp(trk,v,32767)<=32767);
 for(uint32_t i=0;i<NVOICE*2;i++)ko_blend_poll();const slc_src_t *src=slc_get(ko_src());uint32_t n=ko_blend_length(v,src);
 bad+=check("opposite loop edge prepared outside ISR",n==88);
 slc_dec_t d;slc_dec_at(&d,tab.chop[0].b-1,slc_state_at(src,tab.chop[0].b-1));int32_t x;
 slc_fwd(src,v,&d,1,&x);bad+=check("tail crossfades exactly into loop head",x==ko_blend_buffer(0,0,src)[n-1]);
 slc_fwd(src,v,&d,1,&x);bad+=check("overlapped head is skipped once on wrap",d.pos==tab.chop[0].a+n+1);
 tab.chop[0].reverse=1;ko_restore(&tab);slice_note_on(trk,v);for(uint32_t i=0;i<NVOICE*2;i++)ko_blend_poll();n=ko_blend_length(v,src);v->ph[0]=tab.chop[0].a+1;v->ph[2]=tab.chop[0].a;
 slc_rev(src,v,slc_rb(trk,v),1,&x);bad+=check("reverse crossfade meets opposite loop edge",x==ko_blend_buffer(0,0,src)[0]);slc_rev(src,v,slc_rb(trk,v),1,&x);bad+=check("reverse overlap skipped on wrap",v->ph[0]==tab.chop[0].b-n-1);
 bad+=check("v0.5 settings save",ko_store_sector(1)==3);ko_auto();ko_store_load();bad+=check("v0.5 settings reload",ko_table()->chop[0].cents==37 && ko_table()->chop[0].gain==512 && ko_table()->chop[0].xfade==4 && ko_table()->chop[0].reverse==1);
 ko_play_edit=3;ko_chop_turn(0,-1000);bad+=check("cent tuning clamps",ko_table()->chop[0].cents==-100);ko_play_edit=5;ko_chop_turn(0,1000);bad+=check("crossfade clamps",ko_table()->chop[0].xfade==8);
 printf("v0.5 chop finish: %s\n",bad?"FAIL":"PASS");return !!bad;
}
