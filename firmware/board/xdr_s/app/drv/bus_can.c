// ============================================================
// bus_can.c — CAN 通讯底层驱动（板级，直连 HAL）
//
// 本板为 STM32G474 的 FDCAN1（经典帧模式，见 Core/Src/fdcan.c），
// 与 F4 的 bxCAN（HAL_CAN_*）是不同外设，故本驱动走 FDCAN API。
// 实现 abs/bus_com.h 的 tBusOps；HAL 收帧回调由本驱动定义（本板 CAN
// 只有 FDCAN1 一路）。
// ============================================================
#include "bus_can.h"

#include "fdcan.h"

// ---------- 本板配置 ----------
#define CAN_STD_ID_MASK 0x7FFU // 标准帧 11 位 ID 掩码
#define CAN_FILTER_INDEX 0U    // Core 已配 StdFiltersNbr=1，启用 0 号过滤器

// ---------- 实例 ----------
struct tCanBus
{
    FDCAN_HandleTypeDef *hcan; // 外设
    bus_rx_frame_cb rx_cb;     // 收帧回调（由 bus_com 注册）
    void *rx_ctx;              // 回调上下文（bus_com 实例）
};

tCanBus g_can0 = {
    .hcan = &hfdcan1,
    .rx_cb = NULL,
    .rx_ctx = NULL,
};

// ---- 收帧中断 ----
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    (void)RxFifo0ITs; // HAL 已过滤到 RX FIFO0 新报文
    if (!g_can0.hcan || hfdcan->Instance != g_can0.hcan->Instance)
        return;

    // 一次中断可能堆积多帧：把 RX FIFO0 取空
    while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, FDCAN_RX_FIFO0) > 0U)
    {
        uint8_t data[8];
        FDCAN_RxHeaderTypeDef hdr;
        if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &hdr, data) != HAL_OK)
            break;

        if (g_can0.rx_cb)
        {
            // 经典帧 DLC 0..8 即字节数；本驱动只暴露 ≤8 字节载荷
            uint8_t len = (hdr.DataLength <= 8U) ? (uint8_t)hdr.DataLength : 8U;
            g_can0.rx_cb(g_can0.rx_ctx, hdr.Identifier, data, len);
        }
    }
}

// 按标准帧 ID 配置过滤器：精确匹配一个 ID → RX FIFO0
static bool can_config_filter(tCanBus *inst, uint32_t std_id)
{
    FDCAN_FilterTypeDef f = {0};
    f.IdType = FDCAN_STANDARD_ID;
    f.FilterIndex = CAN_FILTER_INDEX;
    f.FilterType = FDCAN_FILTER_MASK;
    f.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    f.FilterID1 = std_id & CAN_STD_ID_MASK;
    f.FilterID2 = CAN_STD_ID_MASK; // 掩码全 1 → 仅接受 FilterID1
    if (HAL_FDCAN_ConfigFilter(inst->hcan, &f) != HAL_OK)
        return false;

    // 未匹配报文与远程帧一律不接收
    return HAL_FDCAN_ConfigGlobalFilter(inst->hcan, FDCAN_REJECT, FDCAN_REJECT,
                                        FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) == HAL_OK;
}

// ---- 驱动接口（tBusOps） ----

static bool can_bus_open(void *handle, uint32_t std_id)
{
    tCanBus *inst = (tCanBus *)handle;
    if (!inst || !inst->hcan)
        return false;

    if (!can_config_filter(inst, std_id))
        return false;
    if (HAL_FDCAN_Start(inst->hcan) != HAL_OK)
        return false;
    if (HAL_FDCAN_ActivateNotification(inst->hcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0U) != HAL_OK)
        return false;
    return true;
}

static bool can_bus_send(void *handle, uint32_t id, const uint8_t *data, uint16_t len)
{
    tCanBus *inst = (tCanBus *)handle;
    if (!inst || !inst->hcan || !data || len > 8U)
        return false;

    FDCAN_TxHeaderTypeDef hdr = {0};
    hdr.Identifier = id & CAN_STD_ID_MASK;
    hdr.IdType = FDCAN_STANDARD_ID;
    hdr.TxFrameType = FDCAN_DATA_FRAME;
    hdr.DataLength = (uint32_t)len; // ≤8：DLC 编码即字节数
    hdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    hdr.BitRateSwitch = FDCAN_BRS_OFF;
    hdr.FDFormat = FDCAN_CLASSIC_CAN;
    hdr.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    hdr.MessageMarker = 0U;

    // TX FIFO 满则返回 false，由上层重试
    return HAL_FDCAN_AddMessageToTxFifoQ(inst->hcan, &hdr, (uint8_t *)data) == HAL_OK;
}

static void can_bus_set_rx_cb(void *handle, bus_rx_frame_cb cb, void *ctx)
{
    tCanBus *inst = (tCanBus *)handle;
    if (!inst)
        return;
    inst->rx_cb = cb;
    inst->rx_ctx = ctx;
}

// ---- 驱动出口 ----
const tBusOps can_bus_ops = {
    .open = can_bus_open,
    .send = can_bus_send,
    .set_rx_cb = can_bus_set_rx_cb,
};
