#ifndef BSP_CRITICAL_H
#define BSP_CRITICAL_H

#include <stdint.h>

/* 保存 PRIMASK 并关中断，退出时恢复进入前的状态。 */
uint32_t bsp_critical_enter(void);
void bsp_critical_exit(uint32_t primask);

#endif
