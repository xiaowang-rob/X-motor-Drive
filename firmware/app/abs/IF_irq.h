#ifndef __IF_IRQ_H
#define __IF_IRQ_H

// ============================================================
// IF_irq.h — 中断 / 节拍相关契约（abs）
//
// 注：FOC 节拍回调的注册入口在板级栅极驱动处
//     （gate_drivers.h 的 gate_register_isrs）—— TIM8 是该驱动的资源，
//     其 HAL 回调由 gate_fd6288q.c 唯一持有。
// ============================================================

// 控制基频（由板级 IF_irq.c 定义）
extern const float F_CON; // 控制基频 (Hz)
extern const float T_CON; // 控制周期 (s)

// 全局中断开关 / 系统复位
void irq_enable(void);
void irq_disable(void);
void system_reset(void);

#endif // __IF_IRQ_H
