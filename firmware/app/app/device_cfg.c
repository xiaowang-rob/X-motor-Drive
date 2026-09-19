// ============================================================
// device_cfg.c — 组装层实现：装配全板设备并对外提供 g_dev
//
// 本文件是唯一 include 板级驱动出口的地方。
// 装配顺序：存储 → 功率级 → 采样（要绑采样点）→ 灯 → 通讯。
//
// 编译期绑定：全部设备已改为"板级钩子 + 静态实例"，
// 本层不再传 ops/handle，只做装配与参数注入（见各 *_board.h）。
// ============================================================

#include "device_cfg.h"

#include "protocol.h"
// ---- 板级 Flash / IAP 出口 ----
#include "board_flash.h"

// 协议层 eEncoderChip 与 abs 层 eEncoderChipId 枚举值必须一一对齐
// （两侧先转 int：两个匿名 enum 直接比较会触发 -Wenum-compare）
_Static_assert((int)ENC_NONE == (int)ENC_CHIP_NONE, "eEncoderChip vs eEncoderChipId: NONE");
_Static_assert((int)MT6816 == (int)ENC_CHIP_MT6816, "eEncoderChip vs eEncoderChipId: MT6816");
_Static_assert((int)MT6835 == (int)ENC_CHIP_MT6835, "eEncoderChip vs eEncoderChipId: MT6835");
_Static_assert((int)AS5047 == (int)ENC_CHIP_AS5047, "eEncoderChip vs eEncoderChipId: AS5047");

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

// app 固件先初始化这个（中断向量表偏移）并开启中断，不然程序无法运行
void app_init(void)
{
    mcu_app_init();
}

// 板载设备初始化
bool dev_base_init(void)
{
    // 注册 MCU Flash 介质（板级钩子，见 board_flash.c）
    if (!flash_init(&g_dev.flash, FLASH_DEV_MCU))
        return false;
    // 注册 MCU App IAP（分区表 + 平台跳转，见 fla_mcu.c）
    tIAPConfig iap_cfg = mcu_iap_config(IAP_APP);
    if (!iap_init(&g_dev.iap, &iap_cfg))
        return false;

    // 注册栅极驱动（板级钩子 + const 实例，见 gate_fd6288q.c）
    if (!gate_drv_init(&g_dev.gate))
        return false;

    // 注册采样前端（板级钩子，见 sen_mcu.c）
    if (!sense_init(&g_dev.sense))
        return false;

    // 电流采样点：跟随功率级 PWM 周期（提前一个计数）
    sense_set_sample_point(&g_dev.sense, g_dev.gate.pwm_period - 1U);

    // 注册 LED（板级钩子，见 led_gpio.c / led_ws28xx.c）
    bool led0 = led_init(&g_dev.led_0, 0U);
    bool led1 = led_init(&g_dev.led_1, 1U);
    bool rgb = rgb_init(&g_dev.rgb);

    if (!led0 || !led1 || !rgb)
        return false;

    // 注册总线（板级钩子，见 board_bus.c / bus_can.c）
    if (!bus_init(&g_dev.can, BUS_PORT_CAN, &s_can_buf))
        return false;

    // 注册串口（板级钩子，见 board_uart.c）
    if (!uart_init(&g_dev.uart, UART_PORT_MCU, &s_uart1_buf))
        return false;
    // 注册 usb 虚拟串口
    if (!uart_init(&g_dev.usb, UART_PORT_USB, &s_usb_buf))
        return false;

    return true;
}

// 协议层型号 → abs 芯片标识
static eEncoderChipId enc_chip_id(eEncoderChip chip)
{
    switch (chip)
    {
    case MT6816:
        return ENC_CHIP_MT6816;
    case MT6835:
        return ENC_CHIP_MT6835;
    case AS5047:
        return ENC_CHIP_AS5047;
    case ENC_NONE:
    default:
        return ENC_CHIP_NONE;
    }
}

// 板载编码器初始化（板载型号固定，见板级 board_encoder.c）
bool dev_int_enc_init(void)
{
    return encoder_init(&g_dev.enc_int, INT_ENCODER, ENC_CHIP_MT6816);
}

// 外接编码器初始化（型号来自参数；未装配不算失败）
bool dev_ext_enc_init(eEncoderChip chip)
{
    eEncoderChipId id = enc_chip_id(chip);
    if (id == ENC_CHIP_NONE)
        return true;
    return encoder_init(&g_dev.enc_ext, EXT_ENCODER, id);
}

void device_cfg_register_foc_isr(void (*sample_cb)(void), void (*ctrl_cb)(void))
{
    gate_drv_register_isrs(sample_cb, ctrl_cb);
}
