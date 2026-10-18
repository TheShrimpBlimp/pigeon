/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN scratch_base_main
#include "ko_test.c"
static void scratch_blocks(uint32_t n){for(uint32_t i=0;i<n;i++){ko_scratch_poll();render_blocks(1);}}
static voice_t *scratch_voice(uint32_t id){track_t *t=&trk[id?2:0];for(uint32_t i=0;i<NVOICE;i++)if(t->v[i].active && ((uint32_t)t->v[i].s[4]&KO_V_SCRATCH))return &t->v[i];return 0;}
int main(void){
 int bad=0;ui_power_on();memset(ko_states,0,sizeof ko_states);memset(ko_flash,255,sizeof ko_flash);
 slc_store_boot();fixture(0,0);fixture(1,1);fixture(2,0);ko_product_boot();ko_setup();render_blocks(32);go_home();song.sel=0;
 ko_table_t tab={0};tab.n=2;tab.chop[0].a=1000;tab.chop[0].b=1400;tab.chop[0].rsv=KO_GATE|KO_NO_LOOP;
 tab.chop[1].a=4000;tab.chop[1].b=8000;tab.chop[1].rsv=KO_LATCH;ko_restore(&tab);ko_table_t saved=*ko_table();
 press(B_EDIT);press(B_FX);bad+=check("EDIT then FX opens scratch deck",ko.view==30 && ko_scratch[0].on);
 input_on(&trk[0],60,100);input_off(&trk[0],60);scratch_blocks(12);voice_t *v=scratch_voice(0);
 bad+=check("key release leaves scratch deck playing",v && v->ph[0]>=1000 && v->ph[0]<1400);
 v->ph[0]=1200;press(B_ENV);bad+=check("ENV sets cue without leaving deck",ko.view==30 && ko_scratch[0].cue==1200);
 ko_draw();uint32_t cuex=12u+200u*(KO_WAVE_COLS-1u)/400u;bad+=check("waveform draws the saved cue marker",host_screen[100*240+cuex]==swap16(T_TEXT));
 v->ph[0]=1300;press(B_LFO);bad+=check("LFO returns to cue on same page",ko.view==30 && v->ph[0]==1200 && !v->ph[1]);
 ko_knob(1,-16);scratch_blocks(8);bad+=check("crossfader closes smoothly without stopping platter",!ko_scratch[0].gain && ko_scratch[0].motor && v->active);
 ko_knob(1,16);scratch_blocks(8);bad+=check("crossfader reopens",ko_scratch[0].gain==32767);

 press(B_PLAY);scratch_blocks(1);bad+=check("brake ramps down instead of stopping abruptly",ko_scratch[0].velocity>0 && ko_scratch[0].velocity<65536);
 scratch_blocks(500);uint32_t pos=v->ph[0];scratch_blocks(6);bad+=check("brake settles to a stopped platter",!ko_scratch[0].velocity && v->ph[0]==pos);
 v->ph[0]=1200;ko_knob(0,-1);scratch_blocks(2);bad+=check("negative jog moves audio backwards",v->ph[0]<1200 && v->ph[0]>=1000);
 v->ph[0]=1200;ko_knob(0,1);scratch_blocks(4);bad+=check("positive jog moves audio forwards",v->ph[0]>1200 && v->ph[0]<1400);
 ko_knob(0,-32);scratch_blocks(650);bad+=check("reverse wraps within chop and jog releases",v->ph[0]>=1000 && v->ph[0]<1400 && !ko_scratch[0].touched);
 pos=v->ph[0];scratch_blocks(6);bad+=check("jog settles to stopped motor",v->ph[0]==pos);
 press(B_PLAY);bad+=check("PLAY toggles platter without starting transport",ko_scratch[0].motor && !transport_req);
 scratch_blocks(160);bad+=check("forward loop stays active across cache and chop boundaries",v->active && v->ph[0]>=1000 && v->ph[0]<1400);
 input_on(&trk[0],61,100);scratch_blocks(8);uint32_t count=0;for(uint32_t i=0;i<NVOICE;i++)count+=trk[0].v[i].active && ((uint32_t)trk[0].v[i].s[4]&KO_V_SCRATCH);
 bad+=check("another chop replaces the deck voice",count==1 && scratch_voice(0)->ph[0]>=4000);
 bad+=check("new chop starts with its own cue",ko_scratch[0].cue==4000);
 input_on(&trk[0],60,100);scratch_blocks(2);bad+=check("returning to an earlier chop recalls its cue",ko_scratch[0].cue==1200);
 ko_table_t shifted=*ko_table();shifted.chop[0].a=1100;ko_restore(&shifted);input_on(&trk[0],60,100);scratch_blocks(2);bad+=check("changing chop boundaries invalidates its old cue",ko_scratch[0].cue==1100);ko_restore(&saved);input_on(&trk[0],61,100);scratch_blocks(2);
 ko_knob(2,1);ko_draw();bad+=check("record view draws without changing metadata",ko_scratch[0].record && !memcmp(&saved,ko_table(),sizeof saved));
 ko_knob(2,-1);ko_draw();bad+=check("waveform view selectable",!ko_scratch[0].record);
 press(B_EDIT);bad+=check("EDIT opens motor controls without losing scratch view",ko.view==30 && ko_scratch[0].controls);
 v=scratch_voice(0);v->ph[0]=5000;ko_knob(0,16);bad+=check("seek moves within selected chop",v->ph[0]>5000 && v->ph[0]<8000);
 ko_knob(0,10000);bad+=check("seek clamps at chop end",v->ph[0]==7999);
 ko_knob(0,-10000);bad+=check("seek clamps at chop start",v->ph[0]==4000);
 ko_knob(1,-50);bad+=check("fine motor speed uses one percent increments",ko_scratch[0].rate==50);
 press(B_ARP);scratch_blocks(400);bad+=check("ARP reverses motor smoothly",ko.view==30 && ko_scratch[0].rate==-50 && ko_scratch[0].velocity==-32768);
 ko_knob(2,-25);bad+=check("brake duration adjustable to zero",!ko_scratch[0].brake_ms);
 ko_knob(3,10);bad+=check("release glide adjustable",ko_scratch[0].release_ms==110);
 ko_draw();press(B_EDIT);ko_knob(3,-4);ko_knob(0,1);bad+=check("fine jog available without changing motor rate",ko_scratch[0].jog==4096 && ko_scratch[0].rate==-50);
 uint32_t selected=ko.selected;ko_scratch[0].touched=0;ko_scratch[0].rate=100;
 turn(EN_PRESET,1);bad+=check("PRESETS tunes by a single cent without selecting a different chop",ko_scratch[0].cents==1 && ko.selected==selected && ko_scratch_speed(0)>65536 && ko_scratch_speed(0)<65585);
 turn(EN_PRESET,1000);bad+=check("positive tuning clamps to one semitone",ko_scratch[0].cents==100 && abs(ko_scratch_speed(0)-(int32_t)(65536*pow(2.,1./12.)))<=2);
 ko_scratch[0].rate=-100;bad+=check("cent tuning also applies to reverse motor",ko_scratch_speed(0)<-65536);
 turn(EN_PRESET,-200);ko_scratch[0].rate=100;bad+=check("negative cent tuning is accurate",ko_scratch[0].cents==-100 && abs(ko_scratch_speed(0)-(int32_t)(65536*pow(2.,-1./12.)))<=2);
 turn(EN_PRESET,100);bad+=check("zero cents returns to exact original speed",!ko_scratch[0].cents && ko_scratch_speed(0)==65536);
 ko_scratch[0].jog=8192;ko_scratch[0].touched=FS;turn(EN_PRESET,40);bad+=check("tuning preserves physical jog response",ko_scratch_speed(0)==8192);
 press(B_SCL);bad+=check("SEL resets tuning without leaving scratch view",ko.view==30 && !ko_scratch[0].cents && ko_scratch[0].tuning==65536);
 turn(EN_PRESET,37);ko_scratch[0].touched=0;ko_scratch[0].rate=-125;ko_scratch[0].sensitivity=2;ko_scratch[0].brake_ms=410;ko_scratch[0].release_ms=85;ko_scratch[0].record=1;
 ko_scratch[1].rate=75;ko_scratch[1].brake_ms=130;ko_scratch_tune(1,-23);
 ko_context=0;int savedprefs=ko_store_sector(1);ko_scratch_defaults(0);ko_store_load();
 bad+=check("scratch preferences survive metadata save and reload",savedprefs==3 && ko_scratch[0].cents==37 && ko_scratch[0].rate==-125 && ko_scratch[0].sensitivity==2 && ko_scratch[0].brake_ms==410 && ko_scratch[0].release_ms==85 && ko_scratch[0].record);
 ko_context=1;ko_scratch_defaults(1);ko_store_load();bad+=check("Song 2 preferences restore independently",ko_scratch[1].rate==75 && ko_scratch[1].cents==-23 && ko_scratch[0].cents==37);ko_context=0;
 ko_scratch_record_t *prefs=(ko_scratch_record_t *)(smp_user_xip(0)+SLC_REC_OFF+KO_SCRATCH_REC_OFF);ko_scratch_record_t good=*prefs;
 prefs->crc^=1;ko_scratch_defaults(0);ko_store_load();bad+=check("corrupt preference CRC is rejected",!ko_scratch[0].cents && ko_scratch[0].rate==100);
 *prefs=good;prefs->pref[0].sensitivity=255;prefs->crc=st_crc32(prefs,sizeof *prefs-4);ko_store_load();bad+=check("out-of-range preference is rejected even with valid CRC",ko_scratch[0].sensitivity==4);
 *prefs=good;ko_store_load();
 (void)ko_store_sector(0);ko_scratch_defaults(0);ko_store_load();bad+=check("ordinary metadata writes preserve scratch preferences",ko_scratch[0].cents==37 && ko_scratch[0].rate==-125);
 uint32_t crc=ko.song_crc;ko.song_crc^=1;ko_scratch_defaults(0);ko_store_load();bad+=check("preferences cannot attach to different audio",!ko_scratch[0].cents && ko_scratch[0].rate==100);ko.song_crc=crc;ko_store_load();ko_context=~0u;
 ko_draw();
 press(B_HOME);pigeon_ui_guard();bad+=check("HOME exits and silences scratch",ko.view==0 && !ko_scratch[0].on && !scratch_voice(0));
 ko.view=5;press(B_FX);bad+=check("reopening deck keeps preferences",ko_scratch[0].cents==37 && ko_scratch[0].rate==-125);input_on(&trk[0],60,100);scratch_blocks(2);smp_user_gen++;ko_scratch_poll();bad+=check("bank replacement stops scratch references",!ko_scratch[0].on && !scratch_voice(0));
 printf("PIGEON scratch: %s\n",bad?"FAILED":"PASSED");return !!bad;
}
