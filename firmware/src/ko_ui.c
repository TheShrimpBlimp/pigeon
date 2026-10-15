/* SPDX-License-Identifier: GPL-3.0-only */
/* HOME is the KO editor; standard SEQ, SAVE, layers and menu remain available. */
static int ko_phrase_dirty(void);
static void ko_pattern_refresh(void);
static void ko_pattern_poll(void);
static int ko_pattern_select(uint32_t slot);
static void ko_pattern_copy(void);
static void ko_pattern_paste(void);
static void ko_pattern_save(void);
static void ko_pattern_keys(uint32_t notes);
static void ko_arrange_turn(uint32_t k,int32_t steps);
static int ko_pattern_start(int arrangement);
static void ko_pattern_preload(void);
static void ko_mixer_turn(uint32_t k,int32_t steps);
static void step_undo_take(track_t *t, uint32_t tag);
static void motion_undo_done(track_t *t);
static int ko_home(void) { return ko.on && ui.home && song.sel < SAMPLER_PARTS && !ui.menu && !ui.confirm && !name_on() && !ui.layer; }
static int ko_ready(void) { return song.sel==2 && FELUCCA_KO_DUAL ? ko_song(&trk[2]) : ko_song(&trk[0]) && ko_kit(&trk[1]); }
static void ko_setup_second(void)
{
    if (transport_busy()) { ui_message("STOP TO SET UP SONG 2"); return; }
    track_t *t=&trk[2];set_engine_of(t,13);
    t->p[P_E0]=3;t->p[P_E1]=SLC_DIV_MAN;t->p[P_E2]=t->p[P_E3]=0;
    t->p[P_E4]=SLC_ONE;t->p[P_E6]=t->p[P_E7]=127;
    t->p[P_ATK]=0;t->p[P_SUS]=127;t->p[P_REL]=30;
    t->p[P_VOICE]=V_POLY;t->p[P_AMODE]=0;t->p[P_SPRD]=0;
    t->p[P_DIST]=t->p[P_CHOR]=t->p[P_DLY]=t->p[P_REV]=0;
    if (seq_is_empty(t)) { t->p[P_SLEN]=16;t->p[P_SDIV]=2; }
    ko_prepare();ko.view=0;ui.force=1;ui_message("SONG 2 READY");
}
static void ko_setup(void)
{
    if (transport_busy()) { ui_message("STOP TO SET UP KO"); return; }
    set_engine_of(&trk[0], 13);
    trk[0].p[P_E0] = 1; trk[0].p[P_E1] = SLC_DIV_MAN;
    trk[0].p[P_E2] = trk[0].p[P_E3] = 0;
    trk[0].p[P_E4] = SLC_ONE; trk[0].p[P_E6] = trk[0].p[P_E7] = 127;
    set_engine_of(&trk[1], 4);
    trk[1].p[P_E0] = SMP_NSETS + 1; trk[1].p[P_E3] = 0; trk[1].p[P_E7] = 1;
    if (FELUCCA_KO_DUAL) {
        set_engine_of(&trk[2],13);
        trk[2].p[P_E0]=3;trk[2].p[P_E1]=SLC_DIV_MAN;
        trk[2].p[P_E2]=trk[2].p[P_E3]=0;trk[2].p[P_E4]=SLC_ONE;
        trk[2].p[P_E6]=trk[2].p[P_E7]=127;
    }
    for (uint32_t j = 0; j < SAMPLER_PARTS; j++) {
        track_t *t = &trk[j];
        t->p[P_ATK] = 0; t->p[P_SUS] = 127; t->p[P_REL] = 30;
        t->p[P_VOICE] = V_POLY; t->p[P_AMODE] = 0; t->p[P_SPRD] = 0;
        t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
        if (seq_is_empty(t)) { t->p[P_SLEN] = 16; t->p[P_SDIV] = 2; }
    }
    ko_prepare(); ko.view = 0; song.octave = 0; ui.force = 1;
    ui_message("KO SOUNDS READY");
}
/* Product slots never inherit a synth or factory sample source from a project. */
static void ko_product_tracks(void)
{
#if FELUCCA_SAMPLER_ONLY
    ko_states[0].on=1;
    for(uint32_t k=0;k<SAMPLER_PARTS;k++) {
        track_t *t=&trk[k];
        t->p[P_SCALE]=0;t->p[P_CHRD]=0;t->p[P_VOIC]=0;
        int ready=k==1 ? ko_kit(t) && t->p[P_E0]==SMP_NSETS+1 : ko_song(t);
        if(ready)continue;
        trk_all_off(t);fm1_irq_off();
        t->eng_req=k==1?4:13;t->p[P_E0]=k==1?SMP_NSETS+1:k==2?3:1;
        if(k==1) {t->p[P_E3]=0;t->p[P_E7]=1;}
        else {t->p[P_E1]=SLC_DIV_MAN;t->p[P_E2]=t->p[P_E3]=0;
            t->p[P_E4]=SLC_ONE;t->p[P_E5]=0;t->p[P_E6]=t->p[P_E7]=127;}
        t->p[P_VOICE]=V_POLY;t->p[P_AMODE]=0;t->p[P_SPRD]=0;
        fm1_irq_on();
    }
#endif
}
static void ko_product_boot(void)
{
#if FELUCCA_SAMPLER_ONLY
    pigeon_restore_tracks=ko_product_tracks;
    song.sel=0;ko_context=~0u;ko_states[0].on=1;
    ko_product_tracks();ko_prepare();
    for(uint32_t k=0;k<1u+FELUCCA_KO_DUAL;k++)ko_states[k].view=0;
    ui.home=1;ui.page=0;ui.force=1;
#endif
}
static int32_t accel_speed(uint32_t role, int32_t s, int32_t range);
static void ko_performance_turn(uint32_t knob,int32_t steps);
static uint8_t ko_copy_chop,ko_variation_slot;
static uint32_t ko_manual_ms, ko_follow_a = ~0u;
static uint32_t ko_tap_last,ko_tap_intervals[4],ko_tap_count;
static uint8_t ko_loop_beats=4;
static void ko_tap_tempo(void) {
    uint32_t now=fm1_ms,delta=now-ko_tap_last;
    if(!ko_tap_last || delta>2000u) {ko_tap_count=0;ko_tap_last=now?now:1;ui_message("TAP AGAIN");return;}
    if(delta<250u)return;
    ko_tap_last=now;
    if(delta>1500u){ko_tap_count=0;ui_message("TAP 40-240 BPM");return;}
    ko_tap_intervals[ko_tap_count++&3u]=delta;uint32_t n=ko_tap_count<4?ko_tap_count:4,sum=0;
    for(uint32_t i=0;i<n;i++)sum+=ko_tap_intervals[i];
    song.g[G_BPM]=(int16_t)clamp((int32_t)((60000u*n+sum/2)/sum),40,240);ui.force=1;
}
static uint32_t ko_loop_bpm(void) {
    const slc_src_t *src=slc_get(ko_src());const ko_table_t *tab=ko_table();
    if(!src || !tab->n)return 0;const ko_chop_t *c=&tab->chop[ko.selected];
    uint32_t rate=(src->rate*44100u+32768u)>>16,len=c->b-c->a;return (60u*ko_loop_beats*rate+len/2u)/len;
}
static void ko_knob(uint32_t k, int32_t s)
{
    if (!ko_ready()) return;
    if (ko.view==0 && song.sel!=1) { ko_manual_ms=fm1_ms; ko_follow_a=~0u; }
    track_t *t = TSEL;
    /* KO always accelerates continuous controls, independently of the
     * standard UI preference. Actions and small discrete choices stay fine. */
    int32_t range = 0;
    if (ko.view == 0 && song.sel!=1) range = k == 3 ? 64 : 127;
    else if (ko.view == 0) {
        static const uint8_t ids[4] = {P_LEVEL, P_E1, P_E2, P_E4};
        const param_desc_t *pd = track_desc(t, ids[k]);
        range = pd->fmt == F_ENUM ? 0 : pd->max - pd->min;
    } else if (ko.view >=10 && ko.view<=13) range=127;
    else if (ko.view <= 2) range = 127;
    else if (ko.view==7 && !k) range=GP[G_BPM].max-GP[G_BPM].min;
    else if (ko.view == 3) {
        static const uint8_t ranges[4] = {48, 7, 99, 49};
        range = ranges[k];
    }
    int32_t fast = accel_speed(EN_K1 + k, s, range);
    /* Zoom has just seven levels: cap its accelerated movement at x2. */
    if (ko.view == 0 && song.sel!=1 && k == 3)
        fast = clamp(fast, s < 0 ? s * 2 : s, s < 0 ? s : s * 2);
    s = fast;
    if (ko.view==8 || ko.view==9 || ko.view==18) ko_arrange_turn(k,s);
    else if(ko.view>=10 && ko.view<=13) ko_mixer_turn(k,s);
    else if(ko.view==23) {
        if(!k)song.g[G_BPM]=(int16_t)param_turn(&GP[G_BPM],song.g[G_BPM],s);
        else if(k==1)ko_loop_beats=(uint8_t)clamp(ko_loop_beats+s,1,16);
        else if(k==2 && song.sel!=1) {uint32_t bpm=ko_loop_bpm();if(bpm>=40 && bpm<=240)song.g[G_BPM]=(int16_t)bpm;else ui_message("CHANGE LOOP BEAT COUNT");}
        else if(k==3 && song.sel!=1)ko_select((uint32_t)clamp((int32_t)ko.selected+s,0,(int32_t)ko_table()->n-1));
    }
    else if(ko.view==27 && song.sel!=1) {
        ko_sync_t *q=&ko_sync[ko_id()];
        if(!k)q->beats=(uint8_t)clamp((int32_t)q->beats+s,1,64);
        else if(k==1)q->on=(uint8_t)(s>0);
        else if(k==2)q->whole=(uint8_t)(s>0);
        else if(k==3)ko_select((uint32_t)clamp((int32_t)ko.selected+s,0,(int32_t)ko_table()->n-1));
        const slc_src_t *src=slc_get(ko_src());const ko_chop_t *c=&ko_table()->chop[ko.selected];
        q->samples=q->whole && src?src->len:c->b-c->a;
        ko.dirty=1;ko.revision++;
    }
    else if(ko.view==24) {
        if(transport_busy())ui_message("STOP TO EDIT STEP");
        else {uint32_t step=ko_seq[song.sel].cursor;step_t *n=&t->step[step];
            if(k==3)ko_seq[song.sel].cursor=(uint8_t)clamp((int32_t)step+s,0,t->p[P_SLEN]-1);
            else {step_undo_take(t,0x7000u|k<<6|step);
                if(!k) {int16_t pitch=0;(void)motion_lock_get(t,step,P_TRANS,&pitch);if(motion_set_lock(t,step,P_TRANS,(int16_t)clamp(pitch+s,-24,24))==2)ui_message("LOCK STORAGE FULL");}
                else if(k==1){step_set_nudge(n,step_nudge(n)+s);t->p[P_SQNT]=0;}
                else step_set_condition(n,(uint32_t)clamp((int32_t)step_condition(n)+s,0,4));
                motion_undo_done(t);}
        }
    }
    else if(ko.view==25 && song.sel!=1) {
        if(!k)ko_choke_group_turn(s);
        else if(k==2)t->p[P_LEVEL]=(int16_t)clamp(t->p[P_LEVEL]+s,0,127);
        else if(transport_busy() || voices_busy())ui_message("STOP / RELEASE TO EDIT");
        else if(k==1) {if(ko_layout[ko_id()]!=(s>0)) (void)ko_layout_switch();}
        else if(s>0) {ko_alternate[ko_id()]=*ko_table();ko.dirty=1;ko.revision++;ui_message("LAYOUT COPIED");}
    }
    else if(ko.view==26) {if(!k)ko_roll_div=(uint8_t)clamp((int32_t)ko_roll_div+s,0,3);else if(k==1)ko_roll_enabled=(uint8_t)(s>0);ko_states[0].dirty=1;ko_states[0].revision++;}
    else if(ko.view==19)ko_envelope_turn(k,s);
    else if(ko.view==28) {
        if(!k)ko_select((uint32_t)clamp((int32_t)ko.selected+s,0,(int32_t)ko_table()->n-1));
        else if(k==1)ko_copy_chop=(uint8_t)clamp((int32_t)ko_copy_chop+s,0,(int32_t)ko_table()->n-1);
        else if(transport_busy() || voices_busy())ui_message("STOP / RELEASE TO EDIT");
        else if(s>0)ui_message(ko_reassign(ko_copy_chop,k==3)?"KEYS REASSIGNED":"CANNOT REASSIGN");
    }
    else if(ko.view==20) {
        if(!k)ko_select((uint32_t)clamp((int32_t)ko.selected+s,0,(int32_t)ko_table()->n-1));
        else if(k==1)ko_copy_chop=(uint8_t)clamp((int32_t)ko_copy_chop+s,0,(int32_t)ko_table()->n-1);
        else if(k==2)ui_message((s>0?ko_duplicate():ko_split())?"CHOP CREATED":"CANNOT CREATE CHOP");
        else ui_message((s>0?ko_copy_settings(ko_copy_chop):ko_join())?"CHOP UPDATED":"CANNOT UPDATE CHOP");
    }
    else if(ko.view==21) {
        if(k<2)rp_put(k?RP_COUNTIN:RP_CLICK,(uint32_t)clamp((int32_t)rp_get(k?RP_COUNTIN:RP_CLICK)+s,0,2));
        else {if(k==2)ko_quantize[song.sel]=(uint8_t)clamp((int32_t)ko_quantize[song.sel]+s,0,2);
            else if(song.sel!=1)ko_choke[ko_id()]=(uint8_t)(s>0);
            ko.dirty=1;ko.revision++;}
    }
    else if(ko.view==22) {
        if(transport_busy() || voices_busy())ui_message("STOP / RELEASE KEYS");
        else if(!k)ko_variation_slot=(uint8_t)clamp((int32_t)ko_variation_slot+s,0,3);
        else if(k<3) {step_undo_take(t,0x6800u|k);
            for(uint32_t step=0;step<NSTEP;step++) {if(k==1)step_clear(&t->step[step]);else (void)motion_clear_locks(t,step);}
            motion_undo_done(t);ui_message(k==1?"TRACK NOTES CLEARED":"TRACK LOCKS CLEARED");}
        else if(s>0) {ko_pattern_copy();ko_pattern_sel=ko_variation_slot;ko_pattern_paste();}
    }
    else if(ko.view==16) {
        ko_pad_params_t *pad=&ko_kit_params[ko_kit_pad];
        if(!k)pad->pitch=(int8_t)clamp(pad->pitch+s,-24,24);
        else if(k==1)pad->level=(uint8_t)clamp(pad->level+s,0,127);
        else if(k==2)pad->decay=(uint8_t)clamp(pad->decay+s,0,127);
        else pad->choke=(uint8_t)clamp(pad->choke+s,0,4);
        ko_states[0].dirty=1;ko_states[0].revision++;
    }
    else if (ko.view==14) {
        if(transport_busy()) ui_message("STOP TO EDIT STEP");
        else { uint32_t step=ko_seq[song.sel].cursor;step_t *n=&t->step[step];
            step_undo_take(t,0x6000u | k<<6 | step);
            if(!k) n->vel=(uint8_t)clamp(n->vel+s,1,127);
            else if(k==1) {int16_t gate=t->p[P_SGATE];(void)motion_lock_get(t,step,P_SGATE,&gate);
                if(motion_set_lock(t,step,P_SGATE,(int16_t)clamp(gate+s,1,127))==2) ui_message("LOCK STORAGE FULL");}
            else if(k==2) step_set_chance(n,(uint32_t)clamp((int32_t)step_chance(n)+s,0,100));
            else step_set_ratchet(n,(uint32_t)clamp((int32_t)step_ratchet(n)+s,1,4));
            motion_undo_done(t); }
    } else if(ko.view==15) {
        if(k<2)step_undo_take(t,0x6600u|k);
        if(!k) t->p[P_SSWING]=(int16_t)clamp(t->p[P_SSWING]+s,0,100);
        else if(k==1) t->p[P_SGATE]=(int16_t)clamp(t->p[P_SGATE]+s,1,127);
        else if(k==2) song.g[G_BPM]=(int16_t)param_turn(&GP[G_BPM],song.g[G_BPM],s);
        if(k<2)motion_undo_done(t);
    }
    else if (ko.view == 7) {
        if (!k) song.g[G_BPM]=(int16_t)param_turn(&GP[G_BPM],song.g[G_BPM],s);
        else if (k==1) {
            if (transport_busy()) ui_message("STOP TO CHANGE TIMING");
            else { ko_seq[song.sel].end_mode=(uint8_t)(s>0);ko.dirty=1;ko.revision++; memset(ko_seq[song.sel].age,0,sizeof ko_seq[song.sel].age); }
        } else if (k==2 && !transport_busy()) ko_seq[song.sel].cursor=(uint8_t)clamp((int32_t)ko_seq[song.sel].cursor+s,0,t->p[P_SLEN]-1);
        else if (k==3) {
            if (transport_busy()) ui_message("STOP TO CHANGE LENGTH");
            else { step_undo_take(t,0x6500u);t->p[P_SLEN]=(int16_t)clamp(t->p[P_SLEN]+s,1,NSTEP);
                if(ko_seq[song.sel].cursor>=t->p[P_SLEN]) ko_seq[song.sel].cursor=(uint8_t)(t->p[P_SLEN]-1);motion_undo_done(t); }
        }
    } else if (ko.view==6) ko_performance_turn(k,s);
    else if (ko.view == 0) {
        if (song.sel!=1) ko_chop_turn(k, s);
        else {
            static const uint8_t ids[4] = {P_LEVEL, P_E1, P_E2, P_E4};
            uint32_t id = ids[k];
            const param_desc_t *pd = track_desc(t, id);
            t->p[id] = (int16_t)param_turn(pd, t->p[id], s);
            motion_capture(t, id, t->p[id]);
        }
    } else if (ko.view <= 2) {
        static const uint8_t ids[2][4] = {{P_E7, P_DLY, P_CHOR, P_REV}, {P_DIST, G_DTIME, G_DFDBK, P_LEVEL}};
        uint32_t id = ids[ko.view - 1u][k];
        int global = ko.view == 2 && (k == 1 || k == 2);
        if (ko.view == 1 && !k && song.sel==1) id = P_E4;
        const param_desc_t *pd = global ? &GP[id] : track_desc(t, id);
        int16_t *p = global ? &song.g[id] : &t->p[id];
        *p = (int16_t)param_turn(pd, *p, s);
        if (!global && !mom.n) motion_capture(t, id, *p);
    } else if (ko.view == 5 && song.sel!=1) {
        if (!k) ko_flag_turn(KO_NO_LOOP, s);
        else if (k == 1) ko_flag_turn(KO_LATCH, s);
        else if (k==3 && s) ui_message(ko_undo() ? "CHOP UNDO / REDO" : "NO CHOP EDIT TO UNDO");
        else if (k == 2 && s) {
            int ok = s > 0 ? ko_split() : ko_join();
            ui_message(ok ? "CHOPS UPDATED" : "CANNOT EDIT CHOP");
        }
    } else {
        ko_comp_t c = ko.comp[song.sel==1];
        if (ko.view == 3) {
            if (!k) c.threshold = (uint8_t)clamp(c.threshold + s, 0, 48);
            if (k == 1) c.ratio = (uint8_t)clamp(c.ratio + s, 1, 8);
            if (k == 2) c.attack = (uint8_t)clamp(c.attack + s, 1, 100);
            if (k == 3) c.release = (uint8_t)clamp(c.release + s, 1, 50);
        } else if (!k) c.makeup = (uint8_t)clamp(c.makeup + s, 0, 12);
        ko.comp[song.sel==1] = c; ko.dirty = 1; ko.revision++;
    }
    ui.force = 1;
}
static struct { uint8_t y; uint16_t ink; char text[48]; } ko_lines[20];
static uint8_t ko_line_n, ko_ui_valid, ko_fx_valid;
static struct {char text[64];int32_t amount;} ko_fx_cache[4];
static void ko_ui_invalidate(void) { ko_ui_valid=0; ko_line_n=0;ko_fx_valid=0; ko_follow_a=~0u; }
static void ko_line(uint32_t y, const char *s, uint16_t ink)
{
    uint32_t i=0;
    while(i<ko_line_n && ko_lines[i].y!=y) i++;
    if(i<ko_line_n && ko_lines[i].ink==ink && str_len(s)<sizeof ko_lines[i].text && !memcmp(ko_lines[i].text,s,str_len(s)+1u)) return;
    if(i<20) { if(i==ko_line_n) ko_line_n++; ko_lines[i].y=(uint8_t)y;
        ko_lines[i].ink=ink; str_cpy(ko_lines[i].text,s,sizeof ko_lines[i].text); }
    draw_text_line(8, y, 224, &AF_S, s, ink, T_BG, 0);
}
static void ko_value(uint32_t row, const char *label, int32_t value)
{
    char b[48], v[16]; str_cpy(b, label, sizeof b); fmt_int(v, value);
    str_cpy(b + str_len(b), v, sizeof b - str_len(b)); ko_line(52 + row * 22, b, T_THEME);
}
/* FX values redraw one geometry panel at a time; no bitmap controls. */
static void ko_fx_panel(uint32_t slot,uint32_t width,const char *label,const char *value,const char *unit,int32_t amount,uint32_t maximum)
{
    char signature[64];str_cpy(signature,label,sizeof signature);
    str_cpy(signature+str_len(signature),value,sizeof signature-str_len(signature));
    str_cpy(signature+str_len(signature),unit,sizeof signature-str_len(signature));
    if((ko_fx_valid&(1u<<slot)) && ko_fx_cache[slot].amount==amount && str_eq(ko_fx_cache[slot].text,signature))return;
    ko_fx_valid|=1u<<slot;ko_fx_cache[slot].amount=amount;str_cpy(ko_fx_cache[slot].text,signature,sizeof ko_fx_cache[slot].text);
    uint32_t x=8u+(slot&1u)*116u,y=70u+(slot/2u)*66u;
    cv_begin(width,62,T_BG);
    cv_rect(0,0,width,1,T_MID);cv_rect(0,61,width,1,T_MID);
    cv_rect(0,0,1,62,T_MID);cv_rect(width-1,0,1,62,T_MID);
    cv_text(7,5,&AF_S,label,T_MID);
    int32_t uw=text_w(&AF_S,unit);int large=text_w(&AF_L,value)<=(int32_t)width-20-uw;
    for(const char *ch=value;*ch;ch++)if(glyph_at(&AF_L,(uint8_t)*ch)<0)large=0;
    const aafont_t *font=large?&AF_L:text_w(&AF_M,value)<=(int32_t)width-14?&AF_M:&AF_S;
    cv_text(7,font==&AF_L?20:font==&AF_M?28:32,font,value,T_THEME);
    if(uw)cv_text((int32_t)width-7-uw,38,&AF_S,unit,T_MID);
    if(maximum)cv_rect(7,56,width-14,2,T_MID);
    uint32_t fill=maximum?(uint32_t)clamp(amount,0,maximum)*(width-14)/maximum:0;
    if(fill)cv_rect(7,56,fill,2,T_THEME);
    cv_blit(x,y);
}
static void ko_fx_number(uint32_t slot,const char *label,int32_t value,const char *unit,uint32_t maximum)
{
    char text[16];fmt_int(text,value);ko_fx_panel(slot,108,label,text,unit,value<0?-value:value,maximum);
}
static void ko_card(uint32_t slot,const char *label,const char *value)
{ko_fx_panel(slot,108,label,value,"",0,0);}
static void ko_page(const char *title,const char *hint,const char *navigation)
{
    ko_line(49,title,T_MID);
    if(hint[0])ko_line(205,hint,T_MID);
    ko_line(224,navigation,T_MID);
}
/* Performance shortcuts live on ARP's page now that black keys play chops. */
static void ko_performance_turn(uint32_t knob,int32_t steps)
{
    if(knob<SAMPLER_PARTS) trk[knob].p[P_MUTE]=(int16_t)(steps>0);
    else if(knob==3) ko_solo=(int8_t)clamp((int32_t)ko_solo+steps,-1,SAMPLER_PARTS-1);
}
static void ko_sample_title(void)
{
    char title[40], name[9]; const char *label = song.sel==1 ? "KIT / " : song.sel==2 ? "S2 / " : "S1 / ";
    const char *n = 0;
#if FELUCCA_KO_STORAGE
    if (ko_bank_active && song.sel!=1) n = ko_id() ? ko_bank_second.name : ko_bank.name;
#endif
    uint32_t slot = song.sel==1 ? 1 : ko_id()?2:0;
    if (!n && usr_nz[slot]
#if FELUCCA_KO_STORAGE
        && !ko_bank_active
#endif
    ) n = ((const smp_user_hdr_t *)smp_user_xip(slot))->name;
    memset(name,0,sizeof name); if (n) memcpy(name,n,8);
    for (uint32_t i=0;i<8;i++) if (name[i] && ((uint8_t)name[i]<32 || (uint8_t)name[i]>126)) name[i]='?';
    str_cpy(title,label,sizeof title); str_cpy(title+str_len(title),name[0]?name: song.sel==1?"DRUM KIT":"EMPTY",sizeof title-str_len(title));
    if ((song.sel!=1 && ko_table()->n) || ko.view==16 || ko.view==14 || ko.view==24) {
        char number[12];fmt_int(number,ko.view==14||ko.view==24?ko_seq[song.sel].cursor+1:ko.view==16?ko_kit_pad+1:ko.selected+1);
        str_cpy(title+str_len(title),ko.view==14||ko.view==24?" / STEP ":ko.view==16?" / PAD ":" / C",sizeof title-str_len(title));
        str_cpy(title+str_len(title),number,sizeof title-str_len(title));
    }
    if (ko_solo==(int32_t)song.sel) str_cpy(title+str_len(title)," / SOLO",sizeof title-str_len(title));
    else if (ko_muted(TSEL)) str_cpy(title+str_len(title)," / MUTE",sizeof title-str_len(title));
    ko_line(29,title,T_THEME);
}
#include "ko_waveform.c"
/* In-window playback updates only a three-pixel strip; following pages the waveform. */
static uint32_t ko_wave_a, ko_wave_span;
static int32_t ko_play_x = -1;
static const voice_t *ko_latest_voice(void)
{
    const voice_t *latest=0;
    for(uint32_t i=0;i<NVOICE;i++) { const voice_t *v=&TSEL->v[i];
        if(v->active && v->s[6]==1 && (!latest || (int32_t)(v->age-latest->age)>0)) latest=v; }
    return latest;
}
/* Reuse the existing canvas. Each changed column lands with its final colours,
 * without a background clear before the waveform or markers. */
static int16_t ko_marks[3]={-1,-1,-1};
static void ko_wave_draw(uint32_t a,uint32_t span,const ko_chop_t *c,int force)
{
    int moved=force || a!=ko_wave_a || span!=ko_wave_span;
    uint32_t points[3]={c->a,c->b,ko.cursor};int16_t marks[3];
    for(uint32_t i=0;i<3;i++) marks[i]=points[i]>=a && points[i]-a<=span ?
        (int16_t)((points[i]-a)*(KO_WAVE_COLS-1)/span) : -1;
    ko_wave_peaks(a,a+span);
    for(uint32_t x=0;x<KO_WAVE_COLS;) {
        int dirty=moved;
        for(uint32_t i=0;i<3;i++) if(marks[i]!=ko_marks[i] && (marks[i]==(int32_t)x || ko_marks[i]==(int32_t)x)) dirty=1;
        if(!dirty) {x++;continue;}
        uint32_t width=moved?KO_WAVE_COLS:1;
        cv_begin(width,69,T_BG);
        for(uint32_t q=0;q<width;q++) {
            int32_t hi=132-ko_wave_cache.hi[x+q]/2,lo=132-ko_wave_cache.lo[x+q]/2;
            cv_rect(q,hi-98,1,lo-hi+1,T_MID);
            for(uint32_t i=0;i<3;i++) if(marks[i]==(int32_t)(x+q)) cv_rect(q,0,1,69,i==2?T_ACCENT:T_THEME);
        }
        cv_blit(12+x,98);x+=width;
    }
    memcpy(ko_marks,marks,sizeof marks);ko_wave_a=a;ko_wave_span=span;
}
static void ko_playhead(void)
{
    int32_t x=-1;
    if (ko_ready() && ko.view==0 && song.sel!=1 && ko_wave_span) {
        const voice_t *latest=ko_latest_voice();
        uint32_t pos=latest ? latest->ph[0] : 0;
        if (latest && pos>=ko_wave_a && pos-ko_wave_a<ko_wave_span)
            x=12+(pos-ko_wave_a)*(KO_WAVE_COLS-1)/ko_wave_span;
    }
    if (x==ko_play_x) return;
    if (ko_play_x>=0) for(int32_t q=0;q<2;q++)
        if(ko_play_x+q!=x && ko_play_x+q!=x+1) lcd_fill((uint32_t)(ko_play_x+q),170,1,3,T_BG);
    if (x>=0) lcd_fill((uint32_t)x,170,2,3,T_ACCENT);
    ko_play_x=x;
}
static void ko_sync_speed_draw(void)
{
    ko_sync_t *q=&ko_sync[ko_id()];const slc_src_t *source=slc_get(ko_src());
    if(source){char text[48]="SPEED ",value[16];
        fmt_int(value,q->on?(ko_sync_step(65536,(source->rate*44100u+32768u)>>16,q->samples,q->beats,song.g[G_BPM])*100u)>>16:100);
        str_cpy(text+str_len(text),value,sizeof text-str_len(text));str_cpy(text+str_len(text),"% / PITCH FOLLOWS",sizeof text-str_len(text));ko_line(205,text,T_MID);}
}
static const char *ko_seq_status(void)
{
    if(seq_counting()) return "COUNT-IN / OCT STEP SIZE";
    if(song.rec & (1u<<song.sel)) return song.playing ? "RECORDING / OCT STEP SIZE" : "REC ARMED / OCT STEP SIZE";
    return song.playing ? "PLAYING / OCT STEP SIZE" : "STOPPED / REC TO ARM";
}
static void ko_seq_draw(int force)
{
    ko_line(199,ko_seq_status(),T_THEME);
    static uint32_t state[16];
    uint32_t current=song.playing ? TSEL->seq_idx : ko_seq[song.sel].cursor;
    uint32_t base=(current/16u)*16u, length=TSEL->p[P_SLEN];
    char status[48], value[12];fmt_int(value,length);
    str_cpy(status,"K4 LENGTH ",sizeof status);str_cpy(status+str_len(status),value,sizeof status-str_len(status));
    fmt_int(value,base/16u+1u);str_cpy(status+str_len(status)," / PAGE ",sizeof status-str_len(status));
    str_cpy(status+str_len(status),value,sizeof status-str_len(status));ko_line(93,status,T_THEME);
    for(uint32_t i=0;i<16;i++) {
        uint32_t step=base+i;const step_t *note=&TSEL->step[step];
        int filled=note->time==ST_NOTE && (note->n||note->hit), active=step==current, enabled=step<length;
        uint32_t sig=step | ((uint32_t)filled<<6) | ((uint32_t)active<<7) | ((uint32_t)enabled<<8) |
            ((uint32_t)song.sel<<9) | ((uint32_t)song.rec<<12) | ((uint32_t)song.playing<<20) | ((uint32_t)note->note[0]<<21);
        if(!force && state[i]==sig+1u) continue;
        state[i]=sig+1u;
        uint32_t x=12+(i&7u)*27,y=115+(i>>3)*30;
        uint16_t ink=!enabled?T_MID:active?T_ACCENT:filled?T_THEME:T_MID;
        char number[12];fmt_int(number,step+1u);
        cv_begin(24,28,T_BG);
        cv_rect(0,0,24,1,ink);cv_rect(0,27,24,1,ink);cv_rect(0,0,1,28,ink);cv_rect(23,0,1,28,ink);
        if(active && enabled) cv_rect(1,1,22,26,T_ACCENT);
        cv_text(0,3,&AF_S,number,active&&enabled?T_BG:ink);
        if(enabled && filled) {char id[12];fmt_int(id,note->n ? note->note[0]-59u : 0);
            cv_text(0,14,&AF_S,id,active?T_BG:T_THEME);}
        else if(enabled) cv_rect(9,23,6,2,T_BG);
        else cv_rect(5,23,14,1,T_MID);
        cv_blit(x,y);
    }
}
#include "ko_arrange_ui.c"
static void ko_draw(void)
{
    static uint32_t revision = ~0u, generation = ~0u;
    static uint8_t part = 255, view = 255, ready=255;
    static uint16_t bg, theme;
    draw_head();
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg); ui.msg2[0] = 0; ui.msg_t = 60;
    }
    if (ui.hot_t) ui.hot_t--;
    if (ui.bpm_t) ui.bpm_t--;
    int layout=!ko_ui_valid || part!=song.sel || view!=ko.view || generation!=smp_user_gen || ready!=ko_ready() || bg!=T_BG || theme!=T_THEME;
    if(revision!=ko.revision) ko_follow_a=~0u;
    if(!layout && ko.view==0 && song.sel!=1 && ko_wave_span && (uint32_t)(fm1_ms-ko_manual_ms)>=1000u) {
        const voice_t *v=ko_latest_voice(); const slc_src_t *src=slc_get(ko_src());
        uint32_t pos=v ? v->ph[0] : 0;
        if(v && src && pos<src->len && (pos<ko_wave_a || pos-ko_wave_a>=ko_wave_span)) {
            ko_follow_a=pos>ko_wave_span/4 ? pos-ko_wave_span/4 : 0;
            if(ko_follow_a>src->len-ko_wave_span) ko_follow_a=src->len-ko_wave_span;
            ui.force=1;
        }
    }
    if(ko.view==8 || ko.view==9) ui.force=1; /* runtime queued/playing slot and section */
    if (!layout && !ui.force && revision == ko.revision && generation == smp_user_gen && part == song.sel && view == ko.view) { ko_playhead(); if(ko.view==7) ko_seq_draw(0); if(ko.view==27) ko_sync_speed_draw(); return; }
    if(layout) {
        ko_line_n=0;ko_fx_valid=0;ko_ui_valid=1;ko_play_x=-1;ko_wave_span=0;ko_follow_a=~0u;
        lcd_fill(0,26,240,214,T_BG);
    }
    if (!ko_ready()) {
        ko_line(65, (song.sel==2 ? "OCT+ SET UP SONG 2" : "OCT+ SET UP SAMPLER"), T_THEME);
        ko_line(92, "PATTERNS ARE KEPT", T_MID);
        ko_line(119, "USR1 SONG / USR2 KIT", T_MID);
    } else if (ko.view == 0 && song.sel!=1) {
        const slc_src_t *s = slc_get(ko_src());
        const ko_table_t *tab = ko_table();
        if (!s || !tab->n) {
            ko_page("SAMPLE / WAVEFORM", "ADD YOUR AUDIO IN THE LOADER", "FX EFFECTS / SCL PATTERNS");
            ko_fx_panel(0,224,song.sel==2?"SONG 2":"SONG 1","EMPTY","",0,0);
            ko_line(146,"LOAD A BANK OR ADD A SONG",T_MID);
        }
        else {
            const ko_chop_t *c = &tab->chop[ko.selected];
            uint32_t span = ko_wave_span_for(s);
            uint32_t a = ko.cursor > span / 2 ? ko.cursor - span / 2 : 0;
            if (a > s->len - span) a = s->len - span;
            if(ko_follow_a!=~0u) a=ko_follow_a;
            ko_line(52, "K1 START / K2 END", T_MID);
            ko_line(74, "K3 SCROLL / K4 ZOOM", T_MID);
            ko_wave_draw(a,span,c,layout);
            ko_value(6, "OCT PITCH semis ", c->pitch);

        }
        if(s && tab->n){ko_line(205,"EDIT CHOPS / FX EFFECTS",T_MID);ko_line(224,"OCT PITCH / SCL PATTERNS",T_MID);}
    } else if (ko.view == 0) {
        ko_page("DRUM KIT", "16 PADS / ONE-SHOT SAMPLES", "ENV PAD / FX EFFECTS");
        ko_fx_number(0,"K1 LEVEL",TSEL->p[P_LEVEL],"",127);
        ko_fx_number(1,"K2 PITCH",TSEL->p[P_E1],"",127);
        ko_fx_number(2,"K3 CRUSH",TSEL->p[P_E2],"",127);
        ko_fx_number(3,"K4 TONE",TSEL->p[P_E4],"",127);
    } else if (ko.view==7) {
        ko_line(52,ko_seq[song.sel].end_mode?"K2 TIMING: SAMPLE END":"K2 TIMING: TEMPO",T_THEME);
        char timing[48], bpm[12];fmt_int(bpm,song.g[G_BPM]);
        str_cpy(timing,"K1 BPM ",sizeof timing);str_cpy(timing+str_len(timing),bpm,sizeof timing-str_len(timing));
        str_cpy(timing+str_len(timing)," / STEP ",sizeof timing-str_len(timing));
        str_cpy(timing+str_len(timing),N_DIV[TSEL->p[P_SDIV]],sizeof timing-str_len(timing));
        ko_line(74,timing,T_THEME);
        ko_seq_draw(layout);
        ko_line(178,"K3 STEP / EDIT CLEAR+NEXT",T_MID);
        ko_line(220,"ENV DETAIL / LFO SWING",T_MID);
    } else if(ko.view==23) {
        ko_page("TEMPO / LOOP", "EDIT TAP TEMPO", "ENV BACK / FX SYNC");
        ko_fx_number(0,"K1 TEMPO",song.g[G_BPM],"BPM",0);
        ko_fx_number(1,"K2 LOOP BEATS",ko_loop_beats,"",0);
        ko_fx_number(2,"K3 APPLY BPM",song.sel==1?0:ko_loop_bpm(),"BPM",0);
        ko_fx_number(3,"K4 CHOP",ko.selected+1,"",0);
    } else if(ko.view==27) {
        ko_sync_t *q=&ko_sync[ko_id()];
        ko_page("SPEED SYNC", "", "FX BACK / EDIT WAVE");
        ko_fx_number(0,"K1 LOOP BEATS",q->beats,"",0);
        ko_card(1,"K2 SYNC",q->on?"ON":"OFF");
        ko_card(2,"K3 REFERENCE",q->whole?"WHOLE SONG":"CHOP");
        ko_fx_number(3,"K4 CHOP",ko.selected+1,"",0);
    } else if(ko.view==24) {
        uint32_t step=ko_seq[song.sel].cursor;step_t *n=&TSEL->step[step];int16_t pitch=0;(void)motion_lock_get(TSEL,step,P_TRANS,&pitch);
        static const char *const condition[]={"ALWAYS","EVERY 2ND","EVERY 4TH","FIRST PASS","AFTER FIRST"};
        ko_page("STEP / PITCH + TIMING", "PRESETS SELECT STEP", "ENV BACK / FX SYNC");
        ko_fx_number(0,"K1 PITCH",pitch,"st",0);ko_fx_number(1,"K2 NUDGE",step_nudge(n),"/16",0);
        ko_card(2,"K3 CONDITION",condition[step_condition(n)]);ko_fx_number(3,"K4 STEP",step+1,"",0);
    } else if(ko.view==25) {
        const ko_table_t *tab=ko_table();
        ko_page("CHOKE / LAYOUT", "GROUP 0 ALLOWS OVERLAP", "FX BACK / EDIT WAVE");
        ko_fx_number(0,"K1 CHOKE GROUP",tab->n?tab->chop[ko.selected].rsv>>KO_CHOKE_SHIFT:0,"",0);
        ko_card(1,"K2 LAYOUT",ko_layout[ko_id()]?"B":"A");ko_fx_number(2,"K3 SONG GAIN",TSEL->p[P_LEVEL],"",127);
        ko_card(3,"K4 COPY LAYOUT","COPY");
    } else if(ko.view==26) {
        ko_page("DRUM ROLLS", "HOLD PADS TO REPEAT", "LFO BACK / EDIT HOME");
        ko_card(0,"K1 RATE",N_DIV[ko_roll_div]);ko_card(1,"K2 ROLL",ko_roll_enabled?"ON":"OFF");
    } else if(ko.view==19) {
        const ko_table_t *tab=ko_table();
        ko_page("CHOP / ENVELOPE", "RELEASE 0: TRACK DEFAULT", "ENV BACK / EDIT WAVE");
        if(tab->n){const ko_chop_t *c=&tab->chop[ko.selected];
            ko_fx_number(0,"K1 ATTACK",((c->rsv>>KO_ATTACK_SHIFT)&31u)*5u,"ms",155);
            ko_fx_number(1,"K2 RELEASE",((c->rsv>>KO_RELEASE_SHIFT)&31u)*10u,"ms",310);
            ko_card(2,"K3 TRIGGER",c->rsv&KO_LATCH?"LATCH":(c->rsv&KO_ONE)||((c->rsv&KO_NO_LOOP)&&!(c->rsv&KO_GATE))?"ONE-SHOT":"HOLD");
            ko_card(3,"K4 LOOP",c->rsv&KO_NO_LOOP?"OFF":"ON");}
    } else if(ko.view==28) {
        ko_page("CHOP / MOVE + SWAP", "MOVE SHIFTS KEYS BETWEEN", "ENV BACK / EDIT WAVE");
        ko_fx_number(0,"K1 SOURCE",ko.selected+1,"",0);ko_fx_number(1,"K2 DESTINATION",ko_copy_chop+1,"",0);
        ko_card(2,"K3 TURN RIGHT","SWAP");ko_card(3,"K4 TURN RIGHT","MOVE");
    } else if(ko.view==20) {
        ko_page("CHOP / COPY + SPLIT", "REC COPY WHOLE CHOP", "ENV MOVE / FX GROUPS");
        ko_fx_number(0,"K1 TARGET",ko.selected+1,"",0);ko_fx_number(1,"K2 COPY FROM",ko_copy_chop+1,"",0);
        ko_card(2,"K3 + / -","DUPE / SPLIT");ko_card(3,"K4 + / -","SET / MERGE");
    } else if(ko.view==21) {
        static const char *const click[]={"OFF","REC","ON"},*const count[]={"OFF","1 BAR","2 BARS"},*const quant[]={"OFF","1/16","1/8"};
        ko_page("RECORDING", "QUANTIZE DURING LIVE RECORD", "ENV TAP / LFO TIMING");
        ko_card(0,"K1 METRONOME",click[rp_get(RP_CLICK)]);ko_card(1,"K2 COUNT-IN",count[rp_get(RP_COUNTIN)]);
        ko_card(2,"K3 QUANTIZE",quant[ko_quantize[song.sel]]);
        ko_card(3,song.sel==1?"PAD CHOKES":"K4 SONG CHOKE",song.sel==1?"ENV ON KIT":ko_choke[ko_id()]?"ON":"OFF");
    } else if(ko.view==22) {
        char slot[2]={(char)('A'+ko_variation_slot),0};
        ko_page("PATTERN / VARIATIONS", "STOP + RELEASE PADS TO EDIT", "ENV BACK / SAVE HOLD UNDO");
        ko_card(0,"K1 DESTINATION",slot);ko_card(1,"K2 CLEAR","NOTES");ko_card(2,"K3 CLEAR","LOCKS");ko_card(3,"K4 DUPLICATE","TO SLOT");
    } else if(ko.view==18) {
        ko_page("ARRANGEMENT / TOOLS", "SAVE STORES ALL TRACKS", "ENV VARIATIONS / EDIT BACK");
        ko_card(0,"K1 LAUNCH",chain_config.rsv[0]==1?"PATTERN END":"NEXT BAR");
        ko_card(1,"K2 MOVE SECTION","LEFT / RIGHT");ko_card(2,"K3 TURN RIGHT","DUPLICATE");ko_fx_number(3,"SECTION",ko_section+1,"",0);
    } else if(ko.view==16) {
        ko_pad_params_t *p=&ko_kit_params[ko_kit_pad];
        ko_page("DRUM PAD", "DECAY 0 FULL / GROUP 0 OFF", "LFO ROLL / ENV BACK");
        ko_fx_number(0,"K1 PITCH",p->pitch,"st",0);ko_fx_number(1,"K2 LEVEL",p->level,"",127);
        ko_fx_number(2,"K3 DECAY",p->decay,"",0);ko_fx_number(3,"K4 CHOKE GROUP",p->choke,"",0);
    } else if(ko.view==14) {
        step_t *n=&TSEL->step[ko_seq[song.sel].cursor];int16_t gate=TSEL->p[P_SGATE];
        (void)motion_lock_get(TSEL,ko_seq[song.sel].cursor,P_SGATE,&gate);
        ko_page("STEP / DYNAMICS", "PRESETS STEP / SAVE HOLD UNDO", "ENV PITCH / LFO SWING");
        ko_fx_number(0,"K1 VELOCITY",n->vel,"",127);ko_fx_number(1,"K2 GATE",gate,"",0);
        ko_fx_number(2,"K3 CHANCE",step_chance(n),"%",100);ko_fx_number(3,"K4 REPEATS",step_ratchet(n),"",0);
    } else if(ko.view==15) {
        ko_page("SEQUENCE / TIMING", "TRACK SWING + DEFAULT GATE", "LFO RECORD / ENV DETAIL");
        ko_fx_number(0,"K1 SWING",TSEL->p[P_SSWING],"%",0);ko_fx_number(1,"K2 GATE",TSEL->p[P_SGATE],"",0);
        ko_fx_number(2,"K3 TEMPO",song.g[G_BPM],"BPM",0);
    } else if (ko.view==8 || ko.view==9) {
        ko_arrange_draw(layout);
    } else if(ko.view>=10 && ko.view<=13) {
        ko_mixer_draw();
    } else if (ko.view==6) {
        ko_page("MIXER / MUTE + SOLO", "KEYS CONTINUE TO PLAY", "ARP NEXT / EDIT WAVE");
        ko_card(0,"K1 SONG 1",trk[0].p[P_MUTE]?"MUTED":"ON");ko_card(1,"K2 DRUM KIT",trk[1].p[P_MUTE]?"MUTED":"ON");
        ko_card(2,"K3 SONG 2",trk[2].p[P_MUTE]?"MUTED":"ON");
        ko_card(3,"K4 SOLO",ko_solo<0?"OFF":ko_solo==0?"SONG 1":ko_solo==1?"DRUM KIT":"SONG 2");
    } else if (ko.view==1 || ko.view==2) {
        ko_line(49,ko.view==1?"FX 1/2 / AMOUNT 0-127":"FX 2/2 / * SHARED DELAY",T_MID);
        if(ko.view==1) {
            ko_fx_number(0,"K1 FILTER",TSEL->p[song.sel==1?P_E4:P_E7],"",127);
            ko_fx_number(1,"K2 DELAY",TSEL->p[P_DLY],"",127);
            ko_fx_number(2,"K3 CHORUS",TSEL->p[P_CHOR],"",127);
            ko_fx_number(3,"K4 REVERB",TSEL->p[P_REV],"",127);
        } else {
            ko_fx_number(0,"K1 DRIVE",TSEL->p[P_DIST],"",127);
            ko_fx_panel(1,108,"K2 TIME *",N_DTIME[song.g[G_DTIME]],"",0,0);
            ko_fx_number(2,"K3 FEEDBK *",song.g[G_DFDBK],"",120);
            ko_fx_number(3,"K4 LEVEL",TSEL->p[P_LEVEL],"",127);
        }
        ko_line(205,"FX NEXT / ENV COMPRESSOR",T_MID);
        ko_line(224,"EDIT RESET / HOME SAMPLE",T_MID);
    } else if (ko.view == 5 && song.sel!=1) {
        const ko_table_t *tab=ko_table();
        ko_page("CHOP / PLAYBACK", "ENV ENVELOPE / LFO CHOPS", "EDIT BACK TO WAVEFORM");
        if(tab->n){const ko_chop_t *c=&tab->chop[ko.selected];
            ko_card(0,"K1 HELD LOOP",c->rsv&KO_NO_LOOP?"OFF":"ON");ko_card(1,"K2 LATCH",c->rsv&KO_LATCH?"ON":"OFF");
            ko_card(2,"K3 + / -","SPLIT / JOIN");ko_card(3,"K4 TURN","UNDO / REDO");}
        else ko_line(96,"ADD A SONG IN SAMPLE LOADER",T_MID);
    } else {
        ko_comp_t c=ko.comp[song.sel==1];int bypass=c.ratio==1 && c.makeup==0;
        ko_line(49,bypass?"COMPRESSOR / BYPASS":"COMPRESSOR / ON",bypass?T_MID:T_THEME);
        if(ko.view==3) {
            ko_fx_number(0,"K1 THRESHOLD",-(int32_t)c.threshold,"dB",48);
            char ratio[16];fmt_int(ratio,c.ratio);str_cpy(ratio+str_len(ratio),":1",sizeof ratio-str_len(ratio));
            ko_fx_panel(1,108,"K2 RATIO",ratio,"",c.ratio-1,7);
            ko_fx_number(2,"K3 ATTACK",c.attack,"ms",100);
            ko_fx_number(3,"K4 RELEASE",c.release*10,"ms",500);
        } else {
            char makeup[16];fmt_int(makeup,c.makeup);ko_fx_panel(0,224,"K1 MAKEUP GAIN",makeup,"dB",c.makeup,12);
            ko_line(146,"DYNAMICS: ENV",T_MID);
            ko_line(168,"RATIO 1 + MAKEUP 0 = BYPASS",T_MID);
            ko_line(190,song.sel==1?"DRUM KIT COMPRESSOR":"SELECTED SONG COMPRESSOR",T_MID);
        }
        ko_line(205,"LFO MAKEUP / ENV DYNAMICS",T_MID);
        ko_line(224,"EDIT BACK TO SAMPLE",T_MID);
    }
    if(ko.view!=8 && ko.view!=9) ko_sample_title();
    if(ko.view==27)ko_sync_speed_draw();
    ko_playhead();
    revision = ko.revision; generation = smp_user_gen; part = song.sel; view = ko.view; ready=(uint8_t)ko_ready();bg=T_BG;theme=T_THEME;ui.force = 0;
}
