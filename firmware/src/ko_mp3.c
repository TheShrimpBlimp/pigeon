/* SPDX-License-Identifier: GPL-3.0-only */
#include "ko_mp3.h"
#include "../vendor/libmad/stream.h"
#include "../vendor/libmad/frame.h"
#include "../vendor/libmad/synth.h"
#include <string.h>
#include <stddef.h>
#ifdef __APPLE__
#define KO_MAD_POOL
#else
#define KO_MAD_POOL __attribute__((section(".pool")))
#endif
static struct {
    struct mad_stream stream;
    struct mad_frame frame;
    struct mad_synth synth;
    unsigned char main_data[MAD_BUFFER_MDLEN];
    mad_fixed_t overlap[2][32][18];
} decoder KO_MAD_POOL;
/* libmad's allocations are bypassed with preallocated buffers. These fail
 * closed if a future library change unexpectedly requests a heap. */
void *ko_mad_malloc(size_t n) { (void)n; return 0; }
void *ko_mad_calloc(size_t n, size_t size) { (void)n; (void)size; return 0; }
void ko_mad_free(void *p) { (void)p; }
static struct {
    uint32_t frames, target, offset, count, next;
    int16_t *pcm;
} window;
int ko_mp3_begin(const uint8_t *data, uint32_t bytes, uint32_t frames, uint32_t offset, uint32_t count, int16_t *pcm)
{
    window.frames = 0;
    if (!pcm || !frames || frames > 4 || bytes > 848 || offset >= 576 || !count || count > 576) return -1;
    uint32_t target=frames-1u;
    if (offset+count>576u) { if(frames<2u)return -1; target--; }
    memset(&decoder,0,sizeof decoder);
    mad_stream_init(&decoder.stream); mad_frame_init(&decoder.frame); mad_synth_init(&decoder.synth);
    decoder.stream.main_data = (void *)&decoder.main_data;
    decoder.frame.overlap = &decoder.overlap;
    mad_stream_buffer(&decoder.stream,data,bytes);
    window.frames=frames; window.target=target; window.offset=offset;
    window.count=count; window.next=0; window.pcm=pcm;
    return 0;
}
int ko_mp3_step(void)
{
    if (!window.frames || window.next >= window.frames) return -1;
    uint32_t j=window.next;
    if (mad_frame_decode(&decoder.frame,&decoder.stream) || decoder.frame.header.layer != MAD_LAYER_III ||
        decoder.frame.header.samplerate != 22050 || decoder.frame.header.mode != MAD_MODE_SINGLE_CHANNEL ||
        decoder.frame.header.bitrate > 64000) { window.frames=0; return -2; }
    mad_synth_frame(&decoder.synth,&decoder.frame);
    if (decoder.synth.pcm.length != 576) { window.frames=0; return -3; }
    if (j>=window.target) {
        uint32_t base=(j-window.target)*576u;
        for(uint32_t i=0;i<576;i++) if(base+i>=window.offset && base+i-window.offset<window.count) {
            int32_t x=decoder.synth.pcm.samples[0][i]>>13;
            window.pcm[base+i-window.offset]=(int16_t)(x < -32768 ? -32768 : x > 32767 ? 32767 : x);
        }
    }
    return ++window.next == window.frames ? 0 : 1;
}
static int decode_window(const uint8_t *data, uint32_t bytes, uint32_t frames, uint32_t offset, uint32_t count, int16_t *pcm)
{
    int rc=ko_mp3_begin(data,bytes,frames,offset,count,pcm);
    if (rc) return rc;
    do { rc=ko_mp3_step(); } while(rc>0);
    return rc;
}

int ko_mp3_decode(const uint8_t *data,uint32_t bytes,uint32_t frames,int16_t pcm[576])
{
    if(frames>3 || bytes>640)return -1;
    return decode_window(data,bytes,frames,0,576,pcm);
}
int ko_mp3_decode_bridge(const uint8_t *data,uint32_t bytes,uint32_t frames,uint32_t offset,int16_t pcm[192])
{
    return decode_window(data,bytes,frames,offset,192,pcm);
}
