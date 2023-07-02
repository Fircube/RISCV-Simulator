#ifndef RISC_V_SIMULATOR_H
#define RISC_V_SIMULATOR_H

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>
#include "alu.h"
#include "decode.h"
#include "memory.h"
#include "predict.h"
#include "utils.h"

const int size = 0x1000000;
int Clock = 0;
Memory<size> memory;
TwoLevelPredictor predictor;
u_int32_t PC = 0;

enum State {
    empty, waitingCDB, executed,
    getAddr, loading, waitingStore, storing
};

struct instruction_queue {
    u_int32_t pc = 0;
    u_int32_t order = 0;
    bool jump = false;
};

struct InstructionQueue {
public:
    bool stall_ = false;
    bool end_ = false;
    Queue<instruction_queue> buffer_;

    bool ifEmpty() { return buffer_.ifEmpty(); }

    bool ifFull() { return buffer_.ifFull(); }

    void enQueue(u_int32_t pc, u_int32_t order, bool jump = false) {
        buffer_.enQueue((instruction_queue) {pc, order, jump});
    }

    void deQueue() { buffer_.deQueue(); }

    void Flush() {
        stall_ = false;
        end_ = false;
        buffer_.clear();
    }

} isq;

struct CommonDataBus {
    int entry = 0; // ROB entry tag
    u_int32_t result = 0;
} cdb;

struct reg_file {
    int entry = 0; // ROB entry tag
    u_int32_t val = 0;
};

class RegFile { // RegFile / FP registers
public:
    reg_file Reg_[32];

    void Flush() {
        for (auto &i: Reg_) { i.entry = 0; }
    }
} rf;

struct reorder_buffer {
    char type = 0; // A、L、S、B
    bool ready = false;
    int entry = 0;
    u_int32_t order = 0;
    u_int32_t dest = 0; // reg->A、L, Addr->S, Nope->B
    u_int32_t val = 0; // regVal, StoreData
    bool jump = false;
    u_int32_t pc_now_ = 0;
    u_int32_t pc_des_ = 0;
};

class ReorderBuffer { // Reorder Buffer
public:
    int entry_num_ = 1;
    Queue<reorder_buffer> buffer_;

    bool ifEmpty() { return buffer_.ifEmpty(); }

    bool ifFull() { return buffer_.ifFull(); }

    void deQueue() { buffer_.deQueue(); }

    int NewEntry() const { return entry_num_; }

    void Flush() {
        entry_num_ = 1;
        buffer_.clear();
    }

    void Issue(Decode &decoder, u_int32_t pc_now, u_int32_t pc_des = 0, bool jump = false) {
        reorder_buffer tmp;
        tmp.type = decoder.type_;
        tmp.order = decoder.order_;
        tmp.pc_now_ = pc_now;
        tmp.pc_des_ = pc_des;
        tmp.jump = jump;
        if (decoder.type_ != 'B' && decoder.type_ != 'S') {
            tmp.dest = decoder.rd_;
            tmp.entry = NewEntry();
            ++entry_num_;
            if (tmp.dest) { // 非F0
                rf.Reg_[tmp.dest].entry = tmp.entry;
            }
        } else {
            tmp.entry = NewEntry();
            ++entry_num_;
        }
        buffer_.enQueue(tmp);
    }

    void Reception() {
        buffer_.getVal(cdb.entry).ready = true;
        buffer_.getVal(cdb.entry).val = cdb.result;
    }
} rob;

struct reservation_station {
    State state = empty;
    RV32I_Order op = NOPE;
    int entry = 0; // ROB entry tag
    u_int32_t Qj = 0, Qk = 0;
    u_int32_t Vj = 0, Vk = 0;
    u_int32_t result = 0;

    bool Ready() const {
        return !(Qj || Qk);
    }
};

class ReservationStation { // Reservation Stations
public:
    static const int kNum = 6;
    reservation_station sta_[kNum];
    std::vector<int> idx_;

    ReservationStation() {
        for (int i = kNum - 1; i >= 0; --i) idx_.push_back(i);
    }

    bool ifFull() const { return idx_.empty(); }

    int AssignTag() {
        int tag = idx_.back();
        idx_.pop_back();
        return tag;
    }

    void RestoreTag(int tag) { idx_.push_back(tag); }

    void Flush() {
        idx_.clear();
        reservation_station tmp;
        for (int i = kNum - 1; i >= 0; --i) {
            idx_.push_back(i);
            sta_[i] = tmp;
        }
    }

    void Issue(int entry, Decode &decoder) { // B、I、R
        int tag = AssignTag();
        sta_[tag].state = waitingCDB;
        sta_[tag].op = decoder.op_;
        sta_[tag].entry = entry;
        int rs1 = decoder.rs1_, e = rf.Reg_[rs1].entry;
        if (e) {
            if (rob.buffer_.getVal(e).ready) sta_[tag].Vj = rob.buffer_.getVal(e).val;
            else sta_[tag].Qj = e;
        } else sta_[tag].Vj = rf.Reg_[rs1].val;
        if (decoder.type_ == 'I') {
            sta_[tag].Vk = decoder.imm_;
            sta_[tag].Qk = 0;
        } else {
            int rs2 = decoder.rs2_;
            e = rf.Reg_[rs2].entry;
            if (e) {
                if (rob.buffer_.getVal(e).ready) sta_[tag].Vk = rob.buffer_.getVal(e).val;
                else sta_[tag].Qk = e;
            } else sta_[tag].Vk = rf.Reg_[rs2].val;
        }
    }

    void Execute() {
        ALU alu;
        for (auto &i: sta_) {
            if (i.state == waitingCDB && i.Ready()) {
                i.result = alu.calc(i.op, i.Vj, i.Vk);
                i.state = executed;
                if (i.op == JALR) {
                    u_int32_t tmp = PC;
                    PC = i.result;
                    i.result = tmp + 4;
                    isq.stall_ = false;
                }
                break;
            }
        }
    }

    bool Broadcast() {
        for (int i = 0; i < kNum; ++i) {
            if (sta_[i].state == executed) {
                cdb = (CommonDataBus) {sta_[i].entry, sta_[i].result};
                sta_[i].state = empty;
                RestoreTag(i);
                return true;
            }
        }
        return false;
    }

    void Reception() {
        for (auto &i: sta_) {
            if (i.state == waitingCDB) {
                if (i.Qj == cdb.entry) i.Qj = 0, i.Vj = cdb.result;
                else if (i.Qk == cdb.entry) i.Qk = 0, i.Vk = cdb.result;
            }
        }
    }
} rs;

struct load_buffer {
    State state = empty;
    RV32I_Order op = NOPE;
    char type = 0;
    int entry = 0; // ROB entry tag
    u_int32_t Qj = 0, Qk = 0;
    u_int32_t Vj = 0, Vk = 0;
    u_int32_t StoreAddr = 0;
    u_int32_t StoreData = 0;
    int time = 0;

    bool Ready() const {
        return !(Qj || Qk);
    }
};

class LoadBuffer { // Load Buffers
    static const int kNum = 3;
public:
    load_buffer sta_[kNum];
    std::vector<int> idx_;

    struct load_clock {
        int tag = 0;
        int time = 0;
    } loadClock_;
    struct store_clock {
        int tag = 0;
        int time = 0;
    } storeClock_;

    LoadBuffer() {
        for (int i = kNum - 1; i >= 0; --i) idx_.push_back(i);
    }

    bool ifFull() const {
        return idx_.empty();
    }

    int AssignTag() {
        int tag = idx_.back();
        idx_.pop_back();
        return tag;
    }

    void RestoreTag(int tag) {
        idx_.push_back(tag);
    }

    void Flush() {
        loadClock_.time = storeClock_.time = 0;
        loadClock_.tag = storeClock_.tag = 0;
        idx_.clear();
        load_buffer tmp;
        for (int i = kNum - 1; i >= 0; --i) {
            idx_.push_back(i);
            sta_[i] = tmp;
        }
    }

    void Issue(int entry, Decode &decoder) { // L、B
        int tag = AssignTag();
        sta_[tag].state = waitingCDB;
        sta_[tag].op = decoder.op_;
        sta_[tag].entry = entry;
        sta_[tag].time = Clock;
        if (decoder.type_ == 'L') {
            sta_[tag].type = 'L';
            int rs1 = decoder.rs1_, e = rf.Reg_[rs1].entry;
            if (e) {
                if (rob.buffer_.getVal(e).ready) sta_[tag].Vj = rob.buffer_.getVal(e).val;
                else sta_[tag].Qj = e;
            } else sta_[tag].Vj = rf.Reg_[rs1].val;
            sta_[tag].Vk = decoder.imm_;
            sta_[tag].Qk = 0;
        } else {
            sta_[tag].type = 'S';
            sta_[tag].Vj = decoder.imm_;
            int rs1 = decoder.rs1_, e = rf.Reg_[rs1].entry;
            if (e) {
                if (rob.buffer_.getVal(e).ready) sta_[tag].Vj += rob.buffer_.getVal(e).val;
                else sta_[tag].Qj = e;
            } else sta_[tag].Vj += rf.Reg_[rs1].val;
            int rs2 = decoder.rs2_;
            e = rf.Reg_[rs2].entry;
            if (e) {
                if (rob.buffer_.getVal(e).ready) sta_[tag].Vk = rob.buffer_.getVal(e).val;
                else sta_[tag].Qk = e;
            } else sta_[tag].Vk = rf.Reg_[rs2].val;
        }
    }

    void Execute() {
        if (loadClock_.time) {
            loadClock_.time--;
            if (!loadClock_.time) sta_[loadClock_.tag].state = executed;
        }
        for (int i = 0; i < kNum; i++) {
            if (sta_[i].state == getAddr && sta_[i].type == 'L') {
                int latestStore = sta_[i].time;
                bool prepared = true;
                for (int j = 0; j < kNum; j++) {
                    if (sta_[j].type == 'S' && sta_[j].state == waitingCDB && sta_[j].time < sta_[i].time) {
                        prepared = false;
                        break;
                    }
                    if (sta_[j].state == waitingStore || sta_[j].state == storing || sta_[j].state == executed) {
                        if ((sta_[i].StoreAddr == sta_[j].StoreAddr) &&
                            ((latestStore < sta_[j].time && sta_[j].time < sta_[i].time) ||
                             (latestStore == sta_[i].time && sta_[j].time < sta_[i].time))) {
                            latestStore = sta_[j].time;
                            sta_[i].StoreData = sta_[j].StoreData;
                        }
                    }
                }
                if (!prepared)continue;
                if (latestStore != sta_[i].time) {
                    sta_[i].state = executed;
                    continue;
                } else if (!loadClock_.time) {
                    Decode decoder;
                    if (sta_[i].op == LB) sta_[i].StoreData = decoder.sext(memory.readByte(sta_[i].StoreAddr), 8);
                    else if (sta_[i].op == LH)
                        sta_[i].StoreData = decoder.sext(memory.readHfWord(sta_[i].StoreAddr), 16);
                    else if (sta_[i].op == LW) sta_[i].StoreData = decoder.sext(memory.readWord(sta_[i].StoreAddr), 32);
                    else if (sta_[i].op == LBU) sta_[i].StoreData = (u_int32_t) memory.readByte(sta_[i].StoreAddr);
                    else if (sta_[i].op == LHU) sta_[i].StoreData = (u_int32_t) memory.readHfWord(sta_[i].StoreAddr);
                    sta_[i].state = loading;
                    loadClock_.tag = i;
                    loadClock_.time = 3;
                }
            }
        }

        for (auto &i: sta_) {
            if (i.state == waitingCDB && i.Ready()) {
                if (i.type == 'L') {
                    i.StoreAddr = i.Vj + i.Vk;
                    i.state = getAddr;
                } else if (i.type == 'S') {
                    i.StoreAddr = rob.buffer_.getVal(i.entry).dest = i.Vj;
                    i.StoreData = i.Vk;
                    i.state = executed;
                }
                break;
            }
        }
    }

    bool Broadcast() {
        for (int i = 0; i < kNum; ++i) {
            if (sta_[i].state == executed && sta_[i].type == 'L') {
                cdb = (CommonDataBus) {sta_[i].entry, sta_[i].StoreData};
                sta_[i].state = empty;
                RestoreTag(i);
                return true;
            }
            if (sta_[i].state == executed && sta_[i].type == 'S') {
                cdb = (CommonDataBus) {sta_[i].entry, sta_[i].StoreData};
                sta_[i].state = waitingStore;
                return true;
            }
        }
        return false;
    }

    void Reception() {
        for (auto &i: sta_) {
            if (i.state == waitingCDB) {
                if (i.Qj == cdb.entry) {
                    if (i.type == 'L') {
                        i.Qj = 0, i.Vj = cdb.result;
                    } else {
                        i.Qj = 0, i.Vj += cdb.result;
                    }
                } else if (i.Qk == cdb.entry) i.Qk = 0, i.Vk = cdb.result;
            }
        }
    }

    void Commit(int entry) {
        for (int i = 0; i < kNum; i++) {
            if (sta_[i].entry == entry) {
                sta_[i].state = storing;
                storeClock_.tag = i;
                storeClock_.time = 3;
                break;
            }
        }
    }

    bool Storing() {
        if (storeClock_.time) {
            storeClock_.time--;
            if (!storeClock_.time) {
                int i = storeClock_.tag;
                if (sta_[i].op == SB) memory.writeByte(sta_[i].StoreAddr, sta_[i].StoreData & 0xff);
                else if (sta_[i].op == SH) memory.writeHfWord(sta_[i].StoreAddr, sta_[i].StoreData & 0xffff);
                else if (sta_[i].op == SW) memory.writeWord(sta_[i].StoreAddr, sta_[i].StoreData);
                sta_[storeClock_.tag].state = empty;
                RestoreTag(i);
                rob.deQueue();
                return false;
            }
        } else return false;
        return true;
    }
} lb;

class Simulator {
public:
    static void Fetch() {
        if (isq.end_) return;
        if (isq.ifFull() || isq.stall_) return;
        u_int32_t order = memory.readWord(PC);

        Decode decoder;
        decoder.SetOrder(order);
        decoder.decode();
        if (decoder.type_ != 'B' && (decoder.type_ == 'S' || decoder.op_ == JALR || decoder.rd_ != 0))
            isq.enQueue(PC, order);
        if (order == 0x0ff00513) {
            isq.end_ = true;
            return;
        }
        if (decoder.op_ == JALR)
            isq.stall_ = true;
        else if (decoder.type_ == 'J') {
            u_int32_t des = decoder.imm_ + PC;
            PC = des;
        } else if (decoder.type_ == 'B') {
            u_int32_t des = decoder.imm_ + PC;
            if (predictor.Predict(PC)) {
                isq.enQueue(PC, order, true);
                PC = des;
            } else {
                isq.enQueue(PC, order, false);
                PC += 4;
            }
        } else PC += 4;
    }

    static void Issue() {
        if (isq.ifEmpty()) {
            Fetch();
            return;
        }
        instruction_queue inst = isq.buffer_[0];
//        std::cout<<std::dec<<inst.order<<'\n';
        Fetch();
        Decode decoder;
        decoder.SetOrder(inst.order);
        decoder.decode();
        if (decoder.type_ == 'U') {
            reorder_buffer tmp;
            tmp.type = decoder.type_;
            tmp.ready = true;
            tmp.order = decoder.order_;
            tmp.dest = decoder.rd_;
            if (decoder.op_ == LUI) tmp.val = decoder.imm_;
            else if (decoder.op_ == AUIPC) tmp.val = decoder.imm_ + inst.pc;
            if (tmp.dest) { // 非F0
                tmp.entry = rf.Reg_[tmp.dest].entry = rob.NewEntry();
                ++rob.entry_num_;
                rob.buffer_.enQueue(tmp);
            }
            isq.deQueue();
        } else if (decoder.type_ == 'J') {
            reorder_buffer tmp;
            tmp.type = decoder.type_;
            tmp.ready = true;
            tmp.order = decoder.order_;
            tmp.dest = decoder.rd_;
            tmp.val = inst.pc + 4;
            if (tmp.dest) { // 非F0
                tmp.entry = rf.Reg_[tmp.dest].entry = rob.NewEntry();
                ++rob.entry_num_;
                rob.buffer_.enQueue(tmp);
            }
            isq.deQueue();
        } else if (decoder.type_ == 'L' || decoder.type_ == 'S') {
            if (lb.ifFull() || rob.ifFull()) return;
            isq.deQueue();
            lb.Issue(rob.NewEntry(), decoder);
            rob.Issue(decoder, inst.pc);
        } else if (decoder.type_ == 'B') {
            if (rs.ifFull() || rob.ifFull()) return;
            isq.deQueue();
            u_int32_t des = decoder.imm_ + inst.pc;
            rs.Issue(rob.NewEntry(), decoder);
            rob.Issue(decoder, inst.pc, des, inst.jump);
        } else { // I、R
            if (rs.ifFull() || rob.ifFull()) return;
            isq.deQueue();
            rs.Issue(rob.NewEntry(), decoder);
            rob.Issue(decoder, inst.pc);
        }
    }

    static void Execute() {
        lb.Execute();
        rs.Execute();
    }

    static void WriteResult() {
        if (!rs.Broadcast() && !lb.Broadcast()) return;
        rs.Reception();
        lb.Reception();
        rob.Reception();
    }

    static void Commit() {
        if (lb.Storing()) return;
        if (rob.ifEmpty()) return;
        reorder_buffer inf = rob.buffer_[0];
        if (!inf.ready) return;
        if (inf.order == 0x0ff00513u) {
            std::cout << std::dec << (rf.Reg_[10].val & 255u) << '\n';
//            std::cout << predictor.Accuracy() << '\n';
            exit(0);
        }
        if (inf.type == 'S') {
            lb.Commit(inf.entry);
        } else if (inf.type == 'B') {
            if (inf.val != inf.jump) {
                if (inf.jump) PC = inf.pc_now_ + 4;
                else PC = inf.pc_des_;
                isq.Flush();
                rs.Flush();
                lb.Flush();
                rob.Flush();
                rf.Flush();
                Fetch();
                predictor.Feedback(inf.pc_now_, inf.val, false);
            } else {
                rob.deQueue();
                predictor.Feedback(inf.pc_now_, inf.val, true);
            }
        } else {
            if (inf.dest) {
                rf.Reg_[inf.dest].val = inf.val;
                if (rf.Reg_[inf.dest].entry == inf.entry) rf.Reg_[inf.dest].entry = 0;
            }
            cdb = (CommonDataBus) {inf.entry, inf.val};
            rs.Reception();
            lb.Reception();

            rob.deQueue();
        }
    }

public:
    static void Read() {
        std::string token;
        u_int32_t addr;
        u_int32_t order;
        while (std::cin >> token) {
            if (token[0] == '@') {
                std::stringstream stream(token.substr(1));
                stream >> std::hex >> addr;
            } else {
                std::stringstream stream(token);
                stream >> std::hex >> order;
                memory.writeByte(addr, order);
                ++addr;
            }
        }
    }

    static void Run() {
        std::vector<int> v = { 1, 2, 3, 4};
        std::random_device rd;
        std::mt19937 rng(rd());
        while (true) {
            ++Clock;
            std::shuffle(v.begin(), v.end(), rng);
            for (int n : v) {
                if(n==1) Commit();
                else if(n==2) WriteResult();
                else if(n==3) Execute();
                else if(n==4) Issue();
            }
        }
    }

};

#endif //RISC_V_SIMULATOR_H
