/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca update loader, the portable part (step 2 of the M-UPGRADE update
 * protocol).
 *
 * The host serves the package ("logical image") with cmd 0x30 reads (ota.c).
 * The loader:
 *   1. reads the UFW header and finds flash.bin;
 *   2. checks that the package's app area decrypts with THIS chip's key
 *      (taken from isd_config.ini in the device's own flash head), so a
 *      package for another key is refused before anything is erased;
 *   2b. reads the whole flash.bin once and checks it against the CRC16 of its
 *      UFW entry (the vendor tools' check), keeping each app sector's CRC16:
 *      a damaged or truncated package is refused before anything is erased;
 *   3. writes ONLY the app area [0x4000, 0x93000), sector by sector, skipping
 *      sectors that are already equal, and verifies each one; a sector that
 *      comes in different from the checked pass is read again, never written;
 *      the flash head (SPL, isd_config) is never written;
 *   4. asks 0xF0000000 ("success"), then invalidates the update record (RAM
 *      and flash: 4K boundary - 256 between 0x93000 and 0xFC000, outside
 *      Felucca's store, where only user data lives) so the SPL boots the new
 *      app, and resets.
 * With a PIGEON boot stub: invalidate its marker before any changed sector,
 * write the marker sector last. An incomplete body boots the staged loader.
 * Legacy firmware without a stub has no cold-boot recovery guarantee.
 *
 * Hooks from the platform (loader.c, ldr_test.c):
 *   ldr_fread(off, p, n)  ldr_erase(off)  ldr_prog(off, p, n)   flash, 0 = ok
 *   ldr_record_clear()    forget the RAM update record
 *   ldr_progress(done, total)
 * plus the ota.c hooks (frames, time, idle). */
#define LDR_APP_LO 0x4000u
#define LDR_APP_HI 0x93000u
#define LDR_REC_LO 0x93000u                     /* update records live above the app ... */
#define LDR_REC_HI 0xFC000u                     /* ... and below Felucca's globals (Felucca's records: 0xE4F00, */
#define LDR_DATA_LO 0x97000u                    /* the stock firmware's: 0xE8F00), never in Felucca's store, whose */
#define LDR_DATA_HI 0xE0000u                    /* bytes (user samples) the host writes */
#define LDR_NSEC ((LDR_APP_HI - LDR_APP_LO) >> 12)
#define LDR_FL_MAX 0x100000u                    /* flash.bin: at most the 1 MiB part */
#include "../hal/ko_layout.h"

static int ldr_fread(uint32_t off, void *p, uint32_t n);
static int ldr_erase(uint32_t off);
static int ldr_prog(uint32_t off, const void *p, uint32_t n);
static void ldr_record_clear(void);
static void ldr_progress(uint32_t done, uint32_t total);

/* SFC cipher: per 32-byte block, key ^ (block offset >> 2) */
static void ldr_sfc(uint8_t *p, uint32_t n, uint32_t base_off, uint32_t key)
{
    uint32_t i, j;
    for (i = 0; i < n; i += 32u) {
        uint32_t k = key ^ ((base_off + i) >> 2);
        for (j = 0; j < 32u && i + j < n; j++) {
            p[i + j] ^= (uint8_t)k;
            k = ((k << 1) ^ (k & 0x8000u ? 0x1021u : 0u)) & 0xFFFFu;
        }
    }
}

/* chip key from the device's flash head: JLFS top entries (ENC 0xFFFF) -> isd_config.ini blob */
static int ldr_chip_key(uint32_t *key)
{
    uint8_t e[32], blob[34];
    uint32_t off, i, sum, k;
    for (off = 32; off < 0x400u; off += 32u) {
        if (ldr_fread(off, e, 32))
            return -1;
        ota_jl_enc(e, 32);
        if (ota_crc16(e + 2, 30, 0) != ota_rd16(e))
            return -2;
        if (!ota_memeq(e + 16, (const uint8_t *)"isd_config.ini", 15)) {
            if (ota_rd16(e + 14))
                return -3;                               /* last entry, not found */
            continue;
        }
        if (ldr_fread(ota_rd32(e + 4), blob, 34) || ota_crc16(blob, 32, 0) != ota_rd16(blob + 32))
            return -4;
        for (i = 0, sum = 0; i < 16u; i++)
            sum += blob[i];
        sum &= 0xFFu;
        sum = sum >= 0xE0u ? 0xAAu : sum <= 0x10u ? 0x55u : sum;
        for (i = 0, k = 0; i < 16u; i++)
            if ((uint32_t)(blob[16 + i] ^ blob[15 - i]) < sum)
                k |= 1u << i;
        *key = k;
        return 0;
    }
    return -3;
}

/* 2b. flash.bin [0, fl_size) from the host, before any erase: its CRC16 must be the UFW entry's (want); sc gets
 * the CRC16 of each app sector as it came in */
static int ldr_check(uint32_t fl_off, uint32_t fl_size, uint32_t want, uint16_t *sc, uint8_t *b)
{
    uint32_t i, n, crc = 0;
    for (i = 0; i < fl_size; i += n) {
        n = fl_size - i > 512u ? 512u : fl_size - i;
        if (ota_read(fl_off + i, b, n))
            return -7;
        crc = ota_crc16(b, n, crc);
        if (i >= LDR_APP_LO && i < LDR_APP_HI) {     /* (512-byte reads from 0: never across a sector) */
            uint32_t k = (i - LDR_APP_LO) >> 12;
            sc[k] = (uint16_t)ota_crc16(b, n, (i & 0xFFFu) ? sc[k] : 0u);
        }
    }
    return crc == want ? 0 : -12;
}

#define PIGEON_MARK_SEC 0x76000u
#define PIGEON_MARK_OFF 0x76FE0u
/* Copyright (C) 2026 ChoralRoot FM-1 contributors: commit-last recovery. */
/* CRC-32 (zlib's), bit by bit: the marker's own 16 bytes only */
static uint32_t ldr_crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu, i;
    while (n--) {
        c ^= *p++;
        for (i = 0; i < 8u; i++)
            c = c & 1u ? (c >> 1) ^ 0xEDB88320u : c >> 1;
    }
    return ~c;
}

/* does the package carry the stub's commit marker? (the 512 bytes that hold it, decrypted in place) */
static int ldr_pkg_marker(uint32_t fl_off, uint8_t *b, uint32_t key)
{
    const uint8_t *m = b + (PIGEON_MARK_OFF & 0x1FFu);
    if (ota_read(fl_off + (PIGEON_MARK_OFF & ~0x1FFu), b, 512))
        return -7;
    ldr_sfc(b, 512, (PIGEON_MARK_OFF & ~0x1FFu) - LDR_APP_LO, key);
    return ota_rd32(m) == 0x4D425243u && ota_rd32(m + 4) == 1u && ota_rd32(m + 16) == ldr_crc32(m, 16);
}

/* the marker's sector erased, once, before the first write elsewhere: from here to the marker's own write the
 * stub refuses the body. Left alone when it is blank already (a resumed install) */
static int ldr_uncommit(uint8_t *cur, uint32_t *open)
{
    uint32_t i, tries;
    if (*open)
        return 0;
    for (tries = 0; tries < 3u; tries++) {
        if (ldr_fread(PIGEON_MARK_SEC, cur, 0x1000u))
            return -8;
        for (i = 0; i < 0x1000u && cur[i] == 0xFFu; i++)
            ;
        if (i == 0x1000u) {
            *open = 1;
            return 0;
        }
        if (tries < 2u && ldr_erase(PIGEON_MARK_SEC))
            return -9;
    }
    return -12;                                          /* it does not erase: nothing else was touched */
}

static int ldr_session(void)
{
    static uint8_t hdr[0x400], sec[0x1000], cur[0x1000];
    static uint16_t sc[LDR_NSEC];
    uint32_t i, k, fl_off, fl_size, ota_off, ota_len, key, s, want = ~0u, done = 0, ko_layout = 0, extra_layout = 0;
    int rc, mark = 0;
    uint32_t open = 0;
    /* 1. UFW header + entry list (ota.c; it leaves the entries decoded in hdr) */
    if ((rc = ota_ufw(hdr, &fl_off, &fl_size, &ota_off, &ota_len)) != 0)
        return rc;
    if (!fl_off || fl_size < LDR_APP_HI || fl_size > LDR_FL_MAX)
        return -3;
    for (i = 0; i < ota_rd16(hdr + 8); i++)
        if (ota_rd16(hdr + 0x40 + i * 0x50u) == 0)
            want = ota_rd16(hdr + 0x40 + i * 0x50u + 4u);   /* flash.bin's data CRC16 */
    /* 2. the package's app area must decrypt with this chip's key */
    if ((rc = ldr_chip_key(&key)) != 0)
        return -40 + rc;
    if (ota_read(fl_off + LDR_APP_LO, sec, 32))
        return -5;
    ldr_sfc(sec, 32, 0, key);
    if (ota_crc16(sec + 2, 30, 0) != ota_rd16(sec))
        return -6;                                       /* package for another chip key */
    /* 2b. the whole package first: nothing is erased for a damaged one */
    if ((rc = ldr_check(fl_off, fl_size, want, sc, sec)) != 0)
        return rc;
    /* A checksum-protected package descriptor authorizes this exact layout.
     * No device-stored length can move the erase boundary. */
    if (ota_read(fl_off+LDR_APP_LO+KO_LAYOUT_ENTRY,sec,32)) return -7;
    ldr_sfc(sec,32,KO_LAYOUT_ENTRY,key);
    if (ota_memeq(sec+16,(const uint8_t *)KO_LAYOUT_NAME,8)) {
        if (ota_crc16(sec+2,30,0) != ota_rd16(sec) || ota_rd32(sec+4) != KO_APP_HI ||
            ota_rd32(sec+8) != KO_APP_DATA_HI-KO_APP_HI) return -14;
        ko_layout = 1;
    }
    if (ota_read(fl_off+LDR_APP_LO+KO_EXTRA_ENTRY,sec,32)) return -7;
    ldr_sfc(sec,32,KO_EXTRA_ENTRY,key);
    if(ota_memeq(sec+16,(const uint8_t *)KO_EXTRA_NAME,8)) {
        if(!ko_layout || ota_crc16(sec+2,30,0)!=ota_rd16(sec) || ota_rd32(sec+4)!=KO_EXTRA_LO ||
           ota_rd32(sec+8)!=KO_EXTRA_HI-KO_EXTRA_LO) return -14;
        extra_layout=1;
    }
    if (ldr_fread(0xA0000u,cur,12)) return -8;
    if (!ko_layout && ota_rd32(cur) == KO_BANK_MAGIC) return -15; /* export/erase bank before downgrade */
    if(!extra_layout && ota_rd32(cur)==KO_BANK_MAGIC && ota_rd32(cur+8)>KO_BANK_BASE_SIZE) return -15;
    if (ko_layout && (mark = ldr_pkg_marker(fl_off, sec, key)) < 0) return mark;
    if(extra_layout && (!mark || ota_rd32(sec+(PIGEON_MARK_OFF&0x1FFu)+8)>KO_EXTRA_LO-0x5000u))return -14;
    /* 3. app area, sector by sector */
    for (uint32_t order = 0; order < LDR_NSEC; order++) {
        s = LDR_APP_LO + order * 0x1000u;
        /* Commit sector last; the intervening KOPOOL2 sectors remain untouched. */
        if (mark) {
            if (order == LDR_NSEC - 1u) s = PIGEON_MARK_SEC;
            else if (s >= PIGEON_MARK_SEC) s += 0x1000u;
        }
        if (ko_layout && s >= KO_APP_HI && s < KO_APP_DATA_HI) {
            done += 0x1000u; ldr_progress(done,LDR_APP_HI-LDR_APP_LO); continue;
        }
        if(extra_layout && s>=KO_EXTRA_LO && s<KO_EXTRA_HI) {
            done+=0x1000u;ldr_progress(done,LDR_APP_HI-LDR_APP_LO);continue;
        }
        uint32_t again;
        for (again = 0;; again++) {                      /* as checked in 2b, else read it again */
            for (k = 0; k < 0x1000u; k += 512u)
                if (ota_read(fl_off + s + k, sec + k, 512))
                    return -7;
            if (ota_crc16(sec, 0x1000u, 0) == sc[(s - LDR_APP_LO) >> 12])
                break;
            if (again == 2u)
                return -13;
        }
        if (ldr_fread(s, cur, 0x1000u))
            return -8;
        if (!ota_memeq(sec, cur, 0x1000u)) {
            if (mark && s != PIGEON_MARK_SEC && (rc = ldr_uncommit(cur, &open)) != 0) return rc;
            uint32_t tries;
            for (tries = 0; tries < 2u; tries++) {
                if (ldr_erase(s))
                    return -9;
                for (k = 0; k < 0x1000u; k += 256u)
                    if (ldr_prog(s + k, sec + k, 256))
                        return -10;
                if (ldr_fread(s, cur, 0x1000u))
                    return -8;
                if (ota_memeq(sec, cur, 0x1000u))
                    break;
            }
            if (tries == 2u)
                return -11;                              /* verify failed twice */
        }
        done += 0x1000u;
        ldr_progress(done, LDR_APP_HI - LDR_APP_LO);
    }
    /* 4. finish: the host confirms, the record goes, the SPL boots the app */
    for (i = 0; i < 4u; i++)
        if (!ota_read(0xF0000000u, sec, 8))
            break;
    if (i == 4u) return -16; /* No successful finish: retain recovery state. */
    ldr_record_clear();
    for (s = LDR_REC_HI; s > LDR_REC_LO; s -= 0x1000u) {   /* flash records: 4K boundary - 256 */
        uint8_t r[80];
        if (s - 0x1000u >= LDR_DATA_LO && s - 0x1000u < LDR_DATA_HI)
            continue;                                    /* Felucca's store: user data that may look like one */
        if (s-0x1000u >= KO_TOP_LO && s-0x1000u < KO_TOP_HI) continue;
        if (ldr_fread(s - 0x100u, r, 80))
            break;
        if (ota_rd16(r + 6) == 0x5441u && ota_rd16(r) && ota_rd16(r) == ota_crc16(r + 2, 78, 0))
            ldr_erase(s - 0x1000u);
    }
    return 0;
}
