/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SAMPLER_ONLY 1
#define FELUCCA_KO 1
#define FELUCCA_KO_DUAL 1
#define BACKUP_TEST_MAIN upstream_backup_main
#include "backup_test.c"

int main(void)
{
    int bad=0;uint32_t len=0,crc=0;
    static up_bank_t bank;
    static fm6_bank_t old;
    static upf_t patches;
    reset();
    bank.magic=UP_BANK_MAGIC;bank.rsize=sizeof(up_rec_t);bank.nslot=UP_PER_BANK;
    bank.r[4].used=UP_USED;bank.r[4].ver=99;memcpy(bank.r[4].name,"Future",7);
    bad+=check("PIGEON restores opaque preset bytes without activating presets",
        !put_all(6,&bank,sizeof bank,st_crc32(&bank,sizeof bank)) && !up_used(4));
    bad+=check("opaque preset bank has its original length and checksum",
        !list(6,&len,&crc) && len==sizeof bank && crc==st_crc32(&bank,sizeof bank));
    uint32_t runtime_len=0,runtime_crc=0;
    list(0,&runtime_len,&runtime_crc);
    bad+=check("reading legacy flash does not overwrite captured runtime project",
        !get(6,0,256) && st_crc32(ED_BK_RAW,runtime_len)==runtime_crc);
    old.magic=FM6_BANK_MAGIC;old.ver=1;old.nslot=FM6_BANK_N;old.used=1;old.v[0][5]=47;
    bad+=check("old FM6 banks round-trip without migration",
        !put_all(8,&old,sizeof old,st_crc32(&old,sizeof old)) &&
        !list(8,&len,&crc) && len==sizeof old && crc==st_crc32(&old,sizeof old));
    bad+=check("empty id 9 preserves an older archive's id 8",
        !put_all(9,0,0,st_crc32(0,0)) && !list(8,&len,&crc) && len==sizeof old);
    reset();patches.magic=UPF_MAGIC;patches.ver=1;patches.nslot=UP_SLOTS;
    patches.used=1;patches.e[0].tag=42;patches.e[0].pk[5]=93;
    bad+=check("FM6 preset patches round-trip as opaque bytes",
        !put_all(9,&patches,sizeof patches,st_crc32(&patches,sizeof patches)) &&
        !list(9,&len,&crc) && len==sizeof patches && crc==st_crc32(&patches,sizeof patches));
    bad+=check("empty id 9 clears a modern legacy patch bank",
        !put_all(9,0,0,st_crc32(0,0)) && !list(9,&len,&crc) && !len);
    patches.nslot=3;
    bad+=check("invalid legacy layouts are rejected before writing",
        put_all(9,&patches,sizeof patches,st_crc32(&patches,sizeof patches))==2);
    printf("PIGEON opaque legacy backup: %s\n",bad?"FAILED":"PASSED");
    return !!bad;
}
