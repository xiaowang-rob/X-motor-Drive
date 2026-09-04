// ============================================================
// motor_drv.c — 电机功率级驱动（usr/drv，v2 直连版）
//
// TIM8 中心对齐：上溢 = 2-shunt 采样点；下溢 = FOC 控制节拍。
// 厂商库符号（htim8/GPIO）经 platform.h，中断回调只在本文件定义。
// ============================================================

#include "platform.h"

#include "motor_drv.h"

static void (*s_sample_cb)(void) = NULL; // 上溢：电流采样
static void (*s_ctrl_cb)(void) = NULL;   // 下溢：FOC 控制

// ---- TIM8 节拍中断（只在本文件定义；上溢/下溢分别转发） ----
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &PWM_GET_HTIM)
    {
        if (TIM8->CR1 & TIM_CR1_DIR) // 中心对齐：向下计数到 0 为上溢(采样点)
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

void motor_drv_register_isrs(void (*sample_cb)(void), void (*ctrl_cb)(void))
{
    s_sample_cb = sample_cb;
    s_ctrl_cb = ctrl_cb;
}

void motor_power_12v(bool on)
{
    HAL_GPIO_WritePin(POWER12V_GPIOx, POWER12V_GPIOx_PIN,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void motor_drv_set_compare(uint16_t ticA, uint16_t ticB, uint16_t ticC)
{
    __HAL_TIM_SetCompare(&PWM_GET_HTIM, TIM_CHANNEL_1, ticC);
    __HAL_TIM_SetCompare(&PWM_GET_HTIM, TIM_CHANNEL_2, ticB);
    __HAL_TIM_SetCompare(&PWM_GET_HTIM, TIM_CHANNEL_3, ticA);
}

void motor_drv_enable(void)
{
    HAL_TIM_PWM_Start(&PWM_GET_HTIM, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&PWM_GET_HTIM, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&PWM_GET_HTIM, TIM_CHANNEL_3);

    HAL_TIMEx_PWMN_Start(&PWM_GET_HTIM, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&PWM_GET_HTIM, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start(&PWM_GET_HTIM, TIM_CHANNEL_3);
}

void motor_drv_disable(void)
{
    HAL_TIM_PWM_Stop(&PWM_GET_HTIM, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(&PWM_GET_HTIM, TIM_CHANNEL_2);
    HAL_TIM_PWM_Stop(&PWM_GET_HTIM, TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Stop(&PWM_GET_HTIM, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Stop(&PWM_GET_HTIM, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Stop(&PWM_GET_HTIM, TIM_CHANNEL_3);
}
