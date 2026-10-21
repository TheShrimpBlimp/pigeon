/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_STORAGE_TEST_MAIN v05_storage_fixture_main
#include "ko_storage_test.c"
int main(int argc,char **argv){
 if(argc!=2)return 2;uint8_t *image;uint32_t size;int bad=0;if(load(argv[1],&image,&size))return 2;
 memset(bank_flash,255,sizeof bank_flash);ui_power_on();slc_store_boot();ko.on=1;ko_setup();pump(32);
 uint32_t used=(uint32_t)image[8]|(uint32_t)image[9]<<8|(uint32_t)image[10]<<16|(uint32_t)image[11]<<24;
 int ok=size==used+4096 && !upload(image,used);pump(32);
 uint8_t wire[300];if(ok)for(uint32_t off=0;off<4096;off+=256){wire21(wire,off);if(request(95,wire,3+packed(wire+3,image+used+off,256)))ok=0;}
 bad+=check("loader v0.5 bank uploads and validates",ok);free(image);pump(32);song.sel=0;go_home();ko_prepare();
 const ko_chop_t *c=&ko_table()->chop[0];
 bad+=check("loader metadata imports with exact tuning and gain",ko_table()->n==2 && c->a==100 && c->b==10000 && c->cents==-37 && c->gain==768);
 bad+=check("loader metadata imports start direction and crossfade",c->play==32768 && c->reverse==1 && c->xfade==8);
 bad+=check("loader playback checkpoint rebuilt by firmware",c->play_st==0);
 printf("v0.5 loader/firmware wire: %s\n",bad?"FAIL":"PASS");return !!bad;
}
