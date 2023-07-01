#ifndef RISC_V_DECODE_H
#define RISC_V_DECODE_H

#include <iostream>
#include "utils.h"

class Decode {
public:
    char type_;
    u_int32_t order_;
    RV32I_Order op_;
    u_int32_t imm_;
    u_int8_t rd_, rs1_, rs2_;
    u_int8_t funct3_, funct7_;
public:
    Decode() {}

    Decode(u_int32_t &order) : order_(order) {}

    void SetOrder(u_int32_t &order) {
        order_ = order;
    }

private:
    u_int32_t substr(u_int32_t str, int l, int r) {
        if (r < 31) str = str & ((1 << (r + 1)) - 1);
        return str >> l;
    }

public:
    u_int32_t sext(u_int32_t data, int len = 32) {
        if (len == 32) return data;
        u_int32_t ext = (data >> (len - 1)) & 1 ? (0xffffffff >> len << len) : 0;
        return data | ext;
    }

private:
    u_int8_t get_opcode() { return substr(order_, 0, 6); }

    u_int8_t get_rd() { return substr(order_, 7, 11); }

    u_int8_t get_rs1() { return substr(order_, 15, 19); }

    u_int8_t get_rs2() { return substr(order_, 20, 24); }

    u_int8_t get_funct3() { return substr(order_, 12, 14); }

    u_int8_t get_funct7() { return substr(order_, 25, 31); }

    u_int32_t get_imm_I() { return sext(substr(order_, 20, 31), 12); }

    u_int32_t get_imm_Ij() { return sext(substr(order_, 20, 31) << 1, 13); }

    u_int32_t get_imm_Iu() { return substr(order_, 20, 31); }

    u_int32_t get_imm_U() { return sext(substr(order_, 12, 31) << 12); }

    u_int32_t get_imm_J() {
        u_int32_t imm = 0;
        imm |= substr(order_, 21, 30) << 1;
        imm |= substr(order_, 20, 20) << 11;
        imm |= substr(order_, 12, 19) << 12;
        imm |= substr(order_, 31, 31) << 20;
        return sext(imm, 21);
    }

    u_int32_t get_imm_B() {
        u_int32_t imm = 0;
        imm |= substr(order_, 8, 11) << 1;
        imm |= substr(order_, 25, 30) << 5;
        imm |= substr(order_, 7, 7) << 11;
        imm |= substr(order_, 31, 31) << 12;
        return sext(imm, 13);
    }

    u_int32_t get_imm_S() {
        u_int32_t imm = 0;
        imm |= substr(order_, 7, 11);
        imm |= substr(order_, 25, 31) << 5;
        return sext(imm, 12);
    }

public:
    void decode() {
        u_int8_t opt;
        opt = get_opcode();
        switch (opt) {
            case 0x37:
                type_ = 'U';
                op_ = LUI;
                rd_ = get_rd();
                imm_ = get_imm_U();
                break;
            case 0x17:
                type_ = 'U';
                op_ = AUIPC;
                rd_ = get_rd();
                imm_ = get_imm_U();
                break;
            case 0x6f:
                type_ = 'J';
                op_ = JAL;
                rd_ = get_rd();
                imm_ = get_imm_J();
                break;
            case 0x67:
                type_ = 'I';
                op_ = JALR;
                rd_ = get_rd();
                rs1_ = get_rs1();
                imm_ = get_imm_Ij();
                break;
            case 0x63:
                type_ = 'B';
                funct3_ = get_funct3();
                rs1_ = get_rs1();
                rs2_ = get_rs2();
                imm_ = get_imm_B();
                switch (funct3_) {
                    case 0:
                        op_ = BEQ;
                        break;
                    case 1:
                        op_ = BNE;
                        break;
                    case 4:
                        op_ = BLT;
                        break;
                    case 5:
                        op_ = BGE;
                        break;
                    case 6:
                        op_ = BLTU;
                        break;
                    case 7:
                        op_ = BGEU;
                        break;
                }
                break;
            case 0x03:
                type_ = 'L';
                funct3_ = get_funct3();
                rd_ = get_rd();
                rs1_ = get_rs1();
                imm_ = get_imm_I();
                switch (funct3_) {
                    case 0:
                        op_ = LB;
                        break;
                    case 1:
                        op_ = LH;
                        break;
                    case 2:
                        op_ = LW;
                        break;
                    case 4:
                        op_ = LBU;
                        break;
                    case 5:
                        op_ = LHU;
                        break;
                }
                break;
            case 0x23:
                type_ = 'S';
                funct3_ = get_funct3();
                rs1_ = get_rs1();
                rs2_ = get_rs2();
                imm_ = get_imm_S();
                switch (funct3_) {
                    case 0:
                        op_ = SB;
                        break;
                    case 1:
                        op_ = SH;
                        break;
                    case 2:
                        op_ = SW;
                        break;
                }
                break;
            case 0x13:
                type_ = 'I';
                funct3_ = get_funct3();
                rd_ = get_rd();
                rs1_ = get_rs1();
                switch (funct3_) {
                    case 0:
                        op_ = ADDI;
                        imm_ = get_imm_I();
                        break;
                    case 2:
                        op_ = SLTI;
                        imm_ = get_imm_I();
                        break;
                    case 3:
                        op_ = SLTIU;
                        imm_ = get_imm_I();
                        break;
                    case 4:
                        op_ = XORI;
                        imm_ = get_imm_I();
                        break;
                    case 6:
                        op_ = ORI;
                        imm_ = get_imm_I();
                        break;
                    case 7:
                        op_ = ANDI;
                        imm_ = get_imm_I();
                        break;
                    case 1:
                        op_ = SLLI;
                        imm_ = get_imm_Iu();
                        break;
                    case 5:
                        imm_ = get_imm_Iu();
                        op_ = ((imm_ >> 10) & 1) ? SRAI : SRLI;
                        break;
                }
                if (op_ == SRAI) imm_ = imm_ << 2 >> 2;
                break;
            case 0x33:
                type_ = 'R';
                funct3_ = get_funct3();
                rd_ = get_rd();
                rs1_ = get_rs1();
                rs2_ = get_rs2();
                funct7_ = get_funct7();
                switch (funct3_) {
                    case 0:
                        op_ = ((funct7_ >> 5) & 1) ? SUB : ADD;
                        break;
                    case 1:
                        op_ = SLL;
                        break;
                    case 2:
                        op_ = SLT;
                        break;
                    case 3:
                        op_ = SLTU;
                        break;
                    case 4:
                        op_ = XOR;
                        break;
                    case 5:
                        op_ = ((funct7_ >> 5) & 1) ? SRA : SRL;
                        break;
                    case 6:
                        op_ = OR;
                        break;
                    case 7:
                        op_ = AND;
                        break;
                }
                break;
        }
    }
};

#endif //RISC_V_DECODE_H
