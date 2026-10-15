/* SPDX-License-Identifier: GPL-3.0-only */
/* Capture real firmware pixels with host fixture audio; no image generation. */
#include <stdio.h>
static FILE *font_trace;
static unsigned trace_fg,trace_bg;
#include <string.h>
static struct {int x0,y0,x1,y1;unsigned fg,bg;char text[256];} pending[256];
static unsigned npending;
static void trace_text(int x0,int y0,int x1,int y1,const char *s,unsigned flags) {
    if(!font_trace || flags&4u || npending>=256)return;
    pending[npending].x0=x0;pending[npending].y0=y0;pending[npending].x1=x1;pending[npending].y1=y1;
    pending[npending].fg=trace_fg;pending[npending].bg=trace_bg;
    snprintf(pending[npending].text,256,"%s",s);npending++;
}
static void trace_blit(int x,int y,int r0){
    if(!font_trace)return;
    for(unsigned i=0;i<npending;i++){
        if(pending[i].y0<r0)continue;
        fprintf(font_trace,"%d %d %d %d %u %u ",pending[i].x0+x,pending[i].y0+y,pending[i].x1+x,pending[i].y1+y,pending[i].fg,pending[i].bg);
        for(const unsigned char *s=(void*)pending[i].text;*s;s++)fprintf(font_trace,"%02x",*s);
        fputc('\n',font_trace);
    }
}
#define GFX_HOOK_TEXT(x0,y0,x1,y1,s,flags) trace_text(x0,y0,x1,y1,s,flags)
#define GFX_HOOK_INK(fg,bg) (trace_fg=(fg),trace_bg=(bg))
#define GFX_HOOK_BLIT(x,y,r0) trace_blit(x,y,r0)
#define GFX_HOOK_BEGIN() (npending=0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#define KO_TEST_MAIN unused_ko_test_main
#include "../tests/ko_test.c"
static void capture(const char *dir,const char *name){
    char path[512];snprintf(path,sizeof path,"%s/%s-text.txt",dir,name);font_trace=fopen(path,"w");
    ko_ui_invalidate();screen_save(dir,name);fclose(font_trace);font_trace=0;
}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    ui_power_on();memset(ko_states,0,sizeof ko_states);
    memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);
    menu_put(MI_KO,1);ko_states[0].on=1;ko_setup();render_blocks(32);song.sel=0;go_home();
    ko_palette_on=1;palette_set(0);ko_select(2);ko.zoom=2;ko.view=0;capture(argv[1],"waveform");
    ko_seq[0].enabled=1;trk[0].p[P_SLEN]=16;ko_seq[0].cursor=6;
    for(uint32_t i=0;i<16;i++){trk[0].step[i]=(step_t){.note={60+i%4},.n=(i%4==0 || i==6),.time=ST_NOTE,.vel=100};}
    ko.view=7;capture(argv[1],"sequencer");
    ko.view=27;ko_sync[0]=(ko_sync_t){.beats=4,.samples=44100,.on=1};capture(argv[1],"tempo-sync");
    ko.view=0;song.sel=0;
    trk[0].p[P_E7]=94;trk[0].p[P_DLY]=32;trk[0].p[P_CHOR]=18;trk[0].p[P_REV]=46;
    trk[0].p[P_DIST]=24;trk[0].p[P_LEVEL]=100;song.g[G_DTIME]=11;song.g[G_DFDBK]=56;
    ko.comp[0]=(ko_comp_t){18,4,12,18,3};
    for(uint32_t view=1;view<=4;view++) {char name[24];snprintf(name,sizeof name,"fx-%u",view);ko.view=view;capture(argv[1],name);}
    return 0;
}
