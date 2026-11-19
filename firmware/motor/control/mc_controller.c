#include "mc_controller.h"

// pid 速度环控制器初始化
void ctrl_vel_init(tCtrl *con, tParameter *param, float t_vl) {
    pi_init(&con->PI_vel,
            param->vlkp,
            param->vlki,
            param->motor_rated_current,
            t_vl);
}

// pid 位置环控制器初始化
void ctrl_pos_init(tCtrl *con, tParameter *param, float t_pl) {

    float pid_output_limit = param->pos_pidmode == POS_PID_PC
                                 ? param->motor_rated_current
                                 : param->velocity_limit;

    pid_init(&con->PID_pos,
             param->plkp,
             param->plki,
             param->plkd,
             pid_output_limit,
             param->plalpha,
             t_pl);
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

    pi_reset(&con->PI_vel);
    pid_reset(&con->PID_pos);
}
