/* SPDX-License-Identifier: GPL-3.0-only */
/* Check PIGEON's actual screens with its shipped Little Character font. */
#include <stdio.h>
#include <stdint.h>
static uint32_t text_checks,text_failures,screen_view,screen_track;
static int lint_active;
static void screen_text(int x0,int y0,int x1,int y1,const char *text,unsigned flags) {
    (void)x0;(void)y0;(void)x1;(void)y1;
    if(!lint_active || flags&4u) return;
    text_checks++;
    if((flags&3u) && !(flags&8u)) {
        printf("screen: track %u view %u clipped text '%s' flags %u\n",screen_track,screen_view,text,flags);
        text_failures++;
    }
}
#define GFX_HOOK_TEXT(x0,y0,x1,y1,s,flags) screen_text(x0,y0,x1,y1,s,flags)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_BLIT(x,y,r0) ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN ko_controls_test_main
#include "ko_test.c"
int main(void) {
    ui_power_on();memset(ko_flash,255,sizeof ko_flash);slc_store_boot();
    fixture(0,0);fixture(1,1);fixture(2,0);menu_put(MI_KO,1);ko_states[0].on=1;ko_setup();render_blocks(32);
    song.sel=2;ko_setup_second();render_blocks(32);song.sel=0;go_home();
    for(screen_track=0;screen_track<SAMPLER_PARTS;screen_track++) {
        song.sel=screen_track;
        for(screen_view=0;screen_view<29;screen_view++) {
            ko.view=screen_view;ko_ui_invalidate();ui.force=1;lint_active=1;ko_draw();lint_active=0;
        }
    }
    for(screen_track=0;screen_track<SAMPLER_PARTS;screen_track++) {
        song.sel=screen_track;song.g[G_DFDBK]=120;ko.comp[song.sel==1]=(ko_comp_t){48,8,100,50,12};
        TSEL->p[P_DLY]=TSEL->p[P_CHOR]=TSEL->p[P_REV]=TSEL->p[P_DIST]=TSEL->p[P_LEVEL]=127;
        for(uint32_t time=0;time<sizeof N_DTIME/sizeof N_DTIME[0];time++) {
            song.g[G_DTIME]=(int16_t)time;
            for(screen_view=1;screen_view<=4;screen_view++){ko.view=screen_view;ko_ui_invalidate();ui.force=1;lint_active=1;ko_draw();lint_active=0;}
        }
    }
    song.sel=0;ui.menu=ui.confirm=ui.layer=ui.lock=0;
    for(uint32_t layer=1;layer<LAYER_N;layer++)if(layer_supported(layer)){
        screen_view=100+layer;ui.layer=ui.lock=layer;ui.force=1;lint_active=1;ui_draw();lint_active=0;
    }
    ui.layer=ui.lock=0;
    for(uint32_t row=0;row<MI_COUNT;row++)if(menu_visible(row)){
        screen_view=200+row;ui.menu=1;ui.menu_sel=row;ui.force=1;lint_active=1;ui_draw();lint_active=0;
    }
    ui.menu=0;
    printf("PIGEON screen text: %u checks, %u clipping failures\n",text_checks,text_failures);
    return !text_checks || text_failures;
}
