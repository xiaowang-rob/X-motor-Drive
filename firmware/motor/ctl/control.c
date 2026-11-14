#include "control.h"

// FOC初始化
void ctrl_foc_init(tCtrl *con, tParameter *param, float alpha, float t_cl) {
    pi_init(&con->PI_iq, param->qclkp, param->qclki, 1.0f, t_cl);
    pi_init(&con->PI_id, param->dclkp, param->dclki, 1.0f, t_cl);

    filter_first_order_lag_init(&con->cf_u, alpha, 0);
    filter_first_order_lag_init(&con->cf_v, alpha, 0);
    filter_first_order_lag_init(&con->cf_w, alpha, 0);
}

// pid 控制器初始化
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
// mit 控制器初始化
void ctrl_mit_init(tCtrl *con, tParameter *param) {
    mit_init(&con->mit,
             param->mit_kp,
             param->mit_kd,
             param->mit_kpmax,
             param->mit_kdmax,
             param->mit_tmax);
}

// 控制器复位
void ctrl_reset(tCtrl *con) {
    filter_first_order_lag_reset(&con->cf_u, 0);
    filter_first_order_lag_reset(&con->cf_v, 0);
    filter_first_order_lag_reset(&con->cf_w, 0);
    pi_reset(&con->PI_iq);
    pi_reset(&con->PI_id);
    pi_reset(&con->PI_vel);
    pi_reset(&con->PI_fw);
    pid_reset(&con->PID_pos);
}
// FOC 电流重构处理
void current_process(tCtrl *con, uint8_t sec, float is_in[3], float is_out[3]) {
    if (sec == 1 || sec == 6) { // 最短相=W
        is_out[0] = is_in[1] + is_in[2];
        is_out[1] = -is_in[1];
        is_out[2] = -is_in[2];
    } else if (sec == 2 || sec == 3) { // 最短相=U
        is_out[0] = -is_in[0];
        is_out[1] = is_in[0] + is_in[2];
        is_out[2] = -is_in[2];
    } else if (sec == 4 || sec == 5) { // 最短相=V
        is_out[0] = -is_in[0];
        is_out[1] = -is_in[1];
        is_out[2] = is_in[0] + is_in[1];
    } else { // sec 0/7: 零矢量
        is_out[0] = is_in[0];
        is_out[1] = is_in[1];
        is_out[2] = is_in[2];
    }

    is_out[0] = filter_first_order_lag(&con->cf_u, is_out[0]);
    is_out[1] = filter_first_order_lag(&con->cf_v, is_out[1]);
    is_out[2] = filter_first_order_lag(&con->cf_w, is_out[2]);
}
