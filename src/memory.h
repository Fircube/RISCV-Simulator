#ifndef RISC_V_MEMORY_H
#define RISC_V_MEMORY_H

#include <iostream>
#include <cstring>

using byte = u_int8_t;
using hfword = u_int16_t;
using word = u_int32_t;
using addr_t = u_int32_t;

template<int memSize>
class Memory {
private:
    u_int8_t mem[memSize];
public:
    Memory() {
        memset(mem, 0, sizeof mem);
    }

    void writeByte(addr_t addr, byte input) {
        mem[addr] = input;
    }

    void writeHfWord(addr_t addr, hfword input) {
        mem[addr] = input & 255;
        mem[addr + 1] = input >> 8 & 255;
    }

    void writeWord(addr_t addr, word input) {
        mem[addr] = input & 255;
        mem[addr + 1] = input >> 8 & 255;
        mem[addr + 2] = input >> 16 & 255;
        mem[addr + 3] = input >> 24 & 255;
    }

    byte readByte(addr_t addr) {
        return mem[addr];
    }

    hfword readHfWord(addr_t addr) {
        return mem[addr] | mem[addr + 1] << 8;
    }

    word readWord(addr_t addr) {
        return mem[addr] | mem[addr + 1] << 8 | mem[addr + 2] << 16 | mem[addr + 3] << 24;
    }
};

#endif //RISC_V_MEMORY_H
