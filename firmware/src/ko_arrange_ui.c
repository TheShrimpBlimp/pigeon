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
    ko_line(52,ko.view==10?"MIXER / LEVELS":ko.view==11?"MIXER / DELAY SENDS":ko.view==12?"MIXER / CHORUS SENDS":"MIXER / REVERB SENDS",T_THEME);
    ko_value(2,"K1 SONG 1 ",trk[0].p[id]);ko_value(3,"K2 DRUM KIT ",trk[1].p[id]);
    if(FELUCCA_KO_DUAL) ko_value(4,"K3 SONG 2 ",trk[2].p[id]);
    ko_value(5,"K4 SELECTED PAN ",TSEL->p[P_PAN]);
    ko_line(207,"ARP NEXT / EDIT BACK",T_MID);
}
static void ko_pattern_tiles(int force)
{
    static uint8_t previous[4]={255,255,255,255};
    for(uint32_t i=0;i<4;i++) {
        uint8_t flags=(uint8_t)(((ko_pattern_mask>>i)&1u) | ((ko_pattern_sel==i)<<1) |
            ((chain.running && ko_launch_mode && ko_pattern_active==i)<<2) |
            ((ko_launch_pending==i && (ui.frame&8u))<<3));
        if(!force && previous[i]==flags) continue;previous[i]=flags;
        uint16_t bg=flags&4?T_ACCENT:flags&2?T_THEME:T_SURF;
        uint16_t ink=flags&6?T_BG:flags&1?T_THEME:T_MID;
        char name[2]={(char)('A'+i),0};
        cv_begin(46,24,bg);cv_text(17,2,&AF_M,name,ink);
        if(flags&8) cv_rect(0,22,46,2,T_ACCENT);
        cv_blit(12+i*55,98);
    }
}
static void ko_arrange_draw(int force)
{
    char text[48];
    if(ko.view==8) {
        ko_line(29,"PIGEON / PATTERNS A-D",T_THEME);
        str_cpy(text,"SELECT ",sizeof text);uint32_t n=str_len(text);text[n]=(char)('A'+ko_pattern_sel);text[n+1]=0;
        str_cpy(text+str_len(text),(ko_pattern_mask&(1u<<ko_pattern_sel))?" / SAVED":" / EMPTY",sizeof text-str_len(text));
        if(ko_copy_slot<4) {str_cpy(text+str_len(text)," / CLIP ",sizeof text-str_len(text));n=str_len(text);text[n]=(char)('A'+ko_copy_slot);text[n+1]=0;}
        ko_line(52,text,T_THEME);
        if(ko_launch_pending<4) {str_cpy(text,"NEXT BAR: ",sizeof text);n=str_len(text);text[n]=(char)('A'+ko_launch_pending);text[n+1]=0;}
        else if(chain.running && ko_launch_mode) {str_cpy(text,"PLAYING: ",sizeof text);n=str_len(text);text[n]=(char)('A'+ko_pattern_active);text[n+1]=0;}
        else str_cpy(text,"KEYS 1-4 SELECT A-D",sizeof text);
        ko_line(74,text,T_MID);
        ko_pattern_tiles(force);
        ko_line(136,"K1 SLOT / K2 COPY FROM",T_MID);
        ko_line(158,"K3 CHAIN ADD+ / REMOVE-",T_MID);
        ko_line(180,"K4 PASTE+ / COPY-",T_MID);
        ko_line(202,"REC SAVE / PLAY LOOP",T_MID);
        ko_line(224,"SCL ARRANGE / EDIT BACK",T_MID);
    } else {
        ko_line(29,"PIGEON / ARRANGEMENT",T_THEME);
        ko_value(0,"SECTIONS ",chain_config.count);
        uint32_t row=chain.running && !ko_launch_mode?chain.row:ko_section;
        ko_value(1,"K1 SECTION ",row+1);
        if(row<chain_config.count) {
            const chain_row_t *r=chain.running && !ko_launch_mode?&chain.config.row[row]:&chain_config.row[row];
            str_cpy(text,"K2 PATTERN ",sizeof text);uint32_t n=str_len(text);text[n]=r->slot[0]<4?(char)('A'+r->slot[0]):'-';text[n+1]=0;
            int mixed=0;for(uint32_t k=1;k<SAMPLER_PARTS;k++) mixed|=r->slot[k]!=r->slot[0];
            if(mixed) str_cpy(text+str_len(text)," / LANES",sizeof text-str_len(text));ko_line(96,text,T_THEME);
            ko_value(3,"K3 REPEATS ",r->repeat);
        } else {ko_line(96,"KEYS 1-4 APPEND A-D",T_MID);ko_line(118,"K3 REPEATS / EMPTY",T_MID);}
        ko_line(147,"K4 INSERT+ / DELETE-",T_MID);
        ko_line(169,"KEYS 1-4 APPEND A-D",T_MID);
        ko_line(199,"REC SAVE / PLAY SONG",T_MID);
        ko_line(220,"SCL WAVE / EDIT BACK",T_MID);
    }
}
