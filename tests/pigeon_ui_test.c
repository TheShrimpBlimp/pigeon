/* SPDX-License-Identifier: GPL-3.0-only */
#define KO_TEST_MAIN ko_controls_test_main
#include "ko_test.c"
int main(void)
{
    int bad=0;
    song.sel=0;song.rec=0;song.playing=0;
    bad+=check("stopped sequencer explains recording gesture",!strcmp(ko_seq_status(),"STOPPED / REC TO ARM"));
    song.rec=1;bad+=check("armed status belongs to selected track",!strcmp(ko_seq_status(),"REC ARMED / OCT STEP SIZE"));
    song.playing=1;bad+=check("armed playback shows recording",!strcmp(ko_seq_status(),"RECORDING / OCT STEP SIZE"));
    song.sel=2;bad+=check("other track arm does not label Song 2 recording",!strcmp(ko_seq_status(),"PLAYING / OCT STEP SIZE"));
    cin_left=1;bad+=check("count-in state is explicit",!strcmp(ko_seq_status(),"COUNT-IN / OCT STEP SIZE"));cin_left=0;
    song.sel=0;song.rec=0;song.playing=0;ui_power_on();memset(ko_states,0,sizeof ko_states);
    memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);
    menu_put(MI_KO,1);ko_states[0].on=1;ko_setup();render_blocks(32);song.sel=0;go_home();
    ko_select(2);ko.zoom=3;ui.force=1;ko_draw();
    /* Hold the viewport fixed while moving a marker. Only its old and new
     * columns should be repainted, with no intermediate blank pixel. */
    ko_follow_a=ko_wave_a;uint32_t cursor=ko.cursor;
    ko_table_t *d=ko_edit();d->chop[ko.selected].b-=17;ko_publish(1);
    ko_follow_a=ko_wave_a;host_fl_begin();ko_draw();uint32_t flashes=host_fl_end(26,240), written=0;
    for(uint32_t i=98*240;i<167*240;i++) written+=host_fl_n[i]!=0;
    bad+=check("marker edit has no intermediate blank pixels",!flashes);
    bad+=check("marker-only edit repaints at most four waveform columns",written<=4*69 && ko.cursor==cursor);
    host_fl_begin();ui.force=1;ko_draw();written=0;
    for(uint32_t i=26*240;i<240*240;i++) written+=host_fl_n[i]!=0;
    flashes=host_fl_end(26,240);
    bad+=check("unchanged forced frame leaves sampler body untouched",!written && !flashes);
    ko_manual_ms=fm1_ms;voice_t *v=&trk[0].v[0];memset(v,0,sizeof *v);v->active=1;v->s[6]=1;v->age=1;v->ph[0]=20000;
    uint32_t old_a=ko_wave_a, selected=ko.selected;ko_draw();
    bad+=check("manual edit pauses automatic following",ko_wave_a==old_a);
    fm1_ms+=1001;host_fl_begin();ko_draw();flashes=host_fl_end(26,240);
    bad+=check("playback follows into visible window without changing edit selection",ko_wave_a<=v->ph[0] && v->ph[0]-ko_wave_a<ko_wave_span && ko.selected==selected && ko.cursor==cursor && !flashes);
    host_fl_begin();v->ph[0]+=ko_wave_span/(KO_WAVE_COLS-1)+1;ko_playhead();flashes=host_fl_end(170,173);
    bad+=check("overlapping playback cursor positions avoid intermediate blank pixels",!flashes);
    v->ph[0]=2048;ko_draw();bad+=check("loop restart follows back to earlier sample position",ko_wave_a<=2048 && 2048-ko_wave_a<ko_wave_span);
    v->active=0;ko.view=7;ko_seq[0].enabled=1;ui.force=1;ko_draw();
    trk[0].p[P_SDIV]=2;uint32_t sixteenth=div_samples(2);press(B_OCTDN);
    bad+=check("sequencer OCT down selects sixteenth triplet-neighbor division in logical order",trk[0].p[P_SDIV]==4);
    press(B_OCTDN);bad+=check("sequencer supports eighth-note step length",trk[0].p[P_SDIV]==1 && abs((int32_t)div_samples(1)-(int32_t)(sixteenth*2))<=1);
    uint32_t eighth_period=step_samples(&trk[0],div_samples(trk[0].p[P_SDIV]),0);
    ko_seq[0].end_mode=0;seq_start();seq_tick(&trk[0],CTL);seq_tick(&trk[0],eighth_period-1);
    bad+=check("eighth-note selection holds until its actual sequencer boundary",trk[0].seq_idx==0);
    seq_tick(&trk[0],1);bad+=check("eighth-note selection advances on its actual boundary",trk[0].seq_idx==1);seq_stop();
    project_t project;project_store_t store;project_capture(&project);
    int saved=proj_pack(&store,&project) && proj_import(&proj_scratch,&store,sizeof store);
    bad+=check("chosen eighth-note division survives project serialization",saved && proj_scratch.t[0].p[P_SDIV]==1);
    press(B_OCTUP);press(B_OCTUP);bad+=check("sequencer OCT up returns to sixteenth-note steps",trk[0].p[P_SDIV]==2);
    host_fl_begin();ko_seq[0].cursor=1;ko_draw();flashes=host_fl_end(26,240);
    bad+=check("sequencer highlight changes without clearing cells first",!flashes);
    trk[0].p[P_SLEN]=16;ko_seq[0].cursor=15;ko_knob(3,16);
    bad+=check("K4 expands sequence to 32 steps",trk[0].p[P_SLEN]==32);
    ko_knob(2,16);bad+=check("cursor reaches second grid page",ko_seq[0].cursor==31);
    host_fl_begin();ko_draw();flashes=host_fl_end(26,240);
    bad+=check("grid page transition stays buffered",!flashes);
    step_t preserved=trk[0].step[31];ko_knob(3,-24);
    bad+=check("shortening clamps cursor and preserves later notes",trk[0].p[P_SLEN]==8 && ko_seq[0].cursor==7 && !memcmp(&preserved,&trk[0].step[31],sizeof preserved));
    ko_knob(3,100);bad+=check("sequence supports 64-step maximum",trk[0].p[P_SLEN]==64);
    ko_knob(3,-100);bad+=check("sequence supports single-step minimum",trk[0].p[P_SLEN]==1 && ko_seq[0].cursor==0);
    press(B_SEQ);press(B_SEQ);bad+=check("reopening sequencer preserves chosen length",trk[0].p[P_SLEN]==1);
    ko_seq[0].end_mode=0;seq_start();seq_tick(&trk[0],CTL);seq_tick(&trk[0],div_samples(trk[0].p[P_SDIV])*2);
    bad+=check("one-step tempo sequence wraps at chosen length",trk[0].seq_idx==0);seq_stop();
    project_capture(&project);saved=proj_pack(&store,&project)&&proj_import(&proj_scratch,&store,sizeof store);
    bad+=check("sequence length survives project serialization",saved && proj_scratch.t[0].p[P_SLEN]==1);
    trk[0].p[P_SLEN]=16;
    ko.view=0;ko_select(2);ko.zoom=5;memset(ui.enc_t,0,sizeof ui.enc_t);fm1_ms+=1000;
    uint32_t start=ko_table()->chop[2].a, end=ko_table()->chop[2].b;
    ko_knob(0,1);uint32_t start_delta=ko_table()->chop[2].a-start;ko_knob(1,1);
    bad+=check("K1 has twice K2 boundary sensitivity at the same zoom",start_delta==2*(ko_table()->chop[2].b-end) && start_delta>0);
    bad+=check("faster K1 retains exact decoder checkpoint",ko_table()->chop[2].st==slc_state_at(slc_get(1),ko_table()->chop[2].a));
    /* Return from a non-KO page must invalidate cached pixels. */
    ui.menu=1;ui_draw();ui.menu=0;ui.force=1;ko_draw();bad+=check("return from menu rebuilds sampler cache",ko_ui_valid && ko_wave_span);
    ko.view=1;ui.force=1;ko_draw();host_fl_begin();ko_knob(1,1);ko_draw();flashes=host_fl_end(26,240);
    bad+=check("FX value changes redraw buffered text without intermediate blanks",!flashes);
    ko.view=8;ko_launch_pending=2;chain_config.rsv[0]=1;ui.force=1;ko_draw();
    int label=0;for(uint32_t i=0;i<ko_line_n;i++)if(ko_lines[i].y==140)label=!strcmp(ko_lines[i].text,"PATTERN END: C");
    bad+=check("queued pattern label names pattern-end preference",label);
    chain_config.rsv[0]=0;ui.force=1;ko_draw();label=0;
    for(uint32_t i=0;i<ko_line_n;i++)if(ko_lines[i].y==140)label=!strcmp(ko_lines[i].text,"NEXT BAR: C");
    bad+=check("queued pattern label follows bar preference",label);ko_launch_pending=255;
    ko.view=27;ko_sync[0]=(ko_sync_t){.samples=((slc_get(1)->rate*44100u+32768u)>>16)*2u,.beats=4,.on=1};song.g[G_BPM]=120;ui.force=1;ko_draw();
    song.g[G_BPM]=180;ui.force=0;ko_draw();label=0;
    for(uint32_t i=0;i<ko_line_n;i++)if(ko_lines[i].y==205)label=!strcmp(ko_lines[i].text,"SPEED 150% / PITCH FOLLOWS");
    bad+=check("sync speed display follows external tempo without forced redraw",label);song.g[G_BPM]=120;
    const char *screens=getenv("KO_SCREEN_DIR");if(screens) {ko_palette_on=1;palette_set(settings.palette);ko.view=7;screen_save(screens,"pigeon-sequencer");ko.view=0;screen_save(screens,"pigeon-waveform");}
    printf("PIGEON UI: %s\n",bad?"FAILED":"PASSED");return bad!=0;
}
