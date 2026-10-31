#ifndef __BL_MAIN_H
#define __BL_MAIN_H

/* Bootloader 版本 */
#define BL_VERSION_MAJOR 1
#define BL_VERSION_MINOR 0
#define BL_VERSION_PATCH 0

#include <stdbool.h>

bool bl_main_init(void);
void bl_main_start(void);

#endif /* __BL_MAIN_H */