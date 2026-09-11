// ============================================================
// uart_drv.c — 串口通讯底层驱动（usr/drv，v2 直连版）
// ============================================================
#include "uart_drivers.h"

#include "usart.h"

#define UART_CH (huart1)

#define RX_BUFFER_SIZE 128
static uint8_t rx_buffer[RX_BUFFER_SIZE];

static uart_rx_done_cb s_rx_done_cb = NULL;

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == UART_CH.Instance)
    {
        HAL_UART_RxEventTypeTypeDef eventType = HAL_UARTEx_GetRxEventType(huart);

        switch (eventType)
        {
        case HAL_UART_RXEVENT_IDLE: // 总线空闲
            s_rx_done_cb(rx_buffer, Size);
            break;

        case HAL_UART_RXEVENT_TC: // DMA缓冲区满
            s_rx_done_cb(rx_buffer, Size);
            break;

        case HAL_UART_RXEVENT_HT: // DMA半传输完成
            s_rx_done_cb(rx_buffer, Size);
            break;
        }

        // 如果是 Normal 模式，通常在此处重新启动接收
        HAL_UARTEx_ReceiveToIdle_DMA(&UART_CH, rx_buffer, RX_BUFFER_SIZE);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    // 错误后重新挂起接收，保证链路可用
    if (huart == &UART_CH && huart->RxState == HAL_UART_STATE_READY)
        HAL_UARTEx_ReceiveToIdle_DMA(&UART_CH, rx_buffer, RX_BUFFER_SIZE);
}
bool uart_init(void)
{
    HAL_UARTEx_ReceiveToIdle_DMA(&UART_CH, rx_buffer, RX_BUFFER_SIZE); // 启动
}

bool uart_tx(uint8_t *data, uint16_t len)
{
    if (!data || len == 0U)
        return false;
    return HAL_OK == HAL_UART_Transmit_DMA(&UART_CH, data, len);
}

void uart_register_rx_done(uart_rx_done_cb cb)
{
    s_rx_done_cb = cb;
}
