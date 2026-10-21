/* SPDX-License-Identifier: GPL-3.0-only */
/* Main-loop pattern editing; project slots A-D retain the existing FUNA ABI. */
static chain_pattern_t ko_clipboard __attribute__((section(".pool")));
static uint8_t ko_clip_valid,ko_paste_target=255;
static uint32_t ko_paste_fingerprint,ko_paste_clip;
static uint32_t ko_saved_fingerprint,ko_discard_fingerprint;
static uint8_t ko_fingerprint_valid,ko_discard_target=255;
static uint32_t ko_hash_bytes(uint32_t h,const void *data,uint32_t bytes){const uint8_t *p=data;while(bytes--)h=(h^*p++)*16777619u;return h;}
static uint32_t ko_phrase_fingerprint(void){
 uint32_t h=2166136261u;
 for(uint32_t k=0;k<SAMPLER_PARTS;k++){
  h=ko_hash_bytes(h,trk[k].step,sizeof trk[k].step);
  for(uint32_t j=0;j<4;j++){int16_t v=motion_base_value(&trk[k],P_SLEN+j);h=ko_hash_bytes(h,&v,sizeof v);}
  h=ko_hash_bytes(h,&trk[k].p[P_SQNT],sizeof trk[k].p[P_SQNT]);
 }
 h=ko_hash_bytes(h,&motion,sizeof motion);return ko_hash_bytes(h,&chain_config,sizeof chain_config);
}
static void ko_pattern_prompt(char *text,uint32_t size) {
    if(ko_discard_target<4 && ko_discard_fingerprint==ko_phrase_fingerprint()){
        str_cpy(text,"LOAD A? AGAIN / SAVE TO KEEP",size);if(size>5)text[5]=(char)('A'+ko_discard_target);
    }else if(ko_paste_target==ko_pattern_sel && ko_paste_fingerprint==ko_phrase_fingerprint()){
        str_cpy(text,"PASTE TO A? LFO AGAIN",size);if(size>9)text[9]=(char)('A'+ko_pattern_sel);
    }
}
static void ko_phrase_clean(void){ko_saved_fingerprint=ko_phrase_fingerprint();ko_fingerprint_valid=1;ko_discard_target=ko_paste_target=255;}
static int ko_phrase_dirty(void){return !ko_fingerprint_valid || ko_saved_fingerprint!=ko_phrase_fingerprint() || !(ko_pattern_mask&(1u<<ko_pattern_sel));}
static void ko_pattern_refresh(void)
{
    if(transport_busy()) return;
    ko_pattern_mask=0;
    for(uint32_t i=0;i<4;i++) {
        if(project_used(i)) ko_pattern_mask|=(uint8_t)(1u<<i);
#if FELUCCA_KO_STORAGE
        ko_pcm_poll();
#endif
    }
}
static void ko_pattern_preload(void)
{
    if(transport_busy()) return;
    ko_launch_ready=0;ko_launch_pending=255;ko_launch_mode=0;ko_bar_pos=0;
    ko_pattern_refresh();
    for(uint32_t i=0;i<4;i++) if(ko_pattern_mask&(1u<<i)) {
        if(!chain_copy_source(i)) ko_pattern_mask&=(uint8_t)~(1u<<i);
#if FELUCCA_KO_STORAGE
        ko_pcm_poll();
#endif
    }
    RING_PUBLISH();ko_launch_ready=ko_pattern_mask!=0;
}
static void ko_pattern_apply(const chain_pattern_t *p)
{
    /* Stopped: audio voices do not read the editable step array. Keep samples,
     * chop edits, levels, mutes and FX, and replace only phrase data. */
    for(uint32_t k=0;k<SAMPLER_PARTS;k++) {
        motion_restore(&trk[k]);
        memcpy(trk[k].step,p->step[k],sizeof trk[k].step);
        memcpy(&trk[k].p[P_SLEN],p->timing[k],sizeof p->timing[k]);
        trk[k].p[P_SQNT]=p->qnt[k];
        ko_seq[k].enabled=1;ko_seq[k].cursor=0;ko_pass[k]=0;ko_pass_started[k]=0;
        pat_sig[k]=~steps_sig(&trk[k]);
    }
    motion=p->motion;undo.trk=0;pat_from_set(0);ui.force=1;
}
static void ko_pattern_poll(void)
{
    if(ko.view!=8)ko_paste_target=ko_discard_target=255;
    uint32_t slot=ko_pattern_commit;
    if(slot<4 && !transport_busy()) {
        ko_pattern_commit=255;ko_pattern_sel=(uint8_t)slot;
        ko_pattern_apply(&chain.source[slot]);pat_from_set(slot+1u);ko_phrase_clean();
    }
}
static int ko_pattern_select(uint32_t slot)
{
    if(slot>=4u) return 0;
    if(transport_busy()) {
        if(!song.playing || !ko_launch_ready || (chain.running && !ko_launch_mode)) {ui_message("STOP / PLAY PATTERNS");return 0;}
        if(!(ko_pattern_mask&(1u<<slot))) {ui_message("EMPTY PATTERN");return 0;}
        if(ko_launch_pending==slot){ko_launch_pending=255;ui.force=1;ui_message("LAUNCH CANCELLED");return 1;}
        ko_pattern_sel=(uint8_t)slot;RING_PUBLISH();ko_launch_pending=(uint8_t)slot;
        ui.force=1;ui_message(chain_config.rsv[0]==1?"QUEUED: PATTERN END":"QUEUED: NEXT BAR");return 1;
    }
    ko_pattern_sel=(uint8_t)slot;
    if(!project_used(slot) || !chain_copy_source(slot)) {ui.force=1;ui_message("EMPTY / SAVE TO STORE");return 0;}
    ko_pattern_apply(&chain.source[slot]);pat_from_set(slot+1u);ko_phrase_clean();ui_message("PATTERN SELECTED");return 1;
}
/* UI-only guard: internal launch/restore paths keep their existing behavior. */
static int ko_pattern_request(uint32_t slot){
 ko_paste_target=255;
 if(slot>=4)return 0;
 if(!transport_busy() && (ko_pattern_mask&(1u<<slot)) && ko_phrase_dirty()){
  uint32_t fingerprint=ko_phrase_fingerprint();
  if(ko_discard_target!=slot || ko_discard_fingerprint!=fingerprint){ko_discard_target=slot;ko_discard_fingerprint=fingerprint;ui_message("UNSAVED: AGAIN TO LOAD");ui.force=1;return 0;}
 }
 ko_discard_target=255;return ko_pattern_select(slot);
}
static void ko_pattern_copy(void)
{
    if(transport_busy()) {ui_message("STOP TO COPY");return;}
    for(uint32_t k=0;k<CHAIN_PARTS;k++) {
        memcpy(ko_clipboard.step[k],trk[k].step,sizeof trk[k].step);
        for(uint32_t j=0;j<4;j++) ko_clipboard.timing[k][j]=motion_base_value(&trk[k],P_SLEN+j);
        ko_clipboard.qnt[k]=(uint8_t)trk[k].p[P_SQNT];
    }
    ko_clipboard.motion=motion;ko_clip_valid=1;ko_copy_slot=ko_pattern_sel;ko_paste_target=255;
    ui.force=1;ui_message("PATTERN COPIED");
}
static void ko_pattern_save(void)
{
    if(transport_busy()) {ui_message("STOP TO SAVE");return;}
    if(voices_busy()) {ui_message("RELEASE PADS TO SAVE");return;}
    if(!project_save(ko_pattern_sel)) {ko_pattern_refresh();ko_phrase_clean();char label[]="SAVED ALL TRACKS TO A";label[sizeof label-2]=(char)('A'+ko_pattern_sel);ui_message(label);}
    ui.force=1;
}
static void ko_pattern_paste(void)
{
    if(transport_busy()) {ui_message("STOP TO PASTE");return;}
    if(voices_busy()) {ui_message("RELEASE PADS TO PASTE");return;}
    if(!ko_clip_valid) {ui_message("COPY A PATTERN FIRST");return;}
    uint32_t fingerprint=ko_phrase_fingerprint(),clip=ko_hash_bytes(2166136261u,&ko_clipboard,sizeof ko_clipboard);
    if((ko_pattern_mask&(1u<<ko_pattern_sel)) || ko_phrase_dirty()){
        if(ko_paste_target!=ko_pattern_sel || ko_paste_fingerprint!=fingerprint || ko_paste_clip!=clip){
            ko_paste_target=ko_pattern_sel;ko_paste_fingerprint=fingerprint;ko_paste_clip=clip;ko_discard_target=255;
            char label[]="PASTE ALL TO A? AGAIN=YES";label[13]=(char)('A'+ko_pattern_sel);ui_message(label);ui.force=1;return;
        }
    }
    ko_paste_target=255;ko_pattern_apply(&ko_clipboard);ko_pattern_save();
}
static void ko_section_add(uint32_t slot)
{
    if(transport_busy()) {ui_message("STOP TO ARRANGE");return;}
    if(chain_config.count>=CHAIN_ROWS) {ui_message("16 SECTIONS FULL");return;}
    uint32_t at=(ko.view==9 || ko.view==18) && ko_section<chain_config.count ? ko_section+1u : chain_config.count;
    for(uint32_t i=chain_config.count;i>at;i--) chain_config.row[i]=chain_config.row[i-1];
    chain_config.row[at]=chain_row_of(slot,1);if(chain_config.rsv[1]==0x4D)chain_config.row[at].slot[3]=0;chain_config.count++;ko_section=(uint8_t)at;ui.force=1;
}
static void ko_section_delete(void)
{
    if(transport_busy()) {ui_message("STOP TO ARRANGE");return;}
    if(!chain_config.count) return;
    uint32_t at=ko.view==8 ? chain_config.count-1u : ko_section;
    if(at>=chain_config.count) return;
    for(uint32_t i=at;i+1u<chain_config.count;i++) chain_config.row[i]=chain_config.row[i+1u];
    chain_config.count--;if(ko_section>=chain_config.count) ko_section=chain_config.count?chain_config.count-1u:0;
    ui.force=1;
}
static void ko_arrange_turn(uint32_t k,int32_t steps)
{
    if(!steps) return;
    if(ko.view==18) {
        if(transport_busy()) {ui_message("STOP TO ARRANGE");return;}
        if(!k) chain_config.rsv[0]=(uint8_t)(steps>0);
        else if(k==1 && chain_config.count) {
            uint32_t next=(uint32_t)clamp((int32_t)ko_section+steps,0,chain_config.count-1);
            chain_row_t row=chain_config.row[ko_section];
            if(next>ko_section) for(uint32_t i=ko_section;i<next;i++)chain_config.row[i]=chain_config.row[i+1];
            else for(uint32_t i=ko_section;i>next;i--)chain_config.row[i]=chain_config.row[i-1];
            chain_config.row[next]=row;ko_section=(uint8_t)next;
        } else if(k==2 && steps>0 && chain_config.count && chain_config.count<CHAIN_ROWS) {
            chain_row_t row=chain_config.row[ko_section];ko_section_add(row.slot[0]);chain_config.row[ko_section]=row;
        }
        ui.force=1;return;
    }
    if(ko.view==8) {
        if(!k) ko_pattern_request((uint32_t)clamp((int32_t)ko_pattern_sel+steps,0,3));
        else if(k==1) {
            if(transport_busy()) {chain_config.rsv[0]=(uint8_t)(steps>0);ui.force=1;ui_message(steps>0?"AT PATTERN END":"AT NEXT BAR");return;}
            uint32_t slot=(uint32_t)clamp((ko_copy_slot<4?ko_copy_slot:0)+steps,0,3);
            if(project_used(slot) && chain_copy_source(slot)) {ko_clipboard=chain.source[slot];ko_clip_valid=1;ko_copy_slot=(uint8_t)slot;ko_paste_target=255;ui_message("SAVED PATTERN COPIED");}
            else ui_message("EMPTY PATTERN");
        } else if(k==2) {if(steps>0) ko_section_add(ko_pattern_sel);else ko_section_delete();}
        else if(k==3) {if(steps>0) ko_pattern_paste();else ko_pattern_copy();}
    } else {
        if(!k) {ko_section=(uint8_t)clamp((int32_t)ko_section+steps,0,chain_config.count?chain_config.count-1u:0);ui.force=1;return;}
        if(transport_busy()) {ui_message("STOP TO ARRANGE");return;}
        if(k==3) {if(steps>0) ko_section_add(ko_pattern_sel);else ko_section_delete();}
        else if(ko_section<chain_config.count) {
            chain_row_t *r=&chain_config.row[ko_section];
            if(k==1) {uint32_t track=song.sel<SAMPLER_PARTS?song.sel:0;r->slot[track]=(uint8_t)clamp((int32_t)r->slot[track]+steps,0,CHAIN_SILENT);if(r->slot[track]<4)ko_pattern_sel=r->slot[track];}
            else if(k==2) r->repeat=(uint8_t)clamp((int32_t)r->repeat+steps,1,16);
        }
    }
    ui.force=1;
}
static void ko_pattern_keys(uint32_t notes)
{
    for(uint32_t key=0;key<27;key++) if((notes>>key)&1u) {
        int32_t pad=ko_pad(key);if(pad<0)continue;
#if FELUCCA_SAMPLER_ONLY
        if(ko.view==9 && chain.running && !ko_launch_mode){if((uint32_t)pad<chain.config.count){ko_scene_pending=ko_scene_pending==pad?255:(uint8_t)pad;ui.force=1;}continue;}
#endif
        if(pad>3)continue;
        if(ko.view==9)ko_section_add((uint32_t)pad);else ko_pattern_request((uint32_t)pad);
    }
}
#if FELUCCA_SAMPLER_ONLY
static void ko_scene_capture(void){
 if(transport_busy() || ko_section>=chain_config.count){ui_message("STOP / SELECT A SECTION");return;}
 if(chain_config.rsv[1]!=0x4D){for(uint32_t i=0;i<chain_config.count;i++)chain_config.row[i].slot[3]=0;chain_config.rsv[1]=0x4D;}
 uint32_t mask=0;for(uint32_t k=0;k<3;k++)mask|=(trk[k].p[P_MUTE]!=0)<<k;
 chain_config.row[ko_section].slot[3]=(uint8_t)mask;ui.force=1;ui_message("SCENE MUTES CAPTURED");
}
#endif
static int ko_pattern_start(int arrangement)
{
    if(transport_busy()) return 0;
    if(arrangement) {ko_launch_ready=ko_launch_mode=0;ko_launch_pending=255;chain_play_ui();return chain.armed;}
    ko_pattern_preload();
    if(!(ko_pattern_mask&(1u<<ko_pattern_sel))) {ui_message("EMPTY PATTERN");return 0;}
    chain.config.rsv[1]=0;chain.config.count=1;chain.config.row[0]=chain_row_of(ko_pattern_sel,1);
    ko_launch_mode=1;RING_PUBLISH();chain.armed=1;transport_req=1;ui.force=1;return 1;
}
