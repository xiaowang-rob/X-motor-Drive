#ifndef __BSP_TIM_ENCS_H
#define __BSP_TIM_ENCS_H
// 需要定时器驱动的编码器
#include "encoder.h"

// abi encoder
tEncDevHandle *Get_ABI_ENCHandle(void);
// hall encoder
tEncDevHandle *Get_HALL_ENCHandle(void);

void bsp_tim_register_cb(void *cb, void *handle);
void bsp_tim_enc_init(void);
#endif
