#ifndef __ENC_MANAGER_H
#define __ENC_MANAGER_H

#include "enc.h"
#include "parameters.h"

typedef struct {
    bool enc_active; // 编码器激活

    float theta_e; // 电角度
    float theta_r; // 转子角度
} tEncManager;

bool em_init(tParameter *param);
void em_loop_task(void);
void em_theta_process_task(float ts, float *th_e, float *th_r, float *vel,
                           float *pos);

void em_set_pos_zero(void);
void em_set_pos_limit(void);

#endif