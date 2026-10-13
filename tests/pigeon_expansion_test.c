/* SPDX-License-Identifier: GPL-3.0-only */
#define KO_TEST_MAIN ko_controls_test_main
#include "ko_test.c"
static void silence(void){for(uint32_t i=0;i<SAMPLER_PARTS;i++){memset(trk[i].v,0,sizeof trk[i].v);trk[i].seq_n=0;}memset(ko_live,0,sizeof ko_live);}
#ifndef PIGEON_EXPANSION_MAIN
#define PIGEON_EXPANSION_MAIN main
#endif
int PIGEON_EXPANSION_MAIN(int argc,char **argv){
 int bad=0;ui_power_on();memset(ko_states,0,sizeof ko_states);memset(ko_seq,0,sizeof ko_seq);
 memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);menu_put(MI_KO,1);ko_states[0].on=1;ko_setup();render_blocks(32);song.sel=0;go_home();
 if(argc>1){project_store_t wire;FILE *f=fopen(argv[1],"rb");if(!f||fread(&wire,1,sizeof wire,f)!=sizeof wire)return 2;fclose(f);int valid=proj_import(&proj_scratch,&wire,sizeof wire);if(argc>2){int pitch=0;for(uint32_t i=0;i<proj_scratch.motion.count;i++){motion_event_t *e=&proj_scratch.motion.event[i];if(e->place==10 && e->param==(P_TRANS|MOTION_LOCK) && e->value==5)pitch=1;}valid=valid && pitch && step_condition(&proj_scratch.t[0].step[10])==2 && step_nudge(&proj_scratch.t[0].step[10])==-4 && proj_scratch.t[0].p[P_SQNT]==0;}return !(valid&&proj_scratch.t[0].step[5].note[0]==69&&proj_scratch.t[0].step[5].n==1&&proj_scratch.t[0].step[5].time==ST_NOTE&&step_chance(&proj_scratch.t[0].step[5])==70&&step_ratchet(&proj_scratch.t[0].step[5])==3&&proj_scratch.t[0].p[P_SLEN]==32);}
 ko.view=0;page_tap(B_EDIT);page_tap(B_ENV);bad+=check("envelope page reachable through EDIT and ENV",ko.view==19);page_tap(B_ENV);page_tap(B_LFO);bad+=check("chop operations reachable from settings",ko.view==20);page_tap(B_EDIT);page_tap(B_SEQ);page_tap(B_LFO);page_tap(B_LFO);bad+=check("recording preferences reachable from timing",ko.view==21);ko.view=0;
 ko_select(0);ko_envelope_turn(0,10);ko_envelope_turn(1,20);ko_envelope_turn(2,0);ko_table_t saved=*ko_table();
 bad+=check("per-chop attack/release and hold flags are editable",((saved.chop[0].rsv>>KO_ATTACK_SHIFT)&31)==10&&((saved.chop[0].rsv>>KO_RELEASE_SHIFT)&31)==20&&(saved.chop[0].rsv&KO_GATE));
 bad+=check("new envelope flags restore in unchanged chop format",ko_restore(&saved));
 uint32_t n=ko_table()->n;bad+=check("duplicate retains settings",ko_duplicate()&&ko_table()->n==n+1&&ko_table()->chop[1].rsv==saved.chop[0].rsv);
 ko_envelope_turn(0,-5);bad+=check("settings copy excludes boundaries",ko_copy_settings(0)&&ko_table()->chop[1].rsv==ko_table()->chop[0].rsv);
 silence();trk_note_on(&trk[0],60,100);voice_t *v=0;for(uint32_t i=0;i<NVOICE;i++)if(trk[0].v[i].active){v=&trk[0].v[i];break;}
 bad+=check("attack starts below full amplitude",v&&ko_env_gain[0][v-trk[0].v]==0);render_blocks(5);uint32_t before=v?ko_env_gain[0][v-trk[0].v]:0;
 trk_note_off(&trk[0],60);render_blocks(1);bad+=check("custom release decreases existing attack gain",v&&ko_env_gain[0][v-trk[0].v]<before);
 silence();ko_table_t modes=*ko_table();modes.chop[0].rsv=KO_ONE;ko_restore(&modes);trk_note_on(&trk[0],60,100);trk_note_off(&trk[0],60);render_blocks(1);int one=0;for(uint32_t i=0;i<NVOICE;i++)one|=trk[0].v[i].active&&trk[0].v[i].stage==2;bad+=check("explicit one-shot ignores early key release",one);
 silence();modes.chop[0].rsv=KO_NO_LOOP|KO_GATE|(20u<<KO_RELEASE_SHIFT);ko_restore(&modes);trk_note_on(&trk[0],60,100);trk_note_off(&trk[0],60);render_blocks(1);int hold=0;for(uint32_t i=0;i<NVOICE;i++)hold|=trk[0].v[i].active&&trk[0].v[i].stage==3;bad+=check("nonlooping hold honors key release",hold);
 silence();modes.chop[0].rsv=KO_LATCH;ko_restore(&modes);trk_note_on(&trk[0],60,100);trk_note_off(&trk[0],60);int latch=0;for(uint32_t i=0;i<NVOICE;i++)latch|=trk[0].v[i].active&&trk[0].v[i].gate;bad+=check("latch retains voice after key release",latch);ko_restore(&saved);
 silence();ko_choke[0]=1;trk_note_on(&trk[0],60,100);trk_note_on(&trk[0],61,100);int choked=0;for(uint32_t i=0;i<NVOICE;i++)if(trk[0].v[i].note==60&&trk[0].v[i].active)choked|=trk[0].v[i].stage==4;
 bad+=check("song choke fades previous chop",choked);ko_choke[0]=0;silence();
 song.playing=1;clk_pos=div_samples(2)/2;clk_step=0;ko_quantize[0]=1;input_on(&trk[0],60,100);
 bad+=check("live quantization queues without sounding early",ko_live[0][0].on&&!voices_busy());
 ko_live_tick(div_samples(2)/2-1);bad+=check("quantized note waits until boundary",!voices_busy());ko_live_tick(1);bad+=check("quantized note starts at boundary",voices_busy()>0);song.playing=0;silence();
 song.playing=1;clk_pos=div_samples(2)/2;clk_step=0;ko_quantize[0]=2;input_on(&trk[0],60,100);uint32_t left=ko_live[0][0].left;
 bad+=check("eighth quantization uses two sixteenth steps",left==div_samples(2)*2-clk_pos);input_off(&trk[0],60);bad+=check("early release remains paired with queued note",ko_live[0][0].up);song.playing=0;ko_live_tick(1);bad+=check("stop cancels quantized input",!ko_live[0][0].on);silence();
 ko.view=21;ko_knob(0,2);ko_knob(1,2);bad+=check("metronome and count-in can both be enabled",rp_get(RP_CLICK)==2&&rp_get(RP_COUNTIN)==2);ko_knob(0,-2);ko_knob(1,-2);bad+=check("metronome and count-in independently toggle off",click_mode==0&&cin_bars==0);
 ko_choke[0]=1;ko_quantize[0]=2;ko_context=0;int stored=ko_store_sector(1);ko_choke[0]=ko_quantize[0]=0;ko_store_load();ko_context=~0u;
 bad+=check("track preferences and envelopes persist",stored==3&&ko_choke[0]==1&&ko_quantize[0]==2&&ko_table()->chop[0].rsv==saved.chop[0].rsv);
 ko.view=22;trk[0].step[2].n=1;trk[0].step[2].time=ST_NOTE;trk[0].step[2].note[0]=60;trk[0].step[2].vel=100;(void)motion_set_lock(&trk[0],2,P_SGATE,50);ko_knob(1,1);int16_t gate=0;
 bad+=check("clear notes preserves locks",!trk[0].step[2].n&&motion_lock_get(&trk[0],2,P_SGATE,&gate));ko_knob(2,1);bad+=check("clear locks is separate",!motion_lock_get(&trk[0],2,P_SGATE,&gate));
 project_t p;project_store_t wire;project_capture(&p);if(!proj_pack(&wire,&p))bad++;else {FILE *f=fopen("build/expansion-project.funa","wb");if(!f)bad++;else{fwrite(&wire,1,sizeof wire,f);fclose(f);}}
 printf("PIGEON expansion: %s\n",bad?"FAILED":"PASSED");return bad!=0;
}
