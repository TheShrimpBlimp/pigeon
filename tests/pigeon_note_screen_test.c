/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
static int note_lint,note_clipped;
static void note_text(int x0,int y0,int x1,int y1,const char *s,unsigned flags){(void)x0;(void)y0;(void)x1;(void)y1;if(note_lint && (flags&3u) && !(flags&8u)){fprintf(stderr,"Clipped note editor text: %s\n",s);note_clipped++;}}
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_BLIT(x,y,r) ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#define GFX_HOOK_TEXT(x0,y0,x1,y1,s,f) note_text(x0,y0,x1,y1,s,f)
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_TEST_MAIN note_screen_base_main
#include "ko_test.c"
int main(void){ui_power_on();memset(ko_flash,255,sizeof ko_flash);slc_store_boot();fixture(0,0);fixture(1,1);fixture(2,0);ko_product_boot();render_blocks(32);go_home();
 for(uint32_t part=0;part<SAMPLER_PARTS;part++){song.sel=part;ko_prepare();TSEL->p[P_SLEN]=64;TSEL->step[0]=(step_t){.note={60,61,62,63},.n=4,.time=ST_NOTE};ko_note_put(TSEL,0,0,11u<<4);ko_note_put(TSEL,0,1,0x4F);ko.view=31;
  for(uint32_t j=0;j<4;j++){ko_note_selected[part]=j;ko_ui_invalidate();ui.force=1;note_lint=1;ko_draw();note_lint=0;}
  for(uint32_t page=0;page<4;page++){ko.view=7;ko_piano_roll[part]=1;ko_seq[part].cursor=page*16;ko_ui_invalidate();ui.force=1;note_lint=1;ko_draw();note_lint=0;}
 }
 song.sel=0;ko_prepare();
 for(uint32_t controls=0;controls<2;controls++)for(uint32_t record=0;record<2;record++){
    ko.view=30;ko_scratch[0].controls=controls;ko_scratch[0].record=record;ko_scratch[0].cents=-100;ko_scratch[0].rate=-400;ko_scratch[0].brake_ms=2000;ko_scratch[0].release_ms=500;ko_ui_invalidate();ui.force=1;note_lint=1;ko_draw();note_lint=0;
 }
 ko.view=29;ko_queue[0].count=64;ko_queue[0].active=1;ko_queue[0].current=91;for(uint32_t i=0;i<64;i++)ko_queue[0].note[i]=91;ko_ui_invalidate();ui.force=1;note_lint=1;ko_draw();note_lint=0;
 for(uint32_t part=0;part<SAMPLER_PARTS;part++){song.sel=part;ko_prepare();ko.view=32;
  for(uint32_t action=0;action<4;action++){ko_tool_action=action;ko_euclid_hits=ko_euclid_cycle=64;ko_euclid_rot=63;ko_human_vel=ko_human_time=63;ko_ui_invalidate();ui.force=1;note_lint=1;ko_draw();note_lint=0;}}
 printf("Note editor and multi-page duration-bar text: %s\n",note_clipped?"FAILED":"PASSED");return !!note_clipped;}
