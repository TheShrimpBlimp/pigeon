/* SPDX-License-Identifier: GPL-3.0-only */
/* Real DSP/UI and NOR hooks: pads, independent edits, one shots, compressors and persistence. */
#include <stdint.h>
#define FELUCCA_KO 1
#define UI_TEST_MAIN baseline_ui_main
#include "ui_test.c"
#define ko_flash host_slots
static uint8_t flash_ok = 1, torn;
static uint32_t erases;
static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (off < SMP_USER_BASE || off - SMP_USER_BASE + n > sizeof ko_flash) return -1;
    memcpy(dst, (uint8_t *)ko_flash + off - SMP_USER_BASE, n); return 0;
}
static int st_erase(uint32_t off)
{
    if (off < SMP_USER_BASE || off - SMP_USER_BASE + 4096u > sizeof ko_flash || (off & 4095u)) return -1;
    memset((uint8_t *)ko_flash + off - SMP_USER_BASE, 255, 4096); erases++; return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    if (torn || off < SMP_USER_BASE || off - SMP_USER_BASE + n > sizeof ko_flash) return -1;
    uint8_t *p = (uint8_t *)ko_flash + off - SMP_USER_BASE; const uint8_t *s = src;
    for (uint32_t i = 0; i < n; i++) { if ((p[i] & s[i]) != s[i]) return -1; p[i] &= s[i]; }
    return 0;
}
#include "../firmware/src/storage.c"
#include "../firmware/src/slice_store.c"
static void fixture(uint32_t slot, int kit)
{
    uint8_t *p = (uint8_t *)ko_flash + slot * SMP_USER_SIZE;
    smp_user_hdr_t *h = (smp_user_hdr_t *)p;
    memset(p, 255, SMP_USER_SIZE); memset(h, 0, sizeof *h);
    h->magic = SMP_USER_MAGIC; h->version = 1; h->nz = kit ? 16 : 1;
    h->data_len = 16384;
    memcpy(p + SMP_USER_DATA, SMP_DATA, h->data_len);
    h->crc = st_crc32(p + SMP_USER_DATA, h->data_len);
    for (uint32_t j = 0; j < h->nz; j++) {
        smp_zone_t *z = &h->zone[j];
        z->off = kit ? j * 1024 : 0; z->n = kit ? 2048 : 32768; z->le = z->n - 1;
        z->rate = 32768; z->root16 = (60 + (kit ? j : 0)) * 16;
        z->lo = kit ? 60 + j : 0; z->hi = kit ? 60 + j : 127;
    }
    smp_user_scan(slot);
}
static void render_blocks(uint32_t blocks)
{
    int32_t out[CTL * 2];
    for (uint32_t j = 0; j < blocks; j++) mix_block(out, CTL);
}
int main(void)
{
    int bad = 0; ui_power_on(); memset(&ko, 0, sizeof ko); memset(ko_flash, 255, sizeof ko_flash);
    slc_store_boot(); fixture(0, 0); fixture(1, 1);
    step_t saved[NSTEP]; memcpy(saved, trk[0].step, sizeof saved);
    menu_put(MI_KO, 1); frame();
    bad += check("KO opt in does not change sounds until explicit setup", trk[0].eng_req == 0 && !ko_ready());
    press(B_OCTUP); render_blocks(16);
    bad += check("setup selects song USR1 and kit USR2 and preserves steps", ko_ready() &&
                 trk[1].p[P_E0] == SMP_NSETS + 1 && !memcmp(saved, trk[0].step, sizeof saved));
    bad += check("automatic chopping has a first attack at zero", ko_table()->n > 0 && ko_table()->chop[0].a == 0);
    ko_table_t tab = {0}; tab.n = 16;
    for (uint32_t j = 0; j < tab.n; j++) { tab.chop[j].a = j * 2048; tab.chop[j].b = (j + 1) * 2048; }
    bad += check("manual table with independent ends restores", ko_restore(&tab));
    int mapping = 1;
    for (uint32_t k = 0; k < 27; k++) {
        int32_t pad = ko_pad(k);
        uint32_t note = pad < 0 ? KB_SILENT : 60 + pad;
        mapping &= kb_map(&trk[0], k) == note && kb_map(&trk[1], k) == note;
    }
    bad += check("both parts map sixteen white keys and silence black keys", mapping);
    static const uint8_t white[16] = {0,2,4,6,7,9,11,12,14,16,18,19,21,23,24,26};
    for (uint32_t j = 0; j < 16; j++) mapping &= ko_pad(white[j]) == (int32_t)j;
    bad += check("physical B keys trigger chops four and eleven; A sharp stays silent",
                 mapping && ko_pad(6) == 3 && ko_pad(18) == 10 && ko_pad(5) == -1 && ko_pad(17) == -1);
    ko_select(1); uint32_t prior_end = ko_table()->chop[0].b;
    ko_chop_turn(0, 1); ko_chop_turn(1, 1); ko_pitch_turn(12);
    bad += check("independent start/end and per-chop pitch keep neighbor intact", ko_table()->chop[0].b == prior_end &&
                 ko_table()->chop[1].a > 2048 && ko_table()->chop[1].b > 4096 && ko_table()->chop[1].pitch == 12);
    uint32_t a, b, st; slc_bounds(slc_get(1), SLC_DIV_MAN, 1, &a, &b, &st);
    bad += check("edited start has an exact ADPCM checkpoint", st == slc_state_at(slc_get(1), a));
    uint32_t before = ko_table()->n;
    bad += check("split then join preserve chop count and validity", ko_split() && ko_join() && ko_table()->n == before);
    tab = *ko_table(); tab.chop[0].b = slc_get(1)->len + 1;
    bad += check("malformed out-of-bounds metadata is refused", !ko_restore(&tab));
    song.sel = 0; go_home(); host_enc[panel.enc[EN_K4]] = 1; frame();
    bad += check("HOME K4 zooms without changing pitch", ko.zoom == 1 && ko_table()->chop[ko.selected].pitch == 12);
    uint32_t selection = ko.selected;
    ko_move_cursor(100);
    bad += check("K3 scrolls without changing selected chop", ko.selected == selection && ko.cursor > ko_table()->chop[selection].b);
    press(B_OCTDN);
    bad += check("OCT changes selected chop pitch", ko_table()->chop[selection].pitch == 11);
    press(B_EDIT);
    bad += check("EDIT opens per-chop loop and latch controls", ko.view == 5);
    ko_select(0); trk_all_off(&trk[0]); render_blocks(200);
    trk_note_on(&trk[0], 60, 100); render_blocks(300);
    voice_t *held = 0;
    for (uint32_t j = 0; j < NVOICE; j++) if (trk[0].v[j].active && trk[0].v[j].note == 60) held = &trk[0].v[j];
    bad += check("default held loop survives several chop lengths", held && held->gate && !(ko_table()->chop[0].rsv & KO_NO_LOOP));
    trk_note_off(&trk[0], 60); render_blocks(500);
    bad += check("held loop releases when key is released", held && !held->active);
    ko_flag_turn(KO_LATCH, 1);
    trk_note_on(&trk[0], 60, 100); render_blocks(2); trk_note_off(&trk[0], 60); render_blocks(300);
    held = 0;
    for (uint32_t j = 0; j < NVOICE; j++) if (trk[0].v[j].active && trk[0].v[j].note == 60) held = &trk[0].v[j];
    bad += check("latch ignores key release", held && held->gate);
    trk_note_on(&trk[0], 60, 100); render_blocks(500);
    bad += check("second press releases latched chop", held && !held->active);
    trk_note_on(&trk[0], 60, 100); render_blocks(2); trk_note_off(&trk[0], 60);
    trk_all_off(&trk[0]); render_blocks(500);
    bad += check("all notes off releases latched loops", voices_busy() == 0);
    ko_flag_turn(KO_NO_LOOP, -1); ko_flag_turn(KO_LATCH, -1);
    trk_note_on(&trk[0], 60, 100); render_blocks(2); trk_note_off(&trk[0], 60); render_blocks(2);
    held = 0;
    for (uint32_t j = 0; j < NVOICE; j++) if (trk[0].v[j].active && trk[0].v[j].note == 60) held = &trk[0].v[j];
    bad += check("loop disabled completes one shot after key release", held && held->active);
    render_blocks(300);
    bad += check("loop disabled frees the voice at chop end", held && !held->active);
    trk_note_on(&trk[0], 60, 100); render_blocks(1);
    for (uint32_t j = 0; j < NVOICE; j++) if (trk[0].v[j].active) voice_kill(&trk[0].v[j]);
    render_blocks(2);
    bad += check("song one shot does not revive a stolen voice", voices_busy() == 0);
    ko_flag_turn(KO_NO_LOOP, 1); ko_select(selection);
    press(B_FX); bad += check("FX tap opens filter and send controls", ko.view == 1 && ui.home);
    press(B_ENV); bad += check("ENV opens independent compression", ko.view == 3 && ui.home);
    song.sel = 1; host_enc[panel.enc[EN_K2]] = 3; frame();
    bad += check("kit compressor ratio does not change song compressor", ko.comp[1].ratio == 4 && ko.comp[0].ratio == 1);
    ko.comp[1] = KO_COMP_DEFAULT;
    trk_all_off(&trk[1]); trk_note_on(&trk[1], 75, 100); render_blocks(2);
    voice_t *hit = 0;
    for (uint32_t j = 0; j < NVOICE; j++) if (trk[1].v[j].active && trk[1].v[j].note == 75) hit = &trk[1].v[j];
    trk_note_off(&trk[1], 75); render_blocks(2);
    bad += check("pad sixteen plays its own zone after key release", hit && hit->active && ((uint32_t)hit->s[4] & 31u) == 15);
    render_blocks(200);
    bad += check("one shot ends and frees the voice", hit && !hit->active);
    trk_note_on(&trk[1], 60, 100); render_blocks(1);
    for (uint32_t j = 0; j < NVOICE; j++) if (trk[1].v[j].active) voice_kill(&trk[1].v[j]);
    render_blocks(2);
    bad += check("compressor/one-shot amplitude cannot revive a stolen voice", voices_busy() == 0);
    int32_t dry[CTL], wet[CTL];
    for (uint32_t j = 0; j < CTL; j++) dry[j] = wet[j] = j & 1 ? -24000 : 24000;
    ko_compress(&trk[0], wet, 0, CTL);
    bad += check("compression bypass is bit identical", !memcmp(dry, wet, sizeof dry));
    ko.comp[0].ratio = 8; ko.comp[0].threshold = 24;
    for (uint32_t j = 0; j < 200; j++) { memcpy(wet, dry, sizeof dry); ko_compress(&trk[0], wet, 0, CTL); }
    bad += check("compression reduces sustained peaks", wet[0] < 8000 && wet[0] > 0);
    int32_t independent = ko_cr[1].gain;
    ko_compress(&trk[0], wet, 0, CTL);
    bad += check("song processing does not change kit gain state", ko_cr[1].gain == independent);
    int32_t side[CTL];
    for (uint32_t j = 0; j < CTL; j++) { wet[j] = 0; side[j] = dry[j]; }
    ko_compress(&trk[0], wet, side, CTL);
    bad += check("SPREAD side-only energy is compressed with linked gain", wet[0] == 0 && side[0] > 0 && side[0] < dry[0]);
    ko_select(1); ko_flag_turn(KO_LATCH, 1);
    ko.dirty = 1; ko.revision++; song.playing = 1; uint32_t e = erases;
    ko_store_poll(); fm1_ms += 2000; ko_store_poll();
    bad += check("metadata never writes while playing", erases == e && ko.dirty);
    song.playing = 0; ko_store_poll();
    bad += check("stopped metadata save commits and clears pending flag", erases > e && !ko.dirty);
    tab = *ko_table(); ko_auto(); ko.comp[0] = KO_COMP_DEFAULT; ko_store_load();
    bad += check("metadata restores ends/pitch and compressor", ko_table()->n == tab.n &&
                 ko_table()->chop[1].pitch == tab.chop[1].pitch &&
                 ko_table()->chop[1].rsv == tab.chop[1].rsv && ko.comp[0].ratio == 8);
    slc_man_t *m = slc_man_begin(0); m->n = 1; m->pos[0] = 0; m->st[0] = slc_state_at(slc_get(1), 0);
    slc_man_commit(0); (void)slc_store_write(0); ko_auto(); ko_store_load();
    bad += check("ordinary slice saves preserve KO record", ko_table()->n == tab.n);
    uint32_t crc = ko.song_crc; ko.song_crc ^= 1; ko_auto(); ko_store_load();
    bad += check("sample CRC mismatch refuses old chop metadata", ko_table()->n != tab.n); ko.song_crc = crc;
    torn = 1; bad += check("torn metadata write is reported", ko_store_sector(1) == 2); torn = 0;
    ko_auto(); ko.comp[0] = KO_COMP_DEFAULT; ko_store_load();
    bad += check("torn record falls back to automatic chops without damaging audio", ko_table()->n > 0 && usr_nz[0]);
    project_t project; project_store_t wire; project_capture(&project);
    bad += check("KO references use unchanged FUNA serialization", proj_pack(&wire, &project) &&
                 sizeof wire == 3840 && proj_import(&proj_scratch, &wire, sizeof wire));
    fixture(2, 0);
    smp_user_hdr_t *first = (smp_user_hdr_t *)ko_flash;
    smp_user_hdr_t *second = (smp_user_hdr_t *)((uint8_t *)ko_flash + 2 * SMP_USER_SIZE);
    first->rsv = 0x4B; second->rsv = 0x4C;
    first->rsv2[0] = second->rsv2[0] = SMP_KO_PAIR;
    first->rsv2[1] = second->rsv2[1] = 123;
    smp_user_scan(0); smp_user_scan(2); ko_prepare();
    bad += check("paired song combines USR1 and USR3 without changing kit", slc_get(1) &&
                 slc_get(1)->len == 65536 && slc_get(1)->nseg == 2 && usr_nz[1] == 16);
    slc_dec_t d, reference;
    slc_dec_at(&d, 32768, slc_state_at(slc_get(1), 32768)); slc_dec_at(&reference, 0, 0);
    int same = 1;
    for (uint32_t j = 0; j < 64; j++) same &= slc_dec_next(slc_get(1), &d) == slc_dec_next(slc_get(3), &reference);
    bad += check("paired boundary restores second chunk decoder state", same);
    second->rsv2[1]++;
    smp_user_scan(2); ko_prepare();
    bad += check("mismatched continuation silences song and drops old chops", !slc_get(1) && !ko_table()->n);
    first->rsv = 0; smp_user_scan(0); ko_prepare();
    const char *screens = getenv("KO_SCREEN_DIR");
    if (screens) {
        (void)ko_restore(&tab); ko_select(1); song.sel = 0; ko.view = 0; go_home(); screen_save(screens, "song");
        song.sel = 1; ko.view = 0; screen_save(screens, "kit");
        ko.view = 1; screen_save(screens, "fx"); ko.view = 2; screen_save(screens, "fx2");
        ko.view = 3; screen_save(screens, "compression"); ko.view = 4; screen_save(screens, "makeup");
    }
    menu_put(MI_KO, 0); frame();
    bad += check("KO can be disabled without changing project parameters", !ko.on && trk[1].p[P_E7] == 1);
    printf("KO: %s\n", bad ? "FAILED" : "PASSED"); return bad != 0;
}
