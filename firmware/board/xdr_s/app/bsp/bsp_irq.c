// ============================================================
// bsp_irq.c — 中断基础设施（板级 bsp）
//
// 只放中断公共设施：控制基频常量 + 全局中断开关 / 复位。
// 各驱动的 HAL_*_Callback 定义在各自驱动文件内（见 bsp_irq.h 说明）。
// ============================================================

#include "bsp_irq.h"

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
