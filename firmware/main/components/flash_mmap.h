#pragma once

#include <stdint.h>

class Cartridge;

typedef struct {
    const uint8_t *prg_base;
    const uint8_t *chr_base;
    uint32_t prg_size;
    uint32_t chr_size;
} MappedROM;

static inline void mappedROM_init(MappedROM *, Cartridge *, uint32_t, uint8_t, uint8_t)
{
}

