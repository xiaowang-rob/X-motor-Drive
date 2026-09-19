// ============================================================
// device_cfg.c — 组装层实现：装配全板设备并对外提供 g_dev
//
// 本文件是唯一 include 板级驱动出口（*_drivers.h）的地方。
// 装配顺序：功率级 → 采样（要绑采样点）→ 编码器 → 存储 → 灯 → 通讯。
// 所有驱动实例都是静态实例（xxx_get_handle()），本层只做"绑定 + 注入缓冲"。
// ============================================================

#include "device_cfg.h"

#include "protocol.h"
// ---- 板级驱动出口 ----
#include "bus_drivers.h"
#include "encoder_drivers.h"
#include "flash_drivers.h"
#include "gate_drivers.h"
#include "led_drivers.h"
#include "sense_drivers.h"
#include "uart_drivers.h"

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
static uint8_t s_usb_frame[UART_MAX_PKT_SIZE + 4];
static uint8_t s_usb_tx[UART_MAX_PKT_SIZE + 5];
static const tUartBuffer s_usb_buf = {
    .rx_queue_buf = s_usb_rxq,
    .rx_queue_size = UART_RXQ_BYTES,
    .frame_buf = s_usb_frame,
    .tx_buf = s_usb_tx,
};

// 全局设备对象
tDevBoard g_dev;

// app固件先初始化这个 中断向量表偏移 并开启中断 不然程序无法运行
void app_init(void)
{
    mcu_app_init();
}
// 板载设备初始化
bool dev_base_init(void)
{
    // 注册mcu flash驱动
    if (!flash_init(&g_dev.flash, &mcu_flash_driver_ops,
                    mcu_flash_get_handle()))
        return false;
    // 注册mcu app iap驱动
    tIAPConfig iap_cfg = mcu_iap_config(IAP_APP);
    if (!iap_init(&g_dev.iap, &iap_cfg))
        return false;

    // 注册栅极驱动
    if (!gate_drv_init(&g_dev.gate, &gate_fd6288q_ops, gate_get_handle()))
        return false;

    // 注册adc驱动
    if (!sense_init(&g_dev.sense, &mcu_adc_ops, sense_get_handle()))
        return false;

    // 电流采样点：跟随功率级 PWM 周期（提前一个计数）
    sense_set_sample_point(&g_dev.sense, g_dev.gate.pwm_period - 1U);

    // 注册led驱动
    bool led0 = led_init(&g_dev.led_0, &led_drv_ops, led_get_handle(0U));
    bool led1 = led_init(&g_dev.led_1, &led_drv_ops, led_get_handle(1U));
    bool rgb = rgb_init(&g_dev.rgb, &rgb_ws28xx_ops, rgb_get_handle());

    if (!led0 || !led1 || !rgb)
        return false;

    // 注册can驱动
    if (bus_init(&g_dev.can, &can_drv_ops, can_get_handle(), &s_can_buf))
        return false;

    // 注册uart驱动
    if (!uart_init(&g_dev.uart, &uart_mcu_ops, uart_mcu_get_handle(),
                   &s_uart1_buf))
        return false;
    // 注册usb驱动
    if (!uart_init(&g_dev.usb, &uart_usb_ops, uart_usb_get_handle(),
                   &s_usb_buf))
        return false;
}

// 板载编码器初始化 可配置是否启用
bool dev_int_enc_init(eEncoderChip chip)
{
    // 板载编码器驱动 mt6816
    EncoderChipHandle mt6816_int = MT6816_register_handle();

    if (!encoder_init(&g_dev.enc_int, &MT6816_driver_ops, mt6816_int, INT_ENCODER))
        return false;
}
// 外接设备驱动（可配置）
bool dev_ext_enc_init(eEncoderChip chip)
{

    bool enc_ok = false;
    switch (chip)
    {
    case ENC_NONE:
        enc_ok = true;
        break;
    case MT6816:
        EncoderChipHandle mt6816_ext = MT6816_register_handle();
        enc_ok = encoder_init(&g_dev.enc_ext, &MT6816_driver_ops, mt6816_ext, EXT_ENCODER);
        break;
    case MT6835:
        EncoderChipHandle mt6835_ext = MT6835_register_handle();
        enc_ok = encoder_init(&g_dev.enc_ext, &MT6835_driver_ops, mt6835_ext, EXT_ENCODER);
        break;
    case AS5047:
        EncoderChipHandle AS5047_ext = AS5047_register_handle();
        enc_ok = encoder_init(&g_dev.enc_ext, &AS5047_driver_ops, AS5047_ext, EXT_ENCODER);
        break;
    default:
        enc_ok = false;
        break;
    }
    return enc_ok;
}

void device_cfg_register_foc_isr(void (*sample_cb)(void), void (*ctrl_cb)(void))
{
    gate_register_isrs(sample_cb, ctrl_cb);
}
