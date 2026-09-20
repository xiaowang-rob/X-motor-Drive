#ifndef __ENC_MT6835_H
#define __ENC_MT6835_H

#include <stdint.h>

#include "encoder.h"

// MT6835：SPI Mode3(CPOL=1,CPHA=1)/8bit，分辨率 16384
// 读角序列：单段 5 字节 {0xA0,0x03,0,0,0} 边发边收
#define MT6835_RESOLUTION 16384U

// 驱动实例（布局私有：只暴露符号，装配层仅取地址）
typedef struct tMT6835Dev tMT6835Dev;
extern tMT6835Dev g_mt6835_ext; // 外接编码器实例
extern tMT6835Dev g_mt6835_int; // 板载编码器实例

// 驱动 ops（abs/encoder.h 的 tEncoderOps）
extern const tEncoderOps mt6835_ops;

#endif // __ENC_MT6835_H
