/* SPDX-License-Identifier: GPL-3.0-only */
/* One shared decoder, eight double-buffered song voice caches. The audio IRQ
 * only reads published PCM. Cache misses play zero and count underruns; they
 * never invoke the decoder or pause the audio clock. */
#include "ko_mp3.h"
#define KO_PCM_N 576u
#define KO_PCM_TAIL 192u /* loop bridge: enough for a maximum-pitch audio block */
typedef struct { int16_t pcm[NVOICE][KO_PCM_N], tail[NVOICE][KO_PCM_TAIL]; } ko_pcm_first_t;
ENG_MEM_FITS("KO PCM",sizeof(ko_pcm_first_t));
static int16_t ko_pcm_second[NVOICE][KO_PCM_N] __attribute__((section(".pool")));
static uint8_t ko_mp3_input[856] __attribute__((section(".pool")));
static struct {
    volatile uint32_t frame[2], want, bridge_pos;
    uint32_t epoch;
} ko_pcm[NVOICE];
static uint32_t ko_pcm_epoch, ko_pcm_underruns, ko_pcm_errors;
static int ko_compressed(const slc_src_t *s)
{
    return ko_bank_active && ko_bank.codec == KO_CODEC_MP3 && s == &slc_usr[0];
}
static void ko_pcm_reset(void)
{
    ko_pcm_epoch++;
    for (uint32_t i = 0; i < NVOICE; i++) {
        ko_pcm[i].frame[0] = ko_pcm[i].frame[1] = ko_pcm[i].want = ~0u;
        ko_pcm[i].bridge_pos=~0u;
        ko_pcm[i].epoch++;
    }
}
static int16_t *ko_pcm_buffer(uint32_t voice, uint32_t slot)
{
    return slot ? ko_pcm_second[voice] : ((ko_pcm_first_t *)ENG_MEM(0))->pcm[voice];
}
static int32_t ko_pcm_next(uint32_t pos, uint32_t voice)
{
    uint32_t absolute = pos+ko_bank.skip, frame = absolute/KO_PCM_N;
    if (voice >= NVOICE || pos >= ko_bank.samples) return 0;
    ko_pcm[voice].want = frame;
    for (uint32_t j = 0; j < 2; j++) if (ko_pcm[voice].frame[j] == frame)
        return ko_pcm_buffer(voice,j)[absolute%KO_PCM_N];
    uint32_t bridge=ko_pcm[voice].bridge_pos;
    if (pos>=bridge && pos-bridge<KO_PCM_TAIL)
        return ((ko_pcm_first_t *)ENG_MEM(0))->tail[voice][pos-bridge];
    ko_pcm_underruns++; return 0;
}
static void ko_pcm_poll(void)
{
    static uint32_t cursor;
    if (!ko_bank_active || ko_bank.codec != KO_CODEC_MP3 || !ko_song(&trk[0])) return;
    /* one frame job per call; timer/audio IRQs remain enabled throughout */
    for (uint32_t n = 0; n < NVOICE; n++) {
        uint32_t v = cursor++%NVOICE, want = ko_pcm[v].want, slot = 0;
        if (!trk[0].v[v].active || want == ~0u || want >= ko_bank.frames) continue;
        if (ko_pcm[v].frame[0] == want || ko_pcm[v].frame[1] == want) {
            want++;
            const voice_t *voice=&trk[0].v[v];
            if (voice->gate && !((uint32_t)voice->s[4]&KO_V_NO_LOOP) &&
                want*KO_PCM_N >= voice->ph[2]+ko_bank.skip) {
                uint32_t chop=((uint32_t)voice->s[4]>>8)&63u;
                want=(ko.table[ko.current].chop[chop].a+ko_bank.skip)/KO_PCM_N;
            }
            if (want >= ko_bank.frames) continue;
            if (ko_pcm[v].frame[0] == want || ko_pcm[v].frame[1] == want) {
                /* Cache a short bridge at the exact loop start. A block
                 * can cross the final frame and loop before the main loop
                 * ever observes that final frame as its current request. */
                uint32_t chop=((uint32_t)voice->s[4]>>8)&63u;
                uint32_t head=ko.table[ko.current].chop[chop].a;
                if (!voice->gate || ((uint32_t)voice->s[4]&KO_V_NO_LOOP) ||
                    ko_pcm[v].bridge_pos==head || ko_bank.samples-head<KO_PCM_TAIL) continue;
                want=(head+ko_bank.skip)/KO_PCM_N;slot=2;
            }
        }
        if (slot!=2 && ko_pcm[v].frame[0] == ko_pcm[v].want) slot = 1;
        uint32_t epoch = ko_pcm[v].epoch, bank_epoch = ko_pcm_epoch;
        uint32_t first = want > 1 ? want-2 : 0;
        uint32_t head=ko.table[ko.current].chop[((uint32_t)trk[0].v[v].s[4]>>8)&63u].a;
        uint32_t offset=(head+ko_bank.skip)%KO_PCM_N;
        uint32_t last=want+1u+(slot==2 && offset+KO_PCM_TAIL>KO_PCM_N);
        uint32_t a = ko_bank_u32(ko_bank.song_off+first*4), b = ko_bank_u32(ko_bank.song_off+last*4);
        if (b <= a || b-a > 840) { ko_pcm_errors++; return; }
        /* Unpublish the victim before decoding into it. */
        if (slot==2) ko_pcm[v].bridge_pos=~0u;
        else ko_pcm[v].frame[slot] = ~0u;
        RING_PUBLISH();
        ko_bank_read(ko_bank.song_off+a,ko_mp3_input,b-a); memset(ko_mp3_input+(b-a),0,8);
        int rc = slot==2 ? ko_mp3_decode_bridge(ko_mp3_input,b-a+8,last-first,offset,((ko_pcm_first_t *)ENG_MEM(0))->tail[v]) :
            ko_mp3_decode(ko_mp3_input,b-a+8,want-first+1,ko_pcm_buffer(v,slot));
        if (rc) { ko_pcm_errors++; return; }
        RING_PUBLISH();
        if (epoch == ko_pcm[v].epoch && bank_epoch == ko_pcm_epoch &&
            trk[0].v[v].active && ko_song(&trk[0])) {
            if (slot==2) {
                RING_PUBLISH();ko_pcm[v].bridge_pos=head;
            } else ko_pcm[v].frame[slot] = want;
        }
        return;
    }
}
