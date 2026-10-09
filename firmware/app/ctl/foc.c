#include "foc.h"
#include "filter.h"
#include "slot_con.h"
#include "bsp_math.h"

// FOC核心初始化
void foc_init(tFOC *foc, tParameter *param,
              float t_cl, float t_vl, float t_pl, float vmax)
{
    pi_init(&foc->PI_iq, param->qclkp, param->qclki, vmax, t_cl);
    pi_init(&foc->PI_id, param->dclkp, param->dclki, vmax, t_cl);
    pi_init(&foc->PI_vel, param->vlkp, param->vlki, param->limit_current, t_vl);
    pi_init(&foc->PI_weakmag, param->vlkp * 0.6f, param->vlki * 0.6f, param->limit_current, t_vl);
    pid_init(&foc->PID_pos, param->plkp, param->plki, param->plkd, param->limit_vel, param->plalpha, t_pl);
    mit_init(&foc->mit, param->mit_kp, param->mit_kd, param->mit_tsta, param->mit_tmax);
}

void foc_reset(tFOC *foc)
{
    pi_reset(&foc->PI_iq);
    pi_reset(&foc->PI_id);
    pi_reset(&foc->PI_vel);
    pi_reset(&foc->PI_weakmag);
    pid_reset(&foc->PID_pos);
}
