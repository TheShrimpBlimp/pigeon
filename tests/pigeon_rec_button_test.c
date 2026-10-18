/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN rec_base_main
#include "ko_test.c"
static void rec_press_edge(void){fm1_in.buttons=host_pressed=1u<<panel.btn[B_REC];ui_input();}
static void rec_release_edge(void){fm1_in.buttons=0;ui_input();}
int main(void){
 int bad=0;ui_power_on();memset(ko_states,0,sizeof ko_states);memset(ko_flash,255,sizeof ko_flash);
 slc_store_boot();fixture(0,0);fixture(1,1);fixture(2,0);ko_product_boot();ko_setup();render_blocks(32);go_home();
 for(uint32_t part=0;part<SAMPLER_PARTS;part++)for(uint32_t roll=0;roll<2;roll++){
  song.sel=part;ko.view=7;ko_piano_roll[part]=roll;song.rec=0;transport_req=0;
  rec_press_edge();bad+=check("REC press arms selected track in both sequence views",song.rec==(1u<<part)&&!transport_req&&!song.playing);
  for(uint32_t n=0;n<5;n++)ui_input();bad+=check("holding REC leaves it armed without opening a layer",song.rec==(1u<<part)&&!ui.layer&&ko.view==7);
  input_on(&trk[part],60,100);input_on(&trk[part],61,100);input_off(&trk[part],60);input_off(&trk[part],61);
  bad+=check("keys record a chord before REC is released",trk[part].step[(ko_seq[part].cursor+trk[part].p[P_SLEN]-1)%trk[part].p[P_SLEN]].n==2);
  rec_release_edge();bad+=check("REC release does not disarm",song.rec==(1u<<part));
  rec_press_edge();bad+=check("second REC press disarms immediately",!song.rec);rec_release_edge();bad+=check("second release stays disarmed",!song.rec);
 }
 for(uint32_t roll=0;roll<2;roll++){
  song.sel=0;ko.view=7;ko_piano_roll[0]=roll;song.rec=0;transport_req=0;cin_bars=0;ko_quantize[0]=0;ko_seq[0].end_mode=0;
  for(uint32_t step=0;step<NSTEP;step++)step_clear(&trk[0].step[step]);ko_seq[0].cursor=0;
  rec_press_edge();rec_release_edge();key_down(white(0));ui_input();render_blocks(2);key_up(white(0));ui_input();render_blocks(2);
  bad+=check("armed physical keys write a stopped sequence step",trk[0].step[0].n==1&&trk[0].step[0].note[0]==60);
  press(B_PLAY);bad+=check("PLAY preserves explicit REC arming",song.rec==1&&transport_req==1);render_blocks(2);
  key_down(white(1));ui_input();render_blocks(2);key_up(white(1));ui_input();render_blocks(2);
  int found=0;for(uint32_t step=0;step<NSTEP;step++)for(uint32_t n=0;n<trk[0].step[step].n;n++)found|=trk[0].step[step].note[n]==61;
  bad+=check("armed physical keys record after starting PLAY",song.playing&&song.rec==1&&found);seq_stop();song.rec=0;
 }
 song.sel=0;ko.view=0;transport_req=0;rec_press_edge();
 bad+=check("waveform REC still requests live recording",song.rec==1&&transport_req==1&&ko.view==0);rec_release_edge();transport_req=0;
 song.playing=1;rec_press_edge();bad+=check("REC press disarms during playback without stopping",!song.rec&&song.playing&&!transport_req);rec_release_edge();
 rec_press_edge();bad+=check("REC press arms during playback without restarting",song.rec==1&&song.playing&&!transport_req);rec_release_edge();seq_stop();song.rec=0;
 song.sel=0;ko.view=29;ko_queue[0].on=1;input_on(trk,60,100);rec_press_edge();
 bad+=check("physical REC cancels queue mode when arming",song.rec==1&&!ko_queue[0].on&&!ko_queue[0].active);rec_release_edge();transport_req=0;song.rec=0;
 ui.menu=1;rec_press_edge();rec_release_edge();bad+=check("REC in settings cannot accidentally arm",!song.rec);ui.menu=0;
 printf("PIGEON REC button: %s\n",bad?"FAILED":"PASSED");return !!bad;
}
