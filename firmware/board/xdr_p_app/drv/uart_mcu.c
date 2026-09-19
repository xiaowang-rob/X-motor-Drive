// ============================================================
// uart_mcu.c — MCU 串口通讯驱动（板级，直连 HAL）
//
// 实例形态：文件内静态实例（外设 huart / DMA 接收缓冲 / 回调），无 ops、无堆。
// 接收：HAL_UARTEx_ReceiveToIdle_DMA（空闲/完成事件判定一帧结束）
// 发送：DMA（缓冲由 abs 层实例持有，见 uart_com.c 的 tx_buf）
// 中断：HAL_UARTEx_RxEventCallback / HAL_UART_ErrorCallback 本体在 bsp_irq，
//       本驱动只注册处理函数。
// 本文件实现 app/abs/uart_com_board.h 的 MCU 路由（由 board_uart.c 分派）。
// ============================================================
#include "uart_mcu.h"

#include "bsp_irq.h"

#include "usart.h"

// ---------- 本板配置 ----------
#define UART_MCU_RX_BUFFER_SIZE 128U

// ---------- 实例 ----------
typedef struct
{
    UART_HandleTypeDef *huart;                  // 外设
    uint8_t rx_buffer[UART_MCU_RX_BUFFER_SIZE]; // DMA 接收缓冲
    uint16_t rx_buffer_size;                    // 缓冲长度
    uart_rx_done_cb rx_cb;                      // 收字节回调（由 abs 层注册）
    void *rx_ctx;                               // 回调上下文（abs 层实例）
} tUartMcu;

static tUartMcu s_uart1 = {
    .huart = &huart1,
    .rx_buffer_size = UART_MCU_RX_BUFFER_SIZE,
    .rx_cb = NULL,
    .rx_ctx = NULL,
};

// ---- 中断 ----
// HAL 回调本体在 bsp_irq；本驱动只注册处理函数（见 uart_mcu_open）。
// bsp_irq 已过滤半传输(HT)，此处只会收到 IDLE / TC。
static void uart_mcu_on_rx_event(void *ctx, uint16_t rx_len, bool is_idle)
{
    (void)is_idle; // IDLE 与 TC 都表示"一块数据到齐"，处理一致
    tUartMcu *inst = (tUartMcu *)ctx;

    if (inst->rx_cb)
        inst->rx_cb(inst->rx_ctx, inst->rx_buffer, rx_len);

    // 重新挂起接收，继续收下一块
    HAL_UARTEx_ReceiveToIdle_DMA(inst->huart, inst->rx_buffer, inst->rx_buffer_size);
}

static void uart_mcu_on_error(void *ctx)
{
    tUartMcu *inst = (tUartMcu *)ctx;
    // 出错后重新挂起接收，保证链路可用（错误后 RxState 未必是 READY）
    HAL_UARTEx_ReceiveToIdle_DMA(inst->huart, inst->rx_buffer, inst->rx_buffer_size);
}

// ---- 板级钩子实现（MCU 路由） ----

bool uart_mcu_open(void)
{
    if (!s_uart1.huart || s_uart1.rx_buffer_size == 0U)
        return false;

    static const tUartIrq s_uart_irq = {
        .on_rx_event = uart_mcu_on_rx_event,
        .on_error = uart_mcu_on_error,
        .ctx = &s_uart1,
    };
    if (!bsp_irq_bind_uart(s_uart1.huart->Instance, &s_uart_irq))
        return false;

    return HAL_UARTEx_ReceiveToIdle_DMA(s_uart1.huart, s_uart1.rx_buffer,
                                        s_uart1.rx_buffer_size) == HAL_OK;
}

bool uart_mcu_send(const uint8_t *data, uint16_t len)
{
    if (!s_uart1.huart || !data || len == 0U)
        return false;
    return HAL_UART_Transmit_DMA(s_uart1.huart, (uint8_t *)data, len) == HAL_OK;
}

void uart_mcu_register_cb(uart_rx_done_cb cb, void *ctx)
{
    s_uart1.rx_cb = cb;
    s_uart1.rx_ctx = ctx;
}
