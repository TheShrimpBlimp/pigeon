/* SPDX-License-Identifier: GPL-3.0-only */
#define main original_loader_main
#include "ldr_test.c"
#undef main
static void refresh_flash_crc(uint32_t flash_offset){
    uint8_t h[0x40+11*0x50];memcpy(h,logical,sizeof h);ota_jl_enc(h,0x40);
    uint32_t n=ota_rd16(h+8);
    for(uint32_t i=0;i<n;i++){
        uint8_t *e=h+0x40+i*0x50;ota_jl_enc(e,0x50);
        if(!ota_rd16(e))ota_wr16(e+4,ota_crc16(logical+flash_offset,OTA_FL_LEN,0));
        ota_jl_enc(e,0x50);
    }
    ota_wr16(h+2,ota_crc16(h+0x40,n*0x50,0));ota_wr16(h,ota_crc16(h+2,0x3E,0));ota_jl_enc(h,0x40);memcpy(logical,h,0x40+n*0x50);
}
int main(int argc,char **argv){
    if(argc!=2)return 2;
    logical=load_logical(argv[1],&logical_len);uint32_t off=flash_off(logical),key;
    memset(nor,255,sizeof nor);memcpy(nor,logical+off,OTA_FL_LEN);if(ldr_chip_key(&key))return 2;
    ota_wr32(nor+0xA0000,KO_BANK_MAGIC);ota_wr32(nor+0xA0008,KO_BANK_CAPACITY);
    uint8_t d[32];uint8_t *dest=logical+off+LDR_APP_LO+KO_EXTRA_ENTRY;
    memcpy(d,dest,32);ldr_sfc(d,32,KO_EXTRA_ENTRY,key);
    ota_wr32(d+4,KO_EXTRA_LO);ota_wr32(d+8,KO_EXTRA_HI-KO_EXTRA_LO);ota_wr16(d,ota_crc16(d+2,30,0));
    ldr_sfc(d,32,KO_EXTRA_ENTRY,key);memcpy(dest,d,32);refresh_flash_crc(off);
    erases=0;int rc=ldr_session();
    int bad=check("loader refuses smaller layout before touching application or bank",rc==-15&&!erases);
    memcpy(d,dest,32);ldr_sfc(d,32,KO_EXTRA_ENTRY,key);ota_wr32(d+4,KO_GROW_LO-4096);
    ota_wr16(d,ota_crc16(d+2,30,0));ldr_sfc(d,32,KO_EXTRA_ENTRY,key);memcpy(dest,d,32);refresh_flash_crc(off);
    erases=0;rc=ldr_session();bad+=check("unknown pool boundary fails before any erase",rc==-14&&!erases);
    printf("Loader bank capacity: %s\n",bad?"FAIL":"PASS");return bad!=0;
}
