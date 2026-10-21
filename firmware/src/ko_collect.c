/* SPDX-License-Identifier: GPL-3.0-only */
/* Keep active chops in place, or append them to the other song's empty keys.
 * Merge overlapping source intervals. Empty keys retain their note numbers. */
static struct {uint32_t source,a,b,out;} ko_keep_ranges[32] __attribute__((section(".pool")));
/* Origins are consumed before ko_usb is initialized for encoding. */
#define ko_keep_origin ko_usb.page
_Static_assert(sizeof ko_usb.page>=KO_CHOPS,"collection origins fit idle encoder page");
static uint32_t ko_keep_scale(uint32_t n,uint32_t rate){return n/rate*22050u+((n%rate)*22050u+rate-1u)/rate;}
static int ko_keep_pcm(uint32_t id,uint32_t pos,uint32_t count,int16_t *pcm){
 const slc_src_t *s=slc_get(id?3:1);if(!s)return 0;
 const ko_bank_hdr_t *h=id?&ko_bank_second:&ko_bank;
 if(h->codec==KO_CODEC_IMA){slc_fill(s,pcm,pos,count,0,0);return 1;}
 uint32_t want=(pos+h->skip)/576u,offset=(pos+h->skip)%576u,first=want>1?want-2:0,last=want+1u+(offset+count>576u);
 if(last>h->frames)return 0;
 uint32_t a=ko_bank_u32(h->song_off+first*4u),b=ko_bank_u32(h->song_off+last*4u);
 if(b<=a || b-a>840)return 0;
 ko_bank_read(h->song_off+a,ko_mp3_input,b-a);memset(ko_mp3_input+b-a,0,8);
 if(ko_mp3_begin(ko_mp3_input,b-a+8,last-first,offset,count,pcm))return 0;
 int rc;do{fm1_wdt_feed();rc=ko_mp3_step();}while(rc>0);return !rc;
}
static int ko_keep_put(int32_t sample){
 uint32_t nibble=ko_usb_encode(sample);
 if(!(ko_usb.samples++&1u))ko_usb.half=(uint8_t)nibble;
 else ko_usb.page[ko_usb.fill++]=ko_usb.half|(uint8_t)(nibble<<4);
 return ko_usb.fill<256 || ko_usb_flush();
}
static int ko_keep_apply(void){
 uint32_t source=ko_id(),target=ko_keep_target,mask=ko_keep_mask[source];
 if(!mask || transport_busy() || song.rec || voices_busy() || ko_usb.state || ko_reclaim_locked()){ui_message("MARK CHOPS / STOP FIRST");return 0;}
 const ko_table_t *from=ko_table();
 ko_keep_table=target==source?*from:ko_states[target].table[ko_states[target].current];
 for(uint32_t j=0;j<ko_keep_table.n;j++)ko_keep_origin[j]=(uint8_t)target;
 for(uint32_t j=0;j<from->n;j++){
  if(target==source){if(!(mask&(1u<<j)))ko_keep_table.chop[j]=(ko_chop_t){.a=0,.b=64,.rsv=KO_ONE|KO_NO_LOOP,.pad=1};continue;}
  if(!(mask&(1u<<j)) || from->chop[j].pad)continue;
  uint32_t at=0;while(at<ko_keep_table.n && !ko_keep_table.chop[at].pad)at++;
  if(at>=32){ui_message("DESTINATION KEYS FULL");return 0;}
  if(at==ko_keep_table.n)ko_keep_table.n++;
  ko_keep_table.chop[at]=from->chop[j];ko_keep_origin[at]=(uint8_t)source;
 }
 uint32_t nr=0;
 for(uint32_t j=0;j<ko_keep_table.n;j++)if(!ko_keep_table.chop[j].pad){
  ko_chop_t *c=&ko_keep_table.chop[j];ko_keep_ranges[nr].source=ko_keep_origin[j];ko_keep_ranges[nr].a=c->a;ko_keep_ranges[nr].b=c->b;nr++;
 }
 if(!nr){ui_message("NO AUDIO SELECTED");return 0;}
 for(uint32_t i=0;i<nr;i++)for(uint32_t j=i+1;j<nr;j++)if(ko_keep_ranges[j].source<ko_keep_ranges[i].source || (ko_keep_ranges[j].source==ko_keep_ranges[i].source && ko_keep_ranges[j].a<ko_keep_ranges[i].a)){
  uint8_t temporary[sizeof ko_keep_ranges[0]];memcpy(temporary,ko_keep_ranges+i,sizeof temporary);ko_keep_ranges[i]=ko_keep_ranges[j];memcpy(ko_keep_ranges+j,temporary,sizeof temporary);
 }
 uint32_t merged=0;
 for(uint32_t i=0;i<nr;i++){
  if(merged && ko_keep_ranges[i].source==ko_keep_ranges[merged-1].source && ko_keep_ranges[i].a<=ko_keep_ranges[merged-1].b){if(ko_keep_ranges[i].b>ko_keep_ranges[merged-1].b)ko_keep_ranges[merged-1].b=ko_keep_ranges[i].b;}
  else ko_keep_ranges[merged++]=ko_keep_ranges[i];
 }
 uint32_t total=64;
 for(uint32_t i=0;i<merged;i++){ko_keep_ranges[i].out=total;uint32_t rate=ko_keep_ranges[i].source?ko_bank_second.rate:ko_bank.rate;total+=ko_keep_scale(ko_keep_ranges[i].b-ko_keep_ranges[i].a,rate);}
 for(uint32_t j=0;j<ko_keep_table.n;j++)if(!ko_keep_table.chop[j].pad){
  ko_chop_t *c=&ko_keep_table.chop[j];
  for(uint32_t i=0;i<merged;i++)if(ko_keep_ranges[i].source==ko_keep_origin[j] && c->a>=ko_keep_ranges[i].a && c->b<=ko_keep_ranges[i].b){
   uint32_t rate=ko_keep_origin[j]?ko_bank_second.rate:ko_bank.rate;
   c->a=ko_keep_ranges[i].out+ko_keep_scale(c->a-ko_keep_ranges[i].a,rate);c->b=ko_keep_ranges[i].out+ko_keep_scale(c->b-ko_keep_ranges[i].a,rate);c->st=c->play_st=0;break;
  }
  if(c->b-c->a<64){ui_message("CHOP TOO SHORT TO CONVERT");return 0;}
 }
 uint32_t start=(ko_bank.used+4095u)&~4095u;
 if(start+((total+1u)/2u)+1280u>KO_BANK_SIZE){if(!ko_reclaim())return 0;start=(ko_bank.used+4095u)&~4095u;}
 if(start+((total+1u)/2u)+1280u>KO_BANK_SIZE){ui_message("NOT ENOUGH FREE SPACE");return 0;}
 ko_line(130,"COLLECTING SELECTED CHOPS",T_THEME);lcd_sync();
 memset(&ko_usb,0,sizeof ko_usb);ko_usb.start=start;ko_usb.capacity=KO_BANK_SIZE-start-1024;ko_usb.target=(uint8_t)target;ko_usb.triggered=1;ko_usb.state=2;
 ko_pcm_reset();
 for(uint32_t off=start;off<KO_BANK_SIZE;off+=4096){fm1_wdt_feed();if(st_erase(ko_usb_phys(off)))goto error;}
 for(uint32_t i=0;i<64;i++)if(!ko_keep_put(0))goto error;
 int16_t *pcm=(int16_t *)cv_px;
 for(uint32_t i=0;i<merged;i++){
  uint32_t id=ko_keep_ranges[i].source,position=ko_keep_ranges[i].a,end=ko_keep_ranges[i].b,rate=id?ko_bank_second.rate:ko_bank.rate;
  uint32_t count=ko_keep_scale(end-position,rate),fraction=0,cached=~0u,available=0;
  for(uint32_t j=0;j<count;j++){
   if(cached==~0u || position<cached || position+1u>=cached+available){cached=position;available=end-position;if(available>192)available=192;if(!ko_keep_pcm(id,cached,available,pcm))goto error;}
   uint32_t at=position-cached,next=at+1u<available?at+1u:at;
   int32_t sample=pcm[at]+((int32_t)(pcm[next]-pcm[at])*(int32_t)fraction)/22050;
   if(!ko_keep_put(sample))goto error;
   fraction+=rate;position+=fraction/22050u;fraction%=22050u;
  }
 }
 if(ko_usb.samples&1u)ko_usb.page[ko_usb.fill++]=ko_usb.half;
 if(!ko_usb_flush())goto error;
 ko_usb.trim_b=ko_usb.samples;ko_usb.state=5;ko_keep_pending=1;
 ko_usb_save();ko_keep_pending=0;ko_keep_confirm=0;
 if(ko_usb.state){ko.view=33;return 0;}
 ui_message("SELECTED CHOPS SAVED");return 1;
error:
 ko_usb_fail(3);ko.view=33;ko_keep_pending=0;return 0;
}
