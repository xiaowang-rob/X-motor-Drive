// ============================================================
// uart_mcu.c — MCU 串口通讯驱动（板级）
// 115200bps，8N1，DMA 接收 + 空闲中断（ReceiveToIdle_DMA），DMA 发送
//
// 本板外设：USART2（Core/Src/usart.c 的 huart2，PA2/PA3，DMA1_Ch2/Ch3）。
// 中断：HAL_UARTEx_RxEventCallback / HAL_UART_ErrorCallback 由本驱动定义
//       （本板 MCU 串口只有 USART2 一路）。
// 实现 abs/uart_com.h 的 tUartOps。
// ============================================================
#include "uart_mcu.h"

#include "usart.h"

// ---------- 本板配置 ----------
#define UART_MCU_RX_BUFFER_SIZE 64U

// ---------- 实例 ----------
struct tUartMcu
{
    UART_HandleTypeDef *huart;                  // 外设
    uint8_t rx_buffer[UART_MCU_RX_BUFFER_SIZE]; // DMA 接收缓冲
    uint16_t rx_buffer_size;                    // 缓冲长度
    uart_rx_done_cb rx_cb;                      // 收字节回调（由 uart_com 注册）
    void *rx_ctx;                               // 回调上下文（uart_com 实例）
};

// 注：符号名沿用 g_uart1（"第 1 路 MCU 串口"），本板接在 USART2 上。
tUartMcu g_uart1 = {
    .huart = &huart2,
    .rx_buffer_size = UART_MCU_RX_BUFFER_SIZE,
    .rx_cb = NULL,
    .rx_ctx = NULL,
};

// ---- 中断 ----
// 半传输(HT) 不代表一帧结束，过滤掉；IDLE / TC 均表示"一块数据到齐"。
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart != g_uart1.huart)
        return;

    HAL_UART_RxEventTypeTypeDef ev = HAL_UARTEx_GetRxEventType(huart);
    if (ev == HAL_UART_RXEVENT_HT)
        return;

    if (g_uart1.rx_cb)
        g_uart1.rx_cb(g_uart1.rx_ctx, g_uart1.rx_buffer, Size);

    // 重新挂起接收，继续收下一块
    HAL_UARTEx_ReceiveToIdle_DMA(g_uart1.huart, g_uart1.rx_buffer, g_uart1.rx_buffer_size);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart != g_uart1.huart)
        return;
    // 出错后重新挂起接收，保证链路可用（错误后 RxState 未必是 READY）
    HAL_UARTEx_ReceiveToIdle_DMA(g_uart1.huart, g_uart1.rx_buffer, g_uart1.rx_buffer_size);
}

// ---- 驱动接口（tUartOps） ----

static bool uart_mcu_open(void *handle)
{
    tUartMcu *inst = (tUartMcu *)handle;
    if (!inst || !inst->huart || inst->rx_buffer_size == 0U)
        return false;

    return HAL_UARTEx_ReceiveToIdle_DMA(inst->huart, inst->rx_buffer,
                                        inst->rx_buffer_size) == HAL_OK;
}

static bool uart_mcu_send(void *handle, const uint8_t *data, uint16_t len)
{
    tUartMcu *inst = (tUartMcu *)handle;
    if (!inst || !inst->huart || !data || len == 0U)
        return false;
    return HAL_UART_Transmit_DMA(inst->huart, (uint8_t *)data, len) == HAL_OK;
}

static void uart_mcu_set_rx_cb(void *handle, uart_rx_done_cb cb, void *ctx)
{
    tUartMcu *inst = (tUartMcu *)handle;
    if (!inst)
        return;
    inst->rx_cb = cb;
    inst->rx_ctx = ctx;
}

// ---- 驱动出口 ----
const tUartOps uart_mcu_ops = {
    .open = uart_mcu_open,
    .send = uart_mcu_send,
    .set_rx_cb = uart_mcu_set_rx_cb,
};
