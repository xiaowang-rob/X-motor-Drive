#include "gate_drv.h"

// ============================================================
// gate_drv.c — 电机功率级业务对象（abs）
//
// 只做"状态维护 + 调用转发"：所有硬件动作经 ops 落到驱动实例。
// ============================================================

bool gate_drv_init(tGateDrv *drv, const tGateDrvOps *ops, GateHandle handle)
{
    if (!drv || !ops || !handle)
        return false;

    drv->ops = ops;
    drv->handle = handle;
    drv->pwm_period = 0U;
    drv->dstate = DEV_OFFLINE;

    if (ops->get_pwm_config)
        ops->get_pwm_config(handle, &drv->pwm_period);

    drv->dstate = DEV_ONLINE;
    return true;
}

void gate_drv_power_on(tGateDrv *drv, bool on)
{
    if (!drv || !drv->ops)
        return;
    drv->ops->power_ctrl(drv->handle, on);
    drv->dstate = on ? DEV_ONLINE : DEV_OFFLINE;
}

void gate_drv_start(tGateDrv *drv)
{
    if (!drv || !drv->ops)
        return;
    drv->ops->start(drv->handle);
    drv->dstate = DEV_RUNNING;
}

void gate_drv_stop(tGateDrv *drv)
{
    if (!drv || !drv->ops)
        return;
    drv->ops->stop(drv->handle);
    drv->dstate = DEV_ONLINE;
}

void gate_drv_set_compare(tGateDrv *drv, uint16_t ticA, uint16_t ticB, uint16_t ticC)
{
    if (!drv || !drv->ops)
        return;
    drv->ops->set_compare(drv->handle, ticA, ticB, ticC);
}
