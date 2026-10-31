/* Vendor Bulk class. Historical filename retained: no CMake/.ioc changes. */
#include "xdr_usbd_vendor_if.h"
#include "bsp_usb_vendor.h"
#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"
#include <string.h>

extern USBD_HandleTypeDef hUsbDeviceFS;

typedef struct {
    uint8_t rx[VENDOR_EPOUT_SIZE];
    uint8_t tx_busy;
    uint8_t rx_armed;
    uint8_t alternate;
    uint8_t status[2];
} VendorState;

static VendorState vendor;

__ALIGN_BEGIN static uint8_t config_desc[] __ALIGN_END = {
    9,
    USB_DESC_TYPE_CONFIGURATION,
    32,
    0,
    1,
    1,
    USBD_IDX_CONFIG_STR,
#if USBD_SELF_POWERED == 1U
    0xC0,
#else
    0x80,
#endif
    USBD_MAX_POWER,
    9,
    USB_DESC_TYPE_INTERFACE,
    0,
    0,
    2,
    0xFF,
    0,
    0,
    USBD_IDX_INTERFACE_STR,
    7,
    USB_DESC_TYPE_ENDPOINT,
    VENDOR_EPIN_ADDR,
    0x02,
    VENDOR_EPIN_SIZE,
    0,
    0,
    7,
    USB_DESC_TYPE_ENDPOINT,
    VENDOR_EPOUT_ADDR,
    0x02,
    VENDOR_EPOUT_SIZE,
    0,
    0,
};

static uint8_t vendor_arm_rx(USBD_HandleTypeDef *pdev) {
    if (!vendor.rx_armed) {
        vendor.rx_armed =
            (USBD_LL_PrepareReceive(
                 pdev, VENDOR_EPOUT_ADDR, vendor.rx, sizeof(vendor.rx)) ==
             USBD_OK);
    }
    return vendor.rx_armed ? USBD_OK : USBD_FAIL;
}

static uint8_t vendor_init(USBD_HandleTypeDef *pdev, uint8_t cfgidx) {
    (void)cfgidx;
    memset(&vendor, 0, sizeof(vendor));
    bsp_usb_vendor_reset();
    if (USBD_LL_OpenEP(
            pdev, VENDOR_EPIN_ADDR, USBD_EP_TYPE_BULK, VENDOR_EPIN_SIZE) !=
        USBD_OK)
        return USBD_FAIL;
    if (USBD_LL_OpenEP(
            pdev, VENDOR_EPOUT_ADDR, USBD_EP_TYPE_BULK, VENDOR_EPOUT_SIZE) !=
        USBD_OK) {
        (void)USBD_LL_CloseEP(pdev, VENDOR_EPIN_ADDR);
        return USBD_FAIL;
    }
    pdev->ep_in[VENDOR_EPIN_ADDR & 0x0FU].is_used = 1U;
    pdev->ep_out[VENDOR_EPOUT_ADDR & 0x0FU].is_used = 1U;
    pdev->ep_in[VENDOR_EPIN_ADDR & 0x0FU].maxpacket = VENDOR_EPIN_SIZE;
    pdev->ep_out[VENDOR_EPOUT_ADDR & 0x0FU].maxpacket = VENDOR_EPOUT_SIZE;
    pdev->pClassData = &vendor;
    if (vendor_arm_rx(pdev) != USBD_OK) {
        (void)USBD_LL_CloseEP(pdev, VENDOR_EPIN_ADDR);
        (void)USBD_LL_CloseEP(pdev, VENDOR_EPOUT_ADDR);
        pdev->ep_in[VENDOR_EPIN_ADDR & 0x0FU].is_used = 0U;
        pdev->ep_out[VENDOR_EPOUT_ADDR & 0x0FU].is_used = 0U;
        pdev->pClassData = NULL;
        return USBD_FAIL;
    }
    return USBD_OK;
}

static uint8_t vendor_deinit(USBD_HandleTypeDef *pdev, uint8_t cfgidx) {
    (void)cfgidx;
    (void)USBD_LL_CloseEP(pdev, VENDOR_EPIN_ADDR);
    (void)USBD_LL_CloseEP(pdev, VENDOR_EPOUT_ADDR);
    pdev->ep_in[VENDOR_EPIN_ADDR & 0x0FU].is_used = 0U;
    pdev->ep_out[VENDOR_EPOUT_ADDR & 0x0FU].is_used = 0U;
    pdev->pClassData = NULL;
    memset(&vendor, 0, sizeof(vendor));
    bsp_usb_vendor_reset();
    return USBD_OK;
}

static uint8_t vendor_setup(USBD_HandleTypeDef *pdev,
                            USBD_SetupReqTypedef *req) {
    /* Standard endpoint CLEAR_FEATURE is already ACKed by the core. Do not
     * discard queued bytes on clear-halt: the host may retry its transfer. */
    if (req->bmRequest == 0x02U && req->bRequest == USB_REQ_CLEAR_FEATURE &&
        req->wValue == USB_FEATURE_EP_HALT && req->wLength == 0U &&
        (req->wIndex == VENDOR_EPIN_ADDR || req->wIndex == VENDOR_EPOUT_ADDR))
        return USBD_OK;

    if (pdev->dev_state != USBD_STATE_CONFIGURED || req->wIndex != 0U ||
        (req->bmRequest & USB_REQ_TYPE_MASK) != USB_REQ_TYPE_STANDARD)
        goto unsupported;

    if (req->bmRequest == 0x81U && req->wValue == 0U) {
        switch (req->bRequest) {
        case USB_REQ_GET_STATUS:
            if (req->wLength == 2U)
                return (uint8_t)USBD_CtlSendData(pdev, vendor.status, 2U);
            break;
        case USB_REQ_GET_INTERFACE:
            if (req->wLength == 1U)
                return (uint8_t)USBD_CtlSendData(pdev, &vendor.alternate, 1U);
            break;
        default:
            break;
        }
    }
    if (req->bmRequest == 0x01U && req->bRequest == USB_REQ_SET_INTERFACE &&
        req->wValue == 0U && req->wLength == 0U)
        return USBD_OK; /* The core sends the status stage for interface
                           requests. */

unsupported:
    USBD_CtlError(pdev, req);
    return USBD_FAIL;
}

static uint8_t vendor_data_in(USBD_HandleTypeDef *pdev, uint8_t epnum) {
    if (pdev->pClassData != &vendor || epnum != (VENDOR_EPIN_ADDR & 0x0FU))
        return USBD_FAIL;
    vendor.tx_busy = 0U;
    bsp_usb_vendor_on_tx_done();
    return USBD_OK;
}

static uint8_t vendor_data_out(USBD_HandleTypeDef *pdev, uint8_t epnum) {
    if (pdev->pClassData != &vendor || epnum != (VENDOR_EPOUT_ADDR & 0x0FU))
        return USBD_FAIL;
    uint32_t len = USBD_LL_GetRxDataSize(pdev, epnum);
    vendor.rx_armed = 0U;
    if (len <= sizeof(vendor.rx))
        bsp_usb_vendor_on_rx(vendor.rx, (uint16_t)len);
    return vendor_arm_rx(pdev);
}

static uint8_t vendor_sof(USBD_HandleTypeDef *pdev) {
    /* Retry a low-level submission failure; also expire incomplete frames even
     * when the host stops writing. No dynamic allocation or blocking waits. */
    (void)vendor_arm_rx(pdev);
    bsp_usb_vendor_service();
    return USBD_OK;
}

static uint8_t *vendor_config(uint16_t *length) {
    *length = sizeof(config_desc);
    return config_desc;
}

/* G474 is Full Speed only. Core rejects HS-only descriptors at FS. */
USBD_ClassTypeDef USBD_VENDOR = {
    .Init = vendor_init,
    .DeInit = vendor_deinit,
    .Setup = vendor_setup,
    .DataIn = vendor_data_in,
    .DataOut = vendor_data_out,
    .SOF = vendor_sof,
    .GetFSConfigDescriptor = vendor_config,
};

uint8_t USBD_VENDOR_IsReady(void) {
    return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED &&
           hUsbDeviceFS.pClassData == &vendor;
}

USBD_StatusTypeDef USBD_VENDOR_Transmit(const uint8_t *data, uint16_t len) {
    if (!USBD_VENDOR_IsReady() || data == NULL || len > VENDOR_EPIN_SIZE)
        return USBD_FAIL;
    if (vendor.tx_busy)
        return USBD_BUSY;
    vendor.tx_busy = 1U;
    USBD_StatusTypeDef status =
        USBD_LL_Transmit(&hUsbDeviceFS, VENDOR_EPIN_ADDR, (uint8_t *)data, len);
    if (status != USBD_OK)
        vendor.tx_busy = 0U;
    return status;
}
