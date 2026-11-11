#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
// Rebuild every report; never use a key-repeat timer as a release condition.
static inline bool sfc_decode(const uint8_t *report, size_t length, uint16_t *out)
{
    const uint8_t usages[12] = {0x1d,0x04,0x2b,0x28,0x52,0x51,0x50,0x4f,0x1b,0x16,0x14,0x1a};
    *out = 0;
    if (length != 8) return false;
    for (size_t k = 2; k < 8; ++k) {
        if (report[k] >= 1 && report[k] <= 3) { *out = 0; return false; }
        for (unsigned b = 0; b < 12; ++b) if (report[k] == usages[b]) *out |= (uint16_t)(1u << b);
    }
    return true;
}
