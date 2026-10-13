/* SPDX-License-Identifier: GPL-3.0-only */
/* Audit cold-boot decisions after every simulated erase/page program. */
#define main legacy_loader_main
#include "ldr_test.c"
#undef main
#include "../firmware/stub/stub_core.h"
static uint32_t chip_key, points, unprotected, failures, first_op, last_op;
static uint8_t expected_stub[4096], original_stub[4096];
static uint8_t snapshots[3][0x100000];
static uint32_t snapshot_count;
static int old_has_stub;
static uint32_t decide(uint32_t key) {
    static uint32_t body[STUB_BODY_MAX/4];
    static stub_crc_t crc;
    uint8_t mark[32];
    stub_result_t r;
    memcpy(mark,nor+STUB_MARK_OFF,32);
    ldr_sfc(mark,32,STUB_MARK_OFF-LDR_APP_LO,chip_key);
    memcpy(body,nor+STUB_BODY_OFF,sizeof body);
    ldr_sfc((uint8_t*)body,sizeof body,STUB_BODY_OFF-LDR_APP_LO,chip_key);
    stub_view_t view={mark,body,nor+STUB_LDR_OFF};
    stub_decide(&view,&crc,key,0,&r);
    return r.decision;
}
static void audit(uint32_t op) {
    points++; if(points==1) first_op=op; last_op=op;
    if(!memcmp(nor+0x4000,expected_stub,4096) ||
       (old_has_stub && !memcmp(nor+0x4000,original_stub,4096))) {
        uint32_t d=decide(0);
        if(d==STUB_UBOOT) failures++;
        if(d==STUB_BOOT && (op != STUB_MARK_SEC+0xf00)) failures++;
    } else if((op & ~0xfffu)==0x4000) unprotected++;
    /* Representative power-cut snapshots: after uncommit, midbody, before commit. */
    if(snapshot_count<3 && (points==1 || op==0x30000 || op==0x92000+0xf00))
        memcpy(snapshots[snapshot_count++],nor,sizeof nor);
}
static void stage(void) {
    uint8_t h[0x400];uint32_t fo,fl,oo,ol;
    rx_full=0;ota_ufw(h,&fo,&fl,&oo,&ol);
    memset(nor+STUB_LDR_OFF,255,STUB_LDR_LEN);
    memcpy(nor+STUB_LDR_OFF+32,logical+oo+32,ol-32);
    ota_stage_head(ol-32);
}
static int exercise(const uint8_t *base, uint32_t nfo,int previous_stub) {
    static uint8_t expected[0x100000];
    old_has_stub=previous_stub;
    memcpy(nor,base,sizeof nor);stage();
    memcpy(original_stub,nor+0x4000,4096);
    memcpy(expected_stub,logical+nfo+0x4000,4096);
    points=unprotected=failures=snapshot_count=0;
    flash_audit=audit;rx_full=0;
    int rc=ldr_session();flash_audit=NULL;
    memcpy(expected,nor,sizeof nor);
    int bad=check("recovery install completes",rc==0 && decide(0)==STUB_BOOT);
    bad+=check("each flash operation boots only a committed body or rescues",failures==0);
    bad+=check("first write invalidates marker; commit page is last",first_op==(STUB_MARK_SEC|1) && last_op==STUB_MARK_SEC+0xf00);
    bad+=check("first stub install has only its 16-operation window",unprotected==(previous_stub?0:16));
    bad+=check("OCT- requests stored loader",decide(1)==STUB_LOADER);
    printf("  audited %u flash operations; %u unprotected; %u snapshots\n",points,unprotected,snapshot_count);
    for(uint32_t i=0;i<snapshot_count;i++) {
        memcpy(nor,snapshots[i],sizeof nor);rx_full=0;
        rc=ldr_session();bad+=check("resume power-cut snapshot preserves all flash data",rc==0 && !memcmp(nor,expected,sizeof nor));
    }
    return bad;
}
int main(int argc,char **argv) {
    if(argc!=4) {puts("usage: recovery OLD.fwsc NEW.fwsc SAME_STUB.fwsc");return 2;}
    size_t n;uint8_t *old=load_logical(argv[1],&n);
    logical=load_logical(argv[2],&logical_len);uint32_t nf=flash_off(logical);
    static uint8_t base[0x100000];memset(base,255,sizeof base);
    memcpy(base,old+flash_off(old),0x93000);memcpy(nor,base,sizeof nor);ldr_chip_key(&chip_key);
    int bad=exercise(base,nf,0);free(old);
    free(logical);logical=load_logical(argv[3],&logical_len);nf=flash_off(logical);
    memcpy(base,nor,sizeof base);bad+=exercise(base,nf,1);
    puts(bad?"PIGEON RECOVERY FAILED":"Pigeon recovery tests passed");return bad!=0;
}
