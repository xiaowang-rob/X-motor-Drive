#ifndef XDR_APP_ABS_BUS_COM_BOARD_H
#define XDR_APP_ABS_BUS_COM_BOARD_H

#include "bus_com.h"

// ============================================================
// bus_com_board.h — 总线通信的板级钩子契约（abs 声明 / 板级实现）
//
// 实现 = board/<B>/drv/board_bus.c（按 eBusPort 分派到 bus_can.c）。
//
// 语义：
//   open         以标准帧 ID 配置过滤器并启动总线、开启收帧中断
//   send         发送一帧（邮箱忙时返回 false，由上层重试）
//   register_cb  注册收帧回调；板级在中断上下文调用 cb(ctx, id, data, len)
// ============================================================

bool bus_board_open(eBusPort port, uint32_t std_id);
bool bus_board_send(eBusPort port, uint32_t id, const uint8_t *data, uint16_t len);
void bus_board_register_cb(eBusPort port, bus_rx_frame_cb cb, void *ctx);

#endif // XDR_APP_ABS_BUS_COM_BOARD_H
