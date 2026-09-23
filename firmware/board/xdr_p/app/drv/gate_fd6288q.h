#ifndef __GATE_FD6288Q_H
#define __GATE_FD6288Q_H

#include "gate_drv.h"

// ============================================================
// gate_fd6288q.h — 功率级驱动（板级，FD6288Q 栅极 + TIM8 六路 PWM）
//
// 实现 abs/gate_drv.h 的 tGateOps，并导出驱动实例供装配层挂钩。
// ============================================================

// 驱动实例（布局私有：只暴露符号，装配层仅取地址）
typedef struct tFd6288q tFd6288q;
extern tFd6288q g_fd6288q;

// 驱动 ops（abs/gate_drv.h 的 tGateOps）
extern const tGateOps fd6288q_ops;

#endif // __GATE_FD6288Q_H
