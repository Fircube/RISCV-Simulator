#ifndef RISC_V_ALU_H
#define RISC_V_ALU_H

#include <iostream>
#include "utils.h"

class ALU {
public:
    u_int32_t calc(RV32I_Order op, u_int32_t r1, u_int32_t r2) {
        switch (op) {
            case NOPE:
                return 0;
            case LUI:
            case AUIPC:
            case JAL:
            case ADDI:
            case ADD:
                return r1 + r2;
            case SUB:
                return r1 - r2;
            case JALR:
                return (r1 + r2) & 0xfffffffe;
            case BEQ:
                return r1 == r2;
            case BNE:
                return r1 != r2;
            case BLT:
            case SLTI:
            case SLT:
                return (int32_t) r1 < (int32_t) r2;
            case BGE:
                return (int32_t) r1 >= (int32_t) r2;
            case BLTU:
            case SLTIU:
            case SLTU:
                return r1 < r2;
            case BGEU:
                return r1 >= r2;
            case XORI:
            case XOR:
                return r1 ^ r2;
            case ORI:
            case OR:
                return r1 | r2;
            case ANDI:
            case AND:
                return r1 & r2;
            case SLLI:
            case SLL:
                return r1 << r2;
            case SRLI:
            case SRL:
                return r1 >> r2;
            case SRAI:
            case SRA:
                return (int32_t) r1 >> r2;

            case LB:
            case LH:
            case LW:
            case LBU:
            case LHU:
            case SB:
            case SH:
            case SW:
                return r1 + r2;
        }
        return 0;
    }
};


#endif //RISC_V_ALU_H
