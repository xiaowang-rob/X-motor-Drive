#ifndef __IRQIF_H
#define __IRQIF_H

// 中断回调接口表

void register_sample_callback(void (*callback)(void));
void register_ctrl_callback(void (*callback)(void));

#endif