/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN backport_base_main
#include "ko_test.c"
static void incoming(uint32_t ch,uint32_t note,uint32_t vel){midi_event(vel?0x90:0x80,ch,note,vel);}
static int held(uint32_t k,uint32_t note){for(uint32_t v=0;v<NVOICE;v++)if(trk[k].v[v].active&&trk[k].v[v].gate&&trk[k].v[v].note==note)return 1;return 0;}
int main(void){
 int bad=0;ui_power_on();memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);fixture(2,0);ko_product_boot();render_blocks(32);go_home();song.sel=0;track_t *t=&trk[0];
 step_t st={0};int ok=1;
 for(uint32_t c=0;c<=KO_CONDITIONS;c++){step_set_condition(&st,c);ok&=step_condition(&st)==c&&step_chance(&st)==100;}
 bad+=check("all conditions retain 100 percent chance",ok);
 for(uint32_t c=1;c<=4;c++){st.probability=(uint8_t)(101+c);ok&=step_condition(&st)==c;}
 bad+=check("legacy PIGEON condition bytes retain their meanings",ok);
 ko_pass_started[0]=0;t->seq_idx=0;ok=1;
 for(uint32_t c=5;c<=13;c++){uint32_t cycle=c<=6?2:c<=9?3:4,pos=c<=6?c-5:c<=9?c-7:c-10;step_set_condition(&st,c);
 for(uint32_t pass=0;pass<12;pass++){ko_pass[0]=pass;ok&=ko_condition_pass(t,&st,0)==(pass%cycle==pos);}}
 step_set_condition(&st,14);ko_fill=0;ok&=!ko_condition_pass(t,&st,0);ko_fill=1;ok&=ko_condition_pass(t,&st,0);step_set_condition(&st,15);ok&=!ko_condition_pass(t,&st,0);ko_fill=0;ok&=ko_condition_pass(t,&st,0);
 bad+=check("loop conditions and momentary FILL/NOT FILL",ok);
 ko.view=7;fm1_in.buttons=1u<<panel.btn[B_ARP];ui_input();ok=ko_fill&&ko.view==7;fm1_in.buttons=0;ui_input();bad+=check("ARP fill holds the sequencer page and clears on release",ok&&!ko_fill&&ko.view==7);
 memset(t->step,0,sizeof t->step);motion_clear(t);t->p[P_SLEN]=4;
 t->step[0]=(step_t){.note={60,61},.n=2,.time=ST_NOTE,.vel=100};step_set_condition(&t->step[0],13);step_set_nudge(&t->step[0],3);ko_note_put(t,0,0,0x54);motion_set_lock(t,2,P_TRANS,7);motion_set_lock(&trk[1],3,P_TRANS,9);
 ok=ko_resize_pattern(t,1)&&t->p[P_SLEN]==8&&!memcmp(&t->step[0],&t->step[4],sizeof st)&&ko_note_data(&motion,t,4,0)==0x54;
 int16_t value=0;ok&=motion_lock_get(t,6,P_TRANS,&value)&&value==7&&motion_lock_get(&trk[1],3,P_TRANS,&value)&&value==9;
 bad+=check("DOUBLE preserves chord conditions nudge note timing and other tracks",ok);
 undo_swap();bad+=check("DOUBLE undo restores length and timing",t->p[P_SLEN]==4&&!ko_note_data(&motion,t,4,0)&&ko_note_data(&motion,t,0,0)==0x54);undo_swap();
 bad+=check("HALF clears discarded notes and their timing",ko_resize_pattern(t,0)&&t->p[P_SLEN]==4&&!t->step[4].n&&!ko_note_data(&motion,t,4,0));
 motion_clear(t);motion.count=MOTION_MAX;for(uint32_t i=0;i<MOTION_MAX;i++)motion.event[i]=(motion_event_t){.place=(uint8_t)(i<4?i:64u+(i-4u)%64u),.param=(uint8_t)(i<68?P_TRANS:P_SGATE),.value=0};motion_store_t before=motion;step_t steps[NSTEP];memcpy(steps,t->step,sizeof steps);
 bad+=check("DOUBLE overflow rejects before changing steps length or automation",!ko_resize_pattern(t,1)&&t->p[P_SLEN]==4&&!memcmp(&motion,&before,sizeof motion)&&!memcmp(steps,t->step,sizeof steps));
 memset(&motion,0,sizeof motion);t->p[P_SLEN]=16;ko_euclid_pattern(t,5,8,1);uint32_t hits=0;for(uint32_t i=0;i<16;i++)hits+=t->step[i].time==ST_NOTE&&t->step[i].n>0;bad+=check("Euclidean cycle repeats with exact requested hit count",hits==10);undo_swap();bad+=check("Euclidean generation is undoable",!memcmp(t->step,steps,sizeof steps));
 memset(&motion,0,sizeof motion);t->p[P_M3SRC]=MS_RAND;t->p[P_M3DST]=MD_VEL;t->p[P_M3AMT]=20;t->p[P_M4SRC]=MS_RAND;t->p[P_M4DST]=MD_TIME;t->p[P_M4AMT]=40;ok=1;uint32_t min=127,max=0;
 for(uint32_t i=0;i<100;i++){uint32_t v=mod_note(t,60,100),d=mod_time(t,60,100,500);ok&=v>=1&&v<=127&&d<=500*7/16;if(v<min)min=v;if(v>max)max=v;}
 bad+=check("humanization varies velocity and keeps timing within step bounds",ok&&min<max);t->p[P_M3AMT]=t->p[P_M4AMT]=0;bad+=check("zero humanization leaves notes and timing unchanged",mod_note(t,60,100)==100&&!mod_time(t,60,100,500));
 memset(t->step,0,sizeof t->step);t->step[0]=(step_t){.note={60},.n=1,.time=ST_NOTE,.vel=100};t->p[P_SLEN]=16;t->p[P_SQNT]=1;t->p[P_M4SRC]=MS_MODW;t->p[P_M4DST]=MD_TIME;t->p[P_M4AMT]=63;t->mw=127;
 seq_start();render_blocks(1);ok=!held(0,60);render_blocks((FS*31u/1000u+CTL-1u)/CTL);bad+=check("TIME delays actual quantized sampler playback then triggers the chop",ok&&held(0,60));seq_stop();trk_all_off(t);render_blocks(32);t->p[P_M4AMT]=0;
 for(uint32_t k=0;k<NTRK;k++)for(uint32_t v=0;v<NVOICE;v++)voice_kill(&trk[k].v[v]);
 ko.view=32;ko_tool_action=3;ko_human_vel=10;ko_human_time=12;page_tap(B_ENV);ok=t->p[P_M3AMT]==10&&t->p[P_M4AMT]==12;undo_swap();bad+=check("humanize apply and undo restore modulation parameters",ok&&!t->p[P_M3AMT]&&!t->p[P_M4AMT]);
 song.g[G_ROUTE]=ROUTE_TRACK;menu_put(MI_TCH1,9);menu_put(MI_TCH2,1);menu_put(MI_TCH3,MCH_OMNI);events_block(CTL);
 bad+=check("custom MIDI channels, OMNI fallback and removed track ignored",midi_slot(9)==0&&midi_slot(1)==1&&midi_slot(15)==2);
 incoming(9,60,100);incoming(1,61,100);bad+=check("MIDI notes route independently to songs and drums",held(0,60)&&midi_notes[1][61]==2&&midi_notes[9][60]==1);menu_put(MI_TCH1,10);events_block(CTL);bad+=check("channel reassignment releases old ownership without stuck notes",!held(0,60)&&!midi_notes[9][60]&&!midi_owners[0]);
 menu_put(MI_TCH3,MCH_OFF);events_block(CTL);bad+=check("MIDI OFF ignores unmatched channels",midi_slot(15)==-1);menu_put(MI_TCH2,10);events_block(CTL);bad+=check("duplicate MIDI channels choose first visible track",midi_slot(10)==0);
 project_t p,q;project_store_t packed;project_capture(&p);p.t[0].step[0].probability=KO_PROB_MAX;ok=proj_pack(&packed,&p)&&proj_unpack(&q,packed.raw,sizeof packed);
 bad+=check("MIDI metadata and conditions fit 3840 bytes alongside per-note timing",ok&&q.g[G_ROUTE]==ROUTE_TRACK&&q.mch[0]==p.mch[0]&&q.mch[2]==MCH_OFF&&q.t[0].step[0].probability==KO_PROB_MAX);
 memset(p.mch,0,sizeof p.mch);p.g[G_ROUTE]=0;proj_pack(&packed,&p);bad+=check("old default headers still decode to default MIDI channels",!packed.raw[65]&&!packed.raw[67]&&proj_unpack(&q,packed.raw,sizeof packed)&&!q.mch[0]&&!q.mch[2]);
 song.sel=0;ko.view=0;go_home();fm1_ms=1000;menu_put(MI_MFOLLOW,1);midi_seen=4;ui_input();bad+=check("MIDI auto select follows an incoming track",song.sel==2);midi_seen=1;fm1_ms+=50;ui_input();bad+=check("MIDI auto select holdoff prevents flicker",song.sel==2);fm1_ms+=300;midi_seen=1;ui_input();bad+=check("MIDI auto select follows after holdoff",song.sel==0);
 ko.view=24;fm1_ms+=300;midi_seen=4;ui_input();bad+=check("MIDI input does not interrupt detailed editing",song.sel==0);
 scrn.st=SCR_OFF;menu_put(MI_MWAKE,0);midi_seen=1;ui_input();bad+=check("MIDI note wakes dark screen",scrn.st==SCR_WAKE);scrn.st=SCR_OFF;menu_put(MI_MWAKE,1);midi_seen=1;ui_input();bad+=check("panel-only wake preference is respected",scrn.st==SCR_OFF);
 memset(&motion,0,sizeof motion);motion.event[0]=(motion_event_t){.place=3,.param=P_TRANS,.value=1};motion.event[1]=(motion_event_t){.place=3,.param=P_LEVEL,.value=2};motion.count=2;uint8_t rows[MOTION_MAX];motion_rows(0,rows);ok=rows[0]==0&&rows[1]==1;motion.event[0].param=P_PAN;motion_rows(0,rows);bad+=check("automation parameter changes do not reorder same-step rows",ok&&rows[0]==0&&rows[1]==1);
 printf("PIGEON upstream backports: %s\n",bad?"FAILED":"PASSED");return !!bad;
}
