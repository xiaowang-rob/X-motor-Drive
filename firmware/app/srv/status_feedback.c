#include "bsp_base.h"
#include "bsp_rgb.h"
#include "status_feedback.h"
#include "foc_main.h"
#include "bsp_led.h"
#include "protection_manager.h"
#include "device.h"

void status_feedback_main_loop()
{
    switch (g_foc.state)
    {
    case FOC_TUNE:
        bsp_rgb_breathe(TIFFANY_BLUE);
        break;
    case FOC_IDLE:
        bsp_rgb_breathe(KLEIN_BLUE);
        break;
    case FOC_RUNNING:
        bsp_rgb_breathe(MARS_GREEN);
        break;
    case FOC_FAULT:
        bsp_rgb_breathe(CHINA_RED);
        break;
    case FOC_WARNING:
        bsp_rgb_breathe(YELLOW);
        break;
    default:
        break;
    }
    led_control(g_pro_manager.drive_state->can_state, g_pro_manager.drive_state->encoder_state);
}
void system_fault_feedback()
{
    bsp_rgb_breathe(RED);
    bsp_delay(500);
    bsp_rgb_breathe((tRGBColor){0, 0, 0});
    bsp_delay(500);
}
