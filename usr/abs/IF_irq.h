#ifndef __IF_IRQ_H
#define __IF_IRQ_H

// 中断回调接口表
void irq_enable(void);
void irq_disable(void);
void system_reset(void);

void register_sample_callback(void (*callback)(void));
void register_ctrl_callback(void (*callback)(void));

#endif