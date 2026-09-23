// ============================================================
// bsp_cfg.c — 本板装配层实现
//
// 本文件是唯一 include 板级驱动出口的地方。
// 装配范式：每个设备对象的**定义处**就地挂上 ops 与 handle；启动时
// 只做"open + 缓冲挂接 + 参数注入"，选型不再散落在驱动里。
//
// 启动顺序：存储 → IAP → 功率级 → 采样（要绑采样点）→ 灯 → 通讯。
// 唯一运行期挂钩处：外接编码器（型号来自参数，见 dev_ext_enc_init）。
//
// 本板（XDr-S / STM32G474）无 WS2812 灯串，仅装配两颗 GPIO LED。
// ============================================================

#include "bsp_cfg.h"

// ---- 板级驱动出口（仅本层 include） ----
#include "bus_can.h"      // g_can2 / can_bus_ops
#include "enc_as5047.h"   // g_as5047_ext / as5047_ops
#include "enc_mt6816.h"   // g_mt6816_int / mt6816_ops
#include "enc_mt6835.h"   // g_mt6835_ext / mt6835_ops
#include "fla_mcu.h"      // g_fla_mcu / fla_mcu_ops
#include "gate_fd6288q.h" // g_fd6288q / fd6288q_ops
#include "led_gpio.h"     // g_led_gpio_0/1 / led_gpio_ops
#include "sen_mcu.h"      // g_sen_mcu / sen_mcu_ops
#include "uart_mcu.h"     // g_uart1 / uart_mcu_ops
#include "uart_usb_cdc.h" // g_uart_usb / uart_usb_ops

// ---- 存储 ----
tFlash g_flash = {
    .ops = &fla_mcu_ops,
    .handle = &g_fla_mcu,
};
tIAP g_iap = {
    .flash_ops = &fla_mcu_ops, // 复用同一介质
    .flash_handle = &g_fla_mcu,
    .parts = &g_app_parts,   // 分区表
    .jump = mcu_iap_jump_bl, // app 跳转到 BL
};

// ---- 功率级 ----
tGateDrv g_gate = {
    .ops = &fd6288q_ops,
    .handle = &g_fd6288q,
};

// ---- 状态反馈 ----
tLed g_led_0 = {
    .ops = &led_gpio_ops,
    .handle = &g_led_gpio_0, // 每颗 LED 一个实例
};
tLed g_led_1 = {
    .ops = &led_gpio_ops,
    .handle = &g_led_gpio_1,
};

// ---- 通讯 ----
tBusDriver g_can = {
    .ops = &can_bus_ops,
    .handle = &g_can2,
};
tUartDriver g_uart = {
    .ops = &uart_mcu_ops,
    .handle = &g_uart1,
};
tUartDriver g_usb = {
    .ops = &uart_usb_ops,
    .handle = &g_uart_usb,
};

// ---- 传感器 ----
tEncoder g_enc_int = {
    .ops = &mt6816_ops, // 板载编码器型号固定
    .handle = &g_mt6816_int,
    .mode = ENC_OFF,
};
tEncoder g_enc_ext = {
    // ops / handle 由参数中的型号决定，见 dev_ext_enc_init
    .ops = &mt6816_ops, // 默认
    .handle = &g_mt6816_int,
    .mode = ENC_OFF,
};
tSense g_sense = {
    .ops = &sen_mcu_ops,
    .handle = &g_sen_mcu,
};

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

// app 固件先初始化这个（中断向量表偏移）并开启中断，不然程序无法运行
void iap_app_init(void)
{
    mcu_iap_app_init();
}

// 板载设备初始化
bool dev_base_init(void)
{
    // 存储：MCU 内部 Flash（介质几何与扇区操作见 fla_mcu.c）
    if (!flash_init(&g_flash))
        return false;
    // IAP：复用同一介质 + 分区表 + 平台跳转（见 fla_mcu.h）
    if (!iap_init(&g_iap))
        return false;

    // 功率级（见 gate_fd6288q.c）
    if (!gate_drv_init(&g_gate))
        return false;

    // 采样前端（见 sen_mcu.c）
    if (!sense_init(&g_sense))
        return false;

    // 电流采样点：跟随功率级 PWM 周期（提前一个计数）
    sense_set_sample_point(&g_sense, g_gate.pwm_period - 1U);

    // 状态反馈（见 led_gpio.c）
    if (!led_init(&g_led_0) || !led_init(&g_led_1))
        return false;

    // 通讯（见 bus_can.c / uart_mcu.c / uart_usb_cdc.c）
    if (!bus_init(&g_can, &s_can_buf))
        return false;
    if (!uart_init(&g_uart, &s_uart1_buf))
        return false;
    if (!uart_init(&g_usb, &s_usb_buf))
        return false;

    return true;
}

// 板载编码器初始化（型号固定，ops/handle 已在定义处挂好）
bool dev_int_enc_init(eEncoderMode mode)
{
    return encoder_init(&g_enc_int, mode);
}

bool dev_ext_enc_init(eEncoderChip chip, eEncoderMode mode)
{
    switch (chip)
    {
    case MT6816:
        g_enc_ext.ops = &mt6816_ops;
        g_enc_ext.handle = &g_mt6816_ext;
        break;
    case MT6835:
        g_enc_ext.ops = &mt6835_ops;
        g_enc_ext.handle = &g_mt6835_ext;
        break;
    case AS5047:
        g_enc_ext.ops = &as5047_ops;
        g_enc_ext.handle = &g_as5047_ext;
        break;

    default:
        g_enc_ext.ops = NULL;
        g_enc_ext.handle = NULL;
        return false; // 该型号未装配
    }
    return encoder_init(&g_enc_ext, mode);
}

void device_cfg_register_foc_isr(void (*sample_cb)(void), void (*ctrl_cb)(void))
{
    gate_drv_register_isrs(&g_gate, sample_cb, ctrl_cb);
}
