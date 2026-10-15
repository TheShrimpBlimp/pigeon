/* SPDX-License-Identifier: GPL-3.0-only */
/* Small, labelled pages: one gesture to select, duplicate or append a phrase. */
static void ko_mixer_turn(uint32_t k,int32_t steps)
{
    uint32_t id=ko.view==10?P_LEVEL:ko.view==11?P_DLY:ko.view==12?P_CHOR:P_REV;
    track_t *t=k<SAMPLER_PARTS?&trk[k]:TSEL;
    if(k==3) id=P_PAN;
    const param_desc_t *pd=track_desc(t,id);
    t->p[id]=(int16_t)param_turn(pd,t->p[id],steps);motion_capture(t,id,t->p[id]);
}
static void ko_mixer_draw(void)
{
    uint32_t id=ko.view==10?P_LEVEL:ko.view==11?P_DLY:ko.view==12?P_CHOR:P_REV;
    ko_page(ko.view==10?"MIXER / LEVEL":ko.view==11?"MIXER / DELAY":ko.view==12?"MIXER / CHORUS":"MIXER / REVERB", "K4 PAN: SELECTED TRACK", "ARP NEXT / EDIT BACK");
    ko_fx_number(0,"K1 SONG 1",trk[0].p[id],"",127);ko_fx_number(1,"K2 DRUM KIT",trk[1].p[id],"",127);
    ko_fx_number(2,"K3 SONG 2",trk[2].p[id],"",127);ko_fx_number(3,"K4 PAN",TSEL->p[P_PAN],"",0);
}
static void ko_pattern_tiles(int force)
{
    static uint8_t previous[4]={255,255,255,255};
    for(uint32_t i=0;i<4;i++) {
        uint8_t flags=(uint8_t)(((ko_pattern_mask>>i)&1u) | ((ko_pattern_sel==i)<<1) |
            ((chain.running && ko_launch_mode && ko_pattern_active==i)<<2) |
            ((ko_launch_pending==i && (ui.frame&8u))<<3));
        if(!force && previous[i]==flags) continue;previous[i]=flags;
        uint16_t bg=flags&4?T_ACCENT:flags&2?T_THEME:T_BG;
        uint16_t ink=flags&6?T_BG:flags&1?T_THEME:T_MID;
        char name[2]={(char)('A'+i),0};
        cv_begin(46,62,T_BG);
        cv_rect(0,0,46,1,ink);cv_rect(0,61,46,1,ink);cv_rect(0,0,1,62,ink);cv_rect(45,0,1,62,ink);
        if(flags&6)cv_rect(1,1,44,60,bg);
        cv_text_in(0,8,46,&AF_L,name,ink,bg);
        cv_text_in(0,45,46,&AF_S,flags&1?"SAVED":"EMPTY",ink,bg);
        if(flags&8) cv_rect(0,59,46,2,T_ACCENT);
        cv_blit(12+i*55,70);
    }
}
static void ko_arrange_draw(int force)
{
    char text[48];
    if(ko.view==8) {
        ko_line(29,"PATTERNS / ALL TRACKS",T_THEME);
        str_cpy(text,"SELECT ",sizeof text);uint32_t n=str_len(text);text[n]=(char)('A'+ko_pattern_sel);text[n+1]=0;
        str_cpy(text+str_len(text),ko_phrase_dirty()?" / UNSAVED":(ko_pattern_mask&(1u<<ko_pattern_sel))?" / SAVED":" / EMPTY",sizeof text-str_len(text));
        if(ko_copy_slot<4) {str_cpy(text+str_len(text)," / CLIP ",sizeof text-str_len(text));n=str_len(text);text[n]=(char)('A'+ko_copy_slot);text[n+1]=0;}
        ko_line(49,text,T_MID);
        if(ko_launch_pending<4) {str_cpy(text,chain_config.rsv[0]==1?"PATTERN END: ":"NEXT BAR: ",sizeof text);n=str_len(text);text[n]=(char)('A'+ko_launch_pending);text[n+1]=0;}
        else if(chain.running && ko_launch_mode) {str_cpy(text,"PLAYING: ",sizeof text);n=str_len(text);text[n]=(char)('A'+ko_pattern_active);text[n+1]=0;}
        else str_cpy(text,"KEYS 1-4 SELECT A-D",sizeof text);
        ko_line(140,text,T_MID);
        ko_pattern_tiles(force);
        ko_line(158,"K1 SLOT / K2 COPY FROM",T_MID);
        ko_line(176,"K3 CHAIN ADD+ / REMOVE-",T_MID);
        ko_line(194,"K4 PASTE+ / COPY-",T_MID);
        ko_line(205,"SAVE ALL / PLAY LOOP",T_MID);
        ko_line(224,"SCL ARRANGE / EDIT BACK",T_MID);
    } else {
        ko_line(29,"ARRANGEMENT / ALL TRACKS",T_THEME);
        ko_page("ARRANGEMENT", "SAVE ALL / PLAY SONG", "SCL WAVE / ENV TOOLS");
        uint32_t row=chain.running && !ko_launch_mode?chain.row:ko_section;
        ko_fx_number(0,"K1 SECTION",row+1,"",0);
        if(row<chain_config.count){
            const chain_row_t *r=chain.running && !ko_launch_mode?&chain.config.row[row]:&chain_config.row[row];
            char pattern[2]={r->slot[0]<4?(char)('A'+r->slot[0]):'-',0};
            int mixed=0;for(uint32_t k=1;k<SAMPLER_PARTS;k++)mixed|=r->slot[k]!=r->slot[0];
            ko_card(1,"K2 PATTERN",mixed?"MIXED LANES":pattern);
            ko_fx_number(2,"K3 REPEATS",r->repeat,"",0);
        }else {ko_card(1,"KEYS 1-4 APPEND","A / B / C / D");ko_card(2,"K3 REPEATS","EMPTY");}
        ko_card(3,"K4 + / -","INSERT / DEL");
    }
}
