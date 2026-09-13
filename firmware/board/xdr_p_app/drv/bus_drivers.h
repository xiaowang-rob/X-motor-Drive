#ifndef __BUS_DRIVERS_H
#define __BUS_DRIVERS_H

#include "bus_com.h"

// ============================================================
// bus_drivers.h — 本板总线驱动出口
//
// 每个驱动以"文件内静态 handle 实例"存在，本头只暴露：
//   - ops 表（abs 层绑定用）
//   - 取实例句柄的函数（组装层 dev 用）
// ============================================================

extern const tBusDriverOps can_drv_ops;

// 取 CAN 实例句柄（静态实例，见 bus_can.c）
BusHandle can_get_handle(void);

#endif // __BUS_DRIVERS_H
