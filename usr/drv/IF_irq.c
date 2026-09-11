#include "IF_irq.h"
#include "main.h"

// 这里由 gate驱动pwm的溢出中断触发
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

static void (*s_sample_cb)(void) = NULL; // 上溢：电流采样
static void (*s_ctrl_cb)(void) = NULL;   // 下溢：FOC 控制

// ---- TIM8 节拍中断（只在本文件定义；上溢/下溢分别转发） ----
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &GATE_PWM_HTIM)
    {
        if (GATE_PWM_HTIM.Instance->CR1 & TIM_CR1_DIR) // 中心对齐：向下计数到 0 为上溢(采样点)
        {
            if (s_sample_cb)
                s_sample_cb();
        }
        else if (s_ctrl_cb)
        {
            s_ctrl_cb();
        }
    }
}

void register_sample_callback(void (*callback)(void))
{
    s_sample_cb = callback; // 仅在本文件定义
}

void register_ctrl_callback(void (*callback)(void))
{
    s_ctrl_cb = callback; // 仅在本文件定义
}
