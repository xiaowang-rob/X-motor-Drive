// ============================================================
// bus_can.c — CAN 通讯底层驱动（板级，直连 HAL）
//
// 实例形态：文件内静态实例（外设 hcan / 过滤组 / 回调），无 ops、无堆。
// 中断：HAL_CAN_RxFifo0MsgPendingCallback 本体在 bsp_irq，本驱动只注册处理函数。
// 本文件实现 app/abs/bus_com_board.h 的 CAN 路由（由 board_bus.c 分派）。
// ============================================================
#include "bus_can.h"

#include "bsp_irq.h"

#include "can.h"

// ---------- 本板配置 ----------
#define CAN2_FILTER_BANK 14U   // CAN2 专用过滤组（14 起）
#define CAN_STD_ID_MASK 0x7FFU // 标准帧 11 位 ID 掩码

// ---------- 实例 ----------
typedef struct
{
    CAN_HandleTypeDef *hcan; // 外设
    uint32_t filter_bank;    // 配置：过滤组
    uint32_t std_id;         // 配置：标准帧 ID
    bus_rx_frame_cb rx_cb;   // 收帧回调（由 abs 层注册）
    void *rx_ctx;            // 回调上下文（abs 层实例）
} tCanBus;

static tCanBus s_can2 = {
    .hcan = &hcan2,
    .filter_bank = CAN2_FILTER_BANK,
    .std_id = 0U,
    .rx_cb = NULL,
    .rx_ctx = NULL,
};

// ---- 收帧中断 ----
// HAL 回调本体在 bsp_irq；本驱动只注册处理函数（见 can_bus_open）。
static void can_on_rx_pending(void *ctx)
{
    tCanBus *inst = (tCanBus *)ctx;

    uint8_t data[8];
    CAN_RxHeaderTypeDef hdr;
    if (HAL_CAN_GetRxMessage(inst->hcan, CAN_RX_FIFO0, &hdr, data) == HAL_OK)
    {
        if (inst->rx_cb)
            inst->rx_cb(inst->rx_ctx, hdr.StdId, data, (uint8_t)hdr.DLC);
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

// ---- 板级钩子实现（CAN 路由） ----

bool can_bus_open(uint32_t std_id)
{
    tCanBus *inst = &s_can2;
    if (!inst->hcan)
        return false;

    // 注册收帧处理函数（绑一次；可能被重复调用）
    static bool s_irq_bound = false;
    if (!s_irq_bound)
    {
        static const tCanIrq s_can_irq = {
            .on_rx_pending = can_on_rx_pending,
            .ctx = &s_can2,
        };
        s_irq_bound = bsp_irq_bind_can(inst->hcan->Instance, &s_can_irq);
        if (!s_irq_bound)
            return false;
    }

    inst->std_id = std_id;
    if (!can_config_filter(inst, std_id))
        return false;
    if (HAL_CAN_Start(inst->hcan) != HAL_OK)
        return false;
    if (HAL_CAN_ActivateNotification(inst->hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
        return false;
    return true;
}

bool can_bus_send(uint32_t id, const uint8_t *data, uint16_t len)
{
    if (!s_can2.hcan || !data || len > 8U)
        return false;

    CAN_TxHeaderTypeDef hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.StdId = id & CAN_STD_ID_MASK;
    hdr.IDE = CAN_ID_STD;
    hdr.RTR = CAN_RTR_DATA;
    hdr.DLC = (uint8_t)len;
    hdr.TransmitGlobalTime = DISABLE;

    uint32_t mailbox;
    if (HAL_CAN_AddTxMessage(s_can2.hcan, &hdr, (uint8_t *)data, &mailbox) != HAL_OK)
        return false; // 邮箱忙：返回 false 由上层重试

    return true;
}

void can_bus_register_cb(bus_rx_frame_cb cb, void *ctx)
{
    s_can2.rx_cb = cb;
    s_can2.rx_ctx = ctx;
}
