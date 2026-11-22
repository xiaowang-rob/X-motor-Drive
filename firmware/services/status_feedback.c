#include "status_feedback.h"

#include "led.h"

void status_fb_loop_task(eMotorState cstate) {
    eIndicator_state state = INDICATOR_STATE_IDLE;
    switch (cstate) {
    case MOTOR_IDLE:
        state = INDICATOR_STATE_IDLE;
        break;
    case MOTOR_OPENLOOP:
    case MOTOR_CURRENT_MODE:
    case MOTOR_PID_SPEED:
    case MOTOR_PID_POSITION:
    case MOTOR_MIT_MODE:
        state = INDICATOR_STATE_RUNNING0;
        break;
    case MOTOR_FAULT:
        state = INDICATOR_STATE_FAULT;
        break;
    default:
        break;
    }
    led_set_state(&g_led, state);
    led_task(&g_led);
}
