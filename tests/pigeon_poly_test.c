/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN poly_base_main
#include "ko_test.c"
int main(void) {
 int bad=0;ui_power_on();memset(ko_states,0,sizeof ko_states);
 memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);fixture(2,0);
 ko_product_boot();ko_setup();render_blocks(32);go_home();
 for(uint32_t part=0;part<SAMPLER_PARTS;part++)for(uint32_t roll=0;roll<2;roll++) {
  song.sel=part;ko.view=7;ko_piano_roll[part]=roll;ko_seq[part].enabled=1;ko_seq[part].cursor=15;trk[part].p[P_SLEN]=16;song.rec=1u<<part;
  for(uint32_t n=60;n<65;n++)input_on(&trk[part],n,100);
  step_t *s=&trk[part].step[15];
  bad+=check("both views/all parts retain four notes on one armed step",s->n==4 && s->note[0]==60 && s->note[3]==63 && ko_seq[part].cursor==15);
  for(uint32_t n=60;n<64;n++)input_off(&trk[part],n);
  bad+=check("extra held note cannot overwrite or advance the chord",s->n==4 && ko_seq[part].cursor==15);
  input_off(&trk[part],64);
  bad+=check("last release advances once and wraps sequence",ko_seq[part].cursor==0);
  input_on(&trk[part],65,100);input_off(&trk[part],65);
  bad+=check("single-note entry still advances once",trk[part].step[0].n==1 && ko_seq[part].cursor==1);
  song.rec=0;ko_seq[part].cursor=2;step_clear(&trk[part].step[2]);
  for(uint32_t k=0;k<4;k++){key_down(white(k));frame();key_up(white(k));frame();}
  bad+=check("unarmed keys add four notes without moving cursor",trk[part].step[2].n==4 && ko_seq[part].cursor==2);
  key_down(white(1));frame();key_up(white(1));frame();
  bad+=check("unarmed key toggles only its own note",trk[part].step[2].n==3 && trk[part].step[2].note[1]==62);
 }
 song.sel=0;song.rec=0;ko.view=7;ko_seq[0].end_mode=0;
 for(uint32_t p=0;p<SAMPLER_PARTS;p++){memset(trk[p].v,0,sizeof trk[p].v);trk[p].seq_n=0;for(uint32_t s=0;s<NSTEP;s++)step_clear(&trk[p].step[s]);}
 trk[0].step[0]=(step_t){.time=ST_NOTE,.n=4,.note={60,61,62,63},.vel=100};
 seq_start();seq_tick(&trk[0],CTL);uint32_t notes=0;
 for(uint32_t v=0;v<NVOICE;v++)if(trk[0].v[v].active && trk[0].v[v].note>=60 && trk[0].v[v].note<64)notes|=1u<<(trk[0].v[v].note-60);
 bad+=check("sequencer playback sounds every note in the chord",notes==15);seq_stop();
 /* Live overdub uses the same four-note storage without replacing the fourth note. */
 trk[0].seq_idx=0;trk[0].seq_pos=0;step_clear(&trk[0].step[0]);
 for(uint32_t n=60;n<65;n++)rec_note(&trk[0],n,100);
 bad+=check("live overdub retains four notes and ignores a fifth",trk[0].step[0].n==4 && trk[0].step[0].note[3]==63);
 song.rec=1;ko_seq[0].cursor=3;input_on(&trk[0],60,100);rec_tap();input_off(&trk[0],60);
 bad+=check("disarming a held chord does not advance cursor",!song.rec && ko_seq[0].cursor==3);
 song.rec=1;input_on(&trk[0],60,100);seq_start();input_off(&trk[0],60);
 bad+=check("starting transport cancels stopped chord entry",!ko_entry[0].active);seq_stop();song.rec=0;
 project_t project;project_store_t store;project_capture(&project);
 bad+=check("four-note steps survive existing project format",proj_pack(&store,&project)&&proj_import(&proj_scratch,&store,sizeof store)&&proj_scratch.t[0].step[0].n==4&&proj_scratch.t[0].step[0].note[3]==63);
 ui_leds(); /* Host matrix is a zero stub; use the measured REC LED position. */
 led_pos[panel.btn[B_REC]]=(7u<<3)|2u;
 for(uint32_t mode=0;mode<LEDS_COUNT;mode++){
  settings_leds=mode;song.rec=1;ui.layer=0;ui_leds();
  bad+=check("REC stays lit when armed in every LED mode",led_of(fm1_led,panel.btn[B_REC])&&!led_of(fm1_led_breath,panel.btn[B_REC]));
  song.rec=0;ui_leds();bad+=check("REC turns off when disarmed",!led_of(fm1_led,panel.btn[B_REC]));
 }
 printf("PIGEON polyphony: %s\n",bad?"FAILED":"PASSED");return !!bad;
}
