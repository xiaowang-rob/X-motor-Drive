#include "bl_status.h"

void bl_status_set(eBlStatus state) {
    eIndicator_state istate = INDICATOR_STATE_IDLE;
    switch (state) {
    case BL_STATUS_IDLE:
        istate = INDICATOR_STATE_IDLE;
        break;
    case BL_STATUS_WRITING:
        istate = INDICATOR_STATE_RUNNING1;
        break;
    case BL_STATUS_SUCCESS:
        istate = INDICATOR_STATE_RUNNING0;
        break;
    case BL_STATUS_ERROR:
        istate = INDICATOR_STATE_FAULT;
        break;
    default:
        break;
    }
    led_set_state(&g_led, istate);
}

void bl_status_process(void) { led_task(&g_led); }
