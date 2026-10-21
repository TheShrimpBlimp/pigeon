/* SPDX-License-Identifier: GPL-3.0-only */
#define KO_TEST_MAIN ko_controls_test_main
#include "ko_test.c"
int main(void) {
 int bad=0;ui_power_on();memset(ko_states,0,sizeof ko_states);memset(ko_seq,0,sizeof ko_seq);
 memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);menu_put(MI_KO,1);ko_states[0].on=1;ko_setup();render_blocks(32);
 go_home();song.sel=0;ko.view=7;ko_seq[0].enabled=1;ko_seq[0].cursor=5;song.rec=0;
 step_clear(&trk[0].step[5]);key_down(white(6));frame();key_up(white(6));frame();
 bad+=check("unarmed grid assigns chop without advancing or sounding",trk[0].step[5].n==1 && trk[0].step[5].note[0]==66 && ko_seq[0].cursor==5 && !voices_busy());
 undo_swap();bad+=check("direct step assignment supports undo",trk[0].step[5].n==0);undo_swap();
 press(B_ENV);bad+=check("ENV opens sampler step detail",ko.view==14);
 int16_t oldgate=trk[0].p[P_SGATE];ko_knob(0,-20);ko_knob(2,-30);ko_knob(3,2);ko_knob(1,20);int16_t gate=0;
 bad+=check("velocity chance repeats and per-step gate share project fields",trk[0].step[5].vel==80 && step_chance(&trk[0].step[5])==70 && step_ratchet(&trk[0].step[5])==3 && motion_lock_get(&trk[0],5,P_SGATE,&gate) && gate==oldgate+20);
 project_t project;project_store_t store;project_capture(&project);int packed=proj_pack(&store,&project)&&proj_import(&proj_scratch,&store,sizeof store);
 bad+=check("detail edits serialize with unchanged project format",packed && proj_scratch.t[0].step[5].vel==80 && step_chance(&proj_scratch.t[0].step[5])==70);
 press(B_LFO);ko_knob(0,25);bad+=check("swing page exposes track swing",ko.view==15 && trk[0].p[P_SSWING]==25);
 song.sel=1;ko.view=0;press(B_ENV);ko_kit_pad=2;ko_knob(0,-3);ko_knob(1,-27);ko_knob(2,10);ko_knob(3,1);
 bad+=check("kit pad has independent pitch level decay choke",ko.view==16 && ko_kit_params[2].pitch==-3 && ko_kit_params[2].level==100 && ko_kit_params[2].decay==10 && ko_kit_params[2].choke==1 && ko_kit_params[1].level==127);
 ko_kit_params[3].choke=1;voice_t *a=&trk[1].v[0],*b=&trk[1].v[1];memset(a,0,sizeof *a);memset(b,0,sizeof *b);a->note=62;a->active=1;a->stage=2;a->env=1<<24;b->note=63;b->active=1;sample_note_on(&trk[1],b);
 bad+=check("choke group requests a short fade on previous pad",a->stage==4 && !a->gate);
 a->stage=2;a->active=1;trk[1].seq_n=1;trk[1].seq_notes[0]=62;
 motion_locked[1][P_SGATE/32u]|=1u<<(P_SGATE%32u);seq_release(&trk[1]);
 bad+=check("explicit kit gate lock fades the sequenced hit",a->stage==4);
 motion_locked[1][P_SGATE/32u]=0;
 memset(trk[1].v,0,sizeof trk[1].v);song.sel=0;
 ko_context=0;int save=ko_store_sector(1);ko_kit_defaults();ko_store_load();ko_context=~0u;
 bad+=check("kit settings persist with sample metadata",save==3 && ko_kit_params[2].pitch==-3 && ko_kit_params[2].level==100 && ko_kit_params[2].choke==1);
 ko.view=1;trk[0].p[P_DLY]=20;btn_down(B_LFO);frame();turn(EN_K2,10);frame();int changed=trk[0].p[P_DLY]>20;btn_up(B_LFO);frame();
#if FELUCCA_SAMPLER_ONLY
 bad+=check("LFO navigates immediately and release keeps its page",!changed && trk[0].p[P_DLY]==20 && ko.view==4);
#else
 bad+=check("held LFO FX tweaks restore on release",changed && trk[0].p[P_DLY]==20 && ko.view==1);
#endif
ko.view=18;chain_defaults(&chain_config);chain_config.count=2;chain_config.row[0]=chain_row_of(0,2);chain_config.row[1]=chain_row_of(1,3);ko_section=0;
 ko_arrange_turn(2,1);bad+=check("section duplication retains repeats and lane assignments",chain_config.count==3 && chain_config.row[1].repeat==2 && chain_config.row[1].slot[0]==0);
 ko_arrange_turn(1,1);bad+=check("moving section reorders without losing contents",ko_section==2 && chain_config.row[1].slot[0]==1 && chain_config.row[2].repeat==2);
 ko_arrange_turn(0,1);bad+=check("pattern end launch is selectable",chain_config.rsv[0]==1);
 song.sel=0;ko.view=1;trk[0].p[P_DLY]=30;press(B_EDIT);bad+=check("EDIT resets performance sends and distortion",trk[0].p[P_DLY]==0 && trk[0].p[P_CHOR]==0 && trk[0].p[P_REV]==0 && trk[0].p[P_DIST]==0);
 chain_config.rsv[0]=1;ko_launch_ready=1;ko_launch_mode=0;ko_launch_pending=1;song.playing=1;chain.running=0;chain.clk=0;
 trk[0].p[P_SLEN]=32;trk[0].p[P_SDIV]=2;trk[0].seq_idx=15;trk[0].seq_pos=0;chain_tick(div_samples(2));
 bad+=check("pattern-end launch does not switch halfway through 32 steps",ko_launch_pending==1);
 for(uint32_t k=0;k<CHAIN_PARTS;k++) {chain.source[1].timing[k][0]=16;chain.source[1].timing[k][1]=2;chain.source[1].timing[k][2]=0;chain.source[1].timing[k][3]=64;}
 trk[0].seq_idx=31;trk[0].seq_pos=0;chain_tick(div_samples(2));
 bad+=check("pattern-end launch switches at completed loop",ko_launch_pending==255 && ko_pattern_active==1 && chain.running);
 seq_stop();ko_launch_ready=0;song.playing=0;ko_launch_pending=255;
 project_capture(&project);packed=proj_pack(&store,&project)&&proj_import(&proj_scratch,&store,sizeof store);
 bad+=check("launch preference survives project serialization",packed && proj_scratch.chain.rsv[0]==1);
 printf("PIGEON workflow: %s\n",bad?"FAILED":"PASSED");return bad!=0;
}
