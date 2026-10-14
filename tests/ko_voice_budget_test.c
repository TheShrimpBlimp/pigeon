/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN underlying_ko_main
#include "ko_test.c"
int main(void){
 int bad=0;ui_power_on();memset(ko_states,0,sizeof ko_states);memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);fixture(2,0);ko_states[0].on=1;ko_setup();song.sel=2;ko_setup_second();song.sel=0;
 for(uint32_t p=0;p<NTRK;p++)trk_all_off(&trk[p]);render_blocks(300);
 for(uint32_t i=0;i<8;i++)trk_note_on(&trk[0],60+i,100);
 bad+=check("eight song voices sound before drums",voice_group_busy(&trk[0])==8);
 for(uint32_t i=0;i<4;i++)trk_note_on(&trk[1],60+i,100);
 bad+=check("four additional drums do not steal song voices",voice_group_busy(&trk[0])==8 && voice_group_busy(&trk[1])==4 && voices_busy()==12);
 trk_note_on(&trk[1],64,100);
 bad+=check("fifth drum steals within kit only",voice_group_busy(&trk[0])==8 && voice_group_busy(&trk[1])==4);
 trk_note_on(&trk[2],60,100);
 bad+=check("second song shares eight song voices and leaves drums intact",voice_group_busy(&trk[0])==8 && voice_group_busy(&trk[1])==4);
 track_select(3);bad+=check("fourth track cannot be selected in standalone PIGEON",song.sel<3);
 for(uint32_t p=0;p<NTRK;p++)trk_all_off(&trk[p]);render_blocks(300);go_home();ko_pattern_sel=1;
 for(uint32_t p=0;p<SAMPLER_PARTS;p++){step_clear(&trk[p].step[0]);trk[p].step[0].time=ST_NOTE;trk[p].step[0].n=1;trk[p].step[0].note[0]=60+p;}
 press(B_SAVE);
 bad+=check("SAVE stores all tracks to selected B slot",project_used(1));
 bad+=check("save confirmation names destination B",strstr(ui.msg,"TO B")!=0);
 for(uint32_t p=0;p<SAMPLER_PARTS;p++)step_clear(&trk[p].step[0]);
 ko_pattern_select(1);int restored=1;for(uint32_t p=0;p<SAMPLER_PARTS;p++)restored &= trk[p].step[0].n==1 && trk[p].step[0].note[0]==60+p;
 bad+=check("saved phrase restores sequences on all three tracks",restored);
 return bad?1:0;
}
