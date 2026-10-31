// ============================================================
//  MCU 串口通讯驱动（板级）
// 115200bps，8N1，DMA 定长接收 + 空闲中断（ReceiveToIdle_DMA），DMA 发送
// ============================================================
#include "bsp_uart.h"

#include "usart.h"

// ---------- 实例 ----------
typedef struct tMcuUart {
    uint8_t rx_buffer[MCU_UART_RX_SIZE]; // DMA 接收缓冲
    volatile uint16_t s_rx_last_pos; // RX 上次交给回调的“位置”（DMA 写指针）
    uart_rx_done_cb rx_cb;           // 收字节回调（由 uart_com 注册）
    void *rx_ctx;                    // 回调上下文（uart_com 实例）
} tMcuUart;

tMcuUart mcu_uart2 = {
    .s_rx_last_pos = 0,
    .rx_cb = NULL,
    .rx_ctx = NULL,
};

// ---- 中断 ----

void USART2_IRQHandler(void) {
    /* ============ IDLE：线路空闲，一帧（或一段）结束 ============ */
    if (LL_USART_IsActiveFlag_IDLE(USART2) &&
        LL_USART_IsEnabledIT_IDLE(USART2)) {
        LL_USART_ClearFlag_IDLE(USART2);

        /* DMA 剩余计数 → 当前写指针位置 */
        uint32_t remain = LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_2);
        uint16_t cur = (uint16_t)(MCU_UART_RX_SIZE - remain);

        /* 计算从上次位置到现在的字节数（处理 CIRCULAR 绕回） */
        uint16_t len;
        if (cur >= mcu_uart2.s_rx_last_pos)
            len = cur - mcu_uart2.s_rx_last_pos;
        else
            len = MCU_UART_RX_SIZE - mcu_uart2.s_rx_last_pos + cur; /* 跨界 */

        if (len > 0 && mcu_uart2.rx_cb) {
            mcu_uart2.rx_cb(mcu_uart2.rx_ctx,
                            mcu_uart2.rx_buffer + mcu_uart2.s_rx_last_pos,
                            len);
        }

        /* 只更新位置，不动 DMA —— 无空窗、不丢字节 */
        mcu_uart2.s_rx_last_pos = cur;
    }

    /* ============ 出错处理 ============ */
    if (LL_USART_IsActiveFlag_ORE(USART2))
        LL_USART_ClearFlag_ORE(USART2);
    if (LL_USART_IsActiveFlag_FE(USART2))
        LL_USART_ClearFlag_FE(USART2);
    if (LL_USART_IsActiveFlag_NE(USART2))
        LL_USART_ClearFlag_NE(USART2);
    if (LL_USART_IsActiveFlag_PE(USART2))
        LL_USART_ClearFlag_PE(USART2);
}

/* TX DMA 完成中断：关闭 TX 请求，避免后续空闲线仍不断发 0x00 */
void DMA1_Channel3_IRQHandler(void) {
    if (LL_DMA_IsActiveFlag_TC3(DMA1)) {
        LL_DMA_ClearFlag_TC3(DMA1);
        LL_USART_DisableDMAReq_TX(USART2); /* ★ 关键：关闭 TX DMA 请求 */
        LL_DMA_DisableIT_TC(DMA1, LL_DMA_CHANNEL_3);

        /* 等最后一个字节真正移位完成再回调（可选） */
        while (!LL_USART_IsActiveFlag_TC(USART2)) {
        }
        LL_USART_ClearFlag_TC(USART2);
    }
}

// ---- 驱动接口（tUartOps） ----

static bool mcu_uart_open(void *handle) {
    tMcuUart *inst = (tMcuUart *)handle;
    if (!inst)
        return false;

    /* 1. 停掉可能还在跑的 DMA / UART RX 请求，防止残留 */
    LL_USART_DisableDMAReq_RX(MCU_UART);
    LL_DMA_DisableChannel(MCU_UART_DMA, MCU_UART_DMA_CH);
    while (LL_DMA_IsEnabledChannel(MCU_UART_DMA, MCU_UART_DMA_CH)) {
    }

    /* 2. 装载地址和长度 */
    LL_DMA_SetDataLength(MCU_UART_DMA, MCU_UART_DMA_CH, MCU_UART_RX_SIZE);
    LL_DMA_SetPeriphAddress(
        MCU_UART_DMA,
        MCU_UART_DMA_CH,
        LL_USART_DMA_GetRegAddr(MCU_UART, LL_USART_DMA_REG_DATA_RECEIVE));
    LL_DMA_SetMemoryAddress(
        MCU_UART_DMA, MCU_UART_DMA_CH, (uint32_t)mcu_uart2.rx_buffer);

    /* 3. 清残留标志（HT/TC/IDLE），否则首次进 ISR 会误触发 */
    LL_DMA_ClearFlag_HT2(MCU_UART_DMA);
    LL_DMA_ClearFlag_TC2(MCU_UART_DMA);
    LL_USART_ClearFlag_IDLE(MCU_UART);

    /* 4. 状态归零 */
    mcu_uart2.s_rx_last_pos = 0;

    /* 5. 开 DMA 通道 + 开 UART 的 DMA RX 请求 + 开 IDLE 中断 */
    LL_DMA_EnableChannel(MCU_UART_DMA, MCU_UART_DMA_CH);
    LL_USART_EnableDMAReq_RX(MCU_UART);
    LL_USART_EnableIT_IDLE(MCU_UART);

    /* HT/TC 中断：CIRCULAR + IDLE 位置差方案下，通常不需要开。
       如果上层协议固定帧长、想按“半满/满”断帧，可以打开：
       LL_DMA_EnableIT_HT(MCU_UART_DMA, MCU_UART_DMA_CH);
       LL_DMA_EnableIT_TC(MCU_UART_DMA, MCU_UART_DMA_CH);
    */

    return true;
}

static bool mcu_uart_send(void *handle, const uint8_t *data, uint16_t len) {
    tMcuUart *inst = (tMcuUart *)handle;
    if (!inst || !data || len == 0U)
        return false;
    /* 上一次还没发完？可以选择等待或返回忙 */
    if (LL_DMA_IsEnabledChannel(MCU_UART_DMA, LL_DMA_CHANNEL_3))
        return false; /* 或者 while 等待，看业务需求 */

    LL_DMA_DisableChannel(MCU_UART_DMA, LL_DMA_CHANNEL_3);
    LL_DMA_ClearFlag_TC3(MCU_UART_DMA);

    LL_DMA_SetDataLength(MCU_UART_DMA, LL_DMA_CHANNEL_3, len);
    LL_DMA_SetPeriphAddress(
        MCU_UART_DMA,
        LL_DMA_CHANNEL_3,
        LL_USART_DMA_GetRegAddr(MCU_UART, LL_USART_DMA_REG_DATA_TRANSMIT));
    LL_DMA_SetMemoryAddress(MCU_UART_DMA, LL_DMA_CHANNEL_3, (uint32_t)data);

    /* 使能 DMAT（传输完成中断）以便关闭 DMAT 请求 */
    LL_USART_EnableDMAReq_TX(MCU_UART);
    LL_DMA_EnableIT_TC(MCU_UART_DMA, LL_DMA_CHANNEL_3);
    LL_DMA_EnableChannel(MCU_UART_DMA, LL_DMA_CHANNEL_3);

    return true;
}

static void mcu_uart_set_rx_cb(void *handle, uart_rx_done_cb cb, void *ctx) {
    tMcuUart *inst = (tMcuUart *)handle;
    if (!inst)
        return;
    inst->rx_cb = cb;
    inst->rx_ctx = ctx;
}

// ---- 驱动出口 ----
const tUartOps mcu_uart_ops = {
    .open = mcu_uart_open,
    .send = mcu_uart_send,
    .set_rx_cb = mcu_uart_set_rx_cb,
};

static const tUartDevHandle s_dev_handle = {
    .ops = &mcu_uart_ops,
    .dev = &mcu_uart2,
};

const tUartDevHandle *Get_MCU_UsartHandle(void) { return &s_dev_handle; }
