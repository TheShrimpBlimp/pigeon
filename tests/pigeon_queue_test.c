/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN queue_base_main
#include "ko_test.c"
static int next_queue(uint32_t id,uint32_t age){
 for(uint32_t b=0;b<4000;b++){render_blocks(1);if(!ko_queue[id].active || ko_queue[id].age!=age)return 1;}return 0;
}
int main(void){
 int bad=0;ui_power_on();memset(ko_states,0,sizeof ko_states);
 memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);fixture(2,0);
 ko_product_boot();ko_setup();render_blocks(32);go_home();song.sel=0;
 for(uint32_t id=0;id<2;id++){
  song.sel=id?2:0;ko_context=id;ko_table_t tab={0};tab.n=4;
  for(uint32_t i=0;i<4;i++){tab.chop[i].a=i*4096;tab.chop[i].b=(i+1)*4096;tab.chop[i].rsv=KO_LATCH|KO_GATE;}
  ko_restore(&tab);ko_context=~0u;
 }
 song.sel=0;ko.view=0;press(B_EDIT);press(B_ARP);
 bad+=check("EDIT then ARP opens song queue page",ko.view==29);
 ko_knob(0,1);bad+=check("K1 enables queue",ko_queue[0].on);
 input_on(&trk[0],60,100);input_off(&trk[0],60);uint32_t age=ko_queue[0].age;
 voice_t *v=&trk[0].v[ko_queue[0].voice];
 bad+=check("first chop starts immediately and release cannot shorten it",ko_queue[0].active && v->gate && (v->s[4]&KO_V_NO_LOOP) && !(v->s[4]&KO_V_LATCH) && !(ko_env_flags[0][v-trk[0].v]&KO_GATE));
 input_on(&trk[0],61,90);input_off(&trk[0],61);input_on(&trk[0],61,80);input_off(&trk[0],61);input_on(&trk[0],62,100);input_off(&trk[0],62);
 ko_knob(0,1);bad+=check("repeated enable does not clear ordered entries",ko_queue[0].count==3 && ko_queue[0].age==age);
 render_blocks(4);bad+=check("queued chops do not interrupt current chop",ko_queue[0].age==age && ko_queue[0].count==3);
 ko_knob(1,-1);bad+=check("K2 left drops last waiting chop without interrupting current",ko_queue[0].count==2 && ko_queue[0].age==age);input_on(&trk[0],62,100);
 ko_knob(2,2);press(B_ENV);bad+=check("ENV removes selected waiting entry on queue page",ko.view==29 && ko_queue[0].count==2 && ko_queue[0].age==age);input_on(&trk[0],62,100);ko_draw();
 uint8_t expected[]={61,61,62};
 for(uint32_t i=0;i<3;i++){bad+=check("queue advances in FIFO order, including repeated chops",next_queue(0,age)&&ko_queue[0].active&&ko_queue[0].current==expected[i]&&ko_queue[0].count==2-i);age=ko_queue[0].age;}
 bad+=check("queue becomes idle after its last chop ends",next_queue(0,age)&&!ko_queue[0].active&&!ko_queue[0].count);
 input_on(&trk[0],63,100);input_off(&trk[0],63);bad+=check("new press restarts an idle queue",ko_queue[0].active&&ko_queue[0].current==63);
 age=ko_queue[0].age;input_on(&trk[0],60,100);ko_knob(1,1);
 bad+=check("clear waiting leaves current chop playing",!ko_queue[0].count&&ko_queue[0].active&&ko_queue[0].age==age);
 for(uint32_t i=0;i<KO_QUEUE_CAP+1;i++)input_on(&trk[0],60+i%4,100);
 bad+=check("full queue retains 64 entries and reports overflow",ko_queue[0].count==KO_QUEUE_CAP&&ko_queue_full);
 for(uint32_t i=0;i<8;i++){age=ko_queue[0].age;next_queue(0,age);input_on(&trk[0],60+i%4,100);}
 bad+=check("FIFO wraps without overwriting pending entries",ko_queue[0].count==KO_QUEUE_CAP);
 ko_knob(2,31);uint32_t removed_index=ko_queue[0].selected,following=ko_queue[0].note[(ko_queue[0].head+removed_index+1)%KO_QUEUE_CAP];uint32_t following_vel=ko_queue[0].vel[(ko_queue[0].head+removed_index+1)%KO_QUEUE_CAP];press(B_ENV);bad+=check("middle removal after ring wrap preserves note and velocity order",ko_queue[0].note[(ko_queue[0].head+removed_index)%KO_QUEUE_CAP]==following && ko_queue[0].vel[(ko_queue[0].head+removed_index)%KO_QUEUE_CAP]==following_vel);input_on(&trk[0],63,100);bad+=check("removing after ring wrap preserves tail ordering",ko_queue[0].count==KO_QUEUE_CAP && ko_queue[0].note[(ko_queue[0].head+ko_queue[0].count-1)%KO_QUEUE_CAP]==63);
 song.sel=2;ko_queue[1].on=1;input_on(&trk[2],62,100);input_off(&trk[2],62);
 bad+=check("Song 2 queue runs independently",ko_queue[1].active&&ko_queue[1].current==62&&ko_queue[0].count==KO_QUEUE_CAP);
 song.sel=0;ko.view=29;press(B_PLAY);
 bad+=check("PLAY stops selected queue without starting sequencer",!ko_queue[0].active&&!ko_queue[0].count&&ko_queue[0].on&&!transport_req&&ko_queue[1].active);
 press(B_HOME);bad+=check("HOME returns to waveform without disabling mode",ko.view==0&&ko_queue[0].on);
 input_on(&trk[0],60,100);rec_tap();bad+=check("arming recording cancels queue mode",!ko_queue[0].on&&!ko_queue[0].active);song.rec=0;
 seq_start();bad+=check("starting sequencer clears performance queues",!ko_queue[1].on&&!ko_queue[1].active);seq_stop();
 ko_queue[0].on=1;input_on(&trk[0],60,100);input_on(&trk[0],61,100);smp_user_gen++;ko_queue_tick(&trk[0]);
 bad+=check("bank replacement cancels queued old chop references",!ko_queue[0].active&&!ko_queue[0].count&&!ko_queue[0].on);
 printf("PIGEON chop queue: %s\n",bad?"FAILED":"PASSED");return !!bad;
}
