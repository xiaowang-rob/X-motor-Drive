// ============================================================
// board_bus.c — 总线板级钩子的分派层
//
// 实现 app/abs/bus_com_board.h：按 eBusPort 把调用转给具体驱动
// （bus_can.c）——直接函数调用，无 ops 表。
// ============================================================
#include "bus_com_board.h"

#include "bus_can.h"

bool bus_board_open(eBusPort port, uint32_t std_id)
{
    switch (port)
    {
    case BUS_PORT_CAN:
        return can_bus_open(std_id);
    default:
        return false;
    }
}

bool bus_board_send(eBusPort port, uint32_t id, const uint8_t *data, uint16_t len)
{
    switch (port)
    {
    case BUS_PORT_CAN:
        return can_bus_send(id, data, len);
    default:
        return false;
    }
}

void bus_board_register_cb(eBusPort port, bus_rx_frame_cb cb, void *ctx)
{
    switch (port)
    {
    case BUS_PORT_CAN:
        can_bus_register_cb(cb, ctx);
        break;
    default:
        break;
    }
}
