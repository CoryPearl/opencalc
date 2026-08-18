#pragma once

#include <stdint.h>

class TFT_eSPI {
public:
    void pushPixelsDMA(const uint16_t*, int) {}
    void pushPixels(const uint16_t*, int) {}
};

