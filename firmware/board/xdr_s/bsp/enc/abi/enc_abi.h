#ifndef __ENC_ABI_H
#define __ENC_ABI_H

#include "enc.h"

// ABI 增量式编码器：使用 TIM3 编码器模式 + 可选 Index 捕获。
const tEncDevHandle *Get_EncAbiHandle(void);

#endif // __ENC_ABI_H
