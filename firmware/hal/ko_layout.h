/* SPDX-License-Identifier: GPL-3.0-only */
/* Optional KO storage ABI 2. Keep in sync with tools/ko_bank.py. */
#pragma once
#ifndef FELUCCA_KO_STORAGE
#define FELUCCA_KO_STORAGE 0
#endif
#define KO_APP_HI 0x78000u
#define KO_EXTRA_LO 0x60000u
#define KO_EXTRA_HI 0x76000u /* retain the boot marker and compatibility sectors */
#define KO_BANK_BASE_SIZE 0x67000u
#define KO_BANK_CAPACITY (KO_BANK_BASE_SIZE+KO_EXTRA_HI-KO_EXTRA_LO)
#define KO_APP_DATA_HI 0x92000u /* cfg_tool and cfg retain their original last sector */
#define KO_TOP_LO 0xEA000u
#define KO_TOP_HI 0xFC000u
#ifdef FELUCCA_WEB
#define KO_BANK_SIZE web_bank_capacity
#else
#define KO_BANK_SIZE KO_BANK_CAPACITY /* 500 KiB: 496 KiB payload + header */
#endif
#define KO_BANK_HEAD 4096u
#define KO_BANK_MAGIC 0x32424F4Bu /* KOB2 */
/* JLFS app-area entry at +224, CRC protected and encrypted like its peers. */
#define KO_LAYOUT_ENTRY 224u
#define KO_LAYOUT_NAME "KOPOOL2"
#define KO_EXTRA_ENTRY 256u
#define KO_EXTRA_NAME "KOPOOL3"

static inline unsigned ko_bank_phys(unsigned off, unsigned *avail)
{
#ifdef FELUCCA_WEB
    if (web_bank_memory) { *avail=off<web_bank_capacity?web_bank_capacity-off:0;return 0x2000000u+off; }
#endif
    /* Append: existing banks retain every original physical byte address. */
    static const unsigned lo[] = {0xA0000u, 0xB4000u, KO_APP_HI, KO_TOP_LO, KO_EXTRA_LO};
    static const unsigned size[] = {0x13000u, 0x28000u, KO_APP_DATA_HI-KO_APP_HI, KO_TOP_HI-KO_TOP_LO, KO_EXTRA_HI-KO_EXTRA_LO};
    unsigned i;
    for (i = 0; i < 5; i++) {
        if (off < size[i]) { *avail = size[i]-off; return lo[i]+off; }
        off -= size[i];
    }
    *avail = 0; return 0;
}
