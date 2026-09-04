// ============================================================
// can_drv.c — CAN 通讯底层驱动（usr/drv，v2 直连版）
// ============================================================

#include "platform.h" // CAN_CH / CAN_INSTANCE / STD_ID_MASK / 时间
#include "com_drivers.h"

#define CAN_SEND_TIMEOUT_MS 1000U

static can_rx_cb s_rx_cb = NULL;

// ---- 收帧中断（只在本文件定义） ----
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    uint8_t data[8];
    CAN_RxHeaderTypeDef hdr;
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &hdr, data) == HAL_OK)
    {
        if (s_rx_cb)
            s_rx_cb(hdr.StdId, data, hdr.DLC);
    }
}

// CAN2 专用过滤组（14 起）
static bool can_config_filter(uint32_t std_id)
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

    return HAL_CAN_ConfigFilter(&CAN_CH, &f) == HAL_OK;
}

bool can_drv_start(uint32_t std_id)
{
    if (!can_config_filter(std_id))
        return false;
    if (HAL_CAN_Start(&CAN_CH) != HAL_OK)
        return false;
    if (HAL_CAN_ActivateNotification(&CAN_CH, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
        return false;
    return true;
}

bool can_drv_send(uint32_t id, const uint8_t *msg, uint8_t len)
{
    if (!msg || len > 8U)
        return false;

    CAN_TxHeaderTypeDef hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.StdId = id & STD_ID_MASK;
    hdr.IDE = CAN_ID_STD;
    hdr.RTR = CAN_RTR_DATA;
    hdr.DLC = len;
    hdr.TransmitGlobalTime = DISABLE;

    uint32_t mailbox;
    if (HAL_CAN_AddTxMessage(&CAN_CH, &hdr, (uint8_t *)msg, &mailbox) != HAL_OK)
        return false;

    uint32_t t0 = platform_get_ms();
    while (HAL_CAN_GetTxMailboxesFreeLevel(&CAN_CH) < 3U)
    {
        if ((platform_get_ms() - t0) > CAN_SEND_TIMEOUT_MS)
            return false;
    }
    return true;
}

void can_drv_register_rx(can_rx_cb cb)
{
    s_rx_cb = cb;
}
