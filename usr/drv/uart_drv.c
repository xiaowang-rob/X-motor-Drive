// ============================================================
// uart_drv.c — 串口通讯底层驱动（usr/drv，v2 直连版）
// ============================================================

#include "platform.h" // UART_CH / UART_INSTANCE
#include "uart_drv.h"

static uart_rx_done_cb s_rx_done_cb = NULL;

// ---- 接收完成/错误中断（只在本文件定义） ----
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &UART_CH && s_rx_done_cb)
        s_rx_done_cb(huart->RxXferSize);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    // 错误后重新挂起接收，保证链路可用
    if (huart == &UART_CH && huart->RxState == HAL_UART_STATE_READY)
        HAL_UART_Receive_DMA(&UART_CH, huart->pRxBuffPtr, huart->RxXferSize);
}

void uart_drv_rx_start(uint8_t *buf, uint16_t len)
{
    if (buf && len)
        HAL_UART_Receive_DMA(&UART_CH, buf, len);
}

bool uart_drv_tx(uint8_t *data, uint16_t len)
{
    if (!data || len == 0U)
        return false;
    return HAL_UART_Transmit_DMA(&UART_CH, data, len) == HAL_OK;
}

void uart_drv_register_rx_done(uart_rx_done_cb cb)
{
    s_rx_done_cb = cb;
}
