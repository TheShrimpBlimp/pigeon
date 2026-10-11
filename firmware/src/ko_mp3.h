/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
/* Cooperative single decoder. Input/output buffers must remain valid until
 * completion. step decodes one frame: 1 pending, 0 complete, negative error. */
int ko_mp3_begin(const uint8_t *data,uint32_t bytes,uint32_t frames,uint32_t offset,uint32_t count,int16_t *pcm);
int ko_mp3_step(void);
/* MPEG2 mono 22050 Hz, <=64 kbps; zero-reservoir frames, at most two
 * predecessor frames for overlap/filter warm-up. Runs only in the main loop. */
int ko_mp3_decode(const uint8_t *data, uint32_t bytes, uint32_t frames, int16_t pcm[576]);

/* A 192-sample loop bridge can span two frames, plus two warm-up frames. */
int ko_mp3_decode_bridge(const uint8_t *data,uint32_t bytes,uint32_t frames,uint32_t offset,int16_t pcm[192]);
