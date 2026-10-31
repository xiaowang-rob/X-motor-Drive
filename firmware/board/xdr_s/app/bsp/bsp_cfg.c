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
#include "bsp_bus.h"
#include "bsp_enc.h"
#include "bsp_flash.h"
#include "bsp_led.h"

#include "bsp_adc.h"
#include "bsp_pwm.h"
#include "bsp_uart.h"
#include "bsp_usb_vendor.h"

#include "parameter_list.h"

const uint32_t TIC_PWM = 4249;
const float F_PWM = PWM_FREQ;  // PWM频率
const float T_PWM = 1 / F_PWM; // PWM周期
const float F_CON = F_PWM;     // 控制基频 (Hz)
const float T_CON = T_PWM;     // 控制周期 (s)

// ---------- 通讯缓冲（调用方提供，多路各自独立） ----------
#define CAN_MP_BLOCKS 16U
CREATE_MEM_POOL_BUF(s_can_mp_buf, sizeof(tBus_Frame), CAN_MP_BLOCKS)
#define CAN_QUEUE_BYTES (CAN_MP_BLOCKS * sizeof(tBus_Frame *))
static uint8_t s_can_queue_buf[CAN_QUEUE_BYTES];

static const tBusBuffer s_can_buf = {
    .mp_buf = s_can_mp_buf,
    .mp_block_num = CAN_MP_BLOCKS,
    .queue_buf = s_can_queue_buf,
    .queue_bytes = CAN_QUEUE_BYTES,
};

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

static uint8_t s_usb_rxq[UART_RXQ_BYTES];
static const tUsbBuffer s_usb_buf = {
    .rx_storage = s_usb_rxq,
    .rx_slot_count = UART_RXQ_BYTES,
    .rx_payload_size = UART_MAX_PKT_SIZE,
};

tBusDriver g_bus_can;
tLed g_led;
tStorage g_storage;
tStorageUnit g_param_storage_unit;
tStorageUnit g_log_storage_unit;
tUartDriver g_uart;
tUsbDriver g_usb;
tEncoder g_enc;
tPwmDrv g_pwm;
tSample g_sample;

// 板载设备初始化
bool bsp_base_init(void) {

    // 存储：MCU 内部 Flash（介质几何与扇区操作见 bsp_mcu_flash.c）
    if (!storage_init(&g_storage, Get_MCU_FlashHandle()))
        return false;
    /* USR：前 3 页参数轮换，最后 1 页日志。策略由 storage 抽象层实现。 */
    if (!storage_unit_register(&g_storage, &g_param_storage_unit, 0U, 3U) ||
        !storage_unit_register(&g_storage, &g_log_storage_unit, 3U, 1U))
        return false;

    // 功率级（见 bsp_mcu_pwm.c）
    if (!pwm_init(&g_pwm, Get_MCU_PWMHandle()))
        return false;

    // 采样驱动启动电流及电压/温度 ADC。
    if (!sample_init(&g_sample, Get_MCU_ADCHandle()))
        return false;

    // 状态反馈（见 bsp_gpio_led.c）
    if (!led_init(&g_led, Get_Gpio_LEDHandle()))
        return false;

    // 通讯（CAN、MCU UART、USB vendor HID 的 UART 帧兼容适配）
    if (!bus_init(&g_bus_can, Get_CanHandle(), &s_can_buf))
        return false;
    if (!uart_init(&g_uart, Get_MCU_UsartHandle(), &s_uart1_buf))
        return false;
    if (!usb_init(&g_usb, Get_USB_VendorHandle(), &s_usb_buf))
        return false;

    return true;
}

// 外接编码器初始化
bool bsp_enc_init(eObsList enc, eSpiEncList spi_enc) {

    tEncDevHandle *handle;

    switch (enc) {
    case OBS_NONE:
        return true;

    case OBS_INT_ENC:
        handle = Get_INT_ENCHandle();
        break;
    case OBS_ENC_HALL:
        handle = Get_HALL_ENCHandle();
        break;
    case OBS_ENC_SPI:
        handle = Get_SPI_ENCHandle(spi_enc);
        break;
    case OBS_ENC_ABI:
        handle = Get_ABI_ENCHandle();
        break;
    case OBS_ENC_SINCOS:
        handle = Get_SINCOS_ENCHandle();
        break;
    default:
        return false;
    }

    return encoder_init(&g_enc, handle);
}
