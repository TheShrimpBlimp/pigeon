/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN workflow_fixture_main
#include "ko_test.c"
int main(void) {
    int bad=0;ui_power_on();memset(ko_states,0,sizeof ko_states);memset(ko_flash,255,sizeof ko_flash);
    slc_store_boot();fixture(0,0);fixture(1,1);fixture(2,0);ko_product_boot();render_blocks(16);go_home();
    song.sel=0;ko.view=0;ko_play_edit=0;
    ko_table_t original=*ko_table();
    ko_chop_turn(0,5);ko_play_edit=1;ko_chop_turn(0,10);ko_play_edit=2;ko_chop_turn(0,1);
    ko_table_t edited=*ko_table();uint32_t crc=st_crc32(ko_flash,sizeof ko_flash);
    ko_table_t other=ko_states[1].table[ko_states[1].current];
    bad+=check("reset restores all original assignments and neutral settings",ko_reset_chops() && !memcmp(ko_table(),&original,sizeof original));
    ko_reset_chops(); /* Repeated reset detents must retain the last real edit. */
    bad+=check("undo restores boundaries point and direction after reset",ko_undo() && !memcmp(ko_table(),&edited,sizeof edited));
    bad+=check("redo restores original assignments",ko_undo() && !memcmp(ko_table(),&original,sizeof original));
    bad+=check("reset and history never rewrite stored audio",crc==st_crc32(ko_flash,sizeof ko_flash));
    bad+=check("reset is independent of the other song",!memcmp(&other,&ko_states[1].table[ko_states[1].current],sizeof other));
    ko_table_t audition={0};audition.n=1;audition.chop[0].a=2048;audition.chop[0].b=16384;audition.chop[0].rsv=KO_LATCH|KO_ONE;ko_restore(&audition);
    ko_play_edit=0;ko.view=5;ko_knob(3,1);ko_knob(3,-1);
    bad+=check("playback page exposes reset and undo",ko_table()->chop[0].a==audition.chop[0].a);
    ko.view=0;kb_tail=1;fm1_in.notes=1;kb_trk[0]=0;kb_note[0]=60;
    ko_knob(0,2);
    voice_t *voice=0;for(uint32_t j=0;j<NVOICE;j++)if(trk[0].v[j].active)voice=&trk[0].v[j];
    bad+=check("held local audition restarts at edited beginning",voice && voice->ph[0]==ko_table()->chop[0].a);
    bad+=check("start audition overrides latch and one-shot",voice && !((uint32_t)voice->s[4]&KO_V_LATCH) && !(ko_env_flags[0][voice-trk[0].v]&KO_ONE));
    ko_knob(1,-2);
    voice=0;for(uint32_t j=0;j<NVOICE;j++)if(trk[0].v[j].active)voice=&trk[0].v[j];
    bad+=check("end edit auditions near the ending",voice && voice->ph[0]>=ko_table()->chop[0].a && voice->ph[2]==ko_table()->chop[0].b);
    bad+=check("audition ignores latch and one-shot release protection",voice && !((uint32_t)voice->s[4]&KO_V_LATCH) && !(ko_env_flags[0][voice-trk[0].v]&KO_ONE));
    bad+=check("audition override is cleared",!ko_tail_preview);
    fm1_in.notes=0;kb_tail=0;trk_all_off(trk);render_blocks(100);
    ko_knob(0,1);bad+=check("ordinary editing does not trigger voices",!voices_busy());
    printf("chop workflow: %s\n",bad?"FAIL":"PASS");return !!bad;
}
