/* SPDX-License-Identifier: GPL-3.0-only */
#define PIGEON_EXPANSION_MAIN expansion_test_main
#include "pigeon_expansion_test.c"
#undef PIGEON_EXPANSION_MAIN
int main(void) {
    int bad=expansion_test_main(0,0);silence();song.playing=0;song.sel=0;go_home();ko.view=0;
    ko.view=21;page_tap(B_ENV);bad+=check("tempo tools reachable from recording preferences",ko.view==23);ko.view=14;page_tap(B_ENV);bad+=check("advanced step controls reachable from details",ko.view==24);ko.view=20;page_tap(B_FX);bad+=check("chop groups and layouts reachable from operations",ko.view==25);song.sel=1;ko.view=16;page_tap(B_LFO);bad+=check("roll page reachable from drum settings",ko.view==26);song.sel=0;ko.view=0;
    ko_tap_last=ko_tap_count=0;fm1_ms=1000;ko_tap_tempo();fm1_ms=1500;ko_tap_tempo();fm1_ms=2000;ko_tap_tempo();
    bad+=check("three taps derive 120 BPM",song.g[G_BPM]==120);fm1_ms=5000;ko_tap_tempo();bad+=check("long gap restarts tapping",ko_tap_count==0);
    ko_table_t layout=*ko_table();layout.chop[0].a=0;layout.chop[0].b=11025;ko_restore(&layout);ko_select(0);ko_loop_beats=1;
    bad+=check("loop beats calculate BPM from sample rate and exact length",ko_loop_bpm()==120);
    ko_alternate[0].n=0;ko_layout[0]=0;bad+=check("alternate layout initially shares original chop layout",ko_layout_switch()&&ko_layout[0]==1);ko_select(0);ko_pitch_turn(5);int16_t b_pitch=ko_table()->chop[0].pitch;
    ko_layout_switch();bad+=check("A layout remains independent",ko_table()->chop[0].pitch==layout.chop[0].pitch);ko_layout_switch();bad+=check("B layout retains edits",ko_table()->chop[0].pitch==b_pitch);
    ko_select(0);ko_choke_group_turn(1);ko_select(1);ko_choke_group_turn(1);ko_choke[0]=0;
    trk_note_on(&trk[0],60,100);trk_note_on(&trk[0],61,100);int cut=0;for(uint32_t i=0;i<NVOICE;i++)cut|=trk[0].v[i].active&&trk[0].v[i].note==60&&trk[0].v[i].stage==4;bad+=check("matching chop groups choke earlier voice",cut);silence();
    ko_select(1);ko_choke_group_turn(-1);trk_note_on(&trk[0],60,100);trk_note_on(&trk[0],61,100);int overlap=0;for(uint32_t i=0;i<NVOICE;i++)overlap|=trk[0].v[i].active&&trk[0].v[i].note==60&&trk[0].v[i].stage!=4;bad+=check("group zero permits unrelated chops to overlap",overlap);silence();
    ko_quantize[0]=0;ko_seq[0].enabled=1;ko_seq[0].cursor=5;ko.view=24;trk[0].step[5].n=1;trk[0].step[5].time=ST_NOTE;trk[0].step[5].note[0]=60;ko_knob(0,7);ko_knob(1,-3);ko_knob(2,1);
    int16_t pitch=0;bad+=check("per-step pitch uses existing motion locks",motion_lock_get(&trk[0],5,P_TRANS,&pitch)&&pitch==7);bad+=check("microtiming edits enable unquantized playback",step_nudge(&trk[0].step[5])==-3&&!trk[0].p[P_SQNT]);
    step_t condition=trk[0].step[5];for(uint32_t c=1;c<=4;c++){step_set_condition(&condition,c);int passes=0;for(uint32_t pass=0;pass<8;pass++){ko_pass[0]=pass;trk[0].seq_idx=5;passes+=ko_condition_pass(&trk[0],&condition,5);}bad+=check("conditional step cycle counts",passes==(c==1?4:c==2?2:c==3?1:7));}
    step_set_condition(&trk[0].step[5],0);motion_begin();seq_play(&trk[0],5,div_samples(2));int pitched=0;for(uint32_t i=0;i<NVOICE;i++)if(trk[0].v[i].active)pitched|=ko_voice_transpose[0][i]==7;bad+=check("pitch is captured by triggered sample voice",pitched);silence();motion_end();
    silence();trk[0].p[P_SLEN]=1;trk[0].p[P_SQNT]=1;trk[0].step[0]=(step_t){.note={60},.n=1,.time=ST_NOTE,.vel=100};step_set_condition(&trk[0].step[0],2);chain_config.count=0;seq_start();int scheduled=0;for(uint32_t pass=0;pass<8;pass++){seq_tick(&trk[0],pass?div_samples(2):1);scheduled+=trk[0].seq_n!=0;silence();}bad+=check("conditional playback fires every fourth full pattern pass",scheduled==2);seq_stop();
    silence();step_t early={0};step_set_condition(&early,1);ko_pass[0]=0;ko_pass_started[0]=1;trk[0].seq_idx=15;bad+=check("early step zero evaluates the upcoming pattern pass",ko_condition_pass(&trk[0],&early,0));
    ko_roll_enabled=1;ko_roll_div=3;ko_quantize[1]=0;input_on(&trk[1],60,100);uint32_t age=0;for(uint32_t i=0;i<NVOICE;i++)if(trk[1].v[i].active)age=trk[1].v[i].age;
    ko_roll_tick(div_samples(3)-1);uint32_t age2=0;for(uint32_t i=0;i<NVOICE;i++)if(trk[1].v[i].active && trk[1].v[i].age>age2)age2=trk[1].v[i].age;bad+=check("held drum roll waits until chosen subdivision",age==age2);ko_roll_tick(1);uint32_t age3=0;for(uint32_t i=0;i<NVOICE;i++)if(trk[1].v[i].active&&trk[1].v[i].age>age3)age3=trk[1].v[i].age;bad+=check("held drum roll retriggers at subdivision",age3>age);input_off(&trk[1],60);bad+=check("roll key release cancels repeats",!ko_roll[0].on);silence();
    ko.view=25;int16_t gain=trk[0].p[P_LEVEL];ko_knob(2,-5);bad+=check("non-destructive gain changes track level",trk[0].p[P_LEVEL]==gain-5);
    ko_context=0;ko.dirty=1;int stored=ko_store_sector(1);ko_layout[0]=0;memset(&ko_alternate[0],0,sizeof ko_alternate[0]);ko_roll_enabled=0;ko_store_load();ko_context=~0u;bad+=check("alternate layout and roll preferences persist",stored==3&&ko_layout[0]==1&&ko_alternate[0].n&&ko_roll_enabled);
    step_set_condition(&trk[0].step[5],2);project_t p,restored;project_store_t wire;project_capture(&p);bad+=check("conditions, nudges and pitch locks keep FUNA round trip",proj_pack(&wire,&p)&&proj_import(&restored,&wire,sizeof wire)&&step_condition(&restored.t[0].step[5])==2&&step_nudge(&restored.t[0].step[5])==-3);
    bad+=check("sync unity preserves speed",ko_sync_step(32768,22050,44100,4,120)==32768);
    bad+=check("sync scales speed with tempo",ko_sync_step(32768,22050,44100,4,180)==49152);
    bad+=check("sync bounds decoder speed",ko_sync_step(32768,22050,44100,1,240)==65536);
    bad+=check("sync ignores invalid reference",ko_sync_step(32768,22050,0,4,120)==32768);
    ko_context=0;ko_sync[0]=(ko_sync_t){.samples=44100,.beats=4,.on=1};ko_store_sector(1);memset(&ko_sync[0],0,sizeof ko_sync[0]);ko_store_load();bad+=check("sync preferences persist with audio",ko_sync[0].samples==44100&&ko_sync[0].on&&ko_sync[0].beats==4);ko_context=~0u;
    song.sel=2;go_home();ko.view=23;page_tap(B_FX);bad+=check("Song 2 sync controls reachable",ko.view==27);ko_knob(0,1);bad+=check("sync beats editable",ko_sync[1].beats>=1);
    printf("PIGEON performance features: %s\n",bad?"FAILED":"PASSED");return bad!=0;
}
