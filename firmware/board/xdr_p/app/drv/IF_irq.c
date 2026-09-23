// ============================================================
// IF_irq.c — 中断 / 节拍基础服务（板级）
//
// 只提供全局中断开关与系统复位；FOC 节拍回调由栅极驱动持有并转发
// （见 gate_fd6288q.c 的 HAL_TIM_PeriodElapsedCallback）。
// ============================================================
#include "IF_irq.h"

#include "main.h"

// 控制基频：由功率级 PWM 决定（20kHz）
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
