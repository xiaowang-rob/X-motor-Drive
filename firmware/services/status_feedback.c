#include "status_feedback.h"

#include "led.h"

void status_feedback_main_loop(eCoreState cstate) {
    eIndicator_state state = INDICATOR_STATE_IDLE;
    switch (cstate) {
    case CORE_TUNING:
    case CORE_WARNING:
        state = INDICATOR_STATE_RUNNING1;
        break;
    case CORE_OPENLOOP:
    case CORE_CURRENT_MODE:
    case CORE_PID_SPEED:
    case CORE_PID_POSITION:
    case CORE_MIT_MODE:
        state = INDICATOR_STATE_RUNNING0;
        break;
    case CORE_FAULT:
        state = INDICATOR_STATE_FAULT;
        break;
    default:
        break;
    }
    led_set_state(&g_led, state);
    led_task(&g_led);
}
