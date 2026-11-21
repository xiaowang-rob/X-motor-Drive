#include "mc_controller.h"

#define POS_PVC_FREQ 5
static uint8_t pvc_tick;

// 控制器复位
void ctrl_reset(tCtrl *con) {

    traj_reset(&con->traj);
    pi_reset(&con->PI_vel);
    pid_reset(&con->PID_pos);

    con->init_flag = false;
    memset(&con->ref, 0, sizeof(con->ref));
}

void ctrl_cmd_process(tCtrl *con, tParameter *param, float ts) {

    traj_Update(&con->traj, ts);
    switch (con->mode) {
    case MOTOR_SPEED_MODE:
        con->ref.vel = con->traj.out.value;
        break;
    case MOTOR_POSITION_MODE:
        con->ref.pos = con->traj.out.value;
        break;
    case MOTOR_MIT_MODE:
        // 使用内置轨迹规划器 输出轨迹信息
        con->ref.pos = con->traj.out.value;
        con->ref.vel = con->traj.out.rate_d1;
        con->ref.tau_ff = param->motor_j * con->traj.out.rate_d2 +
                          param->motor_b * con->traj.out.rate_d1;

        break;
    default:

        break;
    }
}

void ctrl_config(tCtrl *con, tParameter *param, float ts) {
    if (con->init_flag)
        return;

    switch (con->mode) {
    case MOTOR_SPEED_MODE:
        pi_init(&con->PI_vel,
                param->vlkp,
                param->vlki,
                param->motor_rated_current,
                ts);
        break;
    case MOTOR_POSITION_MODE:
        if (param->pos_pidmode == POS_PID_PVC) {
            pid_init(&con->PID_pos,
                     param->plkp,
                     param->plki,
                     param->plkd,
                     param->velocity_limit,
                     param->plalpha,
                     ts * POS_PVC_FREQ);
            pi_init(&con->PI_vel,
                    param->vlkp,
                    param->vlki,
                    param->motor_rated_current,
                    ts);
        } else {
            pid_init(&con->PID_pos,
                     param->plkp,
                     param->plki,
                     param->plkd,
                     param->motor_rated_current,
                     param->plalpha,
                     ts);
        }
        break;
    case MOTOR_MIT_MODE:
        mit_init(&con->mit,
                 param->mit_kp,
                 param->mit_kd,
                 param->mit_kpmax,
                 param->mit_kdmax,
                 param->mit_tmax);

        break;
    default:

        break;
    }
    con->init_flag = true;
}
void ctrl_update(tCtrl *con, tParameter *param, float ts) {
    switch (con->mode) {
    case MOTOR_CURRENT_MODE:
        if (con->tau_enable)
            con->ref.iq = con->ref.tau / param->motor_ke;
        break;
    case MOTOR_SPEED_MODE:
        con->ref.iq = pi_update(&con->PI_vel, con->ref.vel, con->fb.vel);
        break;
    case MOTOR_POSITION_MODE:
        if (param->pos_pidmode == POS_PID_PVC) {
            if (pvc_tick++ >= POS_PVC_FREQ) {
                pvc_tick = 0;
                con->ref.vel =
                    pid_update(&con->PID_pos, con->ref.pos, con->fb.pos);
            }
            con->ref.iq = pi_update(&con->PI_vel, con->ref.vel, con->fb.vel);
        } else {
            con->ref.iq = pid_update(&con->PID_pos, con->ref.pos, con->fb.pos);
        }

        break;

    case MOTOR_MIT_MODE:
        con->ref.tau = mit_update(&con->mit,
                                  con->ref.tau_ff,
                                  con->ref.pos,
                                  con->fb.pos,
                                  con->ref.vel,
                                  con->fb.vel);

        con->ref.iq = con->ref.tau / param->motor_ke;
        break;
    default:

        break;
    }
}
