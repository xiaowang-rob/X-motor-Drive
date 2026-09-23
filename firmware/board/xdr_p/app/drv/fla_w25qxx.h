#ifndef __FLA_W25QXX_H
#define __FLA_W25QXX_H

#include "flash.h"

// ============================================================
// fla_w25qxx.h — W25Q128 外部 SPI NOR 驱动（板级）
//
// 实现 abs/flash.h 的 tFlashOps，并导出驱动实例供装配层挂钩。
// ============================================================

// 驱动实例（布局私有：只暴露符号，装配层仅取地址）
typedef struct tW25Qxx tW25Qxx;
extern tW25Qxx g_fla_w25;

// 驱动 ops（abs/flash.h 的 tFlashOps）
extern const tFlashOps fla_w25_ops;

#endif // __FLA_W25QXX_H
