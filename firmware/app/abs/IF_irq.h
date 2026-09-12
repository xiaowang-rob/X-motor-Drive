#ifndef __IF_IRQ_H
#define __IF_IRQ_H

// 需要在源文件中定义
extern const float F_CON;
extern const float T_CON;

// 中断回调接口表
void irq_enable(void);
void irq_disable(void);
void system_reset(void);

void register_sample_callback(void (*callback)(void));
void register_ctrl_callback(void (*callback)(void));

#endif