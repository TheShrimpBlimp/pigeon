/* SPDX-License-Identifier: GPL-3.0-only */
/* Opposite loop edges are prepared outside the audio interrupt. */
#define KO_BLEND_N 192u
typedef struct {uint32_t a,b,generation,after;uint16_t n;volatile uint8_t ready;uint8_t reverse;} ko_blend_t;
/* Reuse the song engine's existing memory: ADPCM needs only its first 1024 B.
 * MP3 already owns a 192-sample loop bridge, so its crossfade reads that bridge
 * directly. Metadata fits in the unused end of either engine-memory layout. */
#define KO_BLEND_META_OFF 12288u
static ko_blend_t *ko_blend_data(uint32_t id){return (ko_blend_t *)((uint8_t *)ENG_MEM(id?2:0)+KO_BLEND_META_OFF);}
ENG_MEM_FITS("loop crossfade",KO_BLEND_META_OFF+NVOICE*sizeof(ko_blend_t));
static const int16_t *ko_blend_buffer(uint32_t id,uint32_t vi,const slc_src_t *s){
#if FELUCCA_KO_STORAGE
 if(ko_compressed(s)){const ko_blend_t *c=&ko_blend_data(id)[vi];uint32_t head=c->reverse?c->b-c->n:c->a;return ((ko_pcm_first_t *)ENG_MEM(id?2:0))->tail[vi]+(head-ko_pcm[id*NVOICE+vi].bridge_pos);}
#endif
 return ((const int16_t (*)[KO_BLEND_N])((uint8_t *)ENG_MEM(id?2:0)+sizeof(slc_rbuf_t)))[vi];
}
static uint32_t ko_blend_id(const voice_t *v){return FELUCCA_KO_DUAL && (uintptr_t)v>=(uintptr_t)trk[2].v;}
static uint32_t ko_blend_vi(const voice_t *v,uint32_t id){return (uint32_t)(v-trk[id?2:0].v);}
static uint32_t ko_blend_length(const voice_t *v,const slc_src_t *s){
 if(!((uint32_t)v->s[4]&KO_V_FADE))return 0;
 uint32_t id=ko_blend_id(v),vi=ko_blend_vi(v,id);if(vi>=NVOICE)return 0;
 const ko_state_t *ks=&ko_states[id];uint32_t j=((uint32_t)v->s[4]>>8)&63u;if(j>=ks->table[ks->current].n)return 0;
 const ko_chop_t *c=&ks->table[ks->current].chop[j];
#if FELUCCA_KO_STORAGE
 if(ko_compressed(s)){const ko_blend_t *c=&ko_blend_data(id)[vi];uint32_t head=c->reverse?c->b-c->n:c->a,bridge=ko_pcm[id*NVOICE+vi].bridge_pos;if(head<bridge || head-bridge+c->n>KO_PCM_TAIL)return 0;}
#endif
 return ko_blend_data(id)[vi].ready && ko_blend_data(id)[vi].a==c->a && ko_blend_data(id)[vi].b==c->b && ko_blend_data(id)[vi].generation==smp_user_gen && ko_blend_data(id)[vi].reverse==(((uint32_t)v->s[4]>>7)&1u)?ko_blend_data(id)[vi].n:0;
}
static void ko_blend_poll(void){
 static uint32_t cursor;uint32_t k=cursor++%(NVOICE*(1u+FELUCCA_KO_DUAL)),id=k/NVOICE,vi=k%NVOICE;
 voice_t *v=&trk[id?2:0].v[vi];if(!v->active || !((uint32_t)v->s[4]&KO_V_FADE))return;
 const slc_src_t *s=slc_get((uint32_t)v->s[4]&7u);if(!s)return;
 const ko_state_t *ks=&ko_states[id];uint32_t j=((uint32_t)v->s[4]>>8)&63u;if(j>=ks->table[ks->current].n)return;
 const ko_chop_t *c=&ks->table[ks->current].chop[j];uint32_t n=c->xfade*((s->rate*44100u+32768u)>>16)/1000u;
 if(n>KO_BLEND_N)n=KO_BLEND_N;if(n>(c->b-c->a)/2u)n=(c->b-c->a)/2u;
 uint32_t rev=((uint32_t)v->s[4]>>7)&1u,head=rev?c->b-n:c->a;
 if(ko_blend_data(id)[vi].ready && ko_blend_data(id)[vi].generation==smp_user_gen && ko_blend_data(id)[vi].a==c->a && ko_blend_data(id)[vi].b==c->b && ko_blend_data(id)[vi].n==n && ko_blend_data(id)[vi].reverse==rev)return;
 ko_blend_data(id)[vi].ready=0;RING_PUBLISH();if(!n)return;uint32_t after=0;
#if FELUCCA_KO_STORAGE
 if(ko_compressed(s)){
  uint32_t bridge=ko_pcm[id*NVOICE+vi].bridge_pos;
  if(head<bridge || head-bridge+n>KO_PCM_TAIL)return;
 }else
#endif
 {
  slc_dec_t d;slc_dec_at(&d,head,slc_state_at(s,head));for(uint32_t x=0;x<n;x++)((int16_t (*)[KO_BLEND_N])((uint8_t *)ENG_MEM(id?2:0)+sizeof(slc_rbuf_t)))[vi][x]=(int16_t)slc_dec_next(s,&d);after=slc_dec_st(&d);
 }
 ko_blend_data(id)[vi].a=c->a;ko_blend_data(id)[vi].b=c->b;ko_blend_data(id)[vi].generation=smp_user_gen;ko_blend_data(id)[vi].after=after;ko_blend_data(id)[vi].n=(uint16_t)n;ko_blend_data(id)[vi].reverse=(uint8_t)rev;
 RING_PUBLISH();ko_blend_data(id)[vi].ready=1;
}
static __attribute__((noinline)) int32_t ko_blend_sample(const slc_src_t *s,voice_t *v,uint32_t pos,int32_t x,int loop){
 if(!loop)return x;uint32_t n=ko_blend_length(v,s);if(!n)return x;uint32_t id=ko_blend_id(v),vi=ko_blend_vi(v,id),i;
 if(ko_blend_data(id)[vi].reverse){if(pos>=ko_blend_data(id)[vi].a+n)return x;i=ko_blend_data(id)[vi].a+n-1u-pos;}
 else {if(pos<ko_blend_data(id)[vi].b-n)return x;i=pos-(ko_blend_data(id)[vi].b-n);}
 if(i>=n)return x;int32_t y=ko_blend_buffer(id,vi,s)[ko_blend_data(id)[vi].reverse?n-1u-i:i];return x+(y-x)*(int32_t)(i+1u)/(int32_t)n;
}
