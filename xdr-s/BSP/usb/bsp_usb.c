#include "bsp_usb.h"

#include "bsp_config.h"
#include "bsp_dfu.h"
#include "bsp_usb_vendor.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_ll_rcc.h"
#include "tusb.h"

#include <stddef.h>

enum {
    STR_LANG,
    STR_MANUFACTURER,
    STR_PRODUCT,
    STR_SERIAL,
    STR_CONFIG,
    STR_VENDOR,
    STR_DFU,
};

static bool usb_dfu_mode;
static tusb_desc_device_t device_desc = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_VENDOR_SPECIFIC,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = BSP_USB_VID,
    .idProduct = BSP_USB_PID,
    .bcdDevice = 0x0200,
    .iManufacturer = STR_MANUFACTURER,
    .iProduct = STR_PRODUCT,
    .iSerialNumber = STR_SERIAL,
    .bNumConfigurations = 1,
};

static const uint8_t vendor_config[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, STR_CONFIG, TUD_CONFIG_DESC_LEN + TUD_VENDOR_DESC_LEN,
                          TUSB_DESC_CONFIG_ATT_SELF_POWERED, 100),
    TUD_VENDOR_DESCRIPTOR(0, STR_VENDOR, 0x01, 0x81, 64),
};

static const uint8_t dfu_config[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + TUD_DFU_DESC_LEN(1),
                          TUSB_DESC_CONFIG_ATT_SELF_POWERED, 100),
    TUD_DFU_DESCRIPTOR(0, 1, STR_DFU,
                       DFU_ATTR_CAN_DOWNLOAD | DFU_ATTR_MANIFESTATION_TOLERANT,
                       0, CFG_TUD_DFU_XFER_BUFSIZE),
};

bool bsp_usb_start(bool dfu_mode) {
    if (tud_inited())
        return usb_dfu_mode == dfu_mode;

    usb_dfu_mode = dfu_mode;
    device_desc.bDeviceClass = dfu_mode ? 0 : TUSB_CLASS_VENDOR_SPECIFIC;
    device_desc.idProduct = dfu_mode ? XDR_S_BL_USB_PID : BSP_USB_PID;
    LL_RCC_SetUSBClockSource(LL_RCC_USB_CLKSOURCE_HSI48);
    __HAL_RCC_USB_CLK_ENABLE();
    /* USB 回调在主循环运行。中断只处理外设事件。 */
    NVIC_SetPriority(USB_HP_IRQn, 3);
    NVIC_SetPriority(USB_LP_IRQn, 3);
    NVIC_ClearPendingIRQ(USB_HP_IRQn);
    NVIC_ClearPendingIRQ(USB_LP_IRQn);

    const tusb_rhport_init_t init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_FULL,
    };
    return tusb_init(0, &init);
}

void bsp_usb_irq_handler(void) {
    tusb_int_handler(0, true);
}

void bsp_usb_task(void) {
    tud_task_ext(0, false);
}

void tud_mount_cb(void) {
    bsp_usb_vendor_reset();
    bsp_dfu_usb_reset();
}

void tud_umount_cb(void) {
    bsp_usb_vendor_reset();
    bsp_dfu_usb_reset();
}

uint32_t tusb_time_millis_api(void) {
    return HAL_GetTick();
}

const uint8_t *tud_descriptor_device_cb(void) {
    return (const uint8_t *)&device_desc;
}

const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    if (index != 0)
        return NULL;
    return usb_dfu_mode ? dfu_config : vendor_config;
}

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t descriptor[32];
    static const char *const strings[] = {
        [STR_MANUFACTURER] = BSP_USB_MANUFACTURER,
        [STR_PRODUCT] = BSP_USB_PRODUCT,
        [STR_CONFIG] = BSP_USB_CONFIGURATION,
        [STR_VENDOR] = BSP_USB_INTERFACE,
        [STR_DFU] = "XDr-S Firmware",
    };
    const uint32_t *uid = (const uint32_t *)UID_BASE;
    const char hex[] = "0123456789ABCDEF";
    size_t length = 0;

    if (index == STR_LANG) {
        descriptor[1] = BSP_USB_LANGID;
        length = 1;
    } else if (index == STR_SERIAL) {
        uint32_t serial = uid[0] + uid[2];
        uint32_t suffix = uid[1];
        for (size_t i = 0; i < 8; i++)
            descriptor[1 + i] = hex[(serial >> (28 - 4 * i)) & 0x0f];
        for (size_t i = 0; i < 4; i++)
            descriptor[9 + i] = hex[(suffix >> (28 - 4 * i)) & 0x0f];
        length = 12;
    } else if (index < TU_ARRAY_SIZE(strings) && strings[index]) {
        const char *str = strings[index];
        while (str[length] && length < TU_ARRAY_SIZE(descriptor) - 1) {
            descriptor[length + 1] = (uint8_t)str[length];
            length++;
        }
    } else {
        return NULL;
    }

    descriptor[0] = (TUSB_DESC_STRING << 8) | (2 * length + 2);
    return descriptor;
}
