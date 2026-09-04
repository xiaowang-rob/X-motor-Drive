#ifndef __ABS_DEVICE_H
#define __ABS_DEVICE_H

#include <stdint.h>  // uint8_t, uint16_t, uint32_t, uint64_t, int8_t, int16_t, int32_t, int64_t, size_t, ptrdiff_t, NULL
#include <stdbool.h> // bool
#include <string.h>  // NULL
// ============================================================
// device.h — 设备通用状态契约
//
// 跨 drv/abs/业务层共享的设备健康状态。
// 驱动实现经 tXxxDriverOps.get_state() 返回本枚举值，
// 业务层据此判断设备可用性（不解释具体寄存器）。
// ============================================================

typedef enum
{
    DEV_OFFLINE = 0, // 未初始化 / 离线
    DEV_ONLINE,      // 初始化成功，可工作
    DEV_RUN_ERROR,   // 通信/数据错误（可恢复，靠上层重试策略）
    DEV_RUNNING      // 正在正常工作
} eDeviceStatus;

#endif // __ABS_DEVICE_H
