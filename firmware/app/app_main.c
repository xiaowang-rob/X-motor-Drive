#include "app_main.h"

#include "cmd_list.h"
#include "devices.h"

#include "communcation.h"
#include "parameters.h"
#include "protection.h"
#include "slot_con.h"
#include "status_feedback.h"

#include "xfoc_main.h"

#include "bsp_cfg.h"

#define FW_VERSION                                                             \
    VERSION_ENCODE(FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH)

void app_main_init(void) {

    // 2、初始化参数服务 读参数
    if (!param_init(&g_storage))
        return;

    if (!comm_init(&g_param))
        core_enter_fault(FAULT_BUSCOM_INIT);
    // 初始化slot槽服务
    slot_con_init(F_PWM);
    // 初始化保护服务
    pro_manager_init(&g_param);

    core_init(&g_param, g_slotcon.t_high, g_slotcon.t_med, g_slotcon.t_low);
    // 将solt槽任务注册到pwm下溢中断 以 启动solt槽任务
    // pwm_register_callback(NULL, slot_con_update);
}

void app_main_start(void) {

    while (1) {
        status_feedback_main_loop(core.state);
    }
}

const tSlotTask high_schedule[FREQ_HIGH_LOOP] = {
    {0, obs_foc_task} // 内环只有一个 观测器和foc核心任务
};
const tSlotTask medium_schedule[FREQ_MEDIUM_LOOP] = {
    {4, traj_task},
    {5, vl_pid_mit_task},
    {6, field_weakening_task},
};
const tSlotTask low_schedule[FREQ_LOW_LOOP] = {
    {1, pl_pid_task},
};
