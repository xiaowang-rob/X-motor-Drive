// ============================================================
// bsp_irq.c — 中断基础设施（板级 bsp）
//
// 本文件提供：
//   - 控制基频常量（F_CON / T_CON）
//   - 全局中断开关 / 复位
//   - 功率级 PWM 节拍中断：全板唯一持有 HAL_TIM_PeriodElapsedCallback，
//     按事件源（fd6288q_owns_tim）过滤后转发到注册的采样/控制回调
// 其余外设（ADC/UART/FDCAN 等）的 HAL 回调由各自驱动文件自行定义。
// ============================================================

#include "bsp_irq.h"

#include "gate_fd6288q.h"

// PWM 节拍回调（由 pwm_register_callback 注入）
static void (*pwm_up_callback)(void) = NULL;
static void (*pwm_down_callback)(void) = NULL;

// 控制基频：由功率级 PWM 决定（TIM1 中心对齐，20kHz）
const float F_CON = 20000.0f;
const float T_CON = 0.00005f;

void irq_enable(void)
{
    __enable_irq();
}

void irq_disable(void)
{
    __disable_irq();
}

void system_reset(void)
{
    NVIC_SystemReset();
}

// ---------- 功率级节拍中断 ----------
// HAL 回调由本文件独占定义；采样/控制回调在中断上下文运行，须短小。
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (!fd6288q_owns_tim(htim))
        return;

    // 中心对齐：CR1.DIR=1 表示当前向下计数（下溢事件），否则向上（上溢）
    if (htim->Instance->CR1 & TIM_CR1_DIR)
    {
        if (pwm_down_callback)
            pwm_down_callback();
    }
    else
    {
        if (pwm_up_callback)
            pwm_up_callback();
    }
}

void pwm_register_callback(void (*up_cb)(void), void (*down_cb)(void))
{
    pwm_up_callback = up_cb;
    pwm_down_callback = down_cb;
}
