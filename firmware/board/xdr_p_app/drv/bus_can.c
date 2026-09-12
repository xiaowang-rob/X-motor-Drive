// ============================================================
// can_drv.c — CAN 通讯底层驱动
// ============================================================
#include "bus_drivers.h"

#include "can.h"

#define CAN_CH (hcan2)
#define STD_ID_MASK 0x7FF      // 标准帧11位ID掩码
#define EXT_ID_MASK 0xFFFFFFFF // 扩展帧29位ID掩码
#define CAN_SEND_TIMEOUT_MS 1000U

// static bus_rx_frame_cb s_rx_cb = NULL;
typedef struct
{
    CAN_HandleTypeDef *hcan;
    tBusDriverOps *ops;
    bus_rx_frame_cb s_rx_cb;
} tCan_ctx;

const tBusDriverOps can_drv_ops = {
    .init = can_init,
    .send = can_send,
    .register_callback = can_register_rx,
};

// 创建一个驱动实例
tCan_ctx can2 = {
    .hcan = &hcan2,
    .ops = can_drv_ops,
    .s_rx_cb = NULL,
};

// ---- 收帧中断（只在本文件定义） ----
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan == can2.hcan)
    {
        uint8_t data[8];
        CAN_RxHeaderTypeDef hdr;
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &hdr, data) == HAL_OK)
        {
            if (can2.s_rx_cb)
                can2.s_rx_cb(hdr.StdId, data, hdr.DLC);
        }
    }
}

// CAN2 专用过滤组（14 起）
static bool can_config_filter(tCan_ctx *can, uint32_t std_id)
{
    CAN_FilterTypeDef f;
    memset(&f, 0, sizeof(f));
    f.FilterBank = 14;
    f.FilterMode = CAN_FILTERMODE_IDMASK;
    f.FilterScale = CAN_FILTERSCALE_32BIT;
    f.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    f.FilterActivation = CAN_FILTER_ENABLE;

    uint32_t id_reg = (std_id & STD_ID_MASK) << 21; // STID[10:0] → [31:21]
    f.FilterIdHigh = (uint16_t)(id_reg >> 16);
    f.FilterIdLow = (uint16_t)id_reg;
    uint32_t mask_reg = ((uint32_t)STD_ID_MASK << 21) | 0x06U; // 匹配 ID+IDE+RTR
    f.FilterMaskIdHigh = (uint16_t)(mask_reg >> 16);
    f.FilterMaskIdLow = (uint16_t)mask_reg;

    return HAL_CAN_ConfigFilter(can->hcan, &f) == HAL_OK;
}

static bool can_init(tCan_ctx *can, uint32_t std_id)
{

    if (!can2_config_filter(std_id))
        return false;
    if (HAL_CAN_Start(&CAN_CH) != HAL_OK)
        return false;
    if (HAL_CAN_ActivateNotification(&CAN_CH, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
        return false;
    return true;
}

static bool can_send(uint32_t id, const uint8_t *data, uint16_t len)
{
    if (!data || len > 8U)
        return false;

    CAN_TxHeaderTypeDef hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.StdId = id & STD_ID_MASK;
    hdr.IDE = CAN_ID_STD;
    hdr.RTR = CAN_RTR_DATA;
    hdr.DLC = len;
    hdr.TransmitGlobalTime = DISABLE;

    uint32_t mailbox;
    if (HAL_CAN_AddTxMessage(&CAN_CH, &hdr, (uint8_t *)data, &mailbox) != HAL_OK)
        return false; // 未成功发送，返回 false 上层重试

    return true;
}

static void can_register_rx(bus_rx_frame_cb cb)
{
    s_rx_cb = cb;
}
