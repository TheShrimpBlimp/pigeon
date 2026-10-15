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
#ifndef KO_TEST_MAIN
#define KO_TEST_MAIN main
#endif
int KO_TEST_MAIN(void)
{
    int bad = 0; ui_power_on(); memset(&ko, 0, sizeof ko); memset(ko_flash, 255, sizeof ko_flash);
#if FELUCCA_SAMPLER_ONLY
    bad += check("sampler profile identifies itself as PIGEON",
                 !strcmp(FW_DISPLAY_NAME, "PIGEON") && !strcmp(SPLASH_LINES[0], "PIGEON"));
#else
    bad += check("upstream compatibility profile is not branded PIGEON",!strcmp(FW_DISPLAY_NAME,"Felucca"));
#endif
#if BRAND_SPLASH_AVAILABLE && FELUCCA_SAMPLER_ONLY
    draw_splash();
    int photo_ok = BRAND_SPLASH_W == 240 && BRAND_SPLASH_H == 240;
    for (uint32_t y = 0; y < BRAND_SPLASH_H; y++) {
        uint16_t row[BRAND_SPLASH_W];brand_splash_row(y,row);
        for (uint32_t x = 0; x < BRAND_SPLASH_W; x++)
            if ((y < 24 || y >= 123 || x >= 168) && !(x >= 20 && x < 64 && y >= 211 && y < 224)) photo_ok &= host_screen[y*240u+(240u-BRAND_SPLASH_W)/2u+x] ==
                        row[x];
    }
    bad += check("logo splash preserves artwork outside the text area and LCD byte order", photo_ok);
#endif
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
    ko_select(2);ko_chop_turn(0,-1000);
    bad+=check("chop start can move before previous pad start",ko_table()->chop[2].a==0 && ko_table()->chop[1].a>0);
    ko_restore(&tab);ko_select(1);ko_chop_turn(1,1000);ko_chop_turn(0,1000);
    bad+=check("chop start can cross later pads but stays before own end",ko_table()->chop[1].a>ko_table()->chop[2].a && ko_table()->chop[1].a==ko_table()->chop[1].b-SLC_MIN);
    ko_table_t free_chops=*ko_table();
    bad+=check("independent edit does not move neighboring pads",!memcmp(&free_chops.chop[0],&tab.chop[0],8) && !memcmp(&free_chops.chop[2],&tab.chop[2],8));
    bad+=check("out-of-order chop positions restore without reassigning pads",ko_restore(&free_chops) && ko_table()->chop[1].a==free_chops.chop[1].a);
    bad+=check("independent chop positions save in existing metadata record",ko_store_sector(1)==3);
    ko_auto();ko_store_load();bad+=check("independent chop positions reload exactly",!memcmp(&free_chops,ko_table(),sizeof free_chops));
    free_chops.chop[1].a=10000;free_chops.chop[1].b=20000;ko_restore(&free_chops);ko_select(1);ko.cursor=15000;
    bad+=check("split works beyond the next pad start",ko_split() && ko_table()->chop[2].a==15000 && ko_table()->chop[3].a==tab.chop[2].a);
    bad+=check("joining split chops restores their full region",ko_join() && ko_table()->chop[1].a==10000 && ko_table()->chop[1].b==20000);
    ko_select(2);bad+=check("join includes an earlier-positioned adjacent pad",ko_join() && ko_table()->chop[1].a==tab.chop[2].a && ko_table()->chop[1].b==20000 && ko_table()->chop[1].st==slc_state_at(slc_get(1),tab.chop[2].a));
    uint32_t bound_a,bound_b,bound_st;slc_bounds(slc_get(1),SLC_DIV_MAN,1,&bound_a,&bound_b,&bound_st);
    bad+=check("playback reads independent bounds by pad index",bound_a==tab.chop[2].a && bound_b==20000);
    ko_restore(&tab);
    ko_select(1); ko_table_t undo_before=*ko_table();
    ko_pitch_turn(1); ko_table_t undo_after=*ko_table();
    bad += check("chop undo restores full table and redo republishes edit",ko_undo() && !memcmp(ko_table(),&undo_before,sizeof undo_before) && ko_undo() && !memcmp(ko_table(),&undo_after,sizeof undo_after));
    ko_pitch_turn(-1);
    trk[0].p[P_MUTE]=0;trk[1].p[P_MUTE]=1;
    ko_performance_turn(3,1);
    bad += check("solo excludes other tracks without overwriting stored mutes",ko_solo==0 && ko_muted(&trk[1]) && !ko_muted(&trk[0]) && trk[1].p[P_MUTE]==1);
    ko_performance_turn(3,-1);
    bad += check("solo toggle restores prior mute mix",ko_solo==-1 && !ko_muted(&trk[0]) && ko_muted(&trk[1]));
    ko_performance_turn(1,-1);
    bad += check("performance page unmutes kit",!trk[1].p[P_MUTE]);
    ko_performance_turn(0,1); uint32_t live=0;
    trk_note_on(&trk[0],60,100); for(uint32_t i=0;i<NVOICE;i++) live+=trk[0].v[i].active!=0;
    bad += check("muted track refuses new voices",ko_muted(&trk[0]) && !live);
    ko_performance_turn(0,-1);
    ko_wave_a=0;ko_wave_span=32768;ko_play_x=-1;ko.view=0;
    voice_t play_saved=trk[0].v[0];
    trk[0].v[0].active=1;trk[0].v[0].s[6]=1;trk[0].v[0].ph[0]=8192;
    uint16_t waveform_before[240*72];memcpy(waveform_before,host_screen+98*240,sizeof waveform_before);
    ko_playhead();int first_x=ko_play_x;trk[0].v[0].ph[0]=16384;ko_playhead();
    bad += check("playhead moves without modifying waveform pixels or voice position",first_x>=12 && ko_play_x>first_x && !memcmp(waveform_before,host_screen+98*240,sizeof waveform_before) && trk[0].v[0].ph[0]==16384);
    trk[0].v[0].active=0;ko_playhead();
    bad += check("playhead disappears when playback stops",ko_play_x==-1);
    trk[0].v[0]=play_saved;
    ko_select(1);ko.zoom=0;ko.cursor=0;uint32_t selected_before=ko.selected;
    ko_move_cursor(1);uint32_t coarse_scroll=ko.cursor;
    ko.zoom=4;ko.cursor=0;ko_move_cursor(1);uint32_t fine_scroll=ko.cursor;
    bad += check("unzoomed scrolling is faster and follows visible zoom span",coarse_scroll==slc_get(1)->len/24 && fine_scroll==ko_wave_span_for(slc_get(1))/24 && coarse_scroll>fine_scroll*10 && ko.selected==selected_before);
    ko.zoom=0;ko_chop_turn(0,1);uint32_t coarse_start=ko_table()->chop[1].a;
    bad += check("start edit moves scroll cursor exactly to updated boundary",ko.cursor==coarse_start);
    ko_undo();ko.zoom=4;ko_chop_turn(0,1);
    bad += check("zoomed boundary movement is finer without consuming scroll selection",ko_table()->chop[1].a-2048<coarse_start-2048 && ko.cursor==ko_table()->chop[1].a && ko.selected==1);
    ko_undo();ko_chop_turn(1,1);
    bad += check("end edit moves cursor to updated end",ko.cursor==ko_table()->chop[1].b);
    ko_undo();ko_select(15);ko.zoom=0;ko_chop_turn(1,1000);
    bad += check("end boundary at sample length is a valid visible scroll position",ko.cursor==slc_get(1)->len && ko_table()->chop[15].b==slc_get(1)->len);
    ko_move_cursor(1000);bad += check("scrolling clamps at exact sample end",ko.cursor==slc_get(1)->len);
    ko_move_cursor(-1000);bad += check("scrolling clamps at sample start",ko.cursor==0);
    (void)ko_restore(&tab);ko.zoom=0;ko_select(0);
    int mapping = 1;
    for (uint32_t k = 0; k < 27; k++) {
        int32_t pad = ko_pad(k);
        uint32_t note = pad < 0 || pad >= 16 ? KB_SILENT : 60 + pad;
        mapping &= kb_map(&trk[0], k) == note && kb_map(&trk[1], k) == note;
    }
    bad += check("legacy sixteen-chop banks and kit retain white-key assignments", mapping);
    static const uint8_t white[16] = {0,2,4,6,7,9,11,12,14,16,18,19,21,23,24,26};
    for (uint32_t j = 0; j < 16; j++) mapping &= ko_pad(white[j]) == (int32_t)j;
    bad += check("white chops four and eleven stay fixed while black keys add chops",
                 mapping && ko_pad(6) == 3 && ko_pad(18) == 10 && ko_pad(5) == 18 && ko_pad(17) == 23);
    ko_select(1);
    uint32_t unchanged_revision=ko.revision;
    ko_select(1);
    bad += check("repeated chop selection avoids redundant waveform redraw",ko.revision==unchanged_revision);
    ko_move_cursor(1);ko_select(1);
    bad += check("retrigger still recenters a scrolled waveform",ko.cursor==ko_table()->chop[1].a);
    uint32_t prior_end = ko_table()->chop[0].b;
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
    uint32_t prefs = ui_prefs, cursor = ko.cursor;
    ui_prefs &= ~PREF_ACCEL; memset(ui.enc_t, 0, sizeof ui.enc_t);
    ko.cursor = 0; fm1_ms = 1000; ko_knob(2, 1);
    uint32_t fine = ko.cursor;
    fm1_ms += 10; ko_knob(2, 1);
    uint32_t previous = ko.cursor;
    fm1_ms += 10; ko_knob(2, 1);
    bad += check("KO fast scrolling accelerates with standard preference off",
                 ko.cursor - previous == fine * 5 && ko.selected == selection);
    previous = ko.cursor; fm1_ms += 10; ko_knob(2, -1);
    bad += check("KO reversing a knob immediately restores fine control", previous - ko.cursor == fine);
    previous = ko.cursor; fm1_ms += 100; ko_knob(2, -1);
    bad += check("KO slow turns retain single-detent precision", previous - ko.cursor == fine);
    ko.cursor = cursor; ui_prefs = prefs; memset(ui.enc_t, 0, sizeof ui.enc_t);
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
#if FELUCCA_SAMPLER_ONLY
    bad += check("sampler profile stays enabled",ko.on&&menu_get(MI_KO)==1);
    uint32_t legacy_track=SAMPLER_PARTS;
    trk[legacy_track].eng_req = 0; trk[legacy_track].preset = 3; trk[legacy_track].p[P_E0] = 71;
    project_capture(&project);
    int preserved = proj_pack(&wire, &project) && proj_import(&proj_scratch, &wire, sizeof wire);
    preserved &= project_restore_runtime(&proj_scratch) == 0;
    bad += check("reserved-track parameters survive project round trip",
                 preserved && trk[legacy_track].eng_req == 0 && trk[legacy_track].preset == 3 && trk[legacy_track].p[P_E0] == 71);

#if FELUCCA_KO_DUAL
    song.sel=2;ko_setup_second();
    bad += check("second song setup preserves first song and kit assignments",ko_song(&trk[2]) && ko_song(&trk[0]) && ko_kit(&trk[1]));
    host_enc[panel.enc[EN_ALGO]]=-1;frame();
    bad += check("track selector reaches drum track from song two",song.sel==1);
    host_enc[panel.enc[EN_ALGO]]=1;frame();
    bad += check("track selector reaches second song",song.sel==2);
    fixture(2,0);ko_prepare();
    ko_auto();ko_select(0);ko_pitch_turn(3); ko_table_t second_edit=*ko_table();
    song.sel=0;ko_table_t first_edit=*ko_table();ko_pitch_turn(1);
    song.sel=2;
    bad += check("song two undo is independent of song one",ko_undo() && ko_table()->chop[0].pitch==0 && ko_undo() && !memcmp(ko_table(),&second_edit,sizeof second_edit));
    song.sel=0;
    bad += check("song one undo retains its own history",ko_undo() && !memcmp(ko_table(),&first_edit,sizeof first_edit));
#endif
    int only=1;
    for(uint32_t j=0;j<NENGINES;j++)only &= eng_ok(j)==(j==4||j==13);
    bad += check("only SAMPLE and SLICE can be selected",only&&NENG_SHOWN==2);
    if (!FELUCCA_KO_DUAL) trk_note_on(&trk[2],60,100);trk_note_on(&trk[3],60,100);
    uint32_t extra=0;
    for(uint32_t p=SAMPLER_PARTS;p<NPART;p++)for(uint32_t j=0;j<NVOICE;j++)extra+=trk[p].v[j].active!=0;
    bad += check("reserved project tracks cannot consume audio voices",!extra&&ENG_MEM_PARTS==SAMPLER_PARTS);
#else
    bad += check("KO can be disabled without changing project parameters", !ko.on && trk[1].p[P_E7] == 1);
#endif
    printf("KO: %s\n", bad ? "FAILED" : "PASSED"); return bad != 0;
}
