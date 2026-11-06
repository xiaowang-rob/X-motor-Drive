#ifndef __BSP_ENC_H
#define __BSP_ENC_H

#include "encoder.h"
#include "parameter_list.h"

// 总 API：按观测器类型返回驱动句柄（ops + dev）。
// OBS_NONE 或参数无效返回 NULL；OBS_INT_ENC 固定板载 MT6816。
const tEncDevHandle *bsp_enc_get_handle(eObsList enc, eSpiEncList spi_chip);

#endif // __BSP_ENC_H
