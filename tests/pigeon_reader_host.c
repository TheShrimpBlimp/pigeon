/* SPDX-License-Identifier: GPL-3.0-only */
/* Scriptable real PIGEON UI + SR protocol simulator. No physical MIDI or flash. */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define PIGEON_READER 1
#define KO_TEST_MAIN reader_base_main
#include "ko_test.c"
#include "../firmware/src/pigeon_reader.c"
static char heard[256][ANN_MAX+1];static uint32_t heard_n,wire_count;static int blocked;
static FILE *said_log;
static int ann_wire_send(const uint8_t *p,uint32_t n)
{
    if(blocked)return -1;
    pr_wire_sent++;wire_count++;
    if(n>=7 && p[4]==ANN_SAY){uint32_t len=n-7;if(len>ANN_MAX)abort();
        char *out=heard[heard_n++%256];memcpy(out,p+6,len);out[len]=0;
        if(said_log){fprintf(said_log,"%s\t%s\n",p[5]&ANN_KEEP?"WARN":"MSG",out);fflush(said_log);}
    }return 0;
}
static void reader_command(uint32_t cmd)
{const uint8_t frame[]={ANN_H0,ANN_H1,ANN_H2,(uint8_t)cmd,ANN_VER};if(!ann_frame(frame,sizeof frame,fm1_ms))abort();}
static int said(const char *s)
{for(uint32_t n=heard_n>256?heard_n-256:0;n<heard_n;n++)if(strstr(heard[n%256],s))return 1;return 0;}
static void dump_snapshot(void)
{
    pr_snapshot();printf("%s\t%s\n",ui.confirm?"ASK":"SCREEN",pr.context);
    for(uint32_t k=0;k<4;k++)if(pr.value[k][0])printf("%s\t%s\n",ui.menu==1?"ROW":"KNOB",pr.value[k]);
    if(pr.detail[0])printf("DETAIL\t%s\n",pr.detail);
}
static void speakdump(void)
{
    ui.msg_t=ui.confirm=ui.menu=ui.layer=ui.lock=0;
    for(uint32_t track=0;track<SAMPLER_PARTS;track++){
        song.sel=track;ko_prepare();
        for(uint32_t v=0;v<KO_VIEW_COUNT;v++){
            if(v==17 || (track==1 && (v==5||v==19||v==20||v==25||v==27||v==28||v==29||v==30)) || (track!=1 && (v==16||v==26)))continue;
            ko.view=v;dump_snapshot();
        }
    }
    song.sel=0;ko_prepare();
    for(uint32_t layer=1;layer<LAYER_N;layer++)if(layer_supported(layer)){ui.layer=layer;dump_snapshot();}ui.layer=0;
    for(uint32_t row=0;row<MI_COUNT;row++)if(menu_visible(row)){
        ui.menu=1;ui.menu_sel=row;
        uint32_t count=menu_valued(row)?menu_n(row):1;
        for(uint32_t v=0;v<count;v++){if(menu_valued(row))menu_put(row,v);dump_snapshot();}
    }
    ui.menu=2;dump_snapshot();ui.menu=3;dump_snapshot();ui.menu=0;
    for(uint32_t i=1;i<=CF_ERASE_USER;i++){ui.confirm=i;dump_snapshot();}ui.confirm=0;
    for(uint32_t i=0;i<3;i++){
        static const char *const mode[]={"off","record","on"};
        pr_value(0,"metronome",mode[i]);printf("KNOB\t%s\n",pr.value[0]);
        static const char *const cond[]={"always","every second","every fourth","first pass","after first"};
        for(uint32_t j=0;j<5;j++)printf("VALUE\t%s\n",cond[j]);
    }
    for(uint32_t i=0;i<NELEM(N_DTIME);i++)printf("VALUE\t%s\n",N_DTIME[i]);
    for(uint32_t i=0;i<NELEM(N_DIV);i++)printf("VALUE\t%s\n",N_DIV[i]);
    puts("ASK\tClear this phrase? Octave down no. Octave up yes.");
    puts("WARN\tCannot save. Stop playback first.");
}
int main(int argc,char **argv)
{
    ui_power_on();memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);fixture(2,0);
    ko_product_boot();render_blocks(32);go_home();
    const char *log=getenv("PIGEON_SAID_LOG");if(log){said_log=fopen(log,"a");if(!said_log){perror(log);return 1;}}
    FILE *script=argc>1?fopen(argv[1],"r"):stdin;if(!script){perror(argv[1]);return 1;}
    char line[512];uint32_t line_no=0,bad=0;
    while(fgets(line,sizeof line,script)){
        line_no++;line[strcspn(line,"\r\n")]=0;if(!line[0]||line[0]=='#')continue;
        char *arg=strchr(line,' ');if(arg)*arg++=0;else arg="";
        if(!strcmp(line,"listen")){reader_command(ANN_HELLO);frames(160);}
        else if(!strcmp(line,"where")){reader_command(ANN_WHERE);frames(160);}
        else if(!strcmp(line,"disconnect")){reader_command(ANN_BYE);frames(32);}
        else if(!strcmp(line,"wait"))frames(atoi(arg));
        else if(!strcmp(line,"page")){ko.view=atoi(arg);ui.force=1;frames(32);}
        else if(!strcmp(line,"track")){song.sel=atoi(arg);ko_prepare();ui.force=1;frames(32);}
        else if(!strcmp(line,"turn")){int k,n;if(sscanf(arg,"%d %d",&k,&n)!=2||k<1||k>4)return 2;turn(EN_K1+k-1,n);frames(240);}
        else if(!strcmp(line,"preset")){turn(EN_PRESET,atoi(arg));frames(240);}
        else if(!strcmp(line,"press")){int found=0;for(uint32_t k=0;k<NB;k++)if(!strcmp(arg,B_NAME[k])){press(k);frames(240);found=1;break;}if(!found){fprintf(stderr,"Unknown button %s\n",arg);return 2;}}
        else if(!strcmp(line,"note")){input_on(TSEL,atoi(arg),100);frames(32);}
        else if(!strcmp(line,"release")){input_off(TSEL,atoi(arg));frames(32);}
        else if(!strcmp(line,"chord")){step_t *n=&TSEL->step[ko_seq[song.sel].cursor];n->n=4;n->time=ST_NOTE;unsigned a,b,c,d;if(sscanf(arg,"%u %u %u %u",&a,&b,&c,&d)!=4)return 2;n->note[0]=a;n->note[1]=b;n->note[2]=c;n->note[3]=d;frames(240);}
        else if(!strcmp(line,"message")){ui_message(arg);frames(32);}
        else if(!strcmp(line,"said")){if(!said(arg)){fprintf(stderr,"reader:%u missing: %s\n",line_no,arg);bad++;}}
        else if(!strcmp(line,"not-said")){if(said(arg)){fprintf(stderr,"reader:%u unexpected: %s\n",line_no,arg);bad++;}}
        else if(!strcmp(line,"count")){if(heard_n!=(uint32_t)atoi(arg)){fprintf(stderr,"reader:%u expected %s messages, got %u\n",line_no,arg,heard_n);bad++;}}
        else if(!strcmp(line,"clear"))heard_n=0;
        else if(!strcmp(line,"silent")){if(heard_n){fprintf(stderr,"reader:%u expected silence, heard %u messages\n",line_no,heard_n);bad++;}}
        else if(!strcmp(line,"block"))blocked=atoi(arg);
        else if(!strcmp(line,"speakdump"))speakdump();
        else {fprintf(stderr,"reader:%u unknown command %s\n",line_no,line);return 2;}
    }
    if(said_log)fclose(said_log);if(script!=stdin)fclose(script);
    fprintf(stderr,"PIGEON reader scenarios: %u failures, %u wire messages\n",bad,wire_count);return bad?1:0;
}
