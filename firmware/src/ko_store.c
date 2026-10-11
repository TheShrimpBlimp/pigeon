/* SPDX-License-Identifier: GPL-3.0-only */
/* An optional record at +256 in USR1's reserved last sector. SLM1 at +0 stays readable.
 * One copy, as SLM1: a torn write loses edits, never sample audio. Decoder states are rebuilt.
 * Slot CRCs bind each compressor to its audio; projects continue using FUNA unchanged. */
#define KO_REC_MAGIC 0x314F4B46u /* FKO1 */
#define KO_REC_OFF 256u
typedef struct {
    uint32_t magic, song_crc, kit_crc;
    ko_table_t table;
    ko_comp_t comp[2];
    uint8_t rsv[2];
    uint32_t crc;
} ko_record_t;
_Static_assert(KO_REC_OFF + sizeof(ko_record_t) <= 4096u, "KO metadata sector");
static int ko_record_ok(const ko_record_t *r)
{
    return r->magic == KO_REC_MAGIC && r->crc == st_crc32(r, sizeof *r - 4u);
}
static void ko_store_load(void)
{
    if (!slc_rec_room(0)) return;
    const ko_record_t *r = (const ko_record_t *)(smp_user_xip(0) + SLC_REC_OFF + KO_REC_OFF);
    if (!ko_record_ok(r)) return;
    if (r->song_crc == ko.song_crc) {
        (void)ko_restore(&r->table);
        if (ko_comp_valid(&r->comp[0])) ko.comp[0] = r->comp[0];
    }
    if (r->kit_crc == ko.kit_crc && ko_comp_valid(&r->comp[1])) ko.comp[1] = r->comp[1];
}
/* Also called for ordinary SLM1 edits: preserve KO bytes through the shared-sector erase. */
static int ko_store_sector(int update)
{
    slc_rec_t slices;
    ko_record_t r;
    uint32_t off = SMP_USER_BASE + SLC_REC_OFF;
    if (!slc_rec_room(0)) return 1;
    slc_rec_make(0, &slices);
    memset(&r, 0, sizeof r);
    if (update) {
        r.magic = KO_REC_MAGIC; r.song_crc = ko.song_crc; r.kit_crc = ko.kit_crc;
        r.table = *ko_table();
        for (uint32_t j = 0; j < KO_CHOPS; j++) r.table.chop[j].st = 0;
        r.comp[0] = ko.comp[0]; r.comp[1] = ko.comp[1];
        r.crc = st_crc32(&r, sizeof r - 4u);
    } else {
        const ko_record_t *old = (const ko_record_t *)(smp_user_xip(0) + SLC_REC_OFF + KO_REC_OFF);
        if (ko_record_ok(old)) r = *old;
    }
    if (!memcmp(&slices, smp_user_xip(0) + SLC_REC_OFF, sizeof slices) &&
        (!r.magic || !memcmp(&r, smp_user_xip(0) + SLC_REC_OFF + KO_REC_OFF, sizeof r))) return 0;
    if (st_erase(off) || st_prog(off, &slices, sizeof slices) ||
        (r.magic && st_prog(off + KO_REC_OFF, &r, sizeof r))) return 2;
    return 3;
}
static void ko_store_poll(void)
{
    static uint32_t seen, at;
    if (seen != ko.revision) { seen = ko.revision; at = fm1_ms; }
    if (!ko.dirty || !flash_ok || transport_busy() || voices_busy() || fm1_ms - at < 1000u) return;
    int rc = ko_store_sector(1);
    if (rc == 2) { at = fm1_ms; ui_message("KO SAVE ERROR"); return; } /* retry */
    ko.dirty = 0;
    ui_message(rc == 1 ? "KO EDITS NOT SAVED" : "KO SAVED");
}
