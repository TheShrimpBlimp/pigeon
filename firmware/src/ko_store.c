/* SPDX-License-Identifier: GPL-3.0-only */
/* An optional record at +256 in USR1's reserved last sector. SLM1 at +0 stays readable.
 * One copy, as SLM1: a torn write loses edits, never sample audio. Decoder states are rebuilt.
 * Slot CRCs bind each compressor to its audio; projects continue using FUNA unchanged. */
#define KO_REC_MAGIC 0x314F4B46u /* FKO1 */
#define KO_REC_OFF 256u
#define KO_SECOND_REC_OFF 1024u
typedef struct {
    uint32_t magic, song_crc, kit_crc;
    ko_table_t table;
    ko_comp_t comp[2];
    uint8_t rsv[2];
    uint32_t crc;
} ko_record_t;
_Static_assert(KO_SECOND_REC_OFF + sizeof(ko_record_t) <= 4096u, "KO metadata sector");
#define KO_KIT_REC_OFF 1792u
typedef struct {uint32_t magic,kit_crc;ko_pad_params_t pad[16];uint32_t crc;} ko_kit_record_t;
_Static_assert(KO_KIT_REC_OFF+sizeof(ko_kit_record_t)<=4096,"kit record fits");
static int ko_kit_record_ok(const ko_kit_record_t *r) {
    if(r->magic!=0x3150444bu || r->crc!=st_crc32(r,sizeof *r-4)) return 0;
    for(uint32_t i=0;i<16;i++)if(r->pad[i].pitch<-24 || r->pad[i].pitch>24 || r->pad[i].level>127 || r->pad[i].decay>127 || r->pad[i].choke>4)return 0;
    return 1;
}
static int ko_record_ok(const ko_record_t *r)
{
    return r->magic == KO_REC_MAGIC && r->crc == st_crc32(r, sizeof *r - 4u);
}
static void ko_store_load(void)
{
    if (!slc_rec_room(0)) return;
    const ko_record_t *r = (const ko_record_t *)(smp_user_xip(0) + SLC_REC_OFF + (ko_id()?KO_SECOND_REC_OFF:KO_REC_OFF));
    if(!ko_id()) {const ko_kit_record_t *kit=(const ko_kit_record_t *)(smp_user_xip(0)+SLC_REC_OFF+KO_KIT_REC_OFF);
        if(ko_kit_record_ok(kit) && kit->kit_crc==ko.kit_crc) memcpy(ko_kit_params,kit->pad,sizeof ko_kit_params);}
    if (!ko_record_ok(r)) return;
    if (r->song_crc == ko.song_crc) {
        (void)ko_restore(&r->table);
        if(r->rsv[0]<=1) ko_seq[ko_id()?2:0].end_mode=r->rsv[0];
        if (ko_comp_valid(&r->comp[0])) ko.comp[0] = r->comp[0];
    }
    if (r->kit_crc == ko.kit_crc && ko_comp_valid(&r->comp[1])) {ko.comp[1] = r->comp[1];if(!ko_id() && r->rsv[1]<=1) ko_seq[1].end_mode=r->rsv[1];}
}
/* Also called for ordinary SLM1 edits: preserve KO bytes through the shared-sector erase. */
static int ko_store_sector(int update)
{
    slc_rec_t slices;
    ko_record_t records[1+FELUCCA_KO_DUAL];
    uint32_t off=SMP_USER_BASE+SLC_REC_OFF, context=ko_context, changed=0;
    if (!slc_rec_room(0)) return 1;
    slc_rec_make(0,&slices);memset(records,0,sizeof records);
    changed=memcmp(&slices,smp_user_xip(0)+SLC_REC_OFF,sizeof slices)!=0;
    for (uint32_t i=0;i<1u+FELUCCA_KO_DUAL;i++) {
        ko_context=i;
        ko_record_t *r=&records[i];
        uint32_t at=i?KO_SECOND_REC_OFF:KO_REC_OFF;
        const ko_record_t *old=(const ko_record_t *)(smp_user_xip(0)+SLC_REC_OFF+at);
        if (update) {
            r->magic=KO_REC_MAGIC;r->song_crc=ko.song_crc;r->kit_crc=ko.kit_crc;
            r->table=*ko_table();
            for (uint32_t j=0;j<KO_CHOPS;j++) r->table.chop[j].st=0;
            r->comp[0]=ko.comp[0];r->comp[1]=ko.comp[1];r->rsv[0]=ko_seq[i?2:0].end_mode;r->rsv[1]=i?0:ko_seq[1].end_mode;r->crc=st_crc32(r,sizeof *r-4u);
        } else if (ko_record_ok(old)) *r=*old;
        changed |= r->magic && memcmp(r,old,sizeof *r);
    }
    ko_context=context;
    ko_kit_record_t kit;memset(&kit,0,sizeof kit);
    const ko_kit_record_t *oldkit=(const ko_kit_record_t *)(smp_user_xip(0)+SLC_REC_OFF+KO_KIT_REC_OFF);
    if(update) {kit.magic=0x3150444bu;kit.kit_crc=ko_states[0].kit_crc;memcpy(kit.pad,ko_kit_params,sizeof kit.pad);kit.crc=st_crc32(&kit,sizeof kit-4);}
    else if(ko_kit_record_ok(oldkit)) kit=*oldkit;
    changed|=kit.magic && memcmp(&kit,oldkit,sizeof kit);
    if (!changed) return 0;
    if (st_erase(off) || st_prog(off,&slices,sizeof slices)) return 2;
    for (uint32_t i=0;i<1u+FELUCCA_KO_DUAL;i++)
        if (records[i].magic && st_prog(off+(i?KO_SECOND_REC_OFF:KO_REC_OFF),&records[i],sizeof records[i])) return 2;
    if(kit.magic && st_prog(off+KO_KIT_REC_OFF,&kit,sizeof kit))return 2;
    return 3;
}
static void ko_store_poll(void)
{
    static uint32_t seen, at;
    uint32_t revision=ko_states[0].revision, dirty=ko_states[0].dirty;
    if (FELUCCA_KO_DUAL) { revision+=ko_states[1].revision;dirty|=ko_states[1].dirty; }
    if (seen != revision) { seen = revision; at = fm1_ms; }
    if (!dirty || !flash_ok || transport_busy() || voices_busy() || fm1_ms - at < 1000u) return;
    int rc = ko_store_sector(1);
    if (rc == 2) { at = fm1_ms; ui_message("KO SAVE ERROR"); return; } /* retry */
    for (uint32_t i=0;i<1u+FELUCCA_KO_DUAL;i++) ko_states[i].dirty=0;
    ui_message(rc == 1 ? "KO EDITS NOT SAVED" : "KO SAVED");
}
