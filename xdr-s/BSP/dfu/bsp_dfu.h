#ifndef XDR_BSP_DFU_H
#define XDR_BSP_DFU_H

#include "dfu.h"

/* 在启动 Vendor 前由启动程序调用。target 须在 USB 启用期间有效。 */
bool bsp_dfu_start(const tDfuDevHandle *target);
void bsp_dfu_task(void);
/* 由 BSP USB 在主循环处理配置变化时调用。 */
void bsp_dfu_usb_reset(void);

#endif
