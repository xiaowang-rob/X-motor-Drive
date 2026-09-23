#ifndef __ENC_MT6816_H
#define __ENC_MT6816_H

#include <stdint.h>

#include "encoder.h"

// MT6816：SPI Mode3(CPOL=1,CPHA=1)/16bit，分辨率 16384
// 读角序列：段1 发 0x83FF → 段2 发 0x84FF 收有效数据帧
#define MT6816_RESOLUTION 16384U

// 驱动实例（布局私有：只暴露符号，装配层仅取地址）
typedef struct tMT6816Dev tMT6816Dev;
extern tMT6816Dev g_mt6816_ext; // 外接编码器实例
extern tMT6816Dev g_mt6816_int; // 板载编码器实例

// 驱动 ops（abs/encoder.h 的 tEncoderOps）
extern const tEncoderOps mt6816_ops;

#endif // __ENC_MT6816_H
