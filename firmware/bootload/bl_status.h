#ifndef XDR_BL_STATUS_H
#define XDR_BL_STATUS_H
#include "led.h"

typedef enum {
    BL_STATUS_IDLE,
    BL_STATUS_WRITING,
    BL_STATUS_SUCCESS,
    BL_STATUS_ERROR
} eBlStatus;

void bl_status_init(tLed *led);
void bl_status_set(eBlStatus state);
void bl_status_process(void);
#endif
