#ifndef XDR_BL_COMM_H
#define XDR_BL_COMM_H

#include <stdbool.h>

/* 在板级设备初始化完成后调用一次；随后在主循环中轮询。 */
bool bl_comm_init(void);
void bl_comm_poll(void);

#endif
