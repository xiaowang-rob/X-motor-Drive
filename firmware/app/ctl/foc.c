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
    pi_init(&foc->PI_vel, g_param.vlkp, g_param.vlki, g_param.limit_current, t_vl);
    pi_init(&foc->PI_weakmag, g_param.vlkp / 2, g_param.vlki / 2, g_param.limit_current, t_vl);
    pid_init(&foc->PID_pos, g_param.plkp, g_param.plki, g_param.plkd, g_param.limit_vel, g_param.plalpha, t_pl);
    mit_init(&foc->mit, g_param.mit_kp, g_param.mit_kd, g_param.mit_tsta, g_param.mit_tmax);
}

void foc_reset(tFOC *foc)
{
    pi_reset(&foc->PI_iq);
    pi_reset(&foc->PI_id);
    pi_reset(&foc->PI_vel);
    pi_reset(&foc->PI_weakmag);
    pid_reset(&foc->PID_pos);
}
