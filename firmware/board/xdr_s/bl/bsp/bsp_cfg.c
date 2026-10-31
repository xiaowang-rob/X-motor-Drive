// ============================================================
// bsp_cfg.c — 本板装配层实现
//
// 本文件是唯一 include 板级驱动出口的地方。
// 装配范式：BSP 导出类型化句柄（ops + dev），初始化时显式传入
// abs 对象，并挂接调用方缓冲与参数。
//
// 启动顺序：存储 → IAP → 功率级 → 采样（要绑采样点）→ 灯 → 通讯。
// 唯一运行期挂钩处：外接编码器（型号来自参数，见 bsp_enc_init）。
//
// 本板（XDr-S / STM32G474）无 WS2812 灯串，仅装配板载 GPIO LED。
// ============================================================

#include "bsp_cfg.h"

// ---- 板级驱动出口（仅本层 include） ----

#include "bl_iap.h"
#include "bsp_flash.h"
#include "bsp_led.h"
#include "bsp_uart.h"
#include "bsp_usb_vendor.h"

// ---------- 通讯缓冲（调用方提供，多路各自独立） ----------

#define UART_RXQ_BYTES 256U // 接收字节队列容量（须为 2 的幂）

static uint8_t s_uart1_rxq[UART_RXQ_BYTES];
static uint8_t s_uart1_frame[UART_MAX_PKT_SIZE + 4];
static uint8_t s_uart1_tx[UART_MAX_PKT_SIZE + 5];
static const tUartBuffer s_uart1_buf = {
    .rx_queue_buf = s_uart1_rxq,
    .rx_queue_size = UART_RXQ_BYTES,
    .frame_buf = s_uart1_frame,
    .tx_buf = s_uart1_tx,
};

static uint8_t s_usb_rxq[4U * (129U + 2U)];
static const tUsbBuffer s_usb_buf = {
    .rx_storage = s_usb_rxq,
    .rx_slot_count = 4U,
    .rx_payload_size = 129U,
};

tLed g_led;
tStorage g_storage;
tStorageUnit g_param_storage_unit;
tStorageUnit g_log_storage_unit;
tUartDriver g_uart;
tUsbDriver g_usb;

// 板载设备初始化
bool bsp_base_init(void) {

    // 存储：MCU 内部 Flash（介质几何与扇区操作见 bsp_mcu_flash.c）
    if (!storage_init(&g_storage, Get_MCU_FlashHandle()))
        return false;
    /* 与 APP 相同：参数 3 页，日志 1 页，不能由注册顺序隐式分配。 */
    if (!storage_unit_register(&g_storage, &g_param_storage_unit, 0U, 3U) ||
        !storage_unit_register(&g_storage, &g_log_storage_unit, 3U, 1U))
        return false;
    // IAP：复用同一介质 + 分区表 + 平台跳转（见 bsp_flash.h）
    if (!bl_iap_init(Get_MCU_FlashHandle(), &g_app_partition))
        return false;

    // 状态反馈（见 bsp_gpio_led.c）
    if (!led_init(&g_led, Get_Gpio_LEDHandle()))
        return false;

    if (!uart_init(&g_uart, Get_MCU_UsartHandle(), &s_uart1_buf))
        return false;
    if (!usb_init(&g_usb, Get_USB_VendorHandle(), &s_usb_buf))
        return false;

    return true;
}
