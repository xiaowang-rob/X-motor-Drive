#ifndef __ENC_MANAGER_H
#define __ENC_MANAGER_H

#include "enc.h"
#include "parameters.h"

typedef struct {
    tEncoder enc;
    bool enc_active; // 编码器激活

    float theta_offset; //安装角度偏移
    int positive_drive; //正方向
    float theta_e; // 电角度
    float theta_r; // 转子角度
} tEncManager;


bool em_init(tEncManager *em, tParameter *param);

void em_loop_task(tEncManager *em);


#endif
