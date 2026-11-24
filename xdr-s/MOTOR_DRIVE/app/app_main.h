#ifndef __APP_MAIN_H
#define __APP_MAIN_H

#include <stdbool.h>
#include "devices.h"
// 固件版本号
#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 0
#define FW_VERSION_PATCH 0

// 支持的硬件
#define HW_BOARD_NAME "XDr-S"
#define HW_VERSION_MAJOR_MIN 1
#define HW_VERSION_MINOR_MIN 0
#define HW_VERSION_PATCH_MIN 0

#define HW_VERSION_MAJOR_MAX 1
#define HW_VERSION_MINOR_MAX 0
#define HW_VERSION_PATCH_MAX 0


bool app_main_init(void);

void app_main_config(void);

void app_main_start(void);

#endif
