/* SPDX-License-Identifier: GPL-3.0-only */
/* KO metadata belongs to its sample slots, not to projects. No engine/parameter IDs added.
 * Main-loop edits publish an entire table with one byte; audio never reads a partial edit. */
#define KO_PADS 27u
#define KO_CHOPS 32u
/* Reserved metadata bits: zero means LOOP on / LATCH off, including old FKO1 records. */
#define KO_NO_LOOP 1u
#define KO_LATCH 2u
#define KO_ATTACK_SHIFT 2u
#define KO_RELEASE_SHIFT 7u
#define KO_ONE 8192u /* explicit one-shot, independent of stored loop preference */
#define KO_GATE 4096u /* nonlooping hold: release follows the key */
#define KO_CHOKE_SHIFT 14u
#define KO_FLAGS_MASK (KO_NO_LOOP|KO_LATCH|(31u<<KO_ATTACK_SHIFT)|(31u<<KO_RELEASE_SHIFT)|KO_GATE|KO_ONE|(3u<<KO_CHOKE_SHIFT))
#define KO_VOICE (1u << 23)
#define KO_V_NO_LOOP (1u << 22)
#define KO_V_LATCH (1u << 21)
typedef struct { uint32_t a, b, st; int16_t pitch; uint16_t rsv; } ko_chop_t;
typedef struct { uint32_t n; ko_chop_t chop[KO_CHOPS]; } ko_table_t;
typedef struct { uint8_t threshold, ratio, attack, release, makeup; } ko_comp_t;
typedef struct {
    ko_table_t table[2], undo;
    uint8_t undo_valid, undo_selected;
    volatile uint8_t current, on, bank;
    uint8_t selected, view, dirty, loaded, zoom;
    uint32_t generation, cursor, revision, song_crc, kit_crc;
    ko_comp_t comp[2];
} ko_state_t;
static ko_state_t ko_states[1+FELUCCA_KO_DUAL];
static uint32_t ko_context=~0u;
static uint32_t ko_id(void) { return FELUCCA_KO_DUAL && (ko_context<2 ? ko_context : song.sel==2); }
#define ko (ko_states[ko_id()])
static uint32_t ko_src(void) { return ko_id() ? 3u : 1u; }
static ko_state_t *ko_track_state(const track_t *t) { return &ko_states[FELUCCA_KO_DUAL && t==trk+2]; }
static void (*ko_load_hook)(void);
static int ko_song(const track_t *t)
{
    return ko_states[0].on && (t == &trk[0] || (FELUCCA_KO_DUAL && t==trk+2)) && t->eng_req == 13u && t->p[P_E0] == (t==trk+2 ? 3 : 1) && t->p[P_E1] == 5;
}
static int ko_kit(const track_t *t)
{
    return ko_states[0].on && t == &trk[1] && t->eng_req == 4u && t->p[P_E7] == 1;
}
/* Preserve old white-pad note IDs; black keys add chops 17..27. */
static int32_t ko_pad(uint32_t key)
{
    static const int8_t pad[27] = {0,16,1,17,2,18,3,4,19,5,20,6,7,21,8,22,9,23,10,11,24,12,25,13,14,26,15};
    return key < 27u ? pad[key] : -1;
}

/* Solo is a transient performance state; stored mute values remain untouched. */
static volatile int8_t ko_solo = -1;
static int ko_muted(const track_t *t)
{
    return t->p[P_MUTE] || (ko_states[0].on && t < trk+SAMPLER_PARTS &&
                           ko_solo >= 0 && t != trk+ko_solo);
}

/* Pattern notes retain the project ABI; timing preferences use old metadata reserved bytes. */
static struct {
    volatile uint8_t enabled, end_mode, cursor;
    uint8_t pending;
    uint32_t age[NVOICE], baseline[NVOICE];
} ko_seq[SAMPLER_PARTS];
static int ko_seq_on(const track_t *t) { return ko_states[0].on && t>=trk && t<trk+SAMPLER_PARTS && ko_seq[t-trk].enabled && (ko_song(t)||ko_kit(t)); }
static int ko_seq_end(const track_t *t) { return ko_seq_on(t) && ko_seq[t-trk].end_mode; }
static volatile uint8_t ko_seq_firing;

/* Pad settings are bound to the kit CRC in a separate optional metadata record. */
typedef struct { int8_t pitch;uint8_t level,decay,choke; } ko_pad_params_t;
static ko_pad_params_t ko_kit_params[16];
static uint8_t ko_kit_pad;
static uint32_t ko_kit_drop[NVOICE];
static void ko_kit_defaults(void) {memset(ko_kit_params,0,sizeof ko_kit_params);for(uint32_t i=0;i<16;i++)ko_kit_params[i].level=127;}

/* Small optional performance preferences, bound to the corresponding audio CRC. */
static uint8_t ko_quantize[SAMPLER_PARTS],ko_choke[1+FELUCCA_KO_DUAL];
static uint16_t ko_env_gain[1+FELUCCA_KO_DUAL][NVOICE],ko_env_flags[1+FELUCCA_KO_DUAL][NVOICE];
static uint16_t ko_env_attack[1+FELUCCA_KO_DUAL][NVOICE],ko_env_release[1+FELUCCA_KO_DUAL][NVOICE];

/* One alternate layout per song shares the encoded audio and decoder index. */
static ko_table_t ko_alternate[1+FELUCCA_KO_DUAL];
static uint8_t ko_layout[1+FELUCCA_KO_DUAL],ko_roll_enabled,ko_roll_div=2;
static int8_t ko_kit_transpose[NVOICE];
static int8_t ko_voice_transpose[1+FELUCCA_KO_DUAL][NVOICE];

/* Tempo sync uses a captured loop length; chopping never changes the reference. */
typedef struct { uint32_t samples; uint8_t beats,on,whole,rsv; } ko_sync_t;
static ko_sync_t ko_sync[1+FELUCCA_KO_DUAL];
static uint32_t ko_sync_step(uint32_t base,uint32_t native,uint32_t samples,uint32_t beats,uint32_t bpm) {
    if(!native || !samples || !beats)return base;
    uint32_t hz=(samples*bpm+30u*beats)/(60u*beats);
    /* Bound decoder work to half/double speed; pitch controls remain additive. */
    hz=hz<native/2?native/2:hz>native*2?native*2:hz;
    uint32_t ratio=((hz/native)<<16)+((hz%native)<<16)/native;
    return (uint32_t)(((uint64_t)base*ratio)>>16);
}
