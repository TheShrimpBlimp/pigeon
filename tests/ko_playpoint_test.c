/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN playpoint_fixture_main
#include "ko_test.c"
int main(void) {
    int bad=0;ui_power_on();memset(ko_states,0,sizeof ko_states);memset(ko_flash,255,sizeof ko_flash);
    slc_store_boot();fixture(0,0);fixture(1,1);fixture(2,0);ko_product_boot();render_blocks(16);go_home();
    ko_table_t tab={0};tab.n=1;tab.chop[0].a=2048;tab.chop[0].b=16384;tab.chop[0].play=32768;
    bad+=check("play point restores inside unchanged loop bounds",ko_restore(&tab));
    const ko_chop_t c=ko_table()->chop[0];uint32_t mid=ko_play_pos(&c);
    bad+=check("midpoint checkpoint is prepared outside audio",mid==9216 && c.play_st==slc_state_at(slc_get(ko_src()),mid));
    voice_t v={0};v.note=60;v.active=1;
    /* Use a real track voice because decoder/cache routing derives its index. */
    trk_all_off(trk);trk[0].v[0]=v;voice_t *voice=&trk[0].v[0];slice_note_on(trk,voice);
    bad+=check("first note starts midway and retains loop end",voice->ph[0]==mid && voice->ph[2]==c.b);
    slc_dec_t d;slc_dec_at(&d,c.b,c.st);int32_t x=0;
    bad+=check("next loop returns to original boundary",slc_fwd(slc_get(ko_src()),voice,&d,1,&x) && d.pos==c.a+1 && voice->ph[2]==c.b);
    tab.chop[0].reverse=1;ko_restore(&tab);slice_note_on(trk,voice);
    bad+=check("reverse starts at the point with original lower bound",voice->ph[0]==mid+1 && voice->ph[2]==c.a);
    slc_dec_t expected;slc_dec_at(&expected,mid,c.play_st);int32_t expected_sample=slc_dec_next(slc_get(ko_src()),&expected),reverse_sample=0;
    bad+=check("ADPCM reverse decodes the selected sample exactly",slc_rev(slc_get(ko_src()),voice,slc_rb(trk,voice),0,&reverse_sample) && reverse_sample==expected_sample && voice->ph[0]==mid);
    tab.chop[0].reverse=0;ko_restore(&tab);ko_tail_preview=1;slice_note_on(trk,voice);ko_tail_preview=0;
    bad+=check("end audition overrides ordinary playback point",voice->ph[0]!=mid);
    ko_queue_firing=1;slice_note_on(trk,voice);ko_queue_firing=0;
    bad+=check("queued chop uses the same playback point",voice->ph[0]==mid && ((uint32_t)voice->s[4]&KO_V_NO_LOOP));
    trk_all_off(trk);ko_play_edit=1;ko_chop_turn(0,4);
    bad+=check("K1 edits playback point without moving boundaries",ko_table()->chop[0].play>32768 && ko_table()->chop[0].a==c.a && ko_table()->chop[0].b==c.b);
    bad+=check("chop undo restores playback point",ko_undo() && ko_table()->chop[0].play==32768);
    ko_chop_turn(0,-1000);bad+=check("playback point resets at left limit",ko_table()->chop[0].play==0);
    tab.chop[0].reverse=1;ko_restore(&tab);ko_alternate[0]=*ko_table();ko_alternate[0].chop[0].play=12345;
    bad+=check("playback extension saves without growing v0.4 records",sizeof(ko_record_t)==544 && ko_store_sector(1)==3);
    ko_auto();memset(&ko_alternate[0],0,sizeof ko_alternate[0]);ko_store_load();
    bad+=check("current and alternate playback points reload",ko_table()->chop[0].play==32768 && ko_table()->chop[0].reverse==1 && ko_alternate[0].chop[0].play==12345 && ko_alternate[0].chop[0].reverse==1);
    bad+=check("stored play checkpoint rebuilt",ko_table()->chop[0].play_st==c.play_st);
    uint32_t crc=ko.song_crc;ko.song_crc^=1;ko_auto();ko_store_load();bad+=check("replacement audio never inherits playback points",ko_table()->chop[0].play==0);ko.song_crc=crc;
    /* Simulate an old bank with a valid FKO1 table but no extension. */
    memset((uint8_t *)ko_flash+SLC_REC_OFF+KO_PLAY_REC_OFF,255,sizeof(ko_play_record_t));ko_auto();ko_store_load();
    bad+=check("v0.4 banks default to original start",ko_table()->chop[0].play==0 && ko_table()->chop[0].a==c.a);
    printf("play point: %s\n",bad?"FAIL":"PASS");return bad?1:0;
}
