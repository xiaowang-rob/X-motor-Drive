#ifndef __APP_MAIN_H
#define __APP_MAIN_H

#include "protocol.h"
#include <stdbool.h>

typedef struct
{
    eState state;
    bool enc_enable;   // 编码器使能标记
    bool Obs_enable;   // 观测器使能标记
    eObsMode obs_mode; // 观测模式
    eRunMode run_mode; // 运行模式

} tXdr;
#endif