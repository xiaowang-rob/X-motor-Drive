#ifndef __BSP_IRQ_H
#define __BSP_IRQ_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

// ============================================================
// bsp_irq.h — 中断集中分发（板级 bsp）
//
// 问题背景：HAL 的 *_Callback 是全局单点符号。若每个驱动各自定义一份，
//           同类外设出现第二路就会互相挤掉（旧驱动里靠"本文件唯一持有"
//           的注释约定维系，脆弱且不可扩展）。
//
// 本模块是**唯一**定义 HAL 回调的地方：按外设 Instance 查注册表，
// 转发到驱动注册进来的处理函数。驱动只管 register，不碰 HAL 回调符号。
//
// 用法（驱动 init 内）：
//   static const tTimIrq s_tim_irq = { .on_underflow = on_ctrl,
//                                      .on_overflow  = on_sample,
//                                      .ctx = &s_inst };
//   bsp_irq_bind_tim(TIM8, &s_tim_irq);
//
// 约定：
//   - 绑定表大小为编译期常量，绑定失败（表满/重复）返回 false
//   - 处理函数运行在中断上下文：不得阻塞、不得用 HAL_Delay
//   - 传 NULL 回调表示不关心该事件
// ============================================================

// ---- 定时器 ----
// 中心对齐计数：向下计数到 0 → 下溢；向上计数到 ARR → 上溢
typedef struct
{
    void (*on_overflow)(void *ctx);   // 上溢
    void (*on_underflow)(void *ctx);  // 下溢
    void (*on_pulse_done)(void *ctx); // PWM 脉冲序列推送完成（DMA 完成）
    void *ctx;                        // 回传给上面三个回调
} tTimIrq;

bool bsp_irq_bind_tim(TIM_TypeDef *tim, const tTimIrq *irq);

// ---- ADC ----
typedef struct
{
    void (*on_conv_cplt)(void *ctx); // 转换（DMA）完成
    void *ctx;
} tAdcIrq;

bool bsp_irq_bind_adc(ADC_TypeDef *adc, const tAdcIrq *irq);

// ---- UART ----
typedef struct
{
    // 一个接收块结束：is_idle=true 表示空闲中断（不定长帧尾），
    // false 表示缓冲满（定长到达）
    void (*on_rx_event)(void *ctx, uint16_t rx_len, bool is_idle);
    void (*on_error)(void *ctx);
    void *ctx;
} tUartIrq;

bool bsp_irq_bind_uart(USART_TypeDef *uart, const tUartIrq *irq);

// ---- CAN ----
typedef struct
{
    void (*on_rx_pending)(void *ctx); // RX FIFO0 有待收报文
    void *ctx;
} tCanIrq;

bool bsp_irq_bind_can(CAN_TypeDef *can, const tCanIrq *irq);

#endif // __BSP_IRQ_H
