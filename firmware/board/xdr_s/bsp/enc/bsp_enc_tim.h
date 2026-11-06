#ifndef __BSP_ENC_TIM_H
#define __BSP_ENC_TIM_H

#include <stdint.h>

// TIM3 捕获链路层：引脚/时钟初始化、IRQ 分发与寄存器访问。
// ABI / Hall 驱动互斥使用 TIM3，open 时注册自己的 ISR，close 时注销。
// 具体编码器只填写模式参数，不直接访问 TIM3 寄存器。

typedef struct {
    uint32_t psc;
    uint32_t arr;
    uint32_t cr2;
    uint32_t ccmr1;
    uint32_t ccmr2;
    uint32_t smcr;
    uint32_t ccer;
    uint32_t dier;
    uint32_t cr1;
} tTimEncConfig;

void bsp_tim_enc_init(void);
void bsp_tim_register_cb(void (*cb)(void *), void *handle);

// 配置并启动 TIM3：写模式寄存器、产生 UG、清计数与标志，最后写 DIER/CR1。
void bsp_tim_enc_configure(const tTimEncConfig *cfg);
uint32_t bsp_tim_enc_status(void);
void bsp_tim_enc_clear_status(uint32_t mask);
uint32_t bsp_tim_enc_count(void);
void bsp_tim_enc_set_count(uint32_t count);
uint32_t bsp_tim_enc_capture(uint32_t channel);
void bsp_tim_enc_stop(void);

#endif // __BSP_ENC_TIM_H
