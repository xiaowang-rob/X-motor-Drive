// ============================================================
// gate_drv.c — 电机功率级业务对象（abs）
//
// 只做"状态维护 + 转发"：硬件动作经板级钩子（gate_drv_board.h）直接调用。
// 无 ops 表、无句柄强转。
// ============================================================

#include "gate_drv.h"

#include "gate_drv_board.h"

bool gate_drv_init(tGateDrv *drv)
{
    if (!drv)
        return false;

    drv->pwm_period = 0U;
    drv->dstate = DEV_OFFLINE;

    if (!gate_board_open(&drv->pwm_period))
        return false;

    drv->dstate = DEV_ONLINE;
    return true;
}

void gate_drv_power_on(tGateDrv *drv, bool on)
{
    if (!drv)
        return;
    gate_board_power(on);
    drv->dstate = on ? DEV_ONLINE : DEV_OFFLINE;
}

void gate_drv_enable(tGateDrv *drv, bool en)
{
    if (!drv)
        return;
    gate_board_enable(en);
    drv->dstate = en ? DEV_RUNNING : DEV_ONLINE;
}

void gate_drv_set_compare(tGateDrv *drv, uint16_t ticA, uint16_t ticB, uint16_t ticC)
{
    if (!drv)
        return;
    gate_board_set_compare(ticA, ticB, ticC);
}

void gate_drv_register_isrs(void (*sample_cb)(void), void (*ctrl_cb)(void))
{
    gate_board_register_isrs(sample_cb, ctrl_cb);
}
