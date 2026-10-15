/* SPDX-License-Identifier: GPL-3.0-only */
#define KO_TEST_MAIN base_ko_main
#include "ko_test.c"
int main(void){int bad=0;ui_power_on();memset(ko_states,0,sizeof ko_states);memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);menu_put(MI_KO,1);ko_states[0].on=1;ko_setup();song.sel=0;
 ko_table_t layout={0};layout.n=32;for(uint32_t i=0;i<32;i++){layout.chop[i].a=i*512;layout.chop[i].b=i*512+256;layout.chop[i].pitch=i%12;layout.chop[i].rsv=(i%4)<<KO_CHOKE_SHIFT;}
 ko_restore(&layout);ko_select(31);ko_table_t before=*ko_table();uint32_t revision=ko.revision;
 bad+=check("invalid source leaves table untouched",!ko_copy_full(32) && ko.revision==revision && !memcmp(&before,ko_table(),sizeof before));
 bad+=check("whole copy works at 32 chops without inserting",ko_copy_full(0) && ko_table()->n==32 && ko.selected==31 && !memcmp(&ko_table()->chop[31],&before.chop[0],sizeof(ko_chop_t)));
 bad+=check("source and intervening key assignments stay unchanged",!memcmp(ko_table()->chop,before.chop,31*sizeof(ko_chop_t)));
 ko_undo();bad+=check("undo restores overwritten destination",!memcmp(ko_table(),&before,sizeof before));
 ko_select(31);revision=ko.revision;bad+=check("copy to self is a no-op",ko_copy_full(31) && ko.revision==revision);
 go_home();ko.view=20;ko_copy_chop=0;song.playing=1;press(B_REC);bad+=check("copy during playback is refused without arming recording",!song.rec && !memcmp(ko_table(),&before,sizeof before));song.playing=0;press(B_REC);bad+=check("REC in operations copies full chop without arming recording",!song.rec && !memcmp(&ko_table()->chop[31],&before.chop[0],sizeof(ko_chop_t)));
 if(bad)printf("copy UI: rec=%u layer=%u ly=%u view=%u selected=%u busy=%u msg=%s\n",song.rec,ui.layer,ui.ly,ko.view,ko.selected,voices_busy(),ui.msg);
 return bad?1:0;}
