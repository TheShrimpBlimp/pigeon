/* SPDX-License-Identifier: GPL-3.0-only */
/* HOME is the KO editor; standard SEQ, SAVE, layers and menu remain available. */
static int ko_home(void) { return ko.on && ui.home && song.sel < 2u && !ui.menu && !ui.confirm && !name_on() && !ui.layer; }
static int ko_ready(void) { return ko_song(&trk[0]) && ko_kit(&trk[1]); }
static void ko_setup(void)
{
    if (transport_busy()) { ui_message("STOP TO SET UP KO"); return; }
    set_engine_of(&trk[0], 13);
    trk[0].p[P_E0] = 1; trk[0].p[P_E1] = SLC_DIV_MAN;
    trk[0].p[P_E2] = trk[0].p[P_E3] = 0;
    trk[0].p[P_E4] = SLC_ONE; trk[0].p[P_E6] = trk[0].p[P_E7] = 127;
    set_engine_of(&trk[1], 4);
    trk[1].p[P_E0] = SMP_NSETS + 1; trk[1].p[P_E3] = 0; trk[1].p[P_E7] = 1;
    for (uint32_t j = 0; j < 2u; j++) {
        track_t *t = &trk[j];
        t->p[P_ATK] = 0; t->p[P_SUS] = 127; t->p[P_REL] = 30;
        t->p[P_VOICE] = V_POLY; t->p[P_AMODE] = 0; t->p[P_SPRD] = 0;
        t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
        if (seq_is_empty(t)) { t->p[P_SLEN] = 16; t->p[P_SDIV] = 2; }
    }
    ko_prepare(); ko.view = 0; song.octave = 0; ui.force = 1;
    ui_message("KO SOUNDS READY");
}
static void ko_knob(uint32_t k, int32_t s)
{
    if (!ko_ready()) return;
    track_t *t = TSEL;
    if (ko.view == 0) {
        if (!song.sel) ko_chop_turn(k, s);
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
        if (ko.view == 1 && !k && song.sel) id = P_E4;
        const param_desc_t *pd = global ? &GP[id] : track_desc(t, id);
        int16_t *p = global ? &song.g[id] : &t->p[id];
        *p = (int16_t)param_turn(pd, *p, s);
        if (!global) motion_capture(t, id, *p);
    } else if (ko.view == 5 && !song.sel) {
        if (!k) ko_flag_turn(KO_NO_LOOP, s);
        else if (k == 1) ko_flag_turn(KO_LATCH, s);
        else if (k == 2 && s) {
            int ok = s > 0 ? ko_split() : ko_join();
            ui_message(ok ? "CHOPS UPDATED" : "CANNOT EDIT CHOP");
        }
    } else {
        ko_comp_t c = ko.comp[song.sel];
        if (ko.view == 3) {
            if (!k) c.threshold = (uint8_t)clamp(c.threshold + s, 0, 48);
            if (k == 1) c.ratio = (uint8_t)clamp(c.ratio + s, 1, 8);
            if (k == 2) c.attack = (uint8_t)clamp(c.attack + s, 1, 100);
            if (k == 3) c.release = (uint8_t)clamp(c.release + s, 1, 50);
        } else if (!k) c.makeup = (uint8_t)clamp(c.makeup + s, 0, 12);
        ko.comp[song.sel] = c; ko.dirty = 1; ko.revision++;
    }
    ui.force = 1;
}
static void ko_line(uint32_t y, const char *s, uint16_t ink)
{
    draw_text_line(8, y, 224, &AF_S, s, ink, T_BG, 0);
}
static void ko_value(uint32_t row, const char *label, int32_t value)
{
    char b[48], v[16]; str_cpy(b, label, sizeof b); fmt_int(v, value);
    str_cpy(b + str_len(b), v, sizeof b - str_len(b)); ko_line(52 + row * 22, b, T_THEME);
}
static void ko_draw(void)
{
    static uint32_t revision = ~0u, generation = ~0u;
    static uint8_t part = 255, view = 255;
    draw_head();
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg); ui.msg2[0] = 0; ui.msg_t = 60;
    }
    if (ui.hot_t) ui.hot_t--;
    if (ui.bpm_t) ui.bpm_t--;
    if (!ui.force && revision == ko.revision && generation == smp_user_gen && part == song.sel && view == ko.view) return;
    lcd_fill(0, 26, 240, 214, T_BG);
    ko_line(29, song.sel ? "KO / DRUM KIT" : "KO / SONG CHOPS", T_THEME);
    if (!ko_ready()) {
        ko_line(65, "OCT+ SET UP T1 + T2 SOUNDS", T_THEME);
        ko_line(92, "PATTERNS ARE KEPT", T_MID);
        ko_line(119, "USR1 SONG / USR2 KIT", T_MID);
    } else if (ko.view == 0 && !song.sel) {
        const slc_src_t *s = slc_get(1);
        const ko_table_t *tab = ko_table();
        if (!s || !tab->n) ko_line(66, "UPLOAD SONG TO USR1", T_MID);
        else {
            const ko_chop_t *c = &tab->chop[ko.selected];
            uint32_t span = s->len >> ko.zoom;
            if (span < SP_COLS) span = SP_COLS;
            if (span > s->len) span = s->len;
            uint32_t a = ko.cursor > span / 2 ? ko.cursor - span / 2 : 0;
            if (a > s->len - span) a = s->len - span;
            uint32_t b = a + span;
            ko_line(52, "K1 START / K2 END", T_MID);
            ko_line(74, "K3 SCROLL / K4 ZOOM", T_MID);
            slice_env(a, b);
            for (uint32_t x = 0; x < SP_COLS; x++) {
                int32_t hi = 132 - sp.hi[x] / 2, lo = 132 - sp.lo[x] / 2;
                lcd_fill(12 + x, (uint32_t)hi, 1, (uint32_t)(lo - hi + 1), T_MID);
            }
            if (c->a >= a && c->a <= b)
                lcd_fill(12 + (c->a - a) * (SP_COLS - 1) / span, 98, 1, 69, T_THEME);
            if (c->b >= a && c->b <= b)
                lcd_fill(12 + (c->b - a) * (SP_COLS - 1) / span, 98, 1, 69, T_THEME);
            lcd_fill(12 + (ko.cursor - a) * (SP_COLS - 1) / span, 98, 1, 69, T_ACCENT);
            ko_value(6, "OCT PITCH semis ", c->pitch);
            char title[40], number[12]; str_cpy(title, "KO / SONG CHOP ", sizeof title);
            fmt_int(number, ko.selected + 1u); str_cpy(title + str_len(title), number, sizeof title - str_len(title));
            ko_line(29, title, T_THEME);
        }
        ko_line(207, "EDIT LOOP / LATCH / SPLIT", T_MID);
    } else if (ko.view == 0) {
        ko_value(0, "K1 LEVEL ", TSEL->p[P_LEVEL]); ko_value(1, "K2 KIT PITCH ", TSEL->p[P_E1]);
        ko_value(2, "K3 CRUSH ", TSEL->p[P_E2]); ko_value(3, "K4 TONE ", TSEL->p[P_E4]);
        ko_value(5, "LOADED PADS ", usr_nz[1]); ko_line(202, "16 WHITE KEYS / ONE SHOTS", T_MID);
    } else if (ko.view <= 2) {
        if (ko.view == 1) {
            ko_value(0, "K1 FILTER ", TSEL->p[song.sel ? P_E4 : P_E7]);
            ko_value(1, "K2 DELAY ", TSEL->p[P_DLY]); ko_value(2, "K3 CHORUS ", TSEL->p[P_CHOR]);
            ko_value(3, "K4 REVERB ", TSEL->p[P_REV]);
        } else {
            ko_value(0, "K1 DISTORTION ", TSEL->p[P_DIST]);
            ko_line(74, "K2 DELAY TIME (SHARED)", T_MID);
            ko_line(95, N_DTIME[song.g[G_DTIME]], T_THEME);
            ko_value(3, "K3 FEEDBACK ", song.g[G_DFDBK]); ko_value(4, "K4 LEVEL ", TSEL->p[P_LEVEL]);
        }
        ko_line(194, "FX NEXT / ENV COMPRESSOR", T_MID); ko_line(215, "EDIT BACK TO SAMPLE", T_MID);
    } else if (ko.view == 5 && !song.sel) {
        const ko_table_t *tab = ko_table();
        if (tab->n) {
            const ko_chop_t *c = &tab->chop[ko.selected];
            ko_value(0, "CHOP ", ko.selected + 1);
            ko_line(74, c->rsv & KO_NO_LOOP ? "K1 HELD LOOP OFF" : "K1 HELD LOOP ON", T_THEME);
            ko_line(96, c->rsv & KO_LATCH ? "K2 LATCH ON" : "K2 LATCH OFF", T_THEME);
            ko_line(118, "K3 SPLIT+ / JOIN-", T_MID);
            ko_value(4, "OCT PITCH semis ", c->pitch);
        } else ko_line(66, "UPLOAD SONG TO USR1", T_MID);
        ko_line(207, "EDIT BACK TO WAVEFORM", T_MID);
    } else {
        ko_comp_t c = ko.comp[song.sel];
        if (ko.view == 3) {
            ko_value(0, "K1 THRESHOLD dB ", -(int32_t)c.threshold); ko_value(1, "K2 RATIO x:1 ", c.ratio);
            ko_value(2, "K3 ATTACK ms ", c.attack); ko_value(3, "K4 RELEASE ms ", c.release * 10);
        } else { ko_value(0, "K1 MAKEUP dB ", c.makeup); ko_line(90, "RATIO 1 / MAKEUP 0 = BYPASS", T_MID); }
        ko_line(170, "SEPARATE SONG / KIT COMP", T_MID);
        ko_line(194, "LFO MAKEUP / ENV DYNAMICS", T_MID); ko_line(215, "EDIT BACK TO SAMPLE", T_MID);
    }
    revision = ko.revision; generation = smp_user_gen; part = song.sel; view = ko.view; ui.force = 0;
}
