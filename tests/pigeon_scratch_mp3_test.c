/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_STORAGE_TEST_MAIN scratch_storage_base_main
#include "ko_storage_test.c"
static int64_t energy;
static void run_scratch(uint32_t n){for(uint32_t i=0;i<n;i++){for(uint32_t j=0;j<32;j++)ko_pcm_poll();int32_t out[CTL*2];mix_block(out,CTL);for(uint32_t j=0;j<CTL*2;j++)energy+=out[j]<0?-(int64_t)out[j]:out[j];}}
int main(int argc,char **argv){
 if(argc!=2)return 2;uint8_t *image;uint32_t size;int bad=0;
 if(load(argv[1],&image,&size))return 2;
 memset(bank_flash,255,sizeof bank_flash);ui_power_on();song.master_q12=4096;slc_store_boot();ko.on=1;ko_setup();pump(32);
 bad+=check("scratch fixture bank uploads",!upload(image,size));free(image);pump(32);song.sel=0;go_home();ko.view=5;press(B_FX);
 ko_table_t tab=*ko_table();tab.n=1;tab.chop[0].a=1000;tab.chop[0].b=10000;ko_restore(&tab);
 input_on(&trk[0],60,100);input_off(&trk[0],60);run_scratch(150);
 voice_t *v=0;for(uint32_t i=0;i<NVOICE;i++)if(trk[0].v[i].active && ((uint32_t)trk[0].v[i].s[4]&KO_V_SCRATCH))v=&trk[0].v[i];
 bad+=check("MP3 scratch starts and crosses cached frames",v && v->ph[0]>1600 && v->ph[0]<10000);
 if(v){
 ko_scratch[0].motor=0;v->ph[0]=6000;ko_scratch[0].jog=-65536;ko_scratch[0].touched=FS;run_scratch(100);
 bad+=check("MP3 reverse traverses previous PCM frames",v->active && v->ph[0]<5000 && v->ph[0]>=1000);
 ko_scratch[0].jog=65536;ko_scratch[0].touched=FS;v->ph[0]=9950;run_scratch(30);
 bad+=check("MP3 forward wraps to chop start",v->active && v->ph[0]>=1000 && v->ph[0]<3000);
 ko_scratch[0].jog=-65536;ko_scratch[0].touched=FS;v->ph[0]=1010;run_scratch(30);
 bad+=check("MP3 reverse wraps to chop end",v->active && v->ph[0]>8000 && v->ph[0]<10000);
 }
 bad+=check("scratching produces audible PCM",energy>100000);
 ko_scratch_stop(&trk[0]);printf("PIGEON MP3 scratch: %s\n",bad?"FAILED":"PASSED");return !!bad;
}
