/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN piano_base_main
#include "ko_test.c"
int main(void) {
 int bad=0;ui_power_on();memset(ko_states,0,sizeof ko_states);
 memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);fixture(2,0);
 ko_product_boot();ko_setup();go_home();song.sel=0;ko.view=0;
 trk[0].p[P_SLEN]=64;
 for(uint32_t i=0;i<64;i++)trk[0].step[i]=(step_t){.time=ST_NOTE,.n=1,.note={60+i%8},.vel=100};
 press(B_SEQ);bad+=check("SEQ opens step grid",ko.view==7&&!ko_piano_roll[0]);
 press(B_SEQ);bad+=check("second SEQ opens piano roll",ko.view==7&&ko_piano_roll[0]);
 step_t saved[64];memcpy(saved,trk[0].step,sizeof saved);
 ko_seq[0].cursor=63;ui.force=1;ko_draw();
 bad+=check("piano roll reaches step 64 without altering notes",ko_seq[0].cursor==63&&!memcmp(saved,trk[0].step,sizeof saved));
 host_fl_begin();ko_draw();uint32_t flashes=host_fl_end(26,240);
 bad+=check("unchanged piano roll does not erase graph",!flashes);
 ko_knob(2,-1);bad+=check("piano roll uses shared step cursor",ko_seq[0].cursor==62);
 ko_knob(3,-63);bad+=check("piano roll length remains variable 1 to 64",trk[0].p[P_SLEN]==1&&ko_seq[0].cursor==0);
 press(B_SEQ);bad+=check("SEQ returns to same sequence in grid",ko.view==7&&!ko_piano_roll[0]&&!memcmp(saved,trk[0].step,sizeof saved));
 press(B_SEQ);press(B_HOME);bad+=check("HOME exits piano roll",ko.view==0);
 ko.view=7;ko_piano_roll[0]=1;turn(EN_PRESET,4);bad+=check("PRESETS scrolls note rows without editing notes",ko_piano_low[0]==64&&!memcmp(saved,trk[0].step,sizeof saved));
 const char *path=getenv("PIANO_RENDER");
 if(path){trk[0].p[P_SLEN]=32;ko.view=7;ko_piano_roll[0]=1;ko_seq[0].cursor=5;ui.msg_t=0;ui.msg2[0]=0;ko_piano_low[0]=60;ko_palette_on=1;palette_set(0);ui.force=1;ko_ui_invalidate();ui_draw();
 FILE *f=fopen(path,"wb");fprintf(f,"P6\n240 240\n255\n");
 for(uint32_t i=0;i<240*240;i++){uint16_t c=swap16(host_screen[i]);fputc((c>>11)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}fclose(f);}
 printf("PIGEON piano roll: %s\n",bad?"FAILED":"PASSED");return !!bad;
}
