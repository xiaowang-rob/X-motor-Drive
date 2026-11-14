
#include "slot_con.h"

#include "bsp_cfg.h"
#include "bsp_mcu.h"
#include "bsp_pwm.h"

tSlotCon g_slotcon;

CREATE_TIME_SAMPLE(high_s);
uint32_t max_task_us;
uint8_t max_task_ht;
uint8_t max_task_mt;

void slot_con_init(void) {
    TIME_INIT();

    float f_con = F_PWM;
    g_slotcon.base_tic = 0;
    g_slotcon.high_tic = 0;
    g_slotcon.medium_tic = 0;

    g_slotcon.t_high = 1 / f_con;
    g_slotcon.t_med = g_slotcon.t_high * FREQ_MEDIUM_LOOP;
    g_slotcon.t_low = g_slotcon.t_med * FREQ_LOW_LOOP;

    g_slotcon.f_high = f_con;
    g_slotcon.f_med = g_slotcon.f_high * FREQ_MEDIUM_LOOP;
    g_slotcon.f_low = g_slotcon.f_med * FREQ_LOW_LOOP;
}

// 在定时中断中调用
void slot_con_update(void) {

    TIME_SAMPLE_START(high_s);
    for (uint8_t i = 0; i < FREQ_HIGH_LOOP; i++) {
        if (g_slotcon.base_tic == high_schedule[i].slot)
            high_schedule[i].task(g_slotcon.t_high);
    }
    g_slotcon.base_tic++;
    if (FREQ_HIGH_LOOP - 1 <= g_slotcon.base_tic) {
        g_slotcon.base_tic = 0;
        g_slotcon.high_tic++;
        for (uint8_t i = 0; i < FREQ_MEDIUM_LOOP; i++) {
            if (g_slotcon.high_tic == medium_schedule[i].slot)
                medium_schedule[i].task(g_slotcon.t_med);
        }

        if (FREQ_MEDIUM_LOOP - 1 <= g_slotcon.high_tic) {
            g_slotcon.high_tic = 0;
            g_slotcon.medium_tic++;

            for (uint8_t i = 0; i < FREQ_LOW_LOOP; i++) {
                if (g_slotcon.medium_tic == low_schedule[i].slot)
                    low_schedule[i].task(g_slotcon.t_low);
            }
            if (FREQ_LOW_LOOP - 1 <= g_slotcon.medium_tic) {
                g_slotcon.medium_tic = 0;
            }
        }
    }
    TIME_SAMPLE_STOP(high_s);
    if (max_task_us != high_s.max_task_us) {
        max_task_us = high_s.max_task_us;
        max_task_ht = g_slotcon.high_tic;
        max_task_mt = g_slotcon.medium_tic;
    }
}

void slot_con_start(bool start) {
    if (!start)
        pwm_register_callback(NULL, NULL);
    else
        pwm_register_callback(NULL, slot_con_update);
}

bool slot_con_reg_task(eSlotConType type, uint8_t slot, slot_task *task) {
    // TODO:后续可以作为软件注册
    return false;
}