/* SPDX-License-Identifier: GPL-3.0-only */
/* Compare incremental Settings frames with complete repaints, not screenshots. */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN unused_settings_ko_main
#include "ko_test.c"
static uint16_t settings_shot[240*240];
static uint32_t settings_same_as_full(void){
 memcpy(settings_shot,host_screen,sizeof settings_shot);
 ui.force=1;ui_draw();
 return !memcmp(settings_shot,host_screen,sizeof settings_shot);
}
int main(void){
 int bad=0;uint32_t compared=0,clean=1,quiet=1,buffered=1,small=1;
 ui_power_on();fixture(0,0);menu_put(MI_KO,1);ko_states[0].on=1;ko_setup();render_blocks(32);
 song.sel=0;ui_prefs|=PREF_ANIM_OFF;ui.menu=1;ui.menu_sel=MI_TCH1;ui.force=1;ui_draw();
 memcpy(settings_shot,host_screen,sizeof settings_shot);
 menu_put(MI_TCH1,(menu_get(MI_TCH1)+1u)%menu_n(MI_TCH1));
 host_blit_rows=0;ui.force=0;ui_draw();
 bad+=check("MIDI channel edit immediately updates Settings",memcmp(settings_shot,host_screen,sizeof settings_shot)!=0);
 bad+=check("single value sends only its row to the LCD",host_blit_rows>0 && host_blit_rows<=29u);
 bad+=check("MIDI edit matches complete repaint",settings_same_as_full());
 for(uint32_t row=0;row<MI_COUNT;row++)if(menu_visible(row)){
  ui.menu=1;ui.menu_sel=(uint8_t)row;ui.force=1;ui_draw();
  host_blit_rows=0;ui.force=0;ui_draw();quiet&=host_blit_rows==0;
  if(menu_valued(row)){
   uint32_t old=menu_get(row),value=menu_step(row,1);
   if(value==old)value=menu_step(row,-1);
   menu_put(row,value);host_blit_rows=0;host_fl_begin();ui_draw();
   buffered&=host_fl_end(0,240)==0;
   if(row!=MI_COLOR && row!=MI_HOME && value!=old)small&=host_blit_rows<=29u;
   clean&=settings_same_as_full();compared++;
   menu_put(row,old);ui.force=1;ui_draw();
  }
  menu_move(1);ui.force=0;host_fl_begin();ui_draw();buffered&=host_fl_end(0,240)==0;
  clean&=settings_same_as_full();compared++;
  menu_tab_go(1);ui.force=0;ui_draw();clean&=settings_same_as_full();compared++;
  quiet&=mt.pos==(int32_t)menu_tab()*MT_ONE;
 }
 bad+=check("every value/row/tab transition matches full repaint",compared>40 && clean);
 bad+=check("idle Settings does not repaint; tabs settle immediately",quiet);
 bad+=check("value edits remain limited to their row",small);
 bad+=check("selection/value changes never flash intermediate pixels",buffered);
 for(uint32_t mode=2;mode<=3;mode++){
  ui.menu=mode;ui.force=1;ui_draw();ui.menu=1;ui.force=1;ui_draw();
  bad+=check("return from credits/info leaves complete Settings frame",settings_same_as_full());
 }
 printf("Settings: %u frame comparisons\n",compared);return bad!=0;
}
