#include "bsp_dfu.h"

#include "bsp_usb.h"
#include "tusb.h"

static const tDfuDevHandle *firmware_target;
static uint32_t image_size;
static uint32_t next_block;
static bool downloading;
static bool final_block;

static void abort_download(void) {
    if (downloading)
        firmware_target->ops->abort(firmware_target->dev);
    downloading = false;
    final_block = false;
    image_size = 0;
    next_block = 0;
}

bool bsp_dfu_start(const tDfuDevHandle *target) {
    if (!target || !target->ops || !target->ops->begin ||
        !target->ops->write || !target->ops->finish || !target->ops->abort ||
        target->capacity == 0U || target->poll_timeout_ms > 0xFFFFFFU)
        return false;
    if (tud_inited())
        return firmware_target == target;

    firmware_target = target;
    if (bsp_usb_start(true))
        return true;
    firmware_target = NULL;
    return false;
}

void bsp_dfu_task(void) {
    if (firmware_target) {
        bsp_usb_task();
        if (!tud_mounted())
            abort_download();
    }
}

uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state) {
    (void)state;
    return alt == 0U && firmware_target ? firmware_target->poll_timeout_ms : 0U;
}

void tud_dfu_download_cb(uint8_t alt, uint16_t block_num, const uint8_t *data,
                          uint16_t len) {
    uint8_t status = DFU_STATUS_OK;
    if (alt != 0U || !firmware_target) {
        status = DFU_STATUS_ERR_TARGET;
    } else if (len == 0U || len > CFG_TUD_DFU_XFER_BUFSIZE ||
               final_block || block_num != next_block ||
               len > firmware_target->capacity - image_size) {
        status = DFU_STATUS_ERR_ADDRESS;
    } else {
        if (!downloading) {
            downloading = firmware_target->ops->begin(firmware_target->dev);
            if (!downloading)
                status = DFU_STATUS_ERR_ERASE;
        }
        if (downloading) {
            if (firmware_target->ops->write(firmware_target->dev, image_size,
                                            data, len)) {
                image_size += len;
                next_block++;
                final_block = len < CFG_TUD_DFU_XFER_BUFSIZE;
            } else {
                status = DFU_STATUS_ERR_WRITE;
            }
        }
    }

    if (status != DFU_STATUS_OK)
        abort_download();
    tud_dfu_finish_flashing(status);
}

void tud_dfu_manifest_cb(uint8_t alt) {
    uint8_t status = DFU_STATUS_ERR_NOTDONE;
    if (alt == 0U && downloading) {
        if (firmware_target->ops->finish(firmware_target->dev, image_size)) {
            downloading = false;
            status = DFU_STATUS_OK;
        } else {
            status = DFU_STATUS_ERR_VERIFY;
        }
    }
    abort_download();
    tud_dfu_finish_flashing(status);
}

void tud_dfu_abort_cb(uint8_t alt) {
    (void)alt;
    abort_download();
}

void bsp_dfu_usb_reset(void) {
    abort_download();
}
