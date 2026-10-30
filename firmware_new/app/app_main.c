#include "app_main.h"

#include <stdbool.h>

bool app_main_init(void) {

    // 2、初始化参数服务 读参数
    if (!param_init(&g_flash))
        return false;

    // 4、启动副通讯 usb/uart
    if (!uart_start(&g_usb, UART_PACKET_HEAD, UART_PACKET_TAIL))
        return false;
    if (!uart_start(&g_uart, UART_PACKET_HEAD, UART_PACKET_TAIL))
        return false;
    // 启动主总线通信
    if (!bus_start(&g_can, g_param.can_id)) {
        core_enter_fault(FAULT_BUSCOM_INIT);
        return;
    }
    // 初始化slot槽服务
    slot_con_init(F_PWM);
    // 初始化保护服务
    pro_manager_init(&g_param);

    // 将solt槽任务注册到pwm下溢中断 以 启动solt槽任务
    pwm_register_callback(NULL, slot_con_update);

    return true;
}

void app_main_start(void) {

    while (1) {
        // Add your main loop code here
    }
}
