#include "bl_main.h"

#include "bl_comm.h"
#include "bl_iap.h"
#include "bl_status.h"
#include "bsp_cfg.h"
#include "bsp_mcu.h"
#include "parameters.h"

bool bl_main_init(void) {

    if (!param_init(&g_storage)) {
        bl_status_set(BL_STATUS_ERROR);
        return false;
    }
    bl_iap_start();
    /* 无升级请求时先尝试启动 APP，非法向量则留在 BL 等待恢复。 */
    if (bl_iap_should_boot())
        (void)bl_iap_boot();
    if (!bl_comm_init()) {
        bl_status_set(BL_STATUS_ERROR);
        return false;
    }
    return true;
}

void bl_main_start(void) {
    for (;;) {
        bl_comm_poll();
        bl_status_process();
        if (bl_iap_should_boot())
            (void)bl_iap_boot();
        bsp_time_delay_ms(1U);
    }
}
