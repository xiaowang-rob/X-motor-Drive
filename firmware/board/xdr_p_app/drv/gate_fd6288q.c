// ============================================================
// motor_drv.c — 电机功率级驱动（usr/drv，v2 直连版）
//
// TIM8 中心对齐：上溢 = 2-shunt 采样点；下溢 = FOC 控制节拍。
// 厂商库符号（htim8/GPIO）经 platform.h，中断回调只在本文件定义。
// ============================================================

#include "gate_drivers.h"

#include "tim.h"

// ---------- 栅极驱动驱动 PWM + POWER ----------
#define GATE_FPWM 20000U
#define GATE_TPWM 0.00005.f
#define GATE_TIC_PWM 2099 // 定时器周期计数值（ARR，对应 F_PWM=20kHz）

#define T_DEADTIME_us 0.5f // 死区时间
#define T_NOISE_us 0.5f    // 开关噪声时间

#define GATE_PWM_HTIM (htim8) // 电机控制定时器句柄
#define GATE_PWM_A_CHANNEL TIM_CHANNEL_3
#define GATE_PWM_B_CHANNEL TIM_CHANNEL_2
#define GATE_PWM_C_CHANNEL TIM_CHANNEL_1

#define POWER12V_GPIOx GPIOC
#define POWER12V_GPIOx_PIN GPIO_PIN_13

void fd_get_pwm_config(uint32_t *pwm_period)
{
    *pwm_period = GATE_TIC_PWM;
}

void fd_power_ctrl(bool on)
{
    HAL_GPIO_WritePin(POWER12V_GPIOx, POWER12V_GPIOx_PIN,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void fd_set_compare(uint16_t ticA, uint16_t ticB, uint16_t ticC)
{
    __HAL_TIM_SetCompare(&GATE_PWM_HTIM, GATE_PWM_A_CHANNEL, ticA);
    __HAL_TIM_SetCompare(&GATE_PWM_HTIM, GATE_PWM_B_CHANNEL, ticB);
    __HAL_TIM_SetCompare(&GATE_PWM_HTIM, GATE_PWM_C_CHANNEL, ticC);
}

void fd_start_output(void)
{
    HAL_TIM_PWM_Start(&GATE_PWM_HTIM, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&GATE_PWM_HTIM, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&GATE_PWM_HTIM, TIM_CHANNEL_3);

    HAL_TIMEx_PWMN_Start(&GATE_PWM_HTIM, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&GATE_PWM_HTIM, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start(&GATE_PWM_HTIM, TIM_CHANNEL_3);
}

void fd_stop_output(void)
{
    HAL_TIM_PWM_Stop(&GATE_PWM_HTIM, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(&GATE_PWM_HTIM, TIM_CHANNEL_2);
    HAL_TIM_PWM_Stop(&GATE_PWM_HTIM, TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Stop(&GATE_PWM_HTIM, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Stop(&GATE_PWM_HTIM, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Stop(&GATE_PWM_HTIM, TIM_CHANNEL_3);
}

const tGateDrvOps gate_fd6288q_ops = {
    .get_pwm_config = fd_get_pwm_config,
    .power_ctrl = fd_power_ctrl,
    .set_compare = fd_set_compare,
    .start_output = fd_start_output,
    .stop_output = fd_stop_output,
};