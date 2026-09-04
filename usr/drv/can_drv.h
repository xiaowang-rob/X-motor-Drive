#ifndef __CAN_DRV_H
#define __CAN_DRV_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// can_drv.h — CAN 通讯底层驱动（usr/drv，v2 直连版）
//
// 过滤器（标准帧 ID 掩码）+ FIFO0 接收中断，收帧经注册回调交付。
// HAL_CAN_RxFifo0MsgPendingCallback 由本文件唯一持有。
// ============================================================

// 收帧回调（中断上下文，尽快拷贝）
typedef void (*can_rx_cb)(uint32_t id, const uint8_t *data, uint8_t len);

// 以标准帧 ID 配置过滤器并启动（可重复调用以改 ID）
bool can_drv_start(uint32_t std_id);

// 发送标准帧（阻塞等邮箱空出，超时返回 false）
bool can_drv_send(uint32_t id, const uint8_t *msg, uint8_t len);

// 注册收帧回调（NULL 注销）
void can_drv_register_rx(can_rx_cb cb);

#endif // __CAN_DRV_H
