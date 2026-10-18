/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN boot_base_main
#include "ko_test.c"
int main(void)
{
    int bad=0;ui_power_on();memset(ko_states,0,sizeof ko_states);
    memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);
    song.sel=2;ko_context=~0u;ko_states[1].on=1;
    trk[2].eng_req=13;trk[2].p[P_E0]=4;trk[2].p[P_E1]=0;
    trk[2].step[3].note[0]=67;trk[2].p[P_SLEN]=24;
    ko_product_boot();
    bad+=check("boot overrides restored Song 2 with Song 1",song.sel==0);
    bad+=check("both song contexts share enabled sampler state",ko_states[0].on&&ko_states[1].on);
    bad+=check("old source settings become the three product slots",ko_song(trk)&&ko_kit(trk+1)&&ko_song(trk+2));
    bad+=check("repair preserves sequence notes and length",trk[2].step[3].note[0]==67&&trk[2].p[P_SLEN]==24);
    song.sel=2;ui_input();
    bad+=check("switching to Song 2 cannot disable the shared sampler",ko_states[0].on&&ko_states[1].on&&ko_song(trk+2));
    trk_note_on(trk+2,60,100);int32_t audio[CTL*2];int silent=1;
    for(uint32_t n=0;n<100;n++){mix_block(audio,CTL);for(uint32_t i=0;i<CTL*2;i++)silent&=audio[i]==0;}
    bad+=check("empty Song 2 stays silent on the hardware path",silent);
    project_t project;project_capture(&project);project.sel=2;
    project.t[2].p[P_E0]=4;project.t[2].p[P_E1]=0;
    project.sum=proj_sum(&project);
    bad+=check("restoring a project repairs its legacy source",project_restore_runtime(&project)==0&&ko_song(trk+2));
    song.sel=0;ko_context=~0u;ui_input();ko.view=0;go_home();
    fm1_in.buttons=host_pressed=1u<<panel.btn[B_FX];ui_input();
    bad+=check("FX opens on press before release",ko.view==1);
    fm1_in.buttons=0;ui_input();
    bad+=check("FX release does not navigate twice",ko.view==1);
    fm1_in.buttons=host_pressed=1u<<panel.btn[B_HOME];ui_input();
    bad+=check("HOME press returns to waveform",ko.view==0&&!ui.menu&&ui.home);
    fm1_in.buttons=0;ui_input();
    fm1_in.buttons=host_pressed=1u<<panel.btn[B_HOME];ui_input();
    bad+=check("HOME at waveform opens settings without holding",ui.menu==1);
    fm1_in.buttons=0;ui_input();
    fm1_in.buttons=host_pressed=1u<<panel.btn[B_HOME];ui_input();
    bad+=check("HOME closes settings on press",!ui.menu&&ko.view==0);
    fm1_in.buttons=0;ui_input();
    /* Settle pending project-restore panic/engine switches before auditioning. */
    render_blocks(32);
    /* A long ADPCM chop uses an exact checkpoint near its end. */
    ui.menu=0;ko.view=0;song.sel=0;ko_context=~0u;ui_input();
    slc_usr[0].rate=16384;
    ko_table_t *tail_table=ko_edit();tail_table->n=1;
    tail_table->chop[0].a=0;tail_table->chop[0].b=slc_usr[0].len;
    tail_table->chop[0].st=slc_state_at(&slc_usr[0],0);ko_publish(1);
    ko_table_t unchanged=*ko_table();
    for(uint32_t i=0;i<NVOICE;i++)voice_kill(&trk[0].v[i]);
    song.rec=1;ko_seq[0].enabled=1;ko_seq[0].cursor=0;
    step_t before_step=trk[0].step[0];
    int16_t audition_pitch=ko_table()->chop[0].pitch;
    fm1_in.buttons=host_pressed=1u<<panel.btn[B_SCL];ui_input();
    bad+=check("holding SEL preserves waveform and pitch",ko.view==0&&ko_table()->chop[0].pitch==audition_pitch);
    fm1_in.notes=host_notes=1u;keyboard_block();ui_input();
    voice_t *tail_voice=0;
    for(uint32_t i=0;i<NVOICE;i++)if(trk[0].v[i].active)tail_voice=&trk[0].v[i];
    bad+=check("SEL plus chop previews near end",tail_voice&&tail_voice->ph[0]>0&&tail_voice->ph[2]==slc_usr[0].len);
    bad+=check("end preview loops while held without latching",tail_voice&&!(tail_voice->s[4]&KO_V_NO_LOOP)&&!(tail_voice->s[4]&KO_V_LATCH));
    if(tail_voice){
        uint32_t start=tail_voice->ph[0];int wrapped=0;
        for(uint32_t b=0;b<8000 && !wrapped;b++){render_blocks(1);wrapped=tail_voice->active&&tail_voice->ph[0]<start;}
        bad+=check("end audition wraps to original chop start",wrapped && tail_voice->gate && tail_voice->s[6]!=2);
    }
    bad+=check("preview leaves chop boundaries untouched",!memcmp(&unchanged,ko_table(),sizeof unchanged));
    bad+=check("preview never records a step",ko_seq[0].cursor==0&&!memcmp(&before_step,&trk[0].step[0],sizeof before_step));
    fm1_in.notes=host_notes=0;keyboard_block();fm1_in.buttons=0;ui_input();
    bad+=check("end-audition gesture preserves waveform and stored pitch",ko.view==0&&ko_table()->chop[0].pitch==audition_pitch);
    fm1_in.buttons=host_pressed=1u<<panel.btn[B_SCL];ui_input();
    bad+=check("plain SEL waits for release on waveform",ko.view==0);
    fm1_in.buttons=0;ui_input();bad+=check("plain SEL tap opens patterns",ko.view==8);
    press(B_HOME);
    fm1_in.buttons=host_pressed=1u<<panel.btn[B_OCTDN];ui_input();fm1_in.buttons=0;ui_input();
    bad+=check("ordinary OCT- tap still lowers pitch",ko_table()->chop[0].pitch==audition_pitch-1);
    ko_pitch_turn(1);
    bad+=check("releasing audition note stops its loop",tail_voice&&!tail_voice->gate&&tail_voice->stage==3);
    for(uint32_t i=0;i<NVOICE;i++)voice_kill(&trk[0].v[i]);
    trk_note_on(trk,60,100);tail_voice=0;
    for(uint32_t i=0;i<NVOICE;i++)if(trk[0].v[i].active)tail_voice=&trk[0].v[i];
    bad+=check("ordinary triggers still begin at chop start",tail_voice&&tail_voice->ph[0]==0);
    printf("PIGEON boot: %s\n",bad?"FAILED":"PASSED");return !!bad;
}
