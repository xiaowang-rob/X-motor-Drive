#ifndef __ENC_HALL_H
#define __ENC_HALL_H

#include "enc.h"

// 霍尔编码器：TIM3 Hall XOR 捕获 + 6 扇区电角度。
const tEncDevHandle *Get_EncHallHandle(void);

#endif // __ENC_HALL_H
