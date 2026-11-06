#ifndef __ENC_SINCOS_H
#define __ENC_SINCOS_H

#include "encoder.h"

// sin/cos 编码器：ADC 采样两路正交信号，atan2/CORDIC 求传感器相位。
const tEncDevHandle *enc_sincos_handle(void);

#endif // __ENC_SINCOS_H
