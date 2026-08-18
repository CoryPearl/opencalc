#pragma once

#include <stdint.h>

class Bus;
class Cpu6502;

class Apu2A03 {
public:
    void connectBus(Bus *) {}
    void connectCPU(Cpu6502 *) {}
    void reset() {}
    void cpuWrite(uint16_t, uint8_t) {}
    uint8_t cpuRead(uint16_t) { return 0; }
};

