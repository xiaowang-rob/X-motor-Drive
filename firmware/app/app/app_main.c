#include "bsp_cfg.h"

#include "parameters.h"
#include "log.h"
#include "uart_com.h"

static void system_fault(void);

void main_init(void)
{
    // 1、初始化驱动层 （存储-iap-状态-传感-gate-通讯）
    if (!bsp_base_init())
        system_fault();
    // 2、初始化参数服务 读参数
    if (!param_init(&g_flash))
        system_fault();
    // 3、初始化日志服务 读日志
    if (!log_init(&g_flash))
        system_fault();
    // 4、启动副通讯 usb/uart
    if (!uart_start(&g_usb, UART_PACKET_HEAD, UART_PACKET_TAIL))
        system_fault();
    if (!uart_start(&g_uart, UART_PACKET_HEAD, UART_PACKET_TAIL))
        system_fault();
    // 5、core初始化
    if (!core_init())
        system_fault();
}
void system_fault(void)
{
}
