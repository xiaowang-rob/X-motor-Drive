#include "app_main.h"
#include "app_comm.h"

#include "cmd_list.h"
#include "devices.h"

#include "enc_manager.h"
#include "gate_drv.h"
#include "obsdata.h"
#include "parameters.h"
#include "protection.h"
#include "slot_con.h"
#include "status_feedback.h"

#include "motor_ctrl.h"

eFault motor_fault;
tMotorOps mc_ops = {
    .gate_drv_enable = gd_enable,
    .gate_drv_out = gd_output,
    .theta_process = em_theta_process_task,
    .get_current = gd_get_current,
    .sp_calibration = NULL, // 先不启用
};

void app_main_init(void) {

    // 初始化参数服务 读参数
    if (!param_init())
        return;
    // 初始化观测服务 挂钩观测变量地址
    dm_data_init();
    // 辅助通讯初始化
    comm_sup_init();
    // 初始化slot槽服务
    slot_con_init();
    // 栅极驱动模块初始化
    gd_init(&g_pwm, &g_sample);
    // 配置
    app_main_config();
}

void app_main_config(void) {
    // 关闭所有任务
    slot_con_start(false);
    // 初始化保护服务
    pro_manager_init(&g_param);
    // 初始化主总线通讯
    if (!comm_main_init(&g_param))
        pm_handle_fault(FAULT_BUSCOM_INIT);
    // 编码器初始化
    if (!em_init(&g_param))
        pm_handle_fault(FAULT_ENCODER_INIT);

    // 电机算法初始化
    motor_init(
        &g_param, &mc_ops, g_slotcon.t_high, g_slotcon.t_med, g_slotcon.t_low);
    // 将solt槽任务注册到pwm下溢中断 以 启动solt槽任务
    slot_con_start(true);
}

void app_main_start(void) {

    while (1) {
        // 编码器任务
        em_loop_task();
        //  栅极驱动任务
        gd_loop_task(g_motor.state == MOTOR_IDLE);
        // 通讯
        comm_loop_task();
        // 保护
        pm_loop_task(&g_motor);
        // 状态反馈
        status_fb_loop_task(g_motor.state);
    }
}
const tSlotTask high_schedule[FREQ_HIGH_LOOP] = {{0, motor_foc_tim_task}};
const tSlotTask medium_schedule[FREQ_MEDIUM_LOOP] = {
    {4, motor_traj_tim_task},
    {5, motor_vl_pid_mit_tim_task},
    {6, motor_field_weakening_tim_task},
};
const tSlotTask low_schedule[FREQ_LOW_LOOP] = {
    {1, motor_pl_pid_tim_task},
};
