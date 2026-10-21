/* SPDX-License-Identifier: GPL-3.0-only */
/* Main-loop writer. Record into unused, pre-erased pool sectors. Existing bank
 * remains readable until explicit SAVE; cancel/error leaves its header untouched.
 * Saving rewrites the header sector, with the same power-cut limitation as bank
 * upload. USB recordings use mono 22.05 kHz IMA ADPCM, never an IRQ-time MP3 encode. */
#ifndef USB_SAMPLE_ARM
#define USB_SAMPLE_ARM() usb_sample_hw_begin()
#endif
static uint32_t ko_usb_phys(uint32_t off){uint32_t avail;return ko_bank_phys(off,&avail);}
static int ko_usb_write(uint32_t off,const uint8_t *data,uint32_t n){
 while(n){uint32_t avail,phys=ko_bank_phys(off,&avail),take=n<avail?n:avail;if(!avail || st_prog(phys,data,take))return 0;off+=take;data+=take;n-=take;}return 1;
}
/* Unsaved take audition reads pre-erased audio only; no bank header changes.
 * Decode at 22.05 kHz, duplicate each sample at the 44.1 kHz mixer rate. */
static int32_t ko_usb_decode(uint32_t code);
static void ko_usb_audition(void){
 if(ko_usb.state!=4)return;
 fm1_irq_off();uint32_t playing=ko_usb.preview;ko_usb.preview=0;fm1_irq_on();
 if(!playing){
  ko_usb.preview_pred=ko_usb.preview_index=0;
  for(uint32_t pos=0;pos<ko_usb.trim_a;pos++){
   if(!(pos&8191u))fm1_wdt_feed();
   ko_usb_decode((ko_bank_byte(ko_usb.start+(pos>>1))>>((pos&1u)*4u))&15u);
  }
  RING_PUBLISH();ko_usb.preview=ko_usb.trim_a*2u+1u;
 }
 ui.force=1;
}

static int32_t ko_usb_decode(uint32_t code){
 int32_t step=IMA_STEP[ko_usb.preview_index],d=step>>3;
 if(code&4u)d+=step;if(code&2u)d+=step>>1;if(code&1u)d+=step>>2;
 ko_usb.preview_pred=clamp(ko_usb.preview_pred+(code&8u?-d:d),-32768,32767);
 ko_usb.preview_index=clamp(ko_usb.preview_index+IMA_IDX[code&7u],0,88);
 return ko_usb.preview_pred;
}
static void ko_usb_audio(uint32_t n){
 for(uint32_t i=0;i<n && ko_usb.preview;i++){
  uint32_t pos=ko_usb.preview-1u;
  if(pos>=ko_usb.trim_b*2u){ko_usb.preview=0;break;}
  if(!(pos&1u))ko_usb_decode((ko_bank_byte(ko_usb.start+(pos>>2))>>((pos&2u)*2u))&15u);
  mix_l[i]+=ko_usb.preview_pred;mix_r[i]+=ko_usb.preview_pred;ko_usb.preview++;
 }
}
static void ko_usb_trim_turn(uint32_t k,int32_t delta){
 if(ko_usb.state!=4 || k>3 || !delta)return;
 ko_usb.preview=0;delta*=k<2?22:2205;
 if(!(k&1u))ko_usb.trim_a=(uint32_t)clamp((int32_t)ko_usb.trim_a+delta,0,(int32_t)ko_usb.trim_b-64);
 else ko_usb.trim_b=(uint32_t)clamp((int32_t)ko_usb.trim_b+delta,(int32_t)ko_usb.trim_a+64,(int32_t)ko_usb.samples);
 ui.force=1;
}
static void ko_usb_option(uint32_t k,int32_t delta){
 if(ko_usb.state || !delta)return;
 if(k==0)ko_rec_options.threshold=(uint8_t)clamp(ko_rec_options.threshold+delta,0,100);
 if(k==1){ko_rec_options.seconds=(uint8_t)clamp(ko_rec_options.seconds+delta,0,45);ko_rec_options.bars=0;}
 if(k==2){ko_rec_options.bars=(uint8_t)clamp(ko_rec_options.bars+delta,0,32);ko_rec_options.seconds=0;}
 ui.force=1;
}
static void ko_usb_meter(void){
        if(ko.view==33 && ko_usb.ui_second!=fm1_ms/100u){
            ko_usb.ui_second=fm1_ms/100u;
            fm1_irq_off();uint32_t peak=usb_sample.peak;usb_sample.peak=0;fm1_irq_on();
            ko_usb.level=(uint16_t)(peak*100u/32768u);
            if(ko_usb.state<2 && peak>=32767u)ko_usb.clipped=1;
            ui.force=1;
        }
}
static uint32_t ko_usb_remaining(void){
 uint32_t start=ko_bank_active?(ko_bank.used+4095u)&~4095u:KO_BANK_HEAD;
 uint32_t bytes=ko_usb.state?ko_usb.capacity:KO_BANK_SIZE-1024u-start;
 if(!ko_usb.state && (start>=KO_BANK_SIZE-1024u || bytes<4096u))return 0;
 uint32_t samples=ko_usb.state>=4?ko_usb.trim_b-ko_usb.trim_a:ko_usb.state?ko_usb.samples:0u;
 uint32_t limit=bytes*2u;
 if(ko_usb.state && ko_usb.limit<limit)limit=ko_usb.limit;
 if(!ko_usb.state){
  if(song.sel==1){uint32_t other=16;for(uint32_t i=0;i<ko_bank.kit_n;i++)if(i!=ko_kit_pad){uint32_t j=0;for(;j<i;j++)if(j!=ko_kit_pad && ko_bank.zone[j].off==ko_bank.zone[i].off && ko_bank.zone[j].n==ko_bank.zone[i].n)break;if(j==i)other+=(ko_bank.zone[i].n+1u)/2u;}limit=bytes>other+4096u?bytes-other:0;}
  uint32_t requested=ko_rec_options.bars?(uint32_t)ko_rec_options.bars*5292000u/(uint32_t)(song.g[G_BPM]>0?song.g[G_BPM]:120):(uint32_t)ko_rec_options.seconds*22050u;
  if(requested && requested<limit)limit=requested;
 }
 return samples>=limit?0u:(limit-samples)/22050u;
}
static void ko_usb_discard(void){ko_usb.preview=0;usb_sample.armed=0;ko_usb.state=0;ko_usb.error=0;ko_usb.samples=0;ui.force=1;}
static void ko_usb_stop(void){usb_sample.armed=0;if(ko_usb.state==1)ko_usb.state=2;ui.force=1;}
static void ko_usb_fail(uint32_t error){usb_sample.armed=0;ko_usb.error=error;ko_usb.state=3;ui.force=1;}
static int ko_usb_flush(void){
 if(!ko_usb.fill)return 1;
 if(!ko_usb_write(ko_usb.start+ko_usb.written,ko_usb.page,ko_usb.fill)){ko_usb_fail(3);return 0;}
 ko_usb.written+=ko_usb.fill;ko_usb.fill=0;return 1;
}
static uint32_t ko_usb_encode(int32_t sample){
  int32_t step=IMA_STEP[ko_usb.index],delta=sample-ko_usb.predictor;
  uint32_t nibble=delta<0?8u:0u; if(delta<0)delta=-delta;
  int32_t diff=step>>3;
  if(delta>=step){nibble|=4;delta-=step;diff+=step;}
  if(delta>=(step>>1)){nibble|=2;delta-=step>>1;diff+=step>>1;}
  if(delta>=(step>>2)){nibble|=1;diff+=step>>2;}
  ko_usb.predictor=clamp(ko_usb.predictor+(nibble&8u?-diff:diff),-32768,32767);
  ko_usb.index=clamp(ko_usb.index+IMA_IDX[nibble&7u],0,88);
  uint32_t bin=ko_usb.samples/2048u;int32_t wave=sample>>8;
  if(wave<ko_usb.wave[bin*2])ko_usb.wave[bin*2]=(int8_t)wave;
  if(wave>ko_usb.wave[bin*2+1])ko_usb.wave[bin*2+1]=(int8_t)wave;
 return nibble;
}
static void ko_usb_poll(void){
 if(ko_usb.state!=1 && ko_usb.state!=2)return;
 if(usb_sample.fault || !usb.config || usb.suspended){ko_usb_fail(usb_sample.fault?10u+usb_sample.fault:!usb.config?15u:16u);return;}
 uint32_t n=usb_sample.write-usb_sample.read;RING_PUBLISH();if(n>256u)n=256u;
 for(uint32_t i=0;i<n;i++){
  int32_t sample=usb_sample_pcm[usb_sample.read&(USB_SAMPLE_N-1u)];
  RING_PUBLISH();usb_sample.read++;
  if(!ko_usb.triggered){
   uint32_t level=sample<0?(uint32_t)-sample:(uint32_t)sample;
   if(level*100u<(uint32_t)ko_rec_options.threshold*32768u)continue;
   ko_usb.triggered=1;ui.force=1;
  }
  uint32_t nibble=ko_usb_encode(sample);
  if(!(ko_usb.samples&1u))ko_usb.half=(uint8_t)nibble;
  else ko_usb.page[ko_usb.fill++]=ko_usb.half|(uint8_t)(nibble<<4);
  ko_usb.samples++;
  if(ko_usb.fill==sizeof ko_usb.page && !ko_usb_flush())return;
  if((ko_usb.samples>=ko_usb.limit || ko_usb.samples>=ko_usb.capacity*2u)){usb_sample.armed=0;ko_usb.state=2;break;}
 }
 if(ko_usb.state==2 && (usb_sample.read==usb_sample.write || (ko_usb.samples>=ko_usb.limit || ko_usb.samples>=ko_usb.capacity*2u))){
  if(ko_usb.samples&1u)ko_usb.page[ko_usb.fill++]=ko_usb.half;
  if(!ko_usb_flush())return;
  if(ko_usb.samples<64){ko_usb_fail(2);return;}
  ko_usb.trim_b=ko_usb.samples;ko_usb.state=4;ui.force=1;
 }
}
static void ko_usb_start(void){
 if(ko_upload_total){ui_message("FINISH BANK UPLOAD FIRST");return;}
#if FELUCCA_KO_MAINT
 if(ko_reclaim_locked()){ui_message("RECOVERY / USB BACKUP");return;}
#endif
 if(ko_usb.state>=3){ui_message("SAVE OR DISCARD FIRST");return;}
 if(ko_usb.state==1 || ko_usb.state==2 || transport_busy() || song.rec || voices_busy()){ui_message("STOP / KEYS UP");return;}
 if(song.sel && !ko_bank_active){ui_message("SELECT A LOADED SONG");return;}
 if(!flash_ok){ui_message("FLASH UNAVAILABLE");return;}
 if(!usb_sample.alt){ui_message("SELECT USB AUDIO OUT");return;}
 uint32_t start=ko_bank_active?(ko_bank.used+4095u)&~4095u:KO_BANK_HEAD;
 uint32_t end=(KO_BANK_SIZE-1024u)&~255u;
 if(start>=end || end-start<4096u){ui_message("SAMPLE POOL FULL");return;}
 memset(&ko_usb,0,sizeof ko_usb);ko_usb.start=start;ko_usb.capacity=end-start;ko_usb.target=song.sel==1?2u:song.sel==2;ko_usb.pad=ko_kit_pad;
 if(ko_usb.capacity>512u*1024u)ko_usb.capacity=512u*1024u;
 if(ko_usb.target==2){
  uint32_t other=16;
  for(uint32_t i=0;i<ko_bank.kit_n;i++)if(i!=ko_usb.pad){uint32_t j=0;for(;j<i;j++)if(j!=ko_usb.pad && ko_bank.zone[j].off==ko_bank.zone[i].off && ko_bank.zone[j].n==ko_bank.zone[i].n)break;if(j==i)other+=(ko_bank.zone[i].n+1u)/2u;}
  if(other+4096u>=ko_usb.capacity){ko_usb_fail(3);return;}
  ko_usb.capacity=(ko_usb.capacity-other)/2u;
 }
 ko_usb.limit=ko_usb.capacity*2u;ko_usb.triggered=!ko_rec_options.threshold;
 uint32_t requested=ko_rec_options.bars?(uint32_t)ko_rec_options.bars*5292000u/(uint32_t)(song.g[G_BPM]>0?song.g[G_BPM]:120):(uint32_t)ko_rec_options.seconds*22050u;
 if(requested && requested<ko_usb.limit)ko_usb.limit=requested;
 ko_rec_options.reclaim=0;
 // All erasing precedes arming. Already blank sectors need no erase cycle.
 for(uint32_t off=start;off<KO_BANK_SIZE;off+=4096u){
  fm1_wdt_feed();const uint8_t *p=KO_BANK_XIP(ko_usb_phys(off));uint32_t i=0;
  while(i<4096u && p[i]==255u)i++;
  if(i<4096u && st_erase(ko_usb_phys(off))){ko_usb_fail(3);return;}
 }
 fm1_irq_off();int armed=USB_SAMPLE_ARM();fm1_irq_on();
 if(!armed){ko_usb_fail(1);return;}
 ko_usb.state=1;ui.force=1;
}
/* Input cache: USB PCM ring (4 KiB). Output: storage scratch + page buffer.
 * Output never overtakes input; cache the source sector before erasing it.
 * Only uncommitted tail sectors are rewritten, after explicit confirmation. */
static int ko_usb_trim_block(uint32_t n){
 uint32_t off=ko_usb.start+((ko_usb.written-1u)&~4095u),first=n<ST_PAYLOAD_MAX?n:ST_PAYLOAD_MAX;
 return !st_erase(ko_usb_phys(off)) && ko_usb_write(off,st_buf,first) &&
  (n<=ST_PAYLOAD_MAX || ko_usb_write(off+ST_PAYLOAD_MAX,ko_usb.page,n-ST_PAYLOAD_MAX));
}
static int ko_usb_trim(void){
 if(!ko_usb.trim_a && ko_usb.trim_b==ko_usb.samples)return 1;
 uint32_t a=ko_usb.trim_a,b=ko_usb.trim_b;
 uint8_t *input=(uint8_t *)usb_sample_pcm;
 ko_usb.preview_pred=ko_usb.preview_index=ko_usb.predictor=ko_usb.index=0;
 ko_usb.samples=ko_usb.written=ko_usb.fill=0;memset(ko_usb.wave,0,sizeof ko_usb.wave);
 for(uint32_t pos=0;pos<b;pos++){
  if(!(pos&8191u)){fm1_wdt_feed();ko_bank_read(ko_usb.start+(pos>>1),input,4096);}
  int32_t sample=ko_usb_decode((input[(pos>>1)&4095u]>>((pos&1u)*4u))&15u);
  if(pos<a)continue;
  uint32_t nibble=ko_usb_encode(sample);
  if(!(ko_usb.samples++&1u))ko_usb.half=(uint8_t)nibble;
  else ko_usb.page[ko_usb.fill++]=ko_usb.half|(uint8_t)(nibble<<4);
  if(ko_usb.fill==256u){
   uint32_t at=ko_usb.written&4095u;
   if(at<ST_PAYLOAD_MAX)memcpy(st_buf+at,ko_usb.page,256);
   ko_usb.written+=256;ko_usb.fill=0;
   if(!(ko_usb.written&4095u) && !ko_usb_trim_block(4096))return 0;
  }
 }
 if(ko_usb.samples&1u)ko_usb.page[ko_usb.fill++]=ko_usb.half;
 uint32_t at=ko_usb.written&4095u;
 if(at<ST_PAYLOAD_MAX)memcpy(st_buf+at,ko_usb.page,ko_usb.fill);
 ko_usb.written+=ko_usb.fill;ko_usb.fill=0;
 if((ko_usb.written&4095u) && !ko_usb_trim_block(ko_usb.written&4095u))return 0;
 uint32_t wave=(ko_usb.start+ko_usb.written+255u)&~255u;
 for(uint32_t off=(ko_usb.start+ko_usb.written+4095u)&~4095u;off<wave+((ko_usb.samples+2047u)/2048u)*2u;off+=4096u)
  if(st_erase(ko_usb_phys(off)))return 0;
 ko_usb.trim_a=0;ko_usb.trim_b=ko_usb.samples;return 1;
}
static void ko_usb_commit(uint32_t used){
 ko_bank_hdr_t *first=(ko_bank_hdr_t *)st_buf;
 uint32_t crc=ko_bank_crc(KO_BANK_HEAD,used-KO_BANK_HEAD);
 for(uint32_t i=0;i<(first->version==3?2u:1u);i++){
  ko_bank_hdr_t *h=first+i;h->used=used;h->crc=crc;h->hcrc=st_crc32(h,sizeof *h-4);
 }
 if(ko_bank_active)ko_bank_read(ST_PAYLOAD_MAX,ko_usb.page,256);else memset(ko_usb.page,255,256);
 if(!ko_bank_header_valid(first) || !ko_bank_second_valid(first,first+1)){ko_usb_fail(4);return;}
#if FELUCCA_KO_MAINT
 if(ko_keep_pending){ko_keep_record_t *r=(ko_keep_record_t *)(st_buf+KO_KEEP_OFFSET);memset(r,0,sizeof *r);r->magic=KO_KEEP_MAGIC;r->song_crc=ko_bank_crc(ko_usb.start,ko_usb.written);r->slot=ko_usb.target;r->table=ko_keep_table;r->crc=st_crc32(r,sizeof *r-4);}
 if(!ko_reclaim_commit(st_buf,used)){ko_usb_fail(3);return;}
#else
 if(st_erase(ko_usb_phys(0)) || !ko_usb_write(0,st_buf,ST_PAYLOAD_MAX) || !ko_usb_write(ST_PAYLOAD_MAX,ko_usb.page,256)){ko_usb_fail(3);return;}
#endif
 ko_comp_t kit_comp=ko_states[0].comp[1];uint8_t kit_quant=ko_quantize[1];
 if(ko_usb.target==2)memcpy(ko_usb.wave,ko_kit_params,sizeof ko_kit_params);
 song.sel=ko_usb.target==2?1u:ko_usb.target*2u;ko_usb_discard();ko_bank_scan();smp_user_gen++;ko_prepare();
 if(ko_usb.target==2){ko_states[0].comp[1]=kit_comp;ko_quantize[1]=kit_quant;memcpy(ko_kit_params,ko_usb.wave,sizeof ko_kit_params);ko_kit_params[ko_usb.pad]=(ko_pad_params_t){.level=127};ko_kit_pad=ko_usb.pad;ko.dirty=1;ko.view=16;}
 else ko.view=0;
#if FELUCCA_KO_MAINT
 if(ko_keep_pending){ko_restore(&ko_keep_table);ko_alternate[ko_id()]=ko_keep_table;ko_layout[ko_id()]=0;ko.dirty=1;ko.revision++;ko_store_sector(1);}
#endif
 ui_message("TAKE SAVED");
}
static void ko_usb_save_pad(void){
 uint32_t begin=(ko_usb.start+ko_usb.written+255u)&~255u,used=begin,n=ko_bank.kit_n;
 if(n<=ko_usb.pad)n=ko_usb.pad+1u;
 uint32_t src[16],bytes[16],position[16];
 for(uint32_t i=0;i<n;i++){
  src[i]=i==ko_usb.pad?ko_usb.start:i<ko_bank.kit_n?ko_bank.kit_off+ko_bank.zone[i].off:~0u;
  bytes[i]=i==ko_usb.pad?ko_usb.written:i<ko_bank.kit_n?(ko_bank.zone[i].n+1u)/2u:1u;
  uint32_t j=0;for(;j<i;j++)if(src[i]==src[j] && bytes[i]==bytes[j])break;
  position[i]=j<i?position[j]:used-begin;if(j==i)used+=bytes[i];
 }
 if(used>KO_BANK_SIZE){ko_usb_fail(3);return;}
 for(uint32_t off=(ko_usb.start+ko_usb.written+4095u)&~4095u;off<used;off+=4096u)if(st_erase(ko_usb_phys(off))){ko_usb_fail(3);return;}
 ko_bank_read(0,st_buf,ST_PAYLOAD_MAX);ko_bank_hdr_t *h=(ko_bank_hdr_t *)st_buf;
 h->kit_off=begin;h->kit_len=used-begin;h->kit_n=n;
 for(uint32_t i=0;i<n;i++){
  uint32_t j=0;for(;j<i;j++)if(position[j]==position[i])break;
  if(j==i)for(uint32_t at=0;at<bytes[i];at+=256){uint32_t count=bytes[i]-at;if(count>256)count=256;fm1_wdt_feed();if(src[i]==~0u)memset(ko_usb.page,0,count);else ko_bank_read(src[i]+at,ko_usb.page,count);if(!ko_usb_write(begin+position[i]+at,ko_usb.page,count)){ko_usb_fail(3);return;}}
  smp_zone_t z=i<ko_bank.kit_n?ko_bank.zone[i]:(smp_zone_t){.n=2,.le=1,.rate=32768,.root16=(60+i)*16,.lo=60+i,.hi=60+i};
  if(i==ko_usb.pad)z=(smp_zone_t){.n=ko_usb.samples,.le=ko_usb.samples-1,.rate=32768,.root16=(60+i)*16,.lo=60+i,.hi=60+i};
  z.off=position[i];h->zone[i]=z;
 }
 ko_usb_commit(used);
}
static void ko_usb_save(void){
 if(ko_usb.state==4){ko_usb.preview=0;ko_usb.state=5;ui.force=1;return;}
 if(ko_usb.state!=5)return;
 if(ko_usb.trim_b>ko_usb.samples || ko_usb.trim_a>ko_usb.trim_b || ko_usb.trim_b-ko_usb.trim_a<64u){ko_usb_fail(2);return;}
 ko_line(130,"SAVING TAKE",T_THEME);lcd_sync();
 if(!ko_usb_trim()){ko_usb_fail(3);return;}
 if(ko_usb.target==2){ko_usb_save_pad();return;}
 uint32_t bins=(ko_usb.samples+2047u)/2048u,wave=(ko_usb.start+ko_usb.written+255u)&~255u,used=wave+bins*2u;
 if(used>KO_BANK_SIZE || !ko_usb_write(wave,(const uint8_t *)ko_usb.wave,bins*2u)){ko_usb_fail(3);return;}
 // Reuse the existing storage scratch; reserve the last 256 bytes in the page buffer.
 if(ko_bank_active){ko_bank_read(0,st_buf,ST_PAYLOAD_MAX);ko_bank_read(ST_PAYLOAD_MAX,ko_usb.page,256);}
 else {memset(st_buf,255,ST_PAYLOAD_MAX);memset(ko_usb.page,255,256);}
 ko_bank_hdr_t *first=(ko_bank_hdr_t *)st_buf,*record;
 record=first+ko_usb.target;
 memset(record,0,ko_bank_active && !ko_usb.target?__builtin_offsetof(ko_bank_hdr_t,zone):sizeof *record);record->magic=KO_BANK_MAGIC;record->version=2;record->codec=KO_CODEC_IMA;record->rate=22050;
 record->samples=ko_usb.samples;record->song_off=ko_usb.start;record->song_len=ko_usb.written;
 record->kit_off=KO_BANK_HEAD;record->wave_off=wave;record->wave_bins=bins;record->auto_n=1;memcpy(record->name,"USB TAKE",8);
 if(ko_bank_active && !ko_usb.target){
  memcpy(&record->kit_off,&ko_bank.kit_off,3u*sizeof(uint32_t));
 }
 if(ko_bank_active && (ko_usb.target || ko_bank.version==3)){first->version=3;first->rsv[0]=sizeof *first;}
 ko_usb_commit(used);
}
