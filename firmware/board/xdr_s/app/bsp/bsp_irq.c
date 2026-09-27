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
