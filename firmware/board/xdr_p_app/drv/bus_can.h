#ifndef XDR_BOARD_DRV_BUS_CAN_H
#define XDR_BOARD_DRV_BUS_CAN_H

#include "bus_com.h"

// CAN 总线 —— 板级钩子的具体实现之一
bool can_bus_open(uint32_t std_id);
bool can_bus_send(uint32_t id, const uint8_t *data, uint16_t len);
void can_bus_register_cb(bus_rx_frame_cb cb, void *ctx);

#endif // XDR_BOARD_DRV_BUS_CAN_H
