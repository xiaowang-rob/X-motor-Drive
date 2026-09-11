#ifndef __IF_IRQ_H
#define __IF_IRQ_H

// 需要在源文件中定义
extern const float F_CON;
extern const float T_CON;

// ---------- 控制分频 ----------
#define FREQ_HIGH_LOOP 1    // 高环分频 (1 = 每PWM周期执行)
#define FREQ_MEDIUM_LOOP 10 // 中环分频 (每FREQ_HIGH_LOOP*FREQ_MEDIUM_LOOP = 10个PWM周期)
#define FREQ_LOW_LOOP 10    // 低环分频 (每FREQ_MEDIUM_LOOP*FREQ_LOW_LOOP = 100个PWM周期)
// ---------- 计算参数 ----------
#define F_CURRENT (F_PWM / FREQ_HIGH_LOOP)
#define F_SPEED (F_CURRENT / FREQ_MEDIUM_LOOP)
#define F_POSITION (F_SPEED / FREQ_LOW_LOOP)

// 中断回调接口表
void irq_enable(void);
void irq_disable(void);
void system_reset(void);

void register_sample_callback(void (*callback)(void));
void register_ctrl_callback(void (*callback)(void));

#endif