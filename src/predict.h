#ifndef RISC_V_PREDICT_H
#define RISC_V_PREDICT_H

#include <iostream>
#include <cstring>

class TwoLevelPredictor {
private:
    const static int size = 4096;
    const static int p = 233;

    u_int8_t GHR[size]; // 全局分支历史寄存器,存储着过去N个分支的跳转方向
    // 每个分支的历史跳转表 PHT/BHT
    // 存储着同一个分支前几次的跳转状态（常用2BC）
    u_int8_t PHT[8][size];

    int total_;
    int correct_;

    u_int32_t Hash(u_int32_t pc) {
        return (1ll * pc * p) % size;
    }

public:
    TwoLevelPredictor() {
        total_ = correct_ = 0;
        memset(GHR, 0, sizeof(GHR));
        for (int i = 0; i < 7; ++i) {
            for (int j = 0; j < size; ++j) {
                PHT[i][j] = 1;
            }
        }
    }

    bool Predict(u_int32_t pc) {
        u_int32_t key = Hash(pc);
        if ((PHT[GHR[key] & 7][key] >> 1) & 1) return true;
        else return false;
    }

    void Feedback(u_int32_t pc, bool jump, bool right) {
        if (right) correct_++;
        total_++;
        u_int32_t key = Hash(pc);
        if (jump && PHT[GHR[key]][key] < 3) ++PHT[GHR[key]][key];
        else if (!jump && PHT[GHR[key]][key] > 0)--PHT[GHR[key]][key];
        GHR[key] = ((GHR[key] << 1) | jump) & 7;
    }

    double Accuracy() {
        if (total_) return 1.0 * correct_ / total_;
        else return 1.0;
    }

};


#endif //RISC_V_PREDICT_H
