// ============================================================
// bus_can.c — CAN 通讯底层驱动（板级，直连 HAL）
//
// 实例形态：文件内静态 handle，内含
//   - ops 指针（该实例的操作表）
//   - 外设句柄（hcan）
//   - 配置（过滤组 / 标准帧 ID）
//   - 收帧回调 + ctx（回调把实例送回 abs 层）
// 中断 HAL_CAN_RxFifo0MsgPendingCallback 由本文件唯一持有。
// ============================================================
#include "bus_drivers.h"

#include "can.h"

// ---------- 本板配置 ----------
#define CAN2_FILTER_BANK 14U   // CAN2 专用过滤组（14 起）
#define CAN_STD_ID_MASK 0x7FFU // 标准帧 11 位 ID 掩码

// ---------- 实例 handle ----------
typedef struct
{
    const tBusDriverOps *ops; // 该实例的操作表
    CAN_HandleTypeDef *hcan;  // 外设
    uint32_t filter_bank;     // 配置：过滤组
    uint32_t std_id;          // 配置：标准帧 ID
    bus_rx_frame_cb rx_cb;    // 收帧回调（由 abs 层注册）
    void *rx_ctx;             // 回调上下文（abs 层实例）
} tCanBus;

static bool can_init(BusHandle h, uint32_t std_id);
static bool can_send(BusHandle h, uint32_t id, const uint8_t *data, uint16_t len);
static void can_register_rx(BusHandle h, bus_rx_frame_cb cb, void *ctx);

const tBusDriverOps can_drv_ops = {
    .init = can_init,
    .send = can_send,
    .register_callback = can_register_rx,
};

// 静态实例
static tCanBus s_can2 = {
    .ops = &can_drv_ops,
    .hcan = &hcan2,
    .filter_bank = CAN2_FILTER_BANK,
    .std_id = 0U,
    .rx_cb = NULL,
    .rx_ctx = NULL,
};

BusHandle can_get_handle(void)
{
    return (BusHandle)&s_can2;
}

// ---- 收帧中断（只在本文件定义） ----
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan != s_can2.hcan)
        return;

    uint8_t data[8];
    CAN_RxHeaderTypeDef hdr;
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &hdr, data) == HAL_OK)
    {
        if (s_can2.rx_cb)
            s_can2.rx_cb(s_can2.rx_ctx, hdr.StdId, data, (uint8_t)hdr.DLC);
    }
}

// 按标准帧 ID 配置过滤器（TODO：CAN1 的过滤组分配）
static bool can_config_filter(tCanBus *inst, uint32_t std_id)
{
    CAN_FilterTypeDef f;
    memset(&f, 0, sizeof(f));
    f.FilterBank = inst->filter_bank;
    f.FilterMode = CAN_FILTERMODE_IDMASK;
    f.FilterScale = CAN_FILTERSCALE_32BIT;
    f.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    f.FilterActivation = CAN_FILTER_ENABLE;

    uint32_t id_reg = (std_id & CAN_STD_ID_MASK) << 21; // STID[10:0] → [31:21]
    f.FilterIdHigh = (uint16_t)(id_reg >> 16);
    f.FilterIdLow = (uint16_t)id_reg;
    uint32_t mask_reg = (CAN_STD_ID_MASK << 21) | 0x06U; // 匹配 ID + IDE + RTR
    f.FilterMaskIdHigh = (uint16_t)(mask_reg >> 16);
    f.FilterMaskIdLow = (uint16_t)mask_reg;

    return HAL_CAN_ConfigFilter(inst->hcan, &f) == HAL_OK;
}

// ---- ops 实现 ----

static bool can_init(BusHandle h, uint32_t std_id)
{
    tCanBus *inst = (tCanBus *)h;
    if (!inst || !inst->hcan)
        return false;

    inst->std_id = std_id;
    if (!can_config_filter(inst, std_id))
        return false;
    if (HAL_CAN_Start(inst->hcan) != HAL_OK)
        return false;
    if (HAL_CAN_ActivateNotification(inst->hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
        return false;
    return true;
}

static bool can_send(BusHandle h, uint32_t id, const uint8_t *data, uint16_t len)
{
    tCanBus *inst = (tCanBus *)h;
    if (!inst || !inst->hcan || !data || len > 8U)
        return false;

    CAN_TxHeaderTypeDef hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.StdId = id & CAN_STD_ID_MASK;
    hdr.IDE = CAN_ID_STD;
    hdr.RTR = CAN_RTR_DATA;
    hdr.DLC = (uint8_t)len;
    hdr.TransmitGlobalTime = DISABLE;

    uint32_t mailbox;
    if (HAL_CAN_AddTxMessage(inst->hcan, &hdr, (uint8_t *)data, &mailbox) != HAL_OK)
        return false; // 邮箱忙：返回 false 由上层重试

    return true;
}

static void can_register_rx(BusHandle h, bus_rx_frame_cb cb, void *ctx)
{
    tCanBus *inst = (tCanBus *)h;
    if (!inst)
        return;
    inst->rx_cb = cb;
    inst->rx_ctx = ctx;
}
