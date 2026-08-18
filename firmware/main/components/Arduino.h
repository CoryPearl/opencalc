#pragma once

#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef DMA_ATTR
#define DMA_ATTR
#endif

#define HIGH 1
#define LOW 0

static inline void delayMicroseconds(uint32_t us)
{
    esp_rom_delay_us(us);
}

static inline int digitalRead(int)
{
    return HIGH;
}

static inline void digitalWrite(int, int)
{
}

