/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN pattern_base_main
#include "ko_test.c"
int main(void){int bad=0;ui_power_on();memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);fixture(2,0);ko_product_boot();ko_setup();render_blocks(32);go_home();song.sel=0;
 for(uint32_t k=0;k<SAMPLER_PARTS;k++){memset(trk[k].step,0,sizeof trk[k].step);trk[k].step[0]=(step_t){.time=ST_NOTE,.n=1,.note={60},.vel=100};}
 ko_pattern_sel=0;ko_pattern_save();ko.view=8;trk[0].step[0].note[0]=61;press(B_FX);
 bad+=check("FX copies unsaved live phrase without leaving Patterns",ko.view==8 && ko_clip_valid && ko_clipboard.step[0][0].note[0]==61);
 ko_pattern_request(1);press(B_LFO);bad+=check("paste prompts before replacing unsaved phrase",ko_paste_target==1 && !project_used(1));
 trk[0].step[0].note[0]=62;press(B_LFO);bad+=check("editing phrase invalidates earlier paste confirmation",!project_used(1) && trk[0].step[0].note[0]==62);
 trk[0].p[P_LEVEL]=107;press(B_LFO);bad+=check("confirmed paste saves copied phrase and retains mixer",project_used(1) && trk[0].step[0].note[0]==61 && trk[0].p[P_LEVEL]==107);
 trk[0].step[0].note[0]=63;ko_pattern_request(0);char prompt[48]="";ko_pattern_prompt(prompt,sizeof prompt);bad+=check("unsaved load prompt names target without discarding edits",strstr(prompt,"LOAD A?") && trk[0].step[0].note[0]==63 && ko_pattern_sel==1);
 ko_pattern_save();bad+=check("saving keeps current edits and cancels pending discard",!ko_phrase_dirty() && ko_discard_target==255);ko_pattern_request(0);bad+=check("source A remains unchanged",trk[0].step[0].note[0]==60);
 press(B_FX);ko_pattern_request(1);press(B_LFO);bad+=check("occupied destination remains unchanged until confirmation",trk[0].step[0].note[0]==63 && ko_paste_target==1);
 ko.view=0;ko_pattern_poll();ko.view=8;press(B_LFO);bad+=check("leaving Patterns cancels pending paste",trk[0].step[0].note[0]==63 && ko_paste_target==1);press(B_LFO);bad+=check("second paste explicitly replaces destination",trk[0].step[0].note[0]==60 && !ko_phrase_dirty());
 puts(bad?"Pattern editing: FAILED":"Pattern editing: PASSED");return !!bad;}
