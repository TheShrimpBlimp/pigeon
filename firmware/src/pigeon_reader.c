/* SPDX-License-Identifier: GPL-3.0-only */
/* PIGEON's semantic screen observer. No speech work runs in an audio interrupt.
 * Protocol/timing belongs to the unmodified fm1-reader announce.c. */
static const char *const pr_pages[KO_VIEW_COUNT]={
    "Waveform","Effects amounts","Drive and shared delay","Compressor dynamics","Compressor makeup",
    "Chop playback","Mute and solo","Sequencer","Patterns","Arrangement","Mixer levels","Mixer delay sends",
    "Mixer chorus sends","Mixer reverb sends","Step dynamics","Sequence timing","Drum pad",0,
    "Arrangement tools","Chop envelope","Chop copy and split","Recording preferences","Pattern variations",
    "Tempo and loop","Step pitch and timing","Choke and layout","Drum rolls","Speed synchronization","Chop move and swap","Chop queue","Scratch deck","Note timing","Sequence tools"
};
static struct {
    char context[112],value[4][80],detail[ANN_MAX+1],buf[ANN_MAX+1];
    uint32_t context_hash,value_hash[4],step_hash,transport,selected;
    int16_t tuning;
    uint8_t started,where_phase,warning;
} pr __attribute__((section(".pool")));
static void pr_add(char *out,uint32_t size,const char *text)
{uint32_t n=str_len(out);if(n<size)str_cpy(out+n,text,size-n);}
static void pr_number(char *out,uint32_t size,int32_t value)
{char number[16];fmt_int(number,value);pr_add(out,size,number);}
static void pr_value(uint32_t k,const char *name,const char *value)
{
    char *out=pr.value[k];out[0]=0;pr_add(out,80,"Knob ");pr_number(out,80,k+1);
    pr_add(out,80,", ");pr_add(out,80,name);pr_add(out,80," ");pr_add(out,80,value);
}
static void pr_num(uint32_t k,const char *name,int32_t value,const char *unit)
{char text[32]="";pr_number(text,sizeof text,value);if(unit && *unit){pr_add(text,sizeof text," ");pr_add(text,sizeof text,unit);}pr_value(k,name,text);}
static void pr_choice(uint32_t k,const char *name,int on){pr_value(k,name,on?"on":"off");}
static const char *pr_track(void){return song.sel==1?"Drum kit":song.sel==2?"Song 2":"Song 1";}
static uint32_t pr_millis(uint32_t samples,const slc_src_t *src)
{
    if(!src)return 0;
    uint32_t rate=(uint32_t)(((uint64_t)src->rate*44100u+32768u)>>16);
    return rate?(samples/rate)*1000u+((samples%rate)*1000u)/rate:0;
}
static void pr_pattern(char *out,uint32_t size,uint32_t slot)
{char name[2]={(char)('A'+(slot&3u)),0};pr_add(out,size,name);}
static uint32_t pr_step_hash(uint32_t step,const step_t *n) {
    uint32_t h=step*131u+n->n*7u+n->hit*17u+n->time;
    for(uint32_t j=0;j<n->n && j<4;j++)h=h*31u+n->note[j];
    return h;
}
static void pr_step_notes(char *out,uint32_t size,const step_t *n) {
    if(n->time!=ST_NOTE || (!n->n && !n->hit)){pr_add(out,size,"empty");return;}
    if(n->n){pr_add(out,size,"notes ");for(uint32_t j=0;j<n->n && j<4;j++){if(j)pr_add(out,size,", ");pr_number(out,size,n->note[j]);}}
    else {pr_add(out,size,"drum hits ");for(uint32_t j=0;j<16;j++)if(n->hit&(1u<<j)){pr_number(out,size,j+1);pr_add(out,size," ");}}
}
static void pr_snapshot(void)
{
    memset(pr.context,0,sizeof pr.context);memset(pr.value,0,sizeof pr.value);pr.detail[0]=0;
    if(ui.confirm){char a[24],b[24];confirm_text(a,b);str_cpy(pr.context,a,sizeof pr.context);
        if(b[0]){pr_add(pr.context,sizeof pr.context," ");pr_add(pr.context,sizeof pr.context,b);}
        pr_add(pr.context,sizeof pr.context," Octave down no. Octave up yes.");return;}
    if(name_on()){
        str_cpy(pr.context,"Phrase name",sizeof pr.context);pr_value(0,"cursor","");pr_number(pr.value[0],80,nm.cur+1);
        pr_value(1,"character",nm.cur<nm.len?(char[2]){nm.s[nm.cur],0}:"empty");pr_value(2,"random name",nm.s);
        str_cpy(pr.detail,"White keys type letters. Black keys move, insert a space, delete, or change to numbers. Octave up saves. Octave down cancels.",sizeof pr.detail);return;}
    if(ui.menu){
        if(ui.menu==2){str_cpy(pr.context,"PIGEON credits",sizeof pr.context);
            str_cpy(pr.detail,"By The Shrimp Blimp. Based on Felucca by Leo Kuroshita and Hugelton Instruments. Font by Disastrid. Logo by Dendy0097. Screen reader protocol by Charles Vestal. Presets scrolls. Octave down returns.",sizeof pr.detail);return;}
        if(ui.menu==3){str_cpy(pr.context,"PIGEON device information",sizeof pr.context);
            str_cpy(pr.detail,"Firmware ",sizeof pr.detail);pr_add(pr.detail,sizeof pr.detail,FW_DISPLAY_VERSION);
            pr_add(pr.detail,sizeof pr.detail,". Battery level ");pr_number(pr.detail,sizeof pr.detail,batt_shown());
            static const char *const usb_state[]={"off","waiting for host","bus detected","being configured","midi active"};
            pr_add(pr.detail,sizeof pr.detail,". USB ");pr_add(pr.detail,sizeof pr.detail,usb_state[info_usb()]);
            pr_add(pr.detail,sizeof pr.detail,". Audio processor load ");pr_number(pr.detail,sizeof pr.detail,info.cpu);
            pr_add(pr.detail,sizeof pr.detail," percent. Octave down returns.");return;}
        uint32_t tab=menu_tab(),row=ui.menu_sel;str_cpy(pr.context,"Settings, ",sizeof pr.context);pr_add(pr.context,sizeof pr.context,MTAB_NAME[tab]);
        str_cpy(pr.value[0],MI_NAME[row],80);
        if(menu_valued(row)){pr_add(pr.value[0],80," ");pr_add(pr.value[0],80,menu_vname(row,menu_get(row)));}
        pr_add(pr.value[0],80,", ");pr_number(pr.value[0],80,mtab_pos(tab,row)+1);pr_add(pr.value[0],80," of ");pr_number(pr.value[0],80,mtab_rows(tab));
        str_cpy(pr.detail,"Algorithm selects a tab. Presets selects a row. Knobs or octave buttons change the value. Home closes settings.",sizeof pr.detail);return;
    }
    if(ui.uboot){str_cpy(pr.context,"Firmware update countdown",sizeof pr.context);
        str_cpy(pr.detail,"Release either octave button to cancel entering update mode.",sizeof pr.detail);return;}
    if(seq_counting()){str_cpy(pr.context,"Recording count in",sizeof pr.context);
        str_cpy(pr.detail,"Recording starts after the count in. Play button stops.",sizeof pr.detail);return;}
    str_cpy(pr.context,pr_track(),sizeof pr.context);pr_add(pr.context,sizeof pr.context,", ");
    if(ui.layer){
        pr_add(pr.context,sizeof pr.context,ui.layer==LAYER_GLO?"Mixer shortcuts":"Sequence tools");
        if(ui.layer==LAYER_GLO){for(uint32_t k=0;k<SAMPLER_PARTS;k++)pr_num(k,k==0?"song 1 level":k==1?"drum kit level":"song 2 level",trk[k].p[P_LEVEL],"");
            str_cpy(pr.detail,"F, G and A hold solo for song 1, drums and song 2. F sharp, G sharp and A sharp toggle their mutes. C unmutes all. D is midi learn. F in the next octave taps tempo. Octave down undoes. Global closes.",sizeof pr.detail);
        }else{pr_num(0,"length",TSEL->p[P_SLEN],"steps");pr_value(1,"division",N_DIV[TSEL->p[P_SDIV]]);pr_num(2,"swing",TSEL->p[P_SSWING],"percent");pr_num(3,"gate",TSEL->p[P_SGATE],"");
            str_cpy(pr.detail,"White keys clear, reverse, shift left, shift right, randomize and cook the sequence. Black keys select drum lanes. Octave down undoes. Sequence button closes.",sizeof pr.detail);}return;
    }
    uint32_t v=ko.view<KO_VIEW_COUNT?ko.view:0;
    pr_add(pr.context,sizeof pr.context,v==7?(ko_piano_roll[song.sel]?"Piano roll":"Sequence grid"):pr_pages[v]?pr_pages[v]:"Waveform");
    const ko_table_t *table=song.sel==1?0:ko_table();const ko_chop_t *chop=table && table->n?&table->chop[ko.selected]:0;
    uint32_t selected=ko.selected;
    if(v==30){const voice_t *voice=ko_latest_voice();if(voice && ((uint32_t)voice->s[4]&KO_V_SCRATCH))selected=((uint32_t)voice->s[4]>>8)&63u;}
    if(chop && (v==0 || v==5 || v==19 || v==20 || v==25 || v==27 || v==28 || v==30)){
        pr_add(pr.context,sizeof pr.context,", chop ");pr_number(pr.context,sizeof pr.context,selected+1);
    }
    if(v==16){pr_add(pr.context,sizeof pr.context,", pad ");pr_number(pr.context,sizeof pr.context,ko_kit_pad+1);}
    switch(v){
    case 0:
        if(song.sel==1){pr_num(0,"level",TSEL->p[P_LEVEL],"");pr_num(1,"kit pitch",TSEL->p[P_E1],"");pr_num(2,"crush",TSEL->p[P_E2],"");pr_num(3,"tone",TSEL->p[P_E4],"");str_cpy(pr.detail,"Envelope opens pad settings. Effects opens effects.",sizeof pr.detail);}
        else if(chop){const slc_src_t *src=slc_get(ko_src());pr_num(0,"start",pr_millis(chop->a,src),"milliseconds");pr_num(1,"end",pr_millis(chop->b,src),"milliseconds");pr_num(2,"scroll",pr_millis(ko.cursor,src),"milliseconds");pr_num(3,"zoom",1u<<ko.zoom,"times");
            str_cpy(pr.detail,"Octave buttons change pitch. Edit opens chop settings. Presets selects a chop. Hold Select and play a chop to audition its ending, then loop from its beginning.",sizeof pr.detail);}
        else str_cpy(pr.detail,"No song loaded. Add your audio in the sample loader or load a bank.",sizeof pr.detail);
        break;
    case 1:pr_num(0,"filter",TSEL->p[song.sel==1?P_E4:P_E7],"");pr_num(1,"delay",TSEL->p[P_DLY],"");pr_num(2,"chorus",TSEL->p[P_CHOR],"");pr_num(3,"reverb",TSEL->p[P_REV],"");goto effects_help;
    case 2:pr_num(0,"drive",TSEL->p[P_DIST],"");pr_value(1,"shared delay time",N_DTIME[song.g[G_DTIME]]);pr_num(2,"shared feedback",song.g[G_DFDBK],"");pr_num(3,"level",TSEL->p[P_LEVEL],"");
    effects_help:str_cpy(pr.detail,"Effects switches pages. Envelope opens the compressor. Edit resets effects. Home returns to the sample.",sizeof pr.detail);break;
    case 3:case 4:{ko_comp_t c=ko.comp[song.sel==1];if(v==4){pr_num(0,"makeup gain",c.makeup,"decibels");}else{
        pr_num(0,"threshold",-(int32_t)c.threshold,"decibels");char ratio[16]="";pr_number(ratio,sizeof ratio,c.ratio);pr_add(ratio,sizeof ratio,":1");pr_value(1,"ratio",ratio);
        pr_num(2,"attack",c.attack,"milliseconds");pr_num(3,"release",c.release*10,"milliseconds");}
        str_cpy(pr.detail,"Envelope opens dynamics. L F O opens makeup gain. Edit returns to the sample.",sizeof pr.detail);break;}
    case 5:if(chop){pr_choice(0,"held loop",!(chop->rsv&KO_NO_LOOP));pr_choice(1,"latch",chop->rsv&KO_LATCH);pr_value(2,"right splits",", left joins");pr_value(3,"turn to undo or redo","");}
        str_cpy(pr.detail,"Effects opens scratch. Arp opens the chop queue. Envelope opens envelopes. L F O opens chop operations. Edit returns to the waveform.",sizeof pr.detail);break;
    case 6:for(uint32_t k=0;k<SAMPLER_PARTS;k++)pr_choice(k,k==0?"song 1 mute":k==1?"drum mute":"song 2 mute",trk[k].p[P_MUTE]);pr_value(3,"solo",ko_solo<0?"off":ko_solo==0?"song 1":ko_solo==1?"drums":"song 2");break;
    case 7:pr_num(0,"tempo",song.g[G_BPM],"BPM");pr_value(1,"timing",ko_seq[song.sel].end_mode?"sample end":"tempo");pr_num(2,"step",ko_seq[song.sel].cursor+1,"");pr_num(3,"length",TSEL->p[P_SLEN],"steps");
        str_cpy(pr.detail,"Step ",sizeof pr.detail);pr_number(pr.detail,sizeof pr.detail,ko_seq[song.sel].cursor+1);pr_add(pr.detail,sizeof pr.detail,", ");
        pr_step_notes(pr.detail,sizeof pr.detail,&TSEL->step[ko_seq[song.sel].cursor]);pr_add(pr.detail,sizeof pr.detail,". Active steps ");uint32_t base=(ko_seq[song.sel].cursor/16u)*16u,count=0;
        for(uint32_t i=base;i<base+16 && i<(uint32_t)TSEL->p[P_SLEN];i++){const step_t *n=&TSEL->step[i];if(n->time==ST_NOTE && (n->n||n->hit)){if(count++)pr_add(pr.detail,sizeof pr.detail,", ");pr_number(pr.detail,sizeof pr.detail,i+1);}}
        if(!count)pr_add(pr.detail,sizeof pr.detail,"none");
        pr_add(pr.detail,sizeof pr.detail,". Sequence switches grid and roll. Edit clears and advances. Record arms recording.");
        if(ko_piano_roll[song.sel])pr_add(pr.detail,sizeof pr.detail," Presets scrolls note rows.");break;
    case 8:pr.value[0][0]=0;pr_add(pr.value[0],80,"Knob 1, pattern ");pr_pattern(pr.value[0],80,ko_pattern_sel);pr_add(pr.value[0],80,ko_phrase_dirty()?", not saved":ko_pattern_mask&(1u<<ko_pattern_sel)?", saved":", empty");
        pr_value(1,"copy source",ko_copy_slot<4?(char[2]){(char)('A'+ko_copy_slot),0}:"none");pr_value(2,"right adds to arrangement",", left removes");pr_value(3,"right pastes",", left copies");
        pr.detail[0]=0;for(uint32_t i=0;i<4;i++){pr_pattern(pr.detail,sizeof pr.detail,i);pr_add(pr.detail,sizeof pr.detail,ko_pattern_mask&(1u<<i)?" saved. ":" empty. ");}
        pr_add(pr.detail,sizeof pr.detail,"First four white keys select. Save stores all tracks. Play loops. Patterns opens arrangement.");
        if(ko_launch_pending<4){pr_add(pr.detail,sizeof pr.detail," Queued pattern ");pr_pattern(pr.detail,sizeof pr.detail,ko_launch_pending);pr_add(pr.detail,sizeof pr.detail,chain_config.rsv[0]==1?" at pattern end.":" at next bar.");}break;
    case 9:{uint32_t row=ko_section;pr_num(0,"section",row+1,"");if(row<chain_config.count){const chain_row_t *r=&chain_config.row[row];pr_value(1,"pattern",r->slot[0]<4?(char[2]){(char)('A'+r->slot[0]),0}:"empty");pr_num(2,"repeats",r->repeat,"");}pr_value(3,"right inserts",", left deletes");str_cpy(pr.detail,"First four white keys append patterns A through D. Save stores all tracks. Play starts the arrangement. Envelope opens tools.",sizeof pr.detail);break;}
    case 10:case 11:case 12:case 13:{uint32_t id=v==10?P_LEVEL:v==11?P_DLY:v==12?P_CHOR:P_REV;for(uint32_t k=0;k<SAMPLER_PARTS;k++)pr_num(k,k==0?"song 1":k==1?"drums":"song 2",trk[k].p[id],"");pr_num(3,"selected track pan",TSEL->p[P_PAN],"");break;}
    case 14:{uint32_t step=ko_seq[song.sel].cursor;step_t *n=&TSEL->step[step];int16_t gate=TSEL->p[P_SGATE];(void)motion_lock_get(TSEL,step,P_SGATE,&gate);
        pr_num(0,"velocity",n->vel,"");pr_num(1,"gate",gate,"");pr_num(2,"chance",step_chance(n),"percent");pr_num(3,"repeats",step_ratchet(n),"");
        str_cpy(pr.detail,"Selected step ",sizeof pr.detail);pr_number(pr.detail,sizeof pr.detail,step+1);pr_add(pr.detail,sizeof pr.detail,". Envelope opens pitch and timing. Presets selects a step.");break;}
    case 15:pr_num(0,"swing",TSEL->p[P_SSWING],"percent");pr_num(1,"gate",TSEL->p[P_SGATE],"");pr_num(2,"tempo",song.g[G_BPM],"BPM");str_cpy(pr.detail,"L F O opens recording preferences. Envelope opens step details.",sizeof pr.detail);break;
    case 16:{ko_pad_params_t *p=&ko_kit_params[ko_kit_pad];pr_num(0,"pitch",p->pitch,"semitones");pr_num(1,"level",p->level,"");pr_num(2,"decay",p->decay,"");pr_num(3,"choke group",p->choke,"");str_cpy(pr.detail,"Decay zero plays the full sample. Group zero allows overlap. L F O opens rolls. Envelope returns to the kit.",sizeof pr.detail);break;}
    case 18:pr_value(0,"launch",chain_config.rsv[0]==1?"pattern end":"next bar");pr_value(1,"move section", "left or right");pr_value(2,"turn right to duplicate section","");str_cpy(pr.detail,"Selected section ",sizeof pr.detail);pr_number(pr.detail,sizeof pr.detail,ko_section+1);pr_add(pr.detail,sizeof pr.detail,". Envelope opens variations.");break;
    case 19:if(chop){pr_num(0,"attack",((chop->rsv>>KO_ATTACK_SHIFT)&31)*5,"milliseconds");pr_num(1,"release",((chop->rsv>>KO_RELEASE_SHIFT)&31)*10,"milliseconds");pr_value(2,"trigger",chop->rsv&KO_LATCH?"latch":(chop->rsv&KO_ONE)||((chop->rsv&KO_NO_LOOP)&&!(chop->rsv&KO_GATE))?"one shot":"hold");pr_choice(3,"loop",!(chop->rsv&KO_NO_LOOP));}break;
    case 20:pr_num(0,"target",ko.selected+1,"");pr_num(1,"copy source",ko_copy_chop+1,"");pr_value(2,"right duplicates",", left splits");pr_value(3,"right copies settings",", left merges");str_cpy(pr.detail,"Record copies the whole chop. Envelope opens move and swap. Effects opens groups.",sizeof pr.detail);break;
    case 21:{static const char *const click[]={"off","record","on"},*const bars[]={"off","one bar","two bars"},*const quant[]={"off","1/16","1/8"};pr_value(0,"metronome",click[rp_get(RP_CLICK)]);pr_value(1,"count in",bars[rp_get(RP_COUNTIN)]);pr_value(2,"live quantization",quant[ko_quantize[song.sel]]);if(song.sel!=1)pr_choice(3,"song choke",ko_choke[ko_id()]);str_cpy(pr.detail,"Envelope opens tap tempo. L F O opens timing.",sizeof pr.detail);break;}
    case 22:pr_value(0,"destination",(char[2]){(char)('A'+ko_variation_slot),0});pr_value(1,"turn to clear notes","");pr_value(2,"turn to clear locks","");pr_value(3,"turn right to duplicate","");str_cpy(pr.detail,"Stop and release pads before editing. Envelope returns. Hold save to undo.",sizeof pr.detail);break;
    case 23:pr_num(0,"tempo",song.g[G_BPM],"BPM");pr_num(1,"loop beats",ko_loop_beats,"");pr_num(2,"apply tempo",song.sel==1?0:ko_loop_bpm(),"BPM");pr_num(3,"chop",ko.selected+1,"");str_cpy(pr.detail,"Edit taps tempo. Envelope returns. Effects opens synchronization.",sizeof pr.detail);break;
    case 24:{uint32_t step=ko_seq[song.sel].cursor;step_t *n=&TSEL->step[step];int16_t pitch=0;(void)motion_lock_get(TSEL,step,P_TRANS,&pitch);static const char *const cond[]={"always","every second","every fourth","first pass","after first","first of two","second of two","first of three","second of three","third of three","first of four","second of four","third of four","fourth of four","fill","not fill"};pr_num(0,"pitch",pitch,"semitones");pr_num(1,"nudge",step_nudge(n),"sixteenths");pr_value(2,"condition",cond[step_condition(n)]);pr_num(3,"step",step+1,"");break;}
    case 25:pr_num(0,"choke group",chop?chop->rsv>>KO_CHOKE_SHIFT:0,"");pr_value(1,"layout",ko_layout[ko_id()]?"B":"A");pr_num(2,"song gain",TSEL->p[P_LEVEL],"");pr_value(3,"turn to copy layout","");break;
    case 26:pr_value(0,"rate",N_DIV[ko_roll_div]);pr_choice(1,"roll",ko_roll_enabled);str_cpy(pr.detail,"Hold drum pads to repeat. L F O returns to pad settings.",sizeof pr.detail);break;
    case 27:{ko_sync_t *q=&ko_sync[ko_id()];pr_num(0,"loop beats",q->beats,"");pr_choice(1,"synchronization",q->on);pr_value(2,"reference",q->whole?"whole song":"selected chop");pr_num(3,"chop",ko.selected+1,"");str_cpy(pr.detail,"Synchronization changes speed and pitch together. Effects returns. Edit opens the waveform.",sizeof pr.detail);break;}
    case 32:{static const char *const action[]={"double","half","Euclidean rhythm","humanize"};pr_value(0,"action",action[ko_tool_action]);
        if(ko_tool_action==2){pr_num(1,"hits",ko_euclid_hits,"");pr_num(2,"cycle",ko_euclid_cycle,"steps");pr_num(3,"rotation",ko_euclid_rot,"");}
        else if(ko_tool_action==3){pr_num(1,"velocity variation",ko_human_vel,"");pr_num(2,"timing variation",ko_human_time,"");}
        str_cpy(pr.detail,"Envelope applies. Effects returns. Hold Save to undo.",sizeof pr.detail);break;}
    case 31:{uint32_t step=ko_seq[song.sel].cursor;const step_t *st=&TSEL->step[step];uint32_t slot=ko_note_selected[song.sel];if(slot>=st->n)slot=st->n?st->n-1:0;
        uint32_t data=ko_note_data(&motion,TSEL,step,slot);
        if(!st->n){pr_value(0,"note","empty");pr_num(3,"step",step+1,"");break;}
        pr_num(0,"note",st->note[slot],"");
        if(ko_note_length(data))pr_num(1,"length",ko_note_length(data),"steps");else pr_value(1,"length","track gate");
        pr_num(2,"nudge",ko_note_nudge(data),"sixteenths of a step");pr_num(3,"step",step+1,"");break;}
    case 29:{uint32_t id=ko_id();
        pr_choice(0,"queue",ko_queue[id].on);pr_value(1,"left removes last, right clears waiting","");
        pr_num(2,"selected waiting entry",ko_queue[id].count?ko_queue[id].selected+1:0,"");pr_add(pr.value[2],80," of ");pr_number(pr.value[2],80,ko_queue[id].count);pr_value(3,"turn right to stop and clear","");
        str_cpy(pr.detail,"Keys queue chops in order, including repeats. Each plays to the end. Arp returns. Edit or Play stops and clears. Home returns to the waveform with queue mode enabled.",sizeof pr.detail);break;}
    case 30:{uint32_t id=ko_id();
        if(ko_scratch[id].controls){
            pr_add(pr.context,sizeof pr.context,", motor controls");pr_value(0,"seek","within chop");
            pr_num(1,"speed",ko_scratch[id].rate,"percent");pr_add(pr.value[1],80,ko_scratch[id].motor?", motor on":", motor off");pr_num(2,"brake",ko_scratch[id].brake_ms,"milliseconds");pr_num(3,"glide",ko_scratch[id].release_ms,"milliseconds");
        } else {pr_value(0,"scratch","turn backward or forward");pr_num(1,"level",ko_scratch[id].level*100/16,"percent");pr_add(pr.value[1],80,ko_scratch[id].motor?", motor on":", motor off");
        pr_value(2,"view",ko_scratch[id].record?"record":"waveform");pr_num(3,"sensitivity",ko_scratch[id].sensitivity+1, "of 8");}
        str_cpy(pr.detail,"Tap a chop key to loop it. Another key replaces it. Jog returns to motor speed when released. Play toggles the motor. Envelope sets the cue. LFO returns to the cue. Effects returns. Home exits and stops the deck.",sizeof pr.detail);break;}
    case 28:pr_num(0,"source",ko.selected+1,"");pr_num(1,"destination",ko_copy_chop+1,"");pr_value(2,"turn right to swap","");pr_value(3,"turn right to move","");str_cpy(pr.detail,"Moving shifts the keys in between. Envelope returns. Edit opens the waveform.",sizeof pr.detail);break;
    default:break;
    }
    if(!pr.detail[0])str_cpy(pr.detail,"Home returns to the sample. Hold home for settings.",sizeof pr.detail);
}
static int pr_contains(const char *s,const char *needle)
{for(;*s;s++){const char *a=s,*b=needle;while(*b && *a==*b){a++;b++;}if(!*b)return 1;}return 0;}
static void pigeon_reader_message(const char *text,const char *suffix)
{
    if(!ann_on())return;
    while(*text && (uint8_t)*text<32)text++;
    if(str_eq(text,"KEYS WRITE STEPS"))text="Recording armed";
    else if(str_eq(text,"RECORD OFF"))text="Recording disarmed";
    str_cpy(pr.buf,text,sizeof pr.buf);if(suffix)pr_add(pr.buf,sizeof pr.buf,suffix);
    text=pr.buf;uint32_t flags=ANN_NOW;
    if(pr_contains(text,"CANNOT") || pr_contains(text,"FAILED") || pr_contains(text,"FULL") || pr_contains(text,"STOP") || pr_contains(text,"NO ") || pr_contains(text,"NOT ") || pr_contains(text,"UNSAVED"))flags|=ANN_KEEP;
    pr.where_phase=0;pr.warning=1;ann_say(text,0,flags);
}
static void pigeon_reader_calibration(const char *action,const char *control)
{
    if(!ann_on())return;
    str_cpy(pr.buf,"Calibration. ",sizeof pr.buf);pr_add(pr.buf,sizeof pr.buf,action);pr_add(pr.buf,sizeof pr.buf," ");pr_add(pr.buf,sizeof pr.buf,control);
    pr_add(pr.buf,sizeof pr.buf,". Any piano key cancels.");ann_say(pr.buf,0,ANN_NOW|ANN_KEEP);
}
static void pigeon_reader_frame(uint32_t now)
{
    ann_tick(now);if(!ann_on()){pr.started=pr.where_phase=0;return;}
    pr_snapshot();int where=ann_where();uint32_t context=str_hash(7,pr.context),values[4];
    for(uint32_t k=0;k<4;k++)values[k]=str_hash(11,pr.value[k]);
    uint32_t transport=song.playing*2u+((song.rec>>song.sel)&1u);
    if(where || !pr.started){pr.where_phase=1;ann.later_set=0;}
    else if(pr.warning){ann.later_set=0;}
    else if(context!=pr.context_hash){str_cpy(pr.buf,pr.context,sizeof pr.buf);
        if(ui.menu==1 && pr.value[0][0]){pr_add(pr.buf,sizeof pr.buf,". ");pr_add(pr.buf,sizeof pr.buf,pr.value[0]);}
        pr.where_phase=0;ann_later(pr.buf,ANN_NOW,200,now);
    }else{
        pr.buf[0]=0;
        for(uint32_t k=0;k<4;k++)if(values[k]!=pr.value_hash[k] && pr.value[k][0] && !(ko.view==29 && k==2)){
            if(pr.buf[0])pr_add(pr.buf,sizeof pr.buf,". ");pr_add(pr.buf,sizeof pr.buf,!memcmp(pr.value[k],"Knob ",5)?pr.value[k]+8:pr.value[k]);}
        if(ko.view==30 && ko_scratch[ko_id()].cents!=pr.tuning){
            str_cpy(pr.buf,"Tuning ",sizeof pr.buf);pr_number(pr.buf,sizeof pr.buf,ko_scratch[ko_id()].cents);pr_add(pr.buf,sizeof pr.buf," cents");
        }
        if(pr.buf[0]){pr.where_phase=0;ann_later(pr.buf,ANN_NOW,120,now);}
        if(transport!=pr.transport){pr.where_phase=0;
            if((transport^pr.transport)&2u)ann_say(song.playing?"Playing":"Stopped",0,ANN_NOW);
            if((transport^pr.transport)&1u)ann_say((song.rec>>song.sel)&1u?"Recording armed":"Recording disarmed",0,((transport^pr.transport)&2u)?0:ANN_NOW);
        }
        if(!song.playing && !ui.menu && !ui.confirm && (ko.view==7 || ko.view==14 || ko.view==24)){
            uint32_t step=ko_seq[song.sel].cursor;const step_t *n=&TSEL->step[step];
            uint32_t hash=pr_step_hash(step,n);
            if(hash!=pr.step_hash){ann.later_set=0;str_cpy(pr.buf,"Step ",sizeof pr.buf);pr_number(pr.buf,sizeof pr.buf,step+1);
                pr_add(pr.buf,sizeof pr.buf,", ");pr_step_notes(pr.buf,sizeof pr.buf,n);ann_say(pr.buf,0,ANN_NOW);}
        }
    }
    if(pr.where_phase){uint32_t phase=pr.where_phase,accepted=pr_wire_sent;
        const char *text=phase==1?pr.context:phase<=5?pr.value[phase-2]:"";
        if(phase==6 && !ui.menu && !ui.confirm && (ko.view==7 || ko.view==14 || ko.view==24)){
            pr.buf[0]=0;const step_t *st=&TSEL->step[ko_seq[song.sel].cursor];pr_step_notes(pr.buf,sizeof pr.buf,st);
            if(ko.view==7 && ko_piano_roll[song.sel] && st->n){uint32_t slot=ko_note_selected[song.sel];if(slot>=st->n)slot=0;uint32_t data=ko_note_data(&motion,TSEL,ko_seq[song.sel].cursor,slot);
                pr_add(pr.buf,sizeof pr.buf,". Selected note ");pr_number(pr.buf,sizeof pr.buf,st->note[slot]);pr_add(pr.buf,sizeof pr.buf,", length ");
                if(ko_note_length(data)){pr_number(pr.buf,sizeof pr.buf,ko_note_length(data));pr_add(pr.buf,sizeof pr.buf," steps");}else pr_add(pr.buf,sizeof pr.buf,"track gate");
                pr_add(pr.buf,sizeof pr.buf,", nudge ");pr_number(pr.buf,sizeof pr.buf,ko_note_nudge(data));pr_add(pr.buf,sizeof pr.buf," sixteenths of a step");
            }text=pr.buf;
        }
        if(phase==6 && !ui.menu && !ui.confirm && ko.view==8){pr.buf[0]=0;ko_pattern_prompt(pr.buf,sizeof pr.buf);text=pr.buf;}
        if(phase==6 && !ui.menu && !ui.confirm && ko.view==29){
            str_cpy(pr.buf,"Next chops ",sizeof pr.buf);uint8_t notes[8];uint32_t count;
            fm1_irq_off();count=ko_queue[ko_id()].count;for(uint32_t i=0;i<8 && i<count;i++)notes[i]=ko_queue[ko_id()].note[(ko_queue[ko_id()].head+i)%KO_QUEUE_CAP];fm1_irq_on();
            if(!count)pr_add(pr.buf,sizeof pr.buf,"none");
            for(uint32_t i=0;i<8 && i<count;i++){if(i)pr_add(pr.buf,sizeof pr.buf,", ");pr_number(pr.buf,sizeof pr.buf,notes[i]-59);}
            if(count>8)pr_add(pr.buf,sizeof pr.buf,", more waiting");text=pr.buf;
        }
        if(phase==6 && !ui.menu && !ui.confirm && ko.view==30){
            str_cpy(pr.buf,"Presets, tuning ",sizeof pr.buf);pr_number(pr.buf,sizeof pr.buf,ko_scratch[ko_id()].cents);pr_add(pr.buf,sizeof pr.buf," cents");text=pr.buf;
        }
        if(phase==6 && (ui.menu==2 || ui.menu==3 || pr_contains(pr.detail,"No song loaded")))text=pr.detail;
        if(text[0])ann_say(text,0,phase==1?ANN_NOW:0);
        if(!text[0] || pr_wire_sent!=accepted)pr.where_phase=phase==6?0:phase+1;}
    const step_t *n=&TSEL->step[ko_seq[song.sel].cursor];
    pr.step_hash=pr_step_hash(ko_seq[song.sel].cursor,n);
    pr.context_hash=context;memcpy(pr.value_hash,values,sizeof values);pr.transport=transport;pr.tuning=ko_scratch[ko_id()].cents;pr.started=1;pr.warning=0;
}
