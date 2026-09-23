#ifndef __ENC_AS5047_H
#define __ENC_AS5047_H

#include <stdint.h>

#include "encoder.h"

// 驱动实例（布局私有：只暴露符号，装配层仅取地址）
typedef struct tAS5047Dev tAS5047Dev;
extern tAS5047Dev g_as5047_ext; // 外接编码器实例
extern tAS5047Dev g_as5047_int; // 板载编码器实例

// 驱动 ops（abs/encoder.h 的 tEncoderOps）
extern const tEncoderOps as5047_ops;

#endif // __ENC_AS5047_H
