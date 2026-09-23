#ifndef __GATE_FD6288Q_H
#define __GATE_FD6288Q_H

#include "gate_drv.h"

// ============================================================
// gate_fd6288q.h — pwm 栅极驱动
// ============================================================

// ---------- 本板配置 ----------
#define GATE_TIC_PWM 4249U // 定时器周期计数值（ARR，对应 F_PWM=20kHz）

// 相 → 通道映射（与硬件相序一致）
#define GATE_PWM_CH_A TIM_CHANNEL_3
#define GATE_PWM_CH_B TIM_CHANNEL_2
#define GATE_PWM_CH_C TIM_CHANNEL_1

#define POWER12V_GPIO_PORT GPIOB
#define POWER12V_GPIO_PIN GPIO_PIN_0

// 驱动实例（布局私有：只暴露符号，装配层仅取地址）
typedef struct tFd6288q tFd6288q;
extern tFd6288q g_fd6288q;

// 驱动 ops（abs/gate_drv.h 的 tGateOps）
extern const tGateOps fd6288q_ops;

#endif // __GATE_FD6288Q_H
