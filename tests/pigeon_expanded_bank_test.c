/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO_DUAL 1
#define KO_STORAGE_TEST_MAIN baseline_storage_main
#include "ko_storage_test.c"

static uint8_t image[KO_BANK_CAPACITY];
static void make_bank(uint32_t used)
{
    memset(image,0,sizeof image);
    for(uint32_t i=KO_BANK_HEAD;i<used;i++) image[i]=(uint8_t)(i*17u+3u);
    ko_bank_hdr_t *h=(ko_bank_hdr_t *)image;
    h->magic=KO_BANK_MAGIC;h->version=2;h->used=used;h->codec=KO_CODEC_IMA;
    h->rate=22050;h->samples=1000;h->song_off=h->kit_off=KO_BANK_HEAD;
    h->song_len=used-KO_BANK_HEAD-2;h->wave_off=used-2;h->wave_bins=h->auto_n=1;
    h->crc=st_crc32(image+KO_BANK_HEAD,used-KO_BANK_HEAD);
    h->hcrc=st_crc32(h,sizeof *h-4);
}
int main(void)
{
    int bad=0;ui_power_on();memset(bank_flash,255,sizeof bank_flash);
    slc_store_boot();ko.on=1;
    static const uint32_t old_lo[]={0xA0000,0xB4000,0x78000,0xEA000};
    static const uint32_t old_size[]={0x13000,0x28000,0x1A000,0x12000};
    uint32_t logical=0;int stable=1;
    for(uint32_t k=0;k<4;k++)for(uint32_t n=0;n<old_size[k];n+=4096) {
        uint32_t avail;stable&=ko_bank_phys(logical+n,&avail)==old_lo[k]+n;
        if(n+4096==old_size[k]) logical+=old_size[k];
    }
    bad+=check("every old bank sector keeps its physical address",stable&&logical==KO_BANK_BASE_SIZE);
    uint32_t avail;
    bad+=check("new storage is appended below the unchanged boot marker",
        ko_bank_phys(KO_BANK_BASE_SIZE,&avail)==KO_EXTRA_LO && avail==KO_EXTRA_HI-KO_EXTRA_LO &&
        KO_BANK_CAPACITY-KO_BANK_HEAD==496u*1024u && KO_EXTRA_HI==0x76000);
    memset(bank_flash+KO_EXTRA_LO-4096,0xA6,4096);
    memset(bank_flash+KO_EXTRA_HI,0xB7,0x2000);
    make_bank(KO_BANK_CAPACITY);
    bad+=check("a full 496 KiB payload uploads and validates",upload(image,KO_BANK_CAPACITY)==0 && ko_bank_active);
    int intact=1;
    for(uint32_t i=0;i<4096;i++) intact&=bank_flash[KO_EXTRA_LO-4096+i]==0xA6;
    for(uint32_t i=0;i<0x2000;i++) intact&=bank_flash[KO_EXTRA_HI+i]==0xB7;
    bad+=check("bank upload never writes firmware or boot-marker sectors",intact&&!bad_flash);
    uint8_t row[256];ko_bank_read(KO_BANK_BASE_SIZE,row,sizeof row);
    bad+=check("new tail reads back from reclaimed flash",!memcmp(row,image+KO_BANK_BASE_SIZE,sizeof row));
    make_bank(KO_BANK_BASE_SIZE);
    bad+=check("a bank made for the previous layout still uploads",upload(image,KO_BANK_BASE_SIZE)==0 && ko_bank_active);
    bad+=check("bank CRC survives scanning after reboot",(ko_bank_active=0,ko_bank_scan(),ko_bank_active));
    printf("PIGEON expanded bank: %s\n",bad?"FAILED":"PASSED");return !!bad;
}
