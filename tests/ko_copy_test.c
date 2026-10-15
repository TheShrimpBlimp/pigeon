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
 ko_restore(&before);ko_select(0);
 bad+=check("swap retains all chops and seek state",ko_reassign(31,0)&&ko_table()->n==32&&!memcmp(&ko_table()->chop[31],&before.chop[0],sizeof(ko_chop_t))&&!memcmp(&ko_table()->chop[0],&before.chop[31],sizeof(ko_chop_t)));
 ko_undo();bad+=check("swap undo restores exact table",!memcmp(ko_table(),&before,sizeof before));
 ko_select(0);bad+=check("move shifts keys without dropping chops",ko_reassign(31,1)&&!memcmp(ko_table()->chop,before.chop+1,31*sizeof(ko_chop_t))&&!memcmp(&ko_table()->chop[31],before.chop,sizeof(ko_chop_t)));
 ko_undo();ko_select(31);bad+=check("move backwards retains key order",ko_reassign(0,1)&&!memcmp(ko_table()->chop+1,before.chop,31*sizeof(ko_chop_t))&&!memcmp(ko_table()->chop,&before.chop[31],sizeof(ko_chop_t)));
 ko_undo();ko_select(0);uint32_t rev=ko.revision;bad+=check("invalid rearrangement is unchanged",!ko_reassign(32,1)&&ko.revision==rev);
 ko_restore(&before);ko_select(0);go_home();ko.view=20;ko_copy_chop=31;press(B_ENV);
 bad+=check("ENV from chop operations opens key assignment",ko.view==28);
 turn(EN_K3,1);bad+=check("K3 swaps selected source with destination",ko.selected==31&&!memcmp(ko_table()->chop,&before.chop[31],sizeof(ko_chop_t)));
 ko_undo();ko_select(0);turn(EN_K4,1);bad+=check("K4 moves source and shifts intermediate keys",ko.selected==31&&!memcmp(ko_table()->chop,before.chop+1,31*sizeof(ko_chop_t)));
 ko_undo();ko_select(0);song.playing=1;turn(EN_K3,1);song.playing=0;bad+=check("key rearrangement refuses playback",!memcmp(ko_table(),&before,sizeof before));
 press(B_ENV);bad+=check("ENV returns to chop operations",ko.view==20);
 return bad?1:0;}
