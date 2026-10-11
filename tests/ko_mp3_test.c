/* SPDX-License-Identifier: GPL-3.0-only */
/* Seeked decode against uninterrupted libmad state, across the entire minute. */
#define FPM_64BIT
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/src/ko_mp3.h"
#include "../firmware/vendor/libmad/stream.h"
#include "../firmware/vendor/libmad/frame.h"
#include "../firmware/vendor/libmad/synth.h"
static uint32_t rd(const uint8_t *p) { return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
int main(int argc,char **argv)
{
    if(argc!=2)return 2;
    FILE *f=fopen(argv[1],"rb");if(!f)return 2;fseek(f,0,SEEK_END);long len=ftell(f);rewind(f);
    uint8_t *bank=malloc(len+8);if(fread(bank,1,len,f)!=(size_t)len)return 2;fclose(f);memset(bank+len,0,8);
    uint32_t off=rd(bank+28),frames=rd(bank+48),bytes=rd(bank+32),table=(frames+1)*4;
    struct mad_stream stream;struct mad_frame frame;struct mad_synth synth;
    uint8_t main_data[MAD_BUFFER_MDLEN];mad_fixed_t overlap[2][32][18];
    mad_stream_init(&stream);mad_frame_init(&frame);mad_synth_init(&synth);
    stream.main_data=(void *)&main_data;frame.overlap=&overlap;memset(main_data,0,sizeof main_data);memset(overlap,0,sizeof overlap);
    uint8_t *audio=malloc(bytes-table+8);memcpy(audio,bank+off+table,bytes-table);memset(audio+bytes-table,0,8);
    mad_stream_buffer(&stream,audio,bytes-table+8);
    uint32_t mismatches=0;int16_t pcm[576],previous[576],bridge[192];uint8_t input[856];
    for(uint32_t i=0;i<frames;i++){
        if(mad_frame_decode(&frame,&stream)){printf("reference error %u %#x\n",i,stream.error);return 1;}
        mad_synth_frame(&synth,&frame);
        uint32_t first=i>1?i-2:0,a=rd(bank+off+first*4),b=rd(bank+off+(i+1)*4);
        memcpy(input,bank+off+a,b-a);memset(input+(b-a),0,8);
        if(ko_mp3_decode(input,b-a+8,i-first+1,pcm)){printf("seek error %u\n",i);return 1;}
        for(uint32_t j=0;j<576;j++){
            int32_t x=synth.pcm.samples[0][j]>>13;x=x<-32768?-32768:x>32767?32767:x;
            if(pcm[j]!=x)mismatches++;
        }
        if(i){
            uint32_t target=i-1,pre=target>1?target-2:0;
            a=rd(bank+off+pre*4);b=rd(bank+off+(i+1)*4);
            memcpy(input,bank+off+a,b-a);memset(input+(b-a),0,8);
            if(ko_mp3_decode_bridge(input,b-a+8,i-pre+1,500,bridge))return 1;
            for(uint32_t j=0;j<192;j++)if(bridge[j]!=(j<76?previous[500+j]:pcm[j-76]))mismatches++;
        }
        memcpy(previous,pcm,sizeof previous);
    }
    printf("MP3: %u frames, seek/sequential mismatches %u\n",frames,mismatches);
    memset(input,255,sizeof input);
    if(ko_mp3_decode(input,100,1,pcm)==0 || ko_mp3_decode(input,649,3,pcm)==0)return 1;
    free(audio);free(bank);return mismatches?1:0;
}
