#ifndef __MOTOR_DRV_H
#define __MOTOR_DRV_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// motor_drv.h — 电机功率级驱动（usr/drv，v2 直连版）
//
// TIM8 中心对齐 6 路 PWM（CH1-3 + 互补）+ 12V 电源 + FOC 节拍中断。
// HAL 回调（PeriodElapsed 上/下溢）由本文件唯一持有并转发到注册回调。
// ============================================================

// FOC 节拍回调注册：
//   sample_cb —— TIM8 上溢（2-shunt 电流采样点，电机空闲/运行都执行）
//   ctrl_cb   —— TIM8 下溢（FOC 控制主循环）
void motor_drv_register_isrs(void (*sample_cb)(void), void (*ctrl_cb)(void));

// 12V 电源控制（高电平使能）
void motor_power_12v(bool on);

// 三相占空比（比较值）：ticA→CH3、ticB→CH2、ticC→CH1（与硬件相序一致）
void motor_drv_set_compare(uint16_t ticA, uint16_t ticB, uint16_t ticC);

// PWM 输出使能/关断（含互补通道）
void motor_drv_enable(void);
void motor_drv_disable(void);

#endif // __MOTOR_DRV_H
