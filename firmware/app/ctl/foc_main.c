#include "foc_main.h"
#include "svpwm.h"

#include "math_fast.h"
#include "parameter_manager.h"
#include "protection_manager.h"
#include "device.h"
#include "smo.h"

#include "bsp_adc.h"
#include "bsp_base.h"
#include "svpwm.h"

FOC_t g_foc;

static void loop_main_update_task();

#ifdef __DEBUG__
u32 _time_focit_start = 0;
u32 _time_focit_end = 0;
volatile u32 _time_focit_run = 0;
volatile u32 _time_foc_T = 0;

#endif

// 下溢中断
void bsp_foc_it_callback()
{
#ifdef __DEBUG__
    _time_focit_start = bsp_get_tick_us();
#endif

    loop_main_update_task();

#ifdef __DEBUG__
    _time_foc_T = bsp_get_tick_us() - _time_focit_end;
    _time_focit_end = bsp_get_tick_us();
    _time_focit_run = _time_focit_end - _time_focit_start;
#endif
}
// FOC 主初始化函数
void foc_init()
{
    g_foc.val = get_foc_val_adr();
    g_foc.mode = get_foc_mode_adr();

    g_foc.foc_enable = false;
    g_foc.state = FOC_IDLE;
    foc_core_init(&g_Param);
    bsp_power_12v_control(true);
    g_foc.foc_init = true;
}

// FOC 主循环函数
static void loop_main_update_task()
{
    if (!g_foc.foc_init)
        return;
    foc_update_val();
    switch (g_foc.state)
    {
    case FOC_IDLE:
        bsp_adc_idle_track();
        break;
    case FOC_TUNE:
        if (!g_foc.foc_enable)
        {
            g_foc.foc_enable = true;
            foc_core_reset();
            bsp_pwm_enable();
        }
        if (TUNE_DONE == tune_main_loop(g_foc.val))
        {
            foc_core_init(&g_Param);
            foc_state_update(FOC_DISABLE);
        }

        foc_main_loop_task();
        break;
    case FOC_RESET:
        foc_core_reset();
        foc_state_update(FOC_IDLE);
        g_foc.foc_enable = false;
        bsp_pwm_disable();
        bsp_power_12v_control(true);

        break;

    case FOC_ENABLE:
        g_foc.foc_enable = true;
        bsp_pwm_enable();
        foc_state_update(FOC_RUNNING);
        break;
    case FOC_DISABLE:
        g_foc.foc_enable = false;
        bsp_pwm_disable();
        foc_state_update(FOC_RESET);
        break;
    case FOC_RUNNING:
        foc_main_loop_task();
        break;
    case FOC_SHUTDOWN:
        if (foc_shutdown())
            foc_state_update(FOC_FAULT);
        break;
    case FOC_FAULT:
        if (g_foc.foc_enable)
        {
            g_foc.foc_enable = false;
            bsp_pwm_disable();
            bsp_power_12v_control(false);
        }
        break;
    default:
        break;
    }
}

// FOC 状态更新函数
void foc_state_update(eFocState state)
{
    // 状态切换条件：
    // 1、故障状态只能通过复位退出
    if (g_foc.state == FOC_FAULT && state != FOC_RESET)
        return;
    // 2、使能状态只能从空闲进入
    if (state == FOC_ENABLE && g_foc.state != FOC_IDLE)
        return; // 只能从DISABLE进入ENABLE
    if (state == FOC_RESET)
    {
        pro_manager_clear_flag();
        encode_clear_error_flag();
    }
    g_foc.state = state;
}
