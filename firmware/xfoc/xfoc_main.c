#include "xfoc_main.h"

#include "bsp_cfg.h"

#include "log.h"
#include "parameters.h"
#include "uart_com.h"

bool xfoc_start(void) {

    // 2、初始化参数服务 读参数
    if (!param_init(&g_flash))
        return false;
    // 3、初始化日志服务 读日志
    // if (!log_init(&g_flash))
    //     return false;
    // 4、启动副通讯 usb/uart
    if (!uart_start(&g_usb, UART_PACKET_HEAD, UART_PACKET_TAIL))
        return false;
    if (!uart_start(&g_uart, UART_PACKET_HEAD, UART_PACKET_TAIL))
        return false;
    // 5、core初始化
    core_init();

    while (1) {
    }

    return true;
}
