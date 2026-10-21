/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_KO_STORAGE 1
#define FELUCCA_SAMPLER_ONLY 1
static unsigned test_bank_used,test_bank_busy;
#define OTA_BANK_USED() test_bank_used
#define OTA_BANK_BUSY() test_bank_busy
#define main original_ota_test_main
#include "ota_test.c"
#undef main
int main(int argc,char **argv){
    int bad=original_ota_test_main(argc,argv);if(argc<2)return 2;
    uint8_t hdr[1024],descriptor[32],saved[32];uint32_t off,len,ota,n,key;
    reset_dev();ota_deadline=120000;
    if(ota_ufw(hdr,&off,&len,&ota,&n) || ota_bank_key(&key))return 2;
    uint8_t *d=logical+off+0x4000+KO_EXTRA_ENTRY;
    memcpy(saved,d,32);memcpy(descriptor,d,32);ota_bank_sfc(descriptor,32,KO_EXTRA_ENTRY,key);
    test_bank_used=KO_BANK_CAPACITY;
    reset_dev();bad+=check("expanded bank can update to a preserving package",ota_session()==0 && committed);
    ota_wr32(descriptor+4,KO_EXTRA_LO);ota_wr32(descriptor+8,KO_EXTRA_HI-KO_EXTRA_LO);
    ota_wr16(descriptor,ota_crc16(descriptor+2,30,0));ota_bank_sfc(descriptor,32,KO_EXTRA_ENTRY,key);memcpy(d,descriptor,32);
    reset_dev();bad+=check("old layout rejected before staging older loader",ota_session()==-17 && !erases && !committed);
    memcpy(d,saved,32);test_bank_busy=1;test_bank_used=0;
    reset_dev();bad+=check("unfinished bank upload blocks update staging",ota_session()==-17 && !erases && !committed);
    printf("OTA bank layout: %s\n",bad?"FAIL":"PASS");return bad!=0;
}
