#ifndef __BSP_PWM_H
#define __BSP_PWM_H

#include "pwm.h"

#include "tim.h"

// ---------- PWM 配置 ----------
#define PWM_FREQ 20000 // PWM 频率（Hz）
#define PWM_TIM TIM1
#define PWM_ARR (PWM_TIM->ARR) // 定时器周期计数值（ARR，对应 F_PWM=20kHz）

#define GATE_T_NOISE (12 / 1000000000)         // 开关噪声时间
#define GATE_T_DIED ((200 + 100) / 1000000000) // 硬件+软件死区时间
// 相 → 通道映射（与硬件相序一致）
#define GATE_PWM_CH_U LL_TIM_CHANNEL_CH3
#define GATE_PWM_CH_V LL_TIM_CHANNEL_CH2
#define GATE_PWM_CH_W LL_TIM_CHANNEL_CH1

#define POWER12V_GPIO_PORT GPIOB
#define POWER12V_GPIO_PIN GPIO_PIN_0

// TIM1 更新中断转发及控制节拍注册。
void bsp_pwm_irq(void);
void pwm_register_callback(void (*up_cb)(void), void (*down_cb)(void));

// 驱动句柄
const tGateDevHandle *Get_MCU_PWMHandle(void);

// 驱动 ops（abs/pwm.h 的 tGateOps）
extern const tGateOps fd6288q_ops;

#endif // __BSP_PWM_H
