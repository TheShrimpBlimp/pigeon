/* SPDX-License-Identifier: GPL-3.0-only */
#define KO_TEST_MAIN ko_controls_test_main
#include "ko_test.c"
static void quiet(void) {seq_stop();transport_req=0;for(uint32_t p=0;p<SAMPLER_PARTS;p++) trk_all_off(&trk[p]);render_blocks(300);}
static void start_requested(void) {transport_req=0;seq_start();for(uint32_t p=0;p<SAMPLER_PARTS;p++) seq_tick(&trk[p],0);}
static void ticks(uint32_t count)
{
    uint32_t period=div_samples(2);
    for(uint32_t i=0;i<count;i++) {chain_tick(period);for(uint32_t p=0;p<SAMPLER_PARTS;p++) seq_tick(&trk[p],period);}
}
int main(void)
{
    int bad=0;ui_power_on();memset(ko_states,0,sizeof ko_states);memset(ko_flash,255,sizeof ko_flash);
    slc_store_boot();fixture(0,0);fixture(1,1);menu_put(MI_KO,1);ko_states[0].on=1;ko_setup();quiet();song.sel=0;go_home();
    for(uint32_t p=0;p<SAMPLER_PARTS;p++) {
        for(uint32_t i=0;i<NSTEP;i++)step_clear(&trk[p].step[i]);
        trk[p].p[P_SLEN]=16;trk[p].p[P_SDIV]=2;trk[p].p[P_SSWING]=0;trk[p].p[P_SQNT]=1;
        trk[p].step[0].time=ST_NOTE;trk[p].step[0].n=1;trk[p].step[0].note[0]=60;trk[p].step[0].vel=100;
    }
    bad+=check("runtime pattern cache stores only active lanes",sizeof(chain.source[0].step)==UI_TRACK_COUNT*NSTEP*sizeof(step_t));
    song.g[G_SWING]=0;ko_pattern_sel=0;ko_pattern_save();
    bad+=check("pattern A uses an existing project slot",project_used(0) && ko_pattern_mask==1);
    trk[0].step[5]=trk[0].step[0];trk[0].step[5].note[0]=62;trk[0].step[5].probability=55;trk[0].step[5].flags=SF_RATCH;
    motion_set_lock(&trk[0],5,P_REV,40);ko_pattern_copy();
    ko_pattern_select(1);trk[0].p[P_LEVEL]=107;ko_pattern_paste();
    bad+=check("paste asks before replacing phrase data",ko_paste_target==1 && !project_used(1));ko_pattern_paste();
    bad+=check("copy includes unsaved edits, probability and ratchets",project_used(1) && proj_scratch.t[0].step[5].note[0]==62 && proj_scratch.t[0].step[5].probability==55 && proj_scratch.t[0].step[5].flags==SF_RATCH);
    bad+=check("paste retains motion locks and current mixer levels",proj_scratch.motion.count==motion.count && motion.count && trk[0].p[P_LEVEL]==107);
    trk[0].step[0].note[0]=63;ko_pattern_save();ko_pattern_select(0);
    bad+=check("duplicating and editing B leaves source A unchanged",trk[0].step[0].note[0]==60 && trk[0].step[5].n==0);
    uint32_t level=trk[0].p[P_LEVEL];ko_pattern_preload();seq_start();
    uint32_t bar=16*div_samples(2);ko_pattern_select(1);chain_tick(bar-1);
    bad+=check("live pattern switch waits for next bar",!chain.running && ko_launch_pending==1 && trk[0].step[0].note[0]==60);
    chain_tick(1);bad+=check("next-bar launch reads B without copying editable steps or changing sound",chain.running && ko_pattern_active==1 && seq_steps(&trk[0])[0].note[0]==63 && trk[0].step[0].note[0]==60 && trk[0].p[P_LEVEL]==level);
    bad+=check("empty patterns cannot be queued",!ko_pattern_select(2) && ko_launch_pending==255);
    ko.view=8;ko_arrange_turn(1,1);bad+=check("live K2 selects full-pattern boundary",chain_config.rsv[0]==1);
    ko_pattern_select(0);ko_pattern_select(0);bad+=check("selecting pending phrase cancels launch",ko_launch_pending==255);
    ko_pattern_select(0);trk[0].p[P_SLEN]=32;trk[0].seq_idx=15;trk[0].seq_pos=0;chain_tick(bar);
    bad+=check("pattern-end mode does not switch at a shorter bar",ko_launch_pending==0 && ko_pattern_active==1);
    trk[0].seq_idx=31;trk[0].seq_pos=step_samples(trk,div_samples(trk[0].p[P_SDIV]),31)-1;chain_tick(1);
    bad+=check("pattern end launches all tracks together",ko_launch_pending==255 && ko_pattern_active==0 && chain.lane[0]==0 && chain.lane[1]==0 && chain.lane[2]==0);
    ko_arrange_turn(1,-1);bad+=check("live K2 can restore next-bar launches",chain_config.rsv[0]==0);

    ko_pattern_select(0);ko_pattern_select(1);bad+=check("last queued pattern wins",ko_launch_pending==1);chain_tick(bar);
    bad+=check("single pattern keeps looping",song.playing && chain.running && ko_pattern_active==1);
    ko_pattern_copy();bad+=check("copy during playback leaves clipboard unchanged",ko_copy_slot==0);
    quiet();ko_pattern_poll();bad+=check("stopping makes the last playing pattern editable",ko_pattern_sel==1 && trk[0].step[0].note[0]==63 && !ko_launch_ready && ko_launch_pending==255);
    ko_pattern_sel=0;ko_pattern_start(0);start_requested();ticks(48);
    bad+=check("pattern playback loops past three bars",song.playing && chain.running && ko_launch_mode);quiet();ko_pattern_poll();
    chain_defaults(&chain_config);ko.view=9;ko_section=0;ko_section_add(0);ko_arrange_turn(2,1);ko_section_add(1);
    bad+=check("arrangement appends sections and repeat counts",chain_config.count==2 && chain_config.row[0].repeat==2 && chain_config.row[1].slot[0]==1);
    ko_pattern_start(1);start_requested();ticks(16);
    bad+=check("section A repeats before advancing",chain.running && chain.row==0 && chain.remaining==1);ticks(16);
    bad+=check("chain advances all sampler tracks to section B together",chain.running && chain.row==1 && chain.lane[0]==1 && chain.lane[1]==1 && (SAMPLER_PARTS<3 || chain.lane[2]==1));ticks(16);
    bad+=check("arrangement stops after its final repeat",!song.playing && !chain.running);quiet();
    ko_pattern_sel=0;ko_pattern_save();project_used(0);
    if(FELUCCA_SAMPLER_ONLY){
        chain.running=1;chain.lane[NTRK-1]=0;
        bad+=check("legacy hidden lane cannot read beyond compact pattern cache",seq_steps(&trk[NTRK-1])==chain_silent);
        chain.running=0;
    }
    bad+=check("arrangement round-trips through unchanged FUNA format",proj_scratch.chain.count==2 && proj_scratch.chain.row[0].repeat==2 && sizeof(ko_record_t)==544);
    ko.view=9;ko_section=0;ko_section_delete();bad+=check("delete removes selected section and closes the gap",chain_config.count==1 && chain_config.row[0].slot[0]==1);
    while(chain_config.count<CHAIN_ROWS) ko_section_add(0);ko_section_add(1);bad+=check("arrangement respects sixteen-section limit",chain_config.count==CHAIN_ROWS && chain_valid(&chain_config));
    ko.view=0;page_tap(B_SCL);bad+=check("SCL opens pattern selection",ko.view==8);
    frame();uint32_t held_before=trk[0].nheld;fm1_in.notes=host_notes=1u;keyboard_block();frame();fm1_in.notes=host_notes=0;keyboard_block();frame();
    bad+=check("pattern selection keys do not trigger samples",song.grid==4 && trk[0].nheld==held_before);
    page_tap(B_SCL);bad+=check("SCL opens arrangement then returns to waveform",ko.view==9);page_tap(B_SCL);bad+=check("third SCL tap returns waveform",ko.view==0);
    page_tap(B_ARP);bad+=check("ARP opens mixer levels",ko.view==10);
    if(FELUCCA_KO_DUAL) {track_select(2);bad+=check("mixer page stays open when changing selected track",ko.view==10);track_select(0);}
    uint32_t old=trk[1].p[P_LEVEL];ko_knob(1,-1);bad+=check("mixer K2 adjusts kit level from Song 1",trk[1].p[P_LEVEL]==old-1);
    page_tap(B_ARP);bad+=check("mixer retains mute and solo page",ko.view==6);ko_knob(1,1);bad+=check("mixer mute acts on its own track",trk[1].p[P_MUTE]==1);ko_knob(1,-1);
    page_tap(B_ARP);old=trk[0].p[P_DLY];ko_knob(0,1);bad+=check("delay mixer has independent track sends",ko.view==11 && trk[0].p[P_DLY]>old && trk[1].p[P_DLY]==0);
    page_tap(B_ARP);ko_knob(1,1);bad+=check("chorus mixer adjusts kit send",ko.view==12 && trk[1].p[P_CHOR]>0);
    page_tap(B_ARP);ko_knob(0,1);bad+=check("reverb mixer adjusts song send",ko.view==13 && trk[0].p[P_REV]>0);
    ko_draw();host_fl_begin();ko_knob(0,1);ko_draw();bad+=check("mixer edits use buffered partial updates",!host_fl_end(26,240));
    const char *screens=getenv("KO_SCREEN_DIR");if(screens){ko_palette_on=1;palette_set(settings.palette);ko.view=8;screen_save(screens,"patterns");ko.view=9;screen_save(screens,"arrangement");ko.view=10;screen_save(screens,"mixer");}
    quiet();ko_pattern_sel=0;ko_pattern_start(0);start_requested();
    project_t loaded;project_capture(&loaded);loaded.t[0].step[0].note[0]=65;loaded.sum=proj_sum(&loaded);
    project_restore_runtime(&loaded);ko_pattern_poll();
    bad+=check("project load cancels deferred pattern commit",trk[0].step[0].note[0]==65 && ko_pattern_commit==255);
    quiet();chain.running=0;trk[0].p[P_SLEN]=1;trk[0].seq_idx=0;trk[0].seq_pos=0;ko_seq[0].pending=0;memset(ko_seq[0].age,0,sizeof ko_seq[0].age);ko_seq[0].age[0]=123;trk[0].v[0].age=123;trk[0].v[0].active=1;trk[0].v[0].s[6]=1;
    bad+=check("sample-end launch waits for the last chop",!ko_end_boundary(trk,FS*10));trk[0].v[0].s[6]=2;
    bad+=check("sample-end launch detects the finished last chop",ko_end_boundary(trk,CTL));
    printf("KO arrangement: %s\n",bad?"FAILED":"PASSED");return bad!=0;
}
