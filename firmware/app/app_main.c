#include "app_main.h"
#include "app_comm.h"

#include "cmd_list.h"
#include "devices.h"

#include "enc_manager.h"
#include "obsdata.h"
#include "parameters.h"
#include "protection.h"
#include "slot_con.h"
#include "status_feedback.h"

#include "xfoc_main.h"

#include "bsp_cfg.h"
#include "bsp_pwm.h"

eFault motor_fault;

void app_main_init(void) {

    // 2、初始化参数服务 读参数
    if (!param_init())
        return;
    dm_data_init();

    comm_sup_init();

    // 初始化slot槽服务
    slot_con_init(F_PWM);

    app_main_config();
}

void app_main_config(void) {
    // 关闭所有任务
    pwm_register_callback(NULL, NULL);
    // 初始化保护服务
    pro_manager_init(&g_param);
    // 初始化主总线通讯
    if (!comm_main_init(&g_param))
        pm_handle_fault(FAULT_BUSCOM_INIT);
    // 编码器初始化
    if (!em_init(&g_param))
        pm_handle_fault(FAULT_ENCODER_INIT);
    // 电机算法初始化
    motor_init(&g_param, g_slotcon.t_high, g_slotcon.t_med, g_slotcon.t_low);
    // 将solt槽任务注册到pwm下溢中断 以 启动solt槽任务
    pwm_register_callback(NULL, slot_con_update);
}

void app_main_start(void) {

    while (1) {
        em_loop_task();
        motor_loop_task();
        comm_loop_task();
        pm_loop_task(&motor);
        status_fb_loop_task(motor.state);
    }
}

void enc_foc_task(float ts) {
    em_tim_task(&motor, ts);
    motor_foc_tim_task(ts);
}

const tSlotTask high_schedule[FREQ_HIGH_LOOP] = {{0, enc_foc_task}};
const tSlotTask medium_schedule[FREQ_MEDIUM_LOOP] = {
    {4, motor_traj_tim_task},
    {5, motor_vl_pid_mit_tim_task},
    {6, motor_field_weakening_tim_task},
};
const tSlotTask low_schedule[FREQ_LOW_LOOP] = {
    {1, motor_pl_pid_tim_task},
};
