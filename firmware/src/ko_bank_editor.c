/* SPDX-License-Identifier: GPL-3.0-only */
/* Commands 90..96, explicitly negotiated before any erase. All addresses are
 * logical pool offsets, never host-supplied physical flash addresses. */
static ko_bank_hdr_t ko_upload_header;
static uint32_t ko_upload_total, ko_upload_next, ko_upload_hbytes, ko_metadata_next;
static uint32_t ko_wire_u21(const uint8_t *a) { return a[0] | (uint32_t)a[1]<<7 | (uint32_t)a[2]<<14; }
static void ko_wire_out(uint32_t v) { ed_b(v); ed_b(v>>7); ed_b(v>>14); }
static int ko_bank_handle(uint32_t cmd, const uint8_t *a, uint32_t na)
{
    if (cmd < 90 || cmd > 96) return 0;
    autosave_hold();
    if (cmd == 90) {
        ed_b(0x4B); ed_b(2); ko_wire_out(KO_BANK_SIZE); ko_wire_out(KO_BANK_HEAD);
        ed_b(ko_bank_active); ed_b(3); /* codecs: IMA and MP3 */
        return 1;
    }
    if (!flash_ok) { ed_b(4); return 1; }
    if (cmd == 94) { /* read includes the separate metadata sector at virtual bank end */
        if (na != 5) { ed_b(1); return 1; }
        uint32_t off = ko_wire_u21(a), n = a[3] | (uint32_t)a[4]<<7;
        if (!n || n > 256 || off > KO_BANK_SIZE+4096 || n > KO_BANK_SIZE+4096-off) { ed_b(1); return 1; }
        ed_b(0);
        for (uint32_t i = 0; i < n; i+=7) {
            uint32_t count = n-i > 7 ? 7 : n-i, mask = 0;
            for (uint32_t j=0;j<count;j++) {
                uint32_t p = off+i+j;
                ed_smp_buf[j] = p < KO_BANK_SIZE ? ko_bank_byte(p) : smp_user_xip(0)[SLC_REC_OFF+p-KO_BANK_SIZE];
                mask |= (uint32_t)(ed_smp_buf[j]>>7)<<j;
            }
            ed_b(mask); for (uint32_t j=0;j<count;j++) ed_b(ed_smp_buf[j]);
        }
        return 1;
    }
    if (ed_flash_stop()) { ed_b(1); return 1; }
    if (cmd == 96) {
        if (na) { ed_b(1); return 1; }
        ko_upload_total=0; ko_metadata_next=0; ko_bank_active=0; ko_pcm_reset();
        for (uint32_t k=0;k<SMP_USER_SLOTS;k++) {
            usr_nz[k]=0; memset(usr_zone[k],0,sizeof usr_zone[k]); slc_user_scan(k,0);
            if (st_erase(SMP_USER_BASE+k*SMP_USER_SIZE)) { ed_b(4); return 1; }
        }
        smp_user_gen++; ed_b(0); return 1;
    }
    if (cmd == 91) {
        if (na != 3) { ed_b(1); return 1; }
        uint32_t total = ko_wire_u21(a);
        if (total < KO_BANK_HEAD || total > KO_BANK_SIZE) { ed_b(1); return 1; }
        ko_upload_total = 0; ko_metadata_next=0; ko_bank_active = 0; ko_pcm_reset();
        for (uint32_t k=0;k<SMP_USER_SLOTS;k++) {
            usr_nz[k]=0; memset(usr_zone[k],0,sizeof usr_zone[k]); slc_user_scan(k,0);
        }
        smp_user_gen++;
        if (st_erase(SMP_USER_BASE)) { ed_b(4); return 1; }
        ko_upload_total=total; ko_upload_next=KO_BANK_HEAD; ko_upload_hbytes=0;
        memset(&ko_upload_header,0,sizeof ko_upload_header); ed_b(0); return 1;
    }
    if (cmd == 92 || cmd == 95) {
        if (na < 5) { ed_b(1); return 1; }
        uint32_t off=ko_wire_u21(a), n=ed_unpack7(a+3,na-3,ed_smp_buf,256);
        if (!n || (off&255u)) { ed_b(1); return 1; }
        if (cmd == 95) {
            if (!ko_bank_active || off >= 4096 || n != 256 || (off && off != ko_metadata_next)) { ed_b(1); return 1; }
            uint32_t phys=SMP_USER_BASE+SLC_REC_OFF+off;
            if (!off) { ko.dirty=0; ko_metadata_next=0; }
            if ((!off && st_erase(phys)) || st_prog(phys,ed_smp_buf,n)) { ed_b(4); return 1; }
            ko_metadata_next=off+n;
            if (ko_metadata_next==4096) { ko.loaded=0; smp_user_gen++; }
            ed_b(0); return 1;
        }
        if (!ko_upload_total) { ed_b(1); return 1; }
        if (off < KO_BANK_HEAD) {
            if (off != ko_upload_hbytes || n > sizeof ko_upload_header-off || off > sizeof ko_upload_header) {
                ed_b(1); return 1;
            }
            memcpy((uint8_t *)&ko_upload_header+off,ed_smp_buf,n); ko_upload_hbytes+=n; ed_b(0); return 1;
        }
        if (off > ko_upload_total || n > ko_upload_total-off || off > ko_upload_next) { ed_b(1); return 1; }
        if (off < ko_upload_next) {
            uint32_t same=off+n<=ko_upload_next;
            for (uint32_t i=0;i<n;i++) same &= ed_smp_buf[i]==ko_bank_byte(off+i);
            ed_b(same?0:2); return 1;
        }
        uint32_t avail,phys=ko_bank_phys(off,&avail);
        if (n>avail || ((!(off&4095u)) && st_erase(phys)) || st_prog(phys,ed_smp_buf,n)) { ed_b(4); return 1; }
        ko_upload_next+=n; ed_b(0); return 1;
    }
    /* commit, header last; failed CRC cannot publish an incomplete bank */
    if (na || !ko_upload_total || ko_upload_next!=ko_upload_total || ko_upload_hbytes!=sizeof ko_upload_header ||
        ko_upload_header.used!=ko_upload_total || !ko_bank_header_valid(&ko_upload_header)) { ed_b(2); return 1; }
    ko_bank=ko_upload_header;
    if (ko_bank_crc(KO_BANK_HEAD,ko_bank.used-KO_BANK_HEAD)!=ko_bank.crc || !ko_bank_index_valid()) { ed_b(3); return 1; }
    if (st_prog(SMP_USER_BASE,&ko_upload_header,sizeof ko_upload_header)) { ed_b(4); return 1; }
    ko_bank_scan(); ed_b(ko_bank_active?0:5); return 1;
}
