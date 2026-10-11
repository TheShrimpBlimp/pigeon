/* SPDX-License-Identifier: GPL-3.0-only */
/* KO metadata belongs to its sample slots, not to projects. No engine/parameter IDs added.
 * Main-loop edits publish an entire table with one byte; audio never reads a partial edit. */
#define KO_PADS 16u
#define KO_CHOPS 32u
/* Reserved metadata bits: zero means LOOP on / LATCH off, including old FKO1 records. */
#define KO_NO_LOOP 1u
#define KO_LATCH 2u
#define KO_VOICE (1u << 23)
#define KO_V_NO_LOOP (1u << 22)
#define KO_V_LATCH (1u << 21)
typedef struct { uint32_t a, b, st; int16_t pitch; uint16_t rsv; } ko_chop_t;
typedef struct { uint32_t n; ko_chop_t chop[KO_CHOPS]; } ko_table_t;
typedef struct { uint8_t threshold, ratio, attack, release, makeup; } ko_comp_t;
static struct {
    ko_table_t table[2];
    volatile uint8_t current, on, bank;
    uint8_t selected, view, dirty, loaded, zoom;
    uint32_t generation, cursor, revision, song_crc, kit_crc;
    ko_comp_t comp[2];
} ko;
static void (*ko_load_hook)(void);
static int ko_song(const track_t *t)
{
    return ko.on && t == &trk[0] && t->eng_req == 13u && t->p[P_E0] == 1 && t->p[P_E1] == 5;
}
static int ko_kit(const track_t *t)
{
    return ko.on && t == &trk[1] && t->eng_req == 4u && t->p[P_E7] == 1;
}
/* White-key index for the physical F3..G5 keyboard. Black keys are silent in pad mode. */
static int32_t ko_pad(uint32_t key)
{
    static const int8_t pad[27] = {0,-1,1,-1,2,-1,3,4,-1,5,-1,6,7,-1,8,-1,9,-1,10,11,-1,12,-1,13,14,-1,15};
    return key < 27u ? pad[key] : -1;
}
