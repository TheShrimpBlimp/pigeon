/* SPDX-License-Identifier: GPL-3.0-only */
#define KO_TEST_MAIN ko_controls_test_main
#include "ko_test.c"
static void clear_voices(void) { for(uint32_t p=0;p<SAMPLER_PARTS;p++) {memset(trk[p].v,0,sizeof trk[p].v);trk[p].seq_n=0;} }
static uint32_t run_first(int pitch)
{
    seq_stop();clear_voices();song.sel=0;ko_select(0);ko_pitch_turn(pitch-ko_table()->chop[0].pitch);
    seq_start();ko_end_tick(&trk[0],CTL);uint32_t frames=0;
    while(trk[0].seq_idx==0 && frames<2000) {render_blocks(1);frames++;}
    seq_stop();return frames;
}
int main(void)
{
    int bad=0;ui_power_on();memset(ko_states,0,sizeof ko_states);memset(ko_seq,0,sizeof ko_seq);
    memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);menu_put(MI_KO,1);ko_states[0].on=1;ko_setup();render_blocks(32);
    ko_table_t tab={0};tab.n=32;for(uint32_t i=0;i<32;i++){tab.chop[i].a=i*1024;tab.chop[i].b=(i+1)*1024;}
    bad+=check("32-chop table retains existing metadata ABI",ko_restore(&tab) && sizeof(ko_record_t)==544);
    int all=1;uint32_t seen=0;for(uint32_t k=0;k<27;k++){int pad=ko_pad(k);seen|=1u<<pad;all &= kb_map(&trk[0],k)==60u+pad;}
    bad+=check("all 27 keys trigger distinct song chops",all && seen==((1u<<27)-1u));
    ko_select(27);bad+=check("second bank reaches chops 28..32",ko.bank==1 && kb_map(&trk[0],0)==87 && kb_map(&trk[0],9)==255);ko_select(0);
    uint8_t mute=trk[0].p[P_MUTE];go_home();fm1_in.notes=host_notes=1u<<1;frame();fm1_in.notes=0;frame();
    bad+=check("black song key selects chop without muting",ko.selected==16 && trk[0].p[P_MUTE]==mute);
    page_tap(B_SEQ);bad+=check("SEQ opens sixteen-step mode",ko.view==7 && ko_seq[0].enabled && trk[0].p[P_SLEN]==16);
    transport_req=0;rec_tap();bad+=check("REC arms stopped step entry without starting transport",song.rec==1 && !song.playing && !transport_req);
    ko_seq[0].cursor=15;input_on(&trk[0],76,100);
    bad+=check("stopped step entry writes chop and wraps after step sixteen",trk[0].step[15].n==1 && trk[0].step[15].note[0]==76 && ko_seq[0].cursor==0);
    input_off(&trk[0],76);bad+=check("record LED shows current white-key step",ko_key_leds()==1u);
    ko_seq[0].cursor=3;press(B_EDIT);bad+=check("EDIT inserts a rest and advances cursor",trk[0].step[3].time==ST_REST && ko_seq[0].cursor==4);
    song.rec=0;for(uint32_t p=0;p<SAMPLER_PARTS;p++)for(uint32_t i=0;i<NSTEP;i++)step_clear(&trk[p].step[i]);
    for(uint32_t i=0;i<2;i++){step_t *s=&trk[0].step[i];s->time=ST_NOTE;s->n=1;s->note[0]=60+i;s->vel=100;}
    trk[0].p[P_SDIV]=2;trk[0].p[P_SQNT]=1;ko_seq[0].end_mode=1;ko_select(0);ko_flag_turn(KO_LATCH,1);
#if FELUCCA_SAMPLER_ONLY
    /* A reboot/project restore clears this transient flag. No SEQ tap follows. */
    ko_seq[0].enabled=0;
    bad+=check("restored sample-end mode works before opening SEQ",ko_seq_end(&trk[0]));
#endif
    clear_voices();seq_start();ko_end_tick(&trk[0],CTL);
    int one=0;for(uint32_t i=0;i<NVOICE;i++)if(trk[0].v[i].active)one|=!!(trk[0].v[i].s[4]&KO_V_NO_LOOP)&&!(trk[0].v[i].s[4]&KO_V_LATCH);
    bad+=check("sample-end playback overrides loop/latch without editing chop flags",one && (ko_table()->chop[0].rsv&KO_LATCH) && !(ko_table()->chop[0].rsv&KO_NO_LOOP));
    uint32_t before=trk[0].seq_idx;ko_end_tick(&trk[0],CTL);bad+=check("sample-end step waits for its actual voice",trk[0].seq_idx==before);
    seq_stop();uint32_t normal=run_first(0), high=run_first(12), low=run_first(-12);
    bad+=check("sample-end progression follows actual pitched duration",normal>10 && high<normal && low>normal && low<2000);
    ko_seq[0].end_mode=0;clear_voices();seq_start();seq_tick(&trk[0],CTL);uint32_t period=step_samples(&trk[0],div_samples(2),0);
    seq_tick(&trk[0],period-1);bad+=check("tempo mode holds step until tempo boundary",trk[0].seq_idx==0);seq_tick(&trk[0],1);
    bad+=check("tempo mode advances on tempo boundary",trk[0].seq_idx==1);seq_stop();
    clear_voices();trk[0].v[0].active=1;trk[0].v[0].note=76;trk[0].v[0].s[6]=1;
    bad+=check("playback LED lights the black key of sounding chop",ko_key_leds()==(1u<<1));
    song.rec=1;song.playing=1;trk[0].seq_idx=10;bad+=check("live recording LED follows step eleven",ko_key_leds()==(1u<<18));song.rec=song.playing=0;
    clear_voices();ko_seq[0].end_mode=1;seq_start();ko_end_tick(&trk[0],CTL);clear_voices();ko_end_tick(&trk[0],CTL);
    bad+=check("voice stealing cannot deadlock sample-end mode",trk[0].seq_idx==1);seq_stop();
    clear_voices();trk[0].step[0].n=2;trk[0].step[0].note[1]=61;
    seq_start();ko_end_tick(&trk[0],CTL);int chord_voices=0;
    for(uint32_t i=0;i<NVOICE;i++) if(trk[0].v[i].active){chord_voices++;if(trk[0].v[i].note==60)trk[0].v[i].active=0;}
    ko_end_tick(&trk[0],CTL);bad+=check("sample-end chord waits for its longest remaining voice",chord_voices==2 && trk[0].seq_idx==0);
    clear_voices();ko_end_tick(&trk[0],CTL);bad+=check("chord advances after all tracked voices finish",trk[0].seq_idx==1);
    clear_voices();ko_end_tick(&trk[0],CTL);uint32_t rest_idx=trk[0].seq_idx;
    ko_end_tick(&trk[0],CTL);bad+=check("empty step waits one tempo step instead of spinning",rest_idx==2 && trk[0].seq_idx==2);
    trk[0].seq_idx=15;trk[0].seq_pos=period;memset(ko_seq[0].age,0,sizeof ko_seq[0].age);ko_end_tick(&trk[0],CTL);
    bad+=check("sample-end sequence wraps exactly sixteen steps",trk[0].seq_idx==0);seq_stop();clear_voices();
    trk[0].p[P_SLEN]=3;seq_start();trk[0].seq_idx=2;trk[0].seq_pos=period;memset(ko_seq[0].age,0,sizeof ko_seq[0].age);ko_end_tick(&trk[0],CTL);
    bad+=check("sample-end wraps at custom three-step length",trk[0].seq_idx==0);seq_stop();clear_voices();trk[0].p[P_SLEN]=16;
    ko_seq[1].enabled=ko_seq[1].end_mode=1;trk[1].p[P_SLEN]=16;
    for(uint32_t i=0;i<16;i++)step_clear(&trk[1].step[i]);trk[1].step[0].time=ST_NOTE;trk[1].step[0].n=1;trk[1].step[0].note[0]=60;trk[1].step[0].vel=100;
    seq_start();ko_end_tick(&trk[1],CTL);ko_end_tick(&trk[1],CTL);
    bad+=check("drum sample-end step waits for one-shot completion",trk[1].seq_idx==0);
    uint32_t kit_frames=0;while(trk[1].seq_idx==0 && kit_frames<2000){render_blocks(1);kit_frames++;}
    bad+=check("drum sample-end mode advances after the hit",trk[1].seq_idx==1 && kit_frames>10 && kit_frames<2000);seq_stop();
    song.sel=0;ko.view=7;track_select(1);bad+=check("track selection keeps sequence page on enabled kit",ko.view==7);ko_seq[0].enabled=0;track_select(0);
    bad+=check("unconfigured track returns to sample page",ko.view==0);
    ko_seq[0].enabled=ko_seq[0].end_mode=1;clear_voices();trk[0].xf_on=1;trk[0].xf=0;
    seq_start();ko_end_tick(&trk[0],CTL);bad+=check("sample-end captures queued attacks during engine fade",ko_seq[0].pending && trk[0].xp_n);
    ko_end_tick(&trk[0],CTL);bad+=check("pending engine attack does not advance a step",trk[0].seq_idx==0);
    engine_block(&trk[0]);ko_end_tick(&trk[0],CTL);int pending_ok=0;
    for(uint32_t i=0;i<NVOICE;i++)if(trk[0].v[i].active) pending_ok|=(trk[0].v[i].s[4]&KO_V_NO_LOOP) && !(trk[0].v[i].s[4]&KO_V_LATCH);
    bad+=check("delayed attack retains complete-chop timing",!ko_seq[0].pending && pending_ok && trk[0].seq_idx==0);seq_stop();
    clear_voices();go_home();song.sel=0;ko.view=7;ko_seq[0].enabled=1;song.rec=1;transport_req=0;
    press(B_PLAY);
    bad+=check("PLAY after step entry starts playback and disarms recording",transport_req==1 && !(song.rec&1));
    song.playing=1;transport_req=0;trk[0].seq_idx=4;trk[0].v[0].active=1;trk[0].v[0].note=63;trk[0].v[0].s[6]=1;
    bad+=check("step five playing seventh physical key lights chop seven, not step five",ko_key_leds()==(1u<<6));
    rec_tap();bad+=check("REC during playback restores current-step recording LED",(song.rec&1) && ko_key_leds()==(1u<<7));
    song.rec=0;seq_stop();clear_voices();
    const char *screens=getenv("KO_SCREEN_DIR");if(screens) {ko_palette_on=1;palette_set(settings.palette);song.sel=0;go_home();ko.view=7;screen_save(screens,"sequencer");ko.view=6;screen_save(screens,"performance");}
    printf("KO sequencer: %s\n",bad?"FAILED":"PASSED");return bad!=0;
}
