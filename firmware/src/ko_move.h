/* SPDX-License-Identifier: GPL-3.0-only */
/* Resumable leftward bank rewrite. The updater's idle staging area holds the
 * new header, two rotating snapshot logs and one verified redo sector.
 * Caller supplies KM_READ, KM_WRITE, KM_ERASE, KM_BANK_READ, KM_BANK_PHYS,
 * KM_BUFFER (4096 RAM bytes), KM_CRC and KM_FEED. Never used during recording. */
#define KM_BASE 0xE0000u
#define KM_MAGIC 0x314D4F4Bu
#define KM_COMMIT 0x54494D43u
#define KM_SECTOR 4096u
#define KM_SCRATCH (KM_BASE+3u*KM_SECTOR)
typedef struct {uint32_t src,dst,len;} ko_move_range_t;
typedef struct {
 uint32_t magic,generation,stage,next,scratch_crc,used,header_crc,count;
 ko_move_range_t range[5];uint32_t reserved[7],crc,commit;
} ko_move_record_t;
_Static_assert(sizeof(ko_move_record_t)==128,"move snapshot ABI");
#ifndef KM_POOL
#define KM_POOL __attribute__((section(".pool")))
#endif
static ko_move_record_t ko_move_record KM_POOL;
static uint32_t ko_move_log KM_POOL;
static int ko_move_valid(const ko_move_record_t *r){
 if(r->magic!=KM_MAGIC || r->commit!=KM_COMMIT || r->crc!=KM_CRC(r,120) ||
    r->stage>2 || !r->count || r->count>5 || r->used<4096 || r->used>KO_BANK_SIZE ||
    !r->next || r->next>(r->used+4095u)/4096u)return 0;
 uint32_t end=4096,source=4096;
 for(uint32_t i=0;i<r->count;i++){
  const ko_move_range_t *p=r->range+i;
  if(!p->len || p->src<source || p->dst!=end || p->dst>p->src ||
     p->src>KO_BANK_SIZE || p->len>KO_BANK_SIZE-p->src || p->len>r->used-end)return 0;
  source=p->src+p->len;end+=p->len;
 }
 return end==r->used;
}
static int ko_move_load(void){
 ko_move_record_t item;int found=0;
 for(uint32_t sector=1;sector<=2;sector++)for(uint32_t i=0;i<32;i++){
  uint32_t off=KM_BASE+sector*4096+i*128;KM_READ(off,&item,sizeof item);
  if(ko_move_valid(&item) && (!found || (int32_t)(item.generation-ko_move_record.generation)>0)){
   ko_move_record=item;ko_move_log=off;found=1;
  }
 }
 return found;
}
static int ko_move_append(void){
 uint32_t base=ko_move_log&~4095u,off=base+4096;
 uint32_t *words=(uint32_t *)KM_BUFFER;
 for(uint32_t i=ko_move_log+128;i<base+4096;i+=128){
  KM_READ(i,KM_BUFFER,128);uint32_t j=0;while(j<32 && words[j]==~0u)j++;
  if(j==32){off=i;break;}
 }
 if(off==base+4096){off=KM_BASE+(base==KM_BASE+4096?8192:4096);if(KM_ERASE(off))return 0;}
 ko_move_record.generation++;ko_move_record.commit=~0u;
 ko_move_record.crc=KM_CRC(&ko_move_record,120);
 if(KM_WRITE(off,&ko_move_record,124))return 0;
 KM_READ(off,KM_BUFFER,124);if(memcmp(KM_BUFFER,&ko_move_record,124))return 0;
 uint32_t commit=KM_COMMIT;if(KM_WRITE(off+124,&commit,4))return 0;
 ko_move_record_t verify;KM_READ(off,&verify,sizeof verify);
 if(!ko_move_valid(&verify))return 0;
 ko_move_record=verify;ko_move_log=off;return 1;
}
static int ko_move_begin(const ko_move_record_t *plan,const uint8_t *header){
 /* No bank writes occur before the complete first intent is durable. */
 if(KM_ERASE(KM_BASE) || KM_WRITE(KM_BASE,header,4096))return 0;
 KM_READ(KM_BASE,KM_BUFFER,4096);
 if(KM_CRC(KM_BUFFER,4096)!=plan->header_crc)return 0;
 if(KM_ERASE(KM_BASE+4096) || KM_ERASE(KM_BASE+8192))return 0;
 ko_move_record=*plan;ko_move_record.magic=KM_MAGIC;ko_move_record.generation=0;
 ko_move_record.stage=0;ko_move_record.next=plan->next?plan->next:1;ko_move_record.commit=~0u;
 ko_move_log=KM_BASE+4096-128;
 return ko_move_append();
}
/* 1 completed/recovered, 0 nothing pending, -1 retry required; bank stays locked
 * while a valid active snapshot exists. Replay is idempotent after any cut. */
static int ko_move_recover(void){
 if(!ko_move_load() || ko_move_record.stage==2)return 0;
 for(;;){
  KM_FEED();uint32_t dst=ko_move_record.next*4096u<ko_move_record.used?ko_move_record.next:0;
  if(ko_move_record.stage==0){
   memset(KM_BUFFER,255,4096);
   if(!dst){KM_READ(KM_BASE,KM_BUFFER,4096);if(KM_CRC(KM_BUFFER,4096)!=ko_move_record.header_crc)return -1;}
   else for(uint32_t i=0;i<ko_move_record.count;i++){
    const ko_move_range_t *p=ko_move_record.range+i;
    uint32_t lo=p->dst>dst*4096?p->dst:dst*4096,hi=p->dst+p->len;
    if(hi>(dst+1)*4096)hi=(dst+1)*4096;
    if(lo<hi)KM_BANK_READ(p->src+lo-p->dst,(uint8_t *)KM_BUFFER+lo-dst*4096,hi-lo);
   }
   uint32_t crc=KM_CRC(KM_BUFFER,4096);
   if(KM_ERASE(KM_SCRATCH) || KM_WRITE(KM_SCRATCH,KM_BUFFER,4096))return -1;
   KM_READ(KM_SCRATCH,KM_BUFFER,4096);if(KM_CRC(KM_BUFFER,4096)!=crc)return -1;
   ko_move_record.scratch_crc=crc;ko_move_record.stage=1;
   if(!ko_move_append())return -1;
  }
  KM_READ(KM_SCRATCH,KM_BUFFER,4096);
  if(KM_CRC(KM_BUFFER,4096)!=ko_move_record.scratch_crc)return -1;
  uint32_t physical=KM_BANK_PHYS(dst*4096u);
  if(KM_ERASE(physical) || KM_WRITE(physical,KM_BUFFER,4096))return -1;
  KM_READ(physical,KM_BUFFER,4096);
  if(KM_CRC(KM_BUFFER,4096)!=ko_move_record.scratch_crc)return -1;
  if(!dst)ko_move_record.stage=2;
  else {ko_move_record.next++;ko_move_record.stage=0;}
  if(!ko_move_append())return -1;
  if(!dst)return 1;
 }
}
