#ifndef __BUS_CAN_H
#define __BUS_CAN_H

#include "bus_com.h"

// ============================================================
// bus_can.h — CAN 总线驱动（板级）
//
// 实现 abs/bus_com.h 的 tBusOps，并导出驱动实例供装配层挂钩。
// ============================================================

// 驱动实例（布局私有：只暴露符号，装配层仅取地址）
typedef struct tCanBus tCanBus;
extern tCanBus g_can2;

// 驱动 ops（abs/bus_com.h 的 tBusOps）
extern const tBusOps can_bus_ops;

#endif // __BUS_CAN_H
