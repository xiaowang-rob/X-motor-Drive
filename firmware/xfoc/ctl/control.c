#include "control.h"

// FOC初始化
void ctrl_foc_init(tCtrl *con, tParameter *param, float t_cl, float vmax) {
    pi_init(&con->PI_iq, param->qclkp, param->qclki, vmax, t_cl);
    pi_init(&con->PI_id, param->dclkp, param->dclki, vmax, t_cl);
}

// pid初始化
void ctrl_pid_init(tCtrl *con, tParameter *param, float t_ml, float t_ll) {
    pi_init(&con->PI_vel,
            param->vlkp,
            param->vlki,
            param->motor_rated_current,
            t_ml);
    pi_init(&con->PI_fw,
            param->vlkp,
            param->vlki,
            param->motor_rated_current,
            t_ml);
    con->pos_pvc = param->pos_pidmode;
    if (con->pos_pvc == POS_PID_PC)
        pid_init(&con->PID_pos,
                 param->plkp,
                 param->plki,
                 param->plkd,
                 param->motor_rated_current,
                 param->plalpha,
                 t_ml);
    else
        pid_init(&con->PID_pos,
                 param->plkp,
                 param->plki,
                 param->plkd,
                 param->velocity_limit,
                 param->plalpha,
                 t_ll);
}
// mit 初始化
void ctrl_mit_init(tCtrl *con, tParameter *param) {
    mit_init(&con->mit,
             param->mit_kp,
             param->mit_kd,
             param->mit_kpmax,
             param->mit_kdmax,
             param->mit_tmax);
}

void ctrl_foc_reset(tCtrl *con) {
    pi_reset(&con->PI_iq);
    pi_reset(&con->PI_id);
}

void ctrl_pid_reset(tCtrl *con) {
    pi_reset(&con->PI_vel);
    pi_reset(&con->PI_fw);
    pid_reset(&con->PID_pos);
}
