// ============================================================
// uart_mcu.c — MCU 串口通讯驱动（板级，直连 HAL）
//
// 实例形态：文件内静态 handle，内含
//   - ops 指针、外设句柄（huart）
//   - 配置（DMA 接收缓冲 + 长度）
//   - 收字节回调 + ctx（回调把实例送回 abs 层）
// 接收：HAL_UARTEx_ReceiveToIdle_DMA（空闲/完成事件判定一帧结束）
// 发送：DMA（缓冲由 abs 层实例持有，见 uart_com.c 的 tx_buf）
// HAL_UARTEx_RxEventCallback / HAL_UART_ErrorCallback 由本文件唯一持有。
// ============================================================
#include "uart_drivers.h"

#include "usart.h"

// ---------- 本板配置 ----------
#define UART_MCU_RX_BUFFER_SIZE 128U

// ---------- 实例 handle ----------
typedef struct
{
    UART_HandleTypeDef *huart;                  // 外设
    uint8_t rx_buffer[UART_MCU_RX_BUFFER_SIZE]; // 配置：DMA 接收缓冲
    uint16_t rx_buffer_size;                    // 配置：缓冲长度
    uart_rx_done_cb rx_cb;                      // 收字节回调（由 abs 层注册）
    void *rx_ctx;                               // 回调上下文（abs 层实例）
} tUartMcu;

static bool uart_mcu_init(UartHandle h);
static bool uart_mcu_send(UartHandle h, const uint8_t *data, uint16_t len);
static void uart_mcu_register(UartHandle h, uart_rx_done_cb cb, void *ctx);

const tUartDriverOps uart_mcu_ops = {
    .init = uart_mcu_init,
    .send = uart_mcu_send,
    .register_callback = uart_mcu_register,
};

// 静态实例
static tUartMcu s_uart1 = {
    .huart = &huart1,
    .rx_buffer_size = UART_MCU_RX_BUFFER_SIZE,
    .rx_cb = NULL,
    .rx_ctx = NULL,
};

UartHandle uart_mcu_get_handle(void)
{
    return (UartHandle)&s_uart1;
}

// ---- 中断（只在本文件定义） ----
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart != s_uart1.huart)
        return;

    HAL_UART_RxEventTypeTypeDef ev = HAL_UARTEx_GetRxEventType(huart);

    // 空闲(IDLE)：不定长帧结束；完成(TC)：缓冲满。
    // 半传输(HT) 不上报 —— 否则会把半个缓冲当成一帧。
    if ((ev == HAL_UART_RXEVENT_IDLE || ev == HAL_UART_RXEVENT_TC) && s_uart1.rx_cb)
        s_uart1.rx_cb(s_uart1.rx_ctx, s_uart1.rx_buffer, Size);

    HAL_UARTEx_ReceiveToIdle_DMA(huart, s_uart1.rx_buffer, s_uart1.rx_buffer_size);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart != s_uart1.huart)
        return;
    // 出错后重新挂起接收，保证链路可用（错误后 RxState 未必是 READY）
    HAL_UARTEx_ReceiveToIdle_DMA(huart, s_uart1.rx_buffer, s_uart1.rx_buffer_size);
}

// ---- ops 实现 ----

static bool uart_mcu_init(UartHandle h)
{
    tUartMcu *inst = (tUartMcu *)h;
    if (!inst || !inst->huart || inst->rx_buffer_size == 0U)
        return false;
    return HAL_UARTEx_ReceiveToIdle_DMA(inst->huart, inst->rx_buffer,
                                        inst->rx_buffer_size) == HAL_OK;
}

static bool uart_mcu_send(UartHandle h, const uint8_t *data, uint16_t len)
{
    tUartMcu *inst = (tUartMcu *)h;
    if (!inst || !inst->huart || !data || len == 0U)
        return false;
    return HAL_UART_Transmit_DMA(inst->huart, (uint8_t *)data, len) == HAL_OK;
}

static void uart_mcu_register(UartHandle h, uart_rx_done_cb cb, void *ctx)
{
    tUartMcu *inst = (tUartMcu *)h;
    if (!inst)
        return;
    inst->rx_cb = cb;
    inst->rx_ctx = ctx;
}
