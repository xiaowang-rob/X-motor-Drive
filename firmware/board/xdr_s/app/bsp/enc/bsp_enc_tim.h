#ifndef __BSP_ENC_TIM_H
#define __BSP_ENC_TIM_H
// 需要定时器驱动的编码器
#include "bsp_enc.h"

void bsp_tim_register_cb(void *cb, void *handle);
void bsp_tim_enc_init(void);
#endif
