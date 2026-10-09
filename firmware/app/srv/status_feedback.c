#include "status_feedback.h"

#include "bsp_cfg.h"
#include "led.h"

static eCoreState current_state = INIT;

#define LED_ACTIVE

void status_feedback_main_loop(eCoreState cstate)
{
    if (cstate == current_state)
    {
#ifdef LED_ACTIVE
        led_task(&g_led);
#endif

#ifdef RGB_ACTIVE
        rgb_task(&g_rgb);
#endif
    }
    else
    {
#ifdef LED_ACTIVE
        eLedState led_state = LED_OFF;
        switch (current_state)
        {
        case INIT:
            led_state = LED_ON;
            break;
        case IDLE:
            led_state = LED_BLINK_LONG; // 单长闪
            break;
        case TUNING:
            led_state = LED_BLINK_TRIPLE_SHORT;
            break;
        case CURRENT_MODE:
        case PID_SPEED:
        case PID_POSITION:
        case MIT_MODE:
            led_state = LED_BLINK_DOUBLE_SHORT;
            break;
        case FAULT:
            led_state = LED_BLINK_CONT_SHORT;
            break;
        case WARNING:
            led_state = LED_BLINK_SHORT_LONG;
            break;
        default:
            break;
        }
        led_set_state(&g_led, led_state);
#endif

#ifdef RGB_ACTIVE
        eRgbState rgb_state = RGB_OFF;
        tRGBColor rgb_color = RGB_BLACK;
        switch (current_state)
        {
        case INIT:
            rgb_state = RGB_ON;
            rgb_color = RGB_BLUE;
            break;
        case IDLE:
            rgb_state = RGB_BREATHE;
            rgb_color = RGB_BLUE;
            break;
        case TUNING:
            rgb_state = LED_BLINK_TRIPLE_SHORT;
            rgb_color = RGB_GREEN;
            break;
        case RUNNING:
            rgb_state = RGB_BREATHE;
            rgb_color = RGB_GREEN;
            break;
        case FAULT:
            rgb_state = RGB_BLINK_CONT_SHORT;
            rgb_color = RGB_RED;
            break;
        case WARNING:
            rgb_state = RGB_BLINK_SHORT_LONG;
            rgb_color = RGB_YELLOW;
            break;
        default:
            break;
        }
        rgb_set_state(&g_rgb, rgb_state);
        rgb_set_color(&g_rgb, rgb_color);

#endif
    }
}
