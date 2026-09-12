#include "gate_drv.h"

void gate_drv_init(tGateDrv *drv, tGateDrvOps *ops)
{
    drv->ops = ops;
    drv->ops->get_pwm_config(&drv->pwm_period);
}
void gate_drv_power_on(tGateDrv *drv, bool on)
{
    drv->ops->power_ctrl(on);
    drv->dstate = on ? DEV_ONLINE : DEV_OFFLINE;
}
void gate_drv_start(tGateDrv *drv)
{
    drv->ops->start();
    drv->dstate = DEV_RUNNING;
}
void gate_drv_start(tGateDrv *drv)
{
    drv->ops->stop();
    drv->dstate = DEV_ONLINE;
}
void gate_drv_set_compare(tGateDrv *drv, uint16_t ticA, uint16_t ticB, uint16_t ticC)
{
    drv->ops->set_compare(ticA, ticB, ticC);
}