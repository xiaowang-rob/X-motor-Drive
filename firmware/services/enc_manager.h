#ifndef __ENC_MANAGER_H
#define __ENC_MANAGER_H

#include "enc.h"
#include "parameters.h"
#include "pll.h"
#include "xfoc_main.h"

typedef struct {
    tPLL pll; // PLL

    bool enc_active; // 编码器激活

    float theta_e; // 电角度
    float theta_r; // 转子角度
    float vel;     // 转子速度
} tEncManager;

bool em_init(tParameter *param);
void em_loop_task(void);
void em_tim_task(tMotorCtrl *motor, float ts);
#endif