#ifndef XDR_APP_ABS_GATE_DRV_BOARD_H
#define XDR_APP_ABS_GATE_DRV_BOARD_H

#include "device.h"

// ============================================================
// gate_drv_board.h — 栅极驱动的板级钩子契约（abs 声明 / 板级实现）
//
// 与 encoder_board.h 同构：abs 只调用这些函数，定时器/通道/引脚
// 与 HAL 细节由 board/<B>/drv/gate_fd6288q.c 一份实现承担。
//
// 板级实现语义：
//   open         取回 PWM 周期计数值（ARR）；返回 false 表示功率级不可用
//   power        12V 功率级电源开关
//   enable       PWM 输出使能 / 关断
//   set_compare  三相占空比（比较值）；**热路径**，须直写寄存器且无阻塞
//   register_isrs 注册上溢/下溢处理函数
// ============================================================

bool gate_board_open(uint32_t *pwm_period);
void gate_board_power(bool on);
void gate_board_enable(bool en);
void gate_board_set_compare(uint16_t ticA, uint16_t ticB, uint16_t ticC);
void gate_board_register_isrs(void (*sample_cb)(void), void (*ctrl_cb)(void));

#endif // XDR_APP_ABS_GATE_DRV_BOARD_H
