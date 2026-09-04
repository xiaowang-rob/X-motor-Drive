// ============================================================
// motor_drv.c — 电机功率级驱动（usr/drv，v2 直连版）
//
// TIM8 中心对齐：上溢 = 2-shunt 采样点；下溢 = FOC 控制节拍。
// 厂商库符号（htim8/GPIO）经 platform.h，中断回调只在本文件定义。
// ============================================================

#include "platform.h"

#include "gate_fd6288q.h"

void gate_power_12v(bool on)
{
    HAL_GPIO_WritePin(POWER12V_GPIOx, POWER12V_GPIOx_PIN,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void gate_drv_set_compare(uint16_t ticA, uint16_t ticB, uint16_t ticC)
{
    __HAL_TIM_SetCompare(&GATE_PWM_HTIM, GATE_PWM_A_CHANNEL, ticA);
    __HAL_TIM_SetCompare(&GATE_PWM_HTIM, GATE_PWM_B_CHANNEL, ticB);
    __HAL_TIM_SetCompare(&GATE_PWM_HTIM, GATE_PWM_C_CHANNEL, ticC);
}

void gate_drv_start(void)
{
    HAL_TIM_PWM_Start(&GATE_PWM_HTIM, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&GATE_PWM_HTIM, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&GATE_PWM_HTIM, TIM_CHANNEL_3);

    HAL_TIMEx_PWMN_Start(&GATE_PWM_HTIM, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&GATE_PWM_HTIM, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start(&GATE_PWM_HTIM, TIM_CHANNEL_3);
}

void gate_drv_stop(void)
{
    HAL_TIM_PWM_Stop(&GATE_PWM_HTIM, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(&GATE_PWM_HTIM, TIM_CHANNEL_2);
    HAL_TIM_PWM_Stop(&GATE_PWM_HTIM, TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Stop(&GATE_PWM_HTIM, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Stop(&GATE_PWM_HTIM, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Stop(&GATE_PWM_HTIM, TIM_CHANNEL_3);
}
