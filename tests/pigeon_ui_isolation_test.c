/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN isolation_base_main
#include "ko_test.c"
int main(void) {
 int bad=0;ui_power_on();memset(ko_states,0,sizeof ko_states);
 memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);fixture(2,0);
 bad+=check("PIGEON has no legacy page registry",NPAGES==1&&!strcmp(PAGES[0].title,"PIGEON")&&PAGES[0].graph==GR_NONE);
 bad+=check("PIGEON identity requires sampler-only profile",!strcmp(FW_DISPLAY_NAME,"PIGEON"));
 ui_prefs2=0;ui.home=0;ui.page=255;song.sel=3;ko.on=0;ko.view=255;
 ui_input();bad+=check("old settings cannot disable PIGEON or select a legacy page",ko.on&&ui.home&&ui.page==0&&song.sel==0&&ko.view==0);
 ko_setup();song.sel=2;ko_setup_second();song.sel=0;
 for(uint32_t fam=0;fam<FAM_COUNT;fam++){open_family(fam);bad+=check("every page family stays inside PIGEON",ui.home&&ui.page==0&&ko.on&&ko.view<KO_VIEW_COUNT);}
 ui.home=0;ui.page=255;song.sel=3;ui.force=1;ui_draw();
 bad+=check("drawing repairs stale page state before page access",ui.home&&ui.page==0&&song.sel==0);
 const uint8_t buttons[]={B_HOME,B_EDIT,B_ENV,B_LFO,B_FX,B_SCL,B_ARP,B_SEQ,B_GLO,B_SAVE};
 go_home();ko.view=0;
 for(uint32_t i=0;i<sizeof buttons;i++)for(uint32_t j=0;j<3;j++){press(buttons[i]);bad+=check("panel taps cannot open inherited screens",ui.home&&ui.page==0&&ko.on);}
 bad+=check("legacy slice editor is absent",!slice_page_ok()&&!slice_page_on());
 ui.menu=0;go_home();ko.view=0;hold(B_EDIT);
 bad+=check("holding EDIT opens chops without an engine-picker layer",ui.home&&ko.view==5&&!ui.layer&&!ui.lock);
 press(B_EDIT);press(B_EDIT);
 bad+=check("double EDIT cannot lock the old engine picker",!ui.layer&&!ui.lock);
 ui.layer=ui.lock=LAYER_EDIT;ui_input();
 bad+=check("stale engine-picker state is discarded",!ui.layer&&!ui.lock);
 bad+=check("irrelevant product settings are absent",!menu_visible(MI_KO)&&!menu_visible(MI_SERIAL)&&!menu_visible(MI_HOME)&&!menu_visible(MI_SCOPE));
 int hidden_ok=1;
 const uint8_t hidden[]={MI_COLOR,MI_STYLE,MI_LARGE,MI_ACCEL,MI_LATCH,MI_SCLLED,MI_PREVIEW,MI_CHADD,MI_HELP};
 for(uint32_t i=0;i<sizeof hidden;i++)hidden_ok&=!menu_visible(hidden[i]);
 bad+=check("retired overlay and ineffective display settings are absent",hidden_ok);
 for(uint32_t l=1;l<LAYER_N;l++)if(!layer_supported(l)){
   ui.layer=ui.lock=(uint8_t)l;ui_input();
   bad+=check("stale retired overlay is discarded",!ui.layer&&!ui.lock);
 }
 int rows_ok=1;
 for(uint32_t tab=0;tab<MTAB_COUNT;tab++) {
   uint32_t count=mtab_rows(tab);rows_ok&=count>0;
   ui.menu_sel=(uint8_t)mtab_first(tab);
   for(uint32_t row=0;row<count;row++) {
     rows_ok&=menu_visible(ui.menu_sel)&&menu_tab()==tab&&mtab_pos(tab,ui.menu_sel)==row;
     menu_move(1);
   }
   rows_ok&=ui.menu_sel==mtab_first(tab);
   menu_move(-1);rows_ok&=ui.menu_sel==mtab_at(tab,count-1);
 }
 bad+=check("all menu tabs skip removed rows and wrap in both directions",rows_ok);
 ui.menu_sel=MI_KO;(void)menu_tab();
 bad+=check("obsolete menu selections repair safely",ui.menu_sel==mtab_first(MTAB_DISPLAY));
 const uint8_t removed[]={B_FX,B_SCL,B_REC};int removed_ok=1;
 for(uint32_t i=0;i<sizeof removed;i++) {
   ui.menu=0;go_home();ko.view=0;btn_down(removed[i]);frames(700);
   removed_ok&=!ui.layer&&!ui.lock&&!layer_open()&&!(fm1_in.buttons&kb_mask)&&!kb_lock&&!(fm1_in.buttons&perf_mask);
   ui.force=1;ui_draw();btn_up(removed[i]);frames(400);
   removed_ok&=!ui.layer&&!ui.lock&&!lys.dt_l;
 }
 bad+=check("removed held overlays cannot open, lock or capture notes",removed_ok);
 const uint8_t quick[]={B_GLO,B_SEQ};int quick_ok=1;
 for(uint32_t i=0;i<sizeof quick;i++) {
   ui.menu=0;go_home();ko.view=0;btn_down(quick[i]);frames(700);
   quick_ok&=layer_supported(ui.layer)&&ui.home&&ui.page==0;
   if(quick[i]==B_SEQ){TSEL->p[P_SLEN]=16;turn(EN_K1,1);quick_ok&=TSEL->p[P_SLEN]>16;}
   ui.force=1;ui_draw();btn_up(quick[i]);frames(400);
 }
 bad+=check("mixer and sequence tools remain functional",quick_ok);
 printf("PIGEON UI isolation: %s\n",bad?"FAILED":"PASSED");return bad?1:0;
}
