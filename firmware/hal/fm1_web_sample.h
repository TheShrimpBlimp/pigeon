/* SPDX-License-Identifier: GPL-3.0-only */
/* Browser HAL: user sample offsets address the simulated NOR, beyond factory data. */
#pragma once
#include <stdint.h>
static inline uint8_t fm1_web_sample_byte(uintptr_t address)
{
    return *(const volatile uint8_t *)address;
}
