/* SPDX-License-Identifier: GPL-3.0-only */
/* Optional KO storage ABI 2. Keep in sync with tools/ko_bank.py. */
#pragma once
#ifndef FELUCCA_KO_STORAGE
#define FELUCCA_KO_STORAGE 0
#endif
#define KO_APP_HI 0x78000u
#define KO_APP_DATA_HI 0x92000u /* cfg_tool and cfg retain their original last sector */
#define KO_TOP_LO 0xEA000u
#define KO_TOP_HI 0xFC000u
#define KO_BANK_SIZE 0x67000u /* 412 KiB, excludes USR1's metadata sector */
#define KO_BANK_HEAD 4096u
#define KO_BANK_MAGIC 0x32424F4Bu /* KOB2 */
/* JLFS app-area entry at +224, CRC protected and encrypted like its peers. */
#define KO_LAYOUT_ENTRY 224u
#define KO_LAYOUT_NAME "KOPOOL2"

static inline unsigned ko_bank_phys(unsigned off, unsigned *avail)
{
    static const unsigned lo[] = {0xA0000u, 0xB4000u, KO_APP_HI, KO_TOP_LO};
    static const unsigned size[] = {0x13000u, 0x28000u, KO_APP_DATA_HI-KO_APP_HI, KO_TOP_HI-KO_TOP_LO};
    unsigned i;
    for (i = 0; i < 4; i++) {
        if (off < size[i]) { *avail = size[i]-off; return lo[i]+off; }
        off -= size[i];
    }
    *avail = 0; return 0;
}
