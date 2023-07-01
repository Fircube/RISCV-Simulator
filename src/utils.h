#ifndef RISC_V_UTILS_H
#define RISC_V_UTILS_H

#include <iostream>

enum RV32I_Order {
    NOPE,

    LUI, AUIPC,

    JAL, JALR,

    BEQ, BNE, BLT, BGE, BLTU, BGEU,

    LB, LH, LW, LBU, LHU,

    SB, SH, SW,

    ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI,

    ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND,
};

template <typename T, int size = 32>
class Queue{
public:
    int len;
    int head,tail;
    T data[size];

    Queue():len(0),head(0),tail(0){}

    bool ifEmpty(){
        return len==0;
    }

    bool ifFull(){
        return len==size;
    }

    void enQueue(T value){
        if(tail==size) tail=0;
        data[tail]=value;
        ++tail;
        ++len;
    }

    void deQueue(){
        if(head==size-1) head=0;
        else ++head;
        --len;
    }

    void clear(){
        len=head=tail=0;
    }

    T& operator [] (int id) {
        return data[(id+head)%size];
    }

    T& getVal(int id){
        return data[(id-1)%size];
    }
};


#endif //RISC_V_UTILS_H
