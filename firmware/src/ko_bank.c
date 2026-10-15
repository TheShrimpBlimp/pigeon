/* SPDX-License-Identifier: GPL-3.0-only */
/* Flash-backed shared song/kit pool. Header is committed last; legacy FSMP
 * remains readable until a bank upload explicitly replaces it. */
#define KO_VOFF 0x40000000u
enum { KO_CODEC_IMA = 1, KO_CODEC_MP3 = 2 };
typedef struct {
    uint32_t magic, version, used, crc, codec, rate, samples, song_off, song_len;
    uint32_t kit_off, kit_len, kit_n, frames, skip, wave_off, wave_bins, auto_n, rsv[3];
    uint32_t apos[32];
    smp_zone_t zone[16];
    char name[8];
    uint32_t hcrc;
} ko_bank_hdr_t;
_Static_assert(sizeof(ko_bank_hdr_t) == 668u, "KOB2 header ABI");
static ko_bank_hdr_t ko_bank,ko_bank_second;
static uint32_t ko_bank_second_crc;
static uint8_t ko_bank_active;
static uint32_t ko_bank_song_crc, ko_bank_kit_crc;
#ifndef KO_BANK_XIP
#define KO_BANK_XIP(off) fm1_xip_ptr(off)
#endif
static uint8_t ko_bank_byte(uint32_t off)
{
    uint32_t avail, phys = ko_bank_phys(off, &avail);
    return avail ? KO_BANK_XIP(phys)[0] : 0;
}
static void ko_bank_read(uint32_t off, void *dst, uint32_t n)
{
    uint8_t *p = dst;
    /* Translate each contiguous extent once, rather than once per byte of
     * every decoder refill. Never copy across a physical extent boundary. */
    while (n) {
        uint32_t avail, phys = ko_bank_phys(off, &avail);
        if (!avail) { memset(p,0,n); return; }
        uint32_t take = n < avail ? n : avail;
        memcpy(p,KO_BANK_XIP(phys),take);
        p+=take;off+=take;n-=take;
    }
}
static uint32_t ko_bank_u32(uint32_t off)
{
    return ko_bank_byte(off) | (uint32_t)ko_bank_byte(off+1)<<8 |
           (uint32_t)ko_bank_byte(off+2)<<16 | (uint32_t)ko_bank_byte(off+3)<<24;
}
static uint32_t ko_crc_add(uint32_t c, uint8_t b)
{
    c ^= b;
    for (uint32_t j = 0; j < 8; j++) c = (c >> 1) ^ (0xEDB88320u & (0u-(c&1u)));
    return c;
}
static uint32_t ko_bank_crc(uint32_t off, uint32_t n)
{
    uint32_t c = ~0u;
    while (n--) c = ko_crc_add(c, ko_bank_byte(off++));
    return ~c;
}
static int ko_bank_range(uint32_t off, uint32_t n, uint32_t used)
{
    return off >= KO_BANK_HEAD && off <= used && n <= used-off;
}
static int ko_bank_header_valid(const ko_bank_hdr_t *h)
{
    uint32_t c = ~0u, i;
    const uint8_t *b = (const uint8_t *)h;
    for (i = 0; i < sizeof *h-4; i++) c = ko_crc_add(c, b[i]);
    if (h->magic != KO_BANK_MAGIC || (h->version != 2 && !(FELUCCA_KO_DUAL && h->version==3)) || h->hcrc != ~c ||
        h->used < KO_BANK_HEAD || h->used > KO_BANK_SIZE ||
        h->codec < KO_CODEC_IMA || h->codec > KO_CODEC_MP3 ||
        h->rate < 4000u || h->rate > 44100u || !h->samples ||
#ifndef FELUCCA_WEB
        h->samples > 8000000u ||
#endif
        0 ||
        !ko_bank_range(h->song_off,h->song_len,h->used) || !h->song_len ||
        !ko_bank_range(h->kit_off,h->kit_len,h->used) || h->kit_n > 16 ||
        !h->auto_n || h->auto_n > 32 || h->apos[0] ||
        !h->wave_bins || h->wave_bins > 4096 || !ko_bank_range(h->wave_off,h->wave_bins*2,h->used) ||
        h->song_off < h->kit_off+h->kit_len || h->wave_off < h->song_off+h->song_len)
        return 0;
    if (h->version==3) { if (h->rsv[0]!=sizeof *h || h->rsv[1] || h->rsv[2]) return 0; }
    else for (i = 0; i < 3; i++) if (h->rsv[i]) return 0;
    for (i = 0; i < h->auto_n; i++)
        if (h->apos[i] >= h->samples || (i && h->apos[i] <= h->apos[i-1])) return 0;
    for (i = 0; i < h->kit_n; i++) {
        const smp_zone_t *z = &h->zone[i];
        if (z->off > h->kit_len || !z->n || z->n > 2u*(h->kit_len-z->off) ||
            !z->rate || z->rate > 65536u || z->idx > 88 || z->root16 != (60+(int32_t)i)*16 ||
            z->lo != 60+i || z->hi != 60+i || z->looped || z->ls > z->le || z->le >= z->n)
            return 0;
    }
    if (h->codec == KO_CODEC_IMA)
        return !h->frames && !h->skip && h->samples <= h->song_len*2u;
    return h->rate == 22050u && h->frames >= 3u && (h->frames < 16000u ||
#ifdef FELUCCA_WEB
           web_bank_capacity>0x7D000u ||
#endif
           0) &&
           (h->frames+1u)*4u < h->song_len && h->skip <= 2304u && h->skip < h->frames*576u &&
           h->samples <= h->frames*576u-h->skip;
}
static int ko_bank_index_of(const ko_bank_hdr_t *h)
{
    if (h->codec != KO_CODEC_MP3) return 1;
    uint32_t table = (h->frames+1u)*4u, prev = table;
    if (ko_bank_u32(h->song_off) != table) return 0;
    for (uint32_t i = 0; i < h->frames; i++) {
        uint32_t next = ko_bank_u32(h->song_off+(i+1u)*4u), at = h->song_off+prev;
        /* Supported subset: MPEG2 Layer III mono, 22.05k, no reservoir,
         * 8..64kbps. Fixed bounds keep seek work <= three frames. */
        uint8_t b0 = ko_bank_byte(at), b1 = ko_bank_byte(at+1), b2 = ko_bank_byte(at+2), b3 = ko_bank_byte(at+3);
        static const uint8_t kbps[9] = {0,8,16,24,32,40,48,56,64};
        uint32_t br = b2>>4, side = at+4u+((b1&1u)?0u:2u);
        if (next <= prev || next > h->song_len || next-prev > 210u || b0 != 255 ||
            (b1 & 0xFEu) != 0xF2u || (b2&12u) || br < 1 || br > 8 || (b3>>6) != 3 ||
            next-prev != 72000u*kbps[br]/22050u+((b2>>1)&1u) || ko_bank_byte(side)) return 0;
        prev = next;
    }
    return prev == h->song_len;
}
static int ko_bank_second_valid(const ko_bank_hdr_t *first,const ko_bank_hdr_t *second)
{
    return first->version==2 || (FELUCCA_KO_DUAL && second->version==2 &&
        ko_bank_header_valid(second) && second->used==first->used && second->crc==first->crc &&
        second->kit_n==0 && second->kit_len==0 && second->song_off>=first->wave_off+first->wave_bins*2u &&
        ko_bank_index_of(second));
}
static int ko_bank_index_valid(void) { return ko_bank_index_of(&ko_bank); }
static void ko_bank_scan(void)
{
    ko_bank_active = 0;ko_bank_second_crc=0;
    memset(&ko_bank_second,0,sizeof ko_bank_second);
    for (uint32_t k = 0; k < SMP_USER_SLOTS; k++) {
        usr_nz[k] = 0; memset(usr_zone[k], 0, sizeof usr_zone[k]); slc_user_scan(k,0);
    }
    ko_bank_read(0,&ko_bank,sizeof ko_bank);
    if (ko_bank.version==3) ko_bank_read(sizeof ko_bank,&ko_bank_second,sizeof ko_bank_second);
    if (!ko_bank_header_valid(&ko_bank) || ko_bank_crc(KO_BANK_HEAD,ko_bank.used-KO_BANK_HEAD) != ko_bank.crc ||
        !ko_bank_index_valid() || !ko_bank_second_valid(&ko_bank,&ko_bank_second)) return;
    ko_bank_active = 1;
    ko_bank_song_crc = ko_bank_crc(ko_bank.song_off,ko_bank.song_len);
    ko_bank_kit_crc = ko_bank_crc(ko_bank.kit_off,ko_bank.kit_len);
    usr_nz[0] = 1;
    usr_zone[0][0] = (smp_zone_t){.off=KO_VOFF+ko_bank.song_off,.n=ko_bank.codec==KO_CODEC_IMA?ko_bank.samples:0,
        .le=ko_bank.samples-1,.rate=(ko_bank.rate*65536u+22050u)/44100u,.root16=960,.hi=127};
    usr_nz[1] = ko_bank.kit_n;
    for (uint32_t i = 0; i < ko_bank.kit_n; i++) {
        usr_zone[1][i] = ko_bank.zone[i]; usr_zone[1][i].off += KO_VOFF+ko_bank.kit_off;
    }
    if (ko_bank.version==3) {
        ko_bank_second_crc=ko_bank_crc(ko_bank_second.song_off,ko_bank_second.song_len);
        usr_nz[2]=1;
        usr_zone[2][0]=(smp_zone_t){.off=KO_VOFF+ko_bank_second.song_off,
            .n=ko_bank_second.codec==KO_CODEC_IMA ? ko_bank_second.samples : 0,
            .le=ko_bank_second.samples-1,.rate=(ko_bank_second.rate*65536u+22050u)/44100u,.root16=960,.hi=127};
    }
    slc_user_scan(2,usr_nz[2]!=0);
    slc_user_scan(0,1); slc_user_scan(1,ko_bank.kit_n != 0);
    smp_user_gen++;
}
