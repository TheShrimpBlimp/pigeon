/* SPDX-License-Identifier: GPL-3.0-only */
/* One cooperative shared decoder, eight double-buffered song voice caches. The audio IRQ
 * only reads published PCM. Cache misses play zero and count underruns; they
 * never invoke the decoder or pause the audio clock. */
#include "ko_mp3.h"
#define KO_PCM_VOICES (NVOICE*(1u+FELUCCA_KO_DUAL))
#define KO_PCM_N 576u
#define KO_PCM_TAIL 192u /* loop bridge: enough for a maximum-pitch audio block */
typedef struct { int16_t pcm[NVOICE][KO_PCM_N], tail[NVOICE][KO_PCM_TAIL]; } ko_pcm_first_t;
ENG_MEM_FITS("KO PCM",sizeof(ko_pcm_first_t));
static int16_t ko_pcm_second[KO_PCM_VOICES][KO_PCM_N] __attribute__((section(".pool")));
static uint8_t ko_mp3_input[856] __attribute__((section(".pool")));
static struct {
    volatile uint32_t frame[2], want, bridge_pos;
    uint32_t epoch;
} ko_pcm[KO_PCM_VOICES];
static uint32_t ko_pcm_epoch, ko_pcm_underruns, ko_pcm_errors;
static uint32_t ko_pcm_reused;
/* Passive wall-time diagnostics; includes time spent in audio interrupts. */
#ifndef KO_DIAG_NOW
#ifdef FM1_TICKS_PER_US
#define KO_DIAG_NOW() fm1_ticks()
#define KO_DIAG_US(ticks) ((ticks)/FM1_TICKS_PER_US)
#else
/* DSP-only host harnesses have no HAL clock. */
#define KO_DIAG_NOW() 0u
#define KO_DIAG_US(ticks) (ticks)
#endif
#endif
static struct {
    uint32_t decode_calls, decode_us, decode_max_us, poll_gap_max_us, draw_max_us;
    uint32_t attacks, attack_max_us;
} ko_diag;
static uint32_t ko_diag_last_poll, ko_diag_poll_valid;
static uint32_t ko_diag_attack_tick[KO_PCM_VOICES];
static void ko_diag_max(uint32_t *max, uint32_t us) { if (us>*max) *max=us; }
static void ko_diag_reset(void)
{
    memset(&ko_diag,0,sizeof ko_diag);
    ko_diag_poll_valid=0;
    ko_pcm_underruns=ko_pcm_errors=ko_pcm_reused=0;
}

static struct {
    uint32_t active, voice, slot, frame, head, epoch, bank_epoch;
} ko_pcm_job;
static track_t *ko_pcm_track(uint32_t v) { return &trk[v>=NVOICE ? 2 : 0]; }
static voice_t *ko_pcm_voice(uint32_t v) { return &ko_pcm_track(v)->v[v%NVOICE]; }
static const ko_bank_hdr_t *ko_pcm_bank(uint32_t v) { return v>=NVOICE ? &ko_bank_second : &ko_bank; }
static const ko_table_t *ko_pcm_table(uint32_t v) {
    ko_state_t *state=ko_track_state(ko_pcm_track(v));return &state->table[state->current];
}
static int ko_compressed(const slc_src_t *s)
{
    return ko_bank_active && ((ko_bank.codec==KO_CODEC_MP3 && s==&slc_usr[0]) ||
        (FELUCCA_KO_DUAL && ko_bank.version==3 && ko_bank_second.codec==KO_CODEC_MP3 && s==&slc_usr[2]));
}
static void ko_pcm_reset(void)
{
    ko_pcm_epoch++;
    for (uint32_t i = 0; i < KO_PCM_VOICES; i++) {
        ko_pcm[i].frame[0] = ko_pcm[i].frame[1] = ko_pcm[i].want = ~0u;
        ko_pcm[i].bridge_pos=~0u;
        ko_pcm[i].epoch++;
    }
}
static int16_t *ko_pcm_buffer(uint32_t voice, uint32_t slot)
{
    return slot ? ko_pcm_second[voice] : ((ko_pcm_first_t *)ENG_MEM(voice>=NVOICE ? 2 : 0))->pcm[voice%NVOICE];
}
static int32_t ko_pcm_next(uint32_t pos, uint32_t voice)
{
    const ko_bank_hdr_t *bank=ko_pcm_bank(voice);
    uint32_t absolute = pos+bank->skip, frame = absolute/KO_PCM_N;
    if (voice >= KO_PCM_VOICES || pos >= bank->samples) return 0;
    ko_pcm[voice].want = frame;
    for (uint32_t j = 0; j < 2; j++) if (ko_pcm[voice].frame[j] == frame)
        return ko_pcm_buffer(voice,j)[absolute%KO_PCM_N];
    uint32_t bridge=ko_pcm[voice].bridge_pos;
    if (pos>=bridge && pos-bridge<KO_PCM_TAIL)
        return ((ko_pcm_first_t *)ENG_MEM(voice>=NVOICE ? 2 : 0))->tail[voice%NVOICE][pos-bridge];
    ko_pcm_underruns++; return 0;
}
static void ko_pcm_poll(void)
{
    static uint32_t cursor;
    uint32_t now=KO_DIAG_NOW(), sounding=0;
    for (uint32_t v=0;v<KO_PCM_VOICES;v++) sounding |= ko_pcm_voice(v)->active;
    if (sounding && ko_diag_poll_valid)
        ko_diag_max(&ko_diag.poll_gap_max_us,KO_DIAG_US(now-ko_diag_last_poll));
    ko_diag_last_poll=now;ko_diag_poll_valid=sounding;

    if (ko_pcm_job.active) {
        uint32_t v=ko_pcm_job.voice;
        if (ko_pcm_job.epoch!=ko_pcm[v].epoch || ko_pcm_job.bank_epoch!=ko_pcm_epoch ||
            !ko_pcm_voice(v)->active || !ko_song(ko_pcm_track(v))) { ko_pcm_job.active=0; return; }
        uint32_t begin=KO_DIAG_NOW();
        int rc=ko_mp3_step();
        uint32_t us=KO_DIAG_US(KO_DIAG_NOW()-begin);
        ko_diag.decode_calls++;ko_diag.decode_us+=us;
        ko_diag_max(&ko_diag.decode_max_us,us);
        if (rc>0) return;
        ko_pcm_job.active=0;
        if (rc<0) { ko_pcm_errors++; return; }
        RING_PUBLISH();
        if (ko_pcm_job.epoch==ko_pcm[v].epoch && ko_pcm_job.bank_epoch==ko_pcm_epoch &&
            ko_pcm_voice(v)->active && ko_song(ko_pcm_track(v))) {
            if (ko_pcm_job.slot==2) ko_pcm[v].bridge_pos=ko_pcm_job.head;
            else ko_pcm[v].frame[ko_pcm_job.slot]=ko_pcm_job.frame;
        }
        return;
    }
    if (!ko_bank_active) return;
    /* Start one job; subsequent calls decode one frame each, allowing input
     * and display work between seek warm-up frames. IRQs remain enabled. */
    /* Preserve running audio ahead of cold attacks and optional loop bridges.
     * Within each priority, rotate voices as before. */
    for (uint32_t priority = 0; priority < 4; priority++)
    for (uint32_t n = 0; n < KO_PCM_VOICES; n++) {
        uint32_t v = cursor++%KO_PCM_VOICES;
        const ko_bank_hdr_t *bank=ko_pcm_bank(v);
        if (bank->codec!=KO_CODEC_MP3 || !ko_song(ko_pcm_track(v))) continue;
        uint32_t want = ko_pcm[v].want, slot = 0;
        if (!ko_pcm_voice(v)->active || want == ~0u || want >= bank->frames) continue;
        uint32_t rank = ko_pcm_voice(v)->s[6] ? 0u : 2u;
        if (ko_pcm[v].frame[0] == want || ko_pcm[v].frame[1] == want) {
            rank = ko_pcm_voice(v)->s[6] ? 1u : 2u;
            want++;
            const voice_t *voice=ko_pcm_voice(v);
            if (voice->gate && !((uint32_t)voice->s[4]&KO_V_NO_LOOP) &&
                want*KO_PCM_N >= voice->ph[2]+bank->skip) {
                uint32_t chop=((uint32_t)voice->s[4]>>8)&63u;
                want=((*ko_pcm_table(v)).chop[chop].a+bank->skip)/KO_PCM_N;
            }
            if (want >= bank->frames) continue;
            if (ko_pcm[v].frame[0] == want || ko_pcm[v].frame[1] == want) {
                /* Cache a short bridge at the exact loop start. A block
                 * can cross the final frame and loop before the main loop
                 * ever observes that final frame as its current request. */
                uint32_t chop=((uint32_t)voice->s[4]>>8)&63u;
                uint32_t head=(*ko_pcm_table(v)).chop[chop].a;
                if (!voice->gate || ((uint32_t)voice->s[4]&KO_V_NO_LOOP) ||
                    ko_pcm[v].bridge_pos==head || bank->samples-head<KO_PCM_TAIL) continue;
                want=(head+bank->skip)/KO_PCM_N;slot=2;
                rank=3;
            }
        }
        if (rank != priority) continue;
        if (slot!=2 && ko_pcm[v].frame[0] == ko_pcm[v].want) slot = 1;
        uint32_t epoch = ko_pcm[v].epoch, bank_epoch = ko_pcm_epoch;
        uint32_t first = want > 1 ? want-2 : 0;
        uint32_t head=(*ko_pcm_table(v)).chop[((uint32_t)ko_pcm_voice(v)->s[4]>>8)&63u].a;
        uint32_t offset=(head+bank->skip)%KO_PCM_N;
        uint32_t last=want+1u+(slot==2 && offset+KO_PCM_TAIL>KO_PCM_N);
        uint32_t a = ko_bank_u32(bank->song_off+first*4), b = ko_bank_u32(bank->song_off+last*4);
        if (b <= a || b-a > 840) { ko_pcm_errors++; return; }
        /* Unpublish the victim before decoding into it. */
        if (slot==2) ko_pcm[v].bridge_pos=~0u;
        else ko_pcm[v].frame[slot] = ~0u;
        RING_PUBLISH();
        /* Identical source PCM can serve overlapping triggers without another
         * cold seek. Only main-loop code writes these buffers; IRQs read them. */
        for (uint32_t other=0; other<KO_PCM_VOICES; other++) if (other!=v && ko_pcm_bank(other)==bank) {
            const int16_t *source=0;
            if (slot==2 && ko_pcm[other].bridge_pos==head)
                source=((ko_pcm_first_t *)ENG_MEM(other>=NVOICE ? 2 : 0))->tail[other%NVOICE];
            if (slot!=2) for (uint32_t j=0;j<2;j++)
                if (ko_pcm[other].frame[j]==want) source=ko_pcm_buffer(other,j);
            if (!source) continue;
            int16_t *dest=slot==2 ? ((ko_pcm_first_t *)ENG_MEM(v>=NVOICE ? 2 : 0))->tail[v%NVOICE] : ko_pcm_buffer(v,slot);
            memcpy(dest,source,(slot==2 ? KO_PCM_TAIL : KO_PCM_N)*sizeof *dest);
            RING_PUBLISH();
            if (epoch==ko_pcm[v].epoch && bank_epoch==ko_pcm_epoch &&
                ko_pcm_voice(v)->active && ko_song(ko_pcm_track(v))) {
                if (slot==2) ko_pcm[v].bridge_pos=head;
                else ko_pcm[v].frame[slot]=want;
                ko_pcm_reused++;
            }
            return;
        }
        ko_bank_read(bank->song_off+a,ko_mp3_input,b-a); memset(ko_mp3_input+(b-a),0,8);
        int rc = ko_mp3_begin(ko_mp3_input,b-a+8,last-first,slot==2 ? offset : 0,
                             slot==2 ? KO_PCM_TAIL : KO_PCM_N,
                             slot==2 ? ((ko_pcm_first_t *)ENG_MEM(v>=NVOICE ? 2 : 0))->tail[v%NVOICE] : ko_pcm_buffer(v,slot));
        if (rc) { ko_pcm_errors++; return; }
        ko_pcm_job.voice=v; ko_pcm_job.slot=slot; ko_pcm_job.frame=want; ko_pcm_job.head=head;
        ko_pcm_job.epoch=epoch; ko_pcm_job.bank_epoch=bank_epoch; ko_pcm_job.active=1;
        return;
    }
}
