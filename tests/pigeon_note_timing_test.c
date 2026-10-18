/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN note_base_main
#include "ko_test.c"
static voice_t *playing(uint32_t note){for(uint32_t i=0;i<NVOICE;i++)if(trk[0].v[i].active && trk[0].v[i].note==note && trk[0].v[i].gate)return &trk[0].v[i];return 0;}
static void timing_blocks(uint32_t n){render_blocks((n+CTL-1)/CTL);}
int main(int argc,char **argv){
 int bad=0;ui_power_on();memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);fixture(2,0);ko_product_boot();render_blocks(32);go_home();song.sel=0;
 ko_table_t tab={0};tab.n=4;for(uint32_t i=0;i<4;i++){tab.chop[i].a=0;tab.chop[i].b=32768;tab.chop[i].rsv=KO_GATE;}
 ko_restore(&tab);track_t *t=&trk[0];t->p[P_SLEN]=16;t->p[P_SQNT]=0;memset(t->step,0,sizeof t->step);
 t->step[0]=(step_t){.note={60,61},.n=2,.time=ST_NOTE,.vel=100};t->step[1]=(step_t){.note={61},.n=1,.time=ST_NOTE,.vel=100};ko_seq[0].cursor=0;ko.view=7;ko_piano_roll[0]=1;int division=t->p[P_SDIV];press(B_FX);
 bad+=check("piano roll FX opens per-note timing",ko.view==31);
 ko_knob(1,4);bad+=check("one note gets four-step length without changing DIV",ko_note_length(ko_note_data(&motion,t,0,0))==4 && !ko_note_data(&motion,t,0,1) && t->p[P_SDIV]==division);
 ko_knob(0,1);ko_knob(2,4);bad+=check("second note nudge leaves first note unchanged",ko_note_nudge(ko_note_data(&motion,t,0,1))==4 && !ko_note_nudge(ko_note_data(&motion,t,0,0)));
 press(B_FX);ko_draw();int readout=0;for(uint32_t i=0;i<ko_line_n;i++)if(ko_lines[i].y==178 && strstr(ko_lines[i].text,"NUDGE 4/16"))readout=1;
 bad+=check("roll retains selected chord note and shows its timing",ko.view==7 && ko_note_selected[0]==1 && readout);
 uint32_t period=div_samples(t->p[P_SDIV]);seq_start();render_blocks(1);voice_t *long_voice=playing(60);
 bad+=check("first note starts on the step while second waits",playing(60) && !playing(61));
 timing_blocks(period/8);bad+=check("delayed chord note stays pending before nudge",!playing(61));timing_blocks(period/8+CTL*2);bad+=check("delayed chord note starts independently",playing(61)!=0);
 timing_blocks(period);bad+=check("long note survives next empty step",playing(60)!=0);
 timing_blocks(period);bad+=check("ordinary note-offs do not cut a different long note",long_voice && long_voice->active && long_voice->gate);
 timing_blocks(period*2);bad+=check("long note releases after its own duration",!playing(60));seq_stop();
 motion_clear(t);t->step[1]=t->step[0];t->step[0]=(step_t){.time=ST_REST};ko_note_put(t,1,0,12); /* -4/16 */
 seq_start();timing_blocks(period/2);bad+=check("negative nudge waits until preceding step tail",!playing(60));timing_blocks(period/4+CTL*2);
 bad+=check("negative nudge starts one chord note early",playing(60) && !playing(61));timing_blocks(period/4+CTL*2);bad+=check("other chord note remains on nominal step",playing(61)!=0);seq_stop();
 ko_seq[0].cursor=1;ko.view=31;ko_note_selected[0]=0;step_undo_take(t,0);ko_note_put(t,1,0,0x40);motion_undo_done(t);undo_swap();bad+=check("undo restores previous per-note timing",ko_note_data(&motion,t,1,0)==12);undo_swap();bad+=check("redo restores long note",ko_note_data(&motion,t,1,0)==0x40);
 project_t before,after;project_store_t packed;project_capture(&before);
 bad+=check("project retains fixed 3840-byte storage with note timing",sizeof packed==3840 && proj_pack(&packed,&before) && proj_unpack(&after,packed.raw,sizeof packed));
 bad+=check("per-note metadata survives serialization",ko_note_data(&after.motion,t,1,0)==0x40 && after.t[0].step[1].note[1]==61);
 ko_note_put(t,1,1,0x24);ko_step_note(&t->step[1],60,100,1);bad+=check("deleting a note moves its neighbor's timing with it",t->step[1].note[0]==61 && ko_note_data(&motion,t,1,0)==0x24 && !ko_note_data(&motion,t,1,1));
 seq_start();render_blocks(1);seq_stop();int clear=1;for(uint32_t j=0;j<KO_NOTE_JOBS;j++)clear&=!ko_note_job[0][j].on;bad+=check("stop clears pending and long-note jobs",clear);
 song.sel=1;track_t *kit=&trk[1];memset(kit->step,0,sizeof kit->step);kit->p[P_SLEN]=16;kit->p[P_SQNT]=0;
 kit->step[0]=(step_t){.note={60,61},.n=2,.time=ST_NOTE,.vel=100};ko_note_put(kit,0,0,4);motion.on&=~2u;seq_start();render_blocks(1);
 int sixty=0,sixtyone=0;for(uint32_t i=0;i<NVOICE;i++)if(kit->v[i].active){sixty|=kit->v[i].note==60;sixtyone|=kit->v[i].note==61;}
 bad+=check("drum notes have independent nudge even with sound automation off",!sixty && sixtyone);
 timing_blocks(div_samples(kit->p[P_SDIV])/4u+CTL*2);sixty=0;for(uint32_t i=0;i<NVOICE;i++)sixty|=kit->v[i].active && kit->v[i].note==60;
 bad+=check("nudged drum triggers at its own offset",sixty);seq_stop();song.sel=0;
 motion_store_t saved=motion;memset(&motion,0,sizeof motion);motion.count=MOTION_MAX;
 for(uint32_t j=0;j<MOTION_MAX;j++)motion.event[j]=(motion_event_t){.place=j%64,.param=(uint8_t)(MOTION_LOCK|(j<64?P_SGATE:P_TRANS)),.value=0};
 bad+=check("full timing storage rejects an edit without losing other events",ko_note_put(t,3,0,0x40)==2 && motion.count==MOTION_MAX);
 motion.event[0]=(motion_event_t){.place=3,.param=MOTION_LOCK|P_NOTE1,.value=-28};
 t->step[3]=(step_t){.note={60,61},.n=2,.time=ST_NOTE};ko_step_note(&t->step[3],60,100,1);
 bad+=check("note deletion preserves neighboring timing in a full store",motion.count==MOTION_MAX && ko_note_data(&motion,t,3,0)==0x24 && t->step[3].note[0]==61);
 motion=saved;
 if(argc>1){project_store_t old;FILE *f=fopen(argv[1],"rb");if(!f)return 2;size_t n=fread(old.raw,1,sizeof old,f);fclose(f);bad+=check("existing v0.4/v0.3 projects migrate with default note timing",n==sizeof old && proj_unpack(&after,old.raw,sizeof old) && after.t[0].p[P_NOTE0]==-64);}
 printf("PIGEON per-note timing: %s\n",bad?"FAILED":"PASSED");return !!bad;
}
