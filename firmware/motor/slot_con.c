
#include "slot_con.h"

#include "bsp_mcu.h"
#include "bsp_pwm.h"

CREATE_TIME_SAMPLE(high_s);
uint32_t max_task_us;
uint8_t max_task_ht;
uint8_t max_task_mt;

void slot_con_init(tSlotCon *sc, float f_con) {
    TIME_INIT();

    sc->base_tic = 0;
    sc->high_tic = 0;
    sc->medium_tic = 0;

    sc->t_high = 1 / f_con;
    sc->t_med = sc->t_high * FREQ_MEDIUM_LOOP;
    sc->t_low = sc->t_med * FREQ_LOW_LOOP;

    sc->f_high = f_con;
    sc->f_med = sc->f_high / FREQ_MEDIUM_LOOP;
    sc->f_low = sc->f_med / FREQ_LOW_LOOP;
}

// 在定时中断中调用
void slot_con_callback(tSlotCon *sc) {

    TIME_SAMPLE_START(high_s);
    for (uint8_t i = 0; i < FREQ_HIGH_LOOP; i++) {
        if (sc->base_tic == high_schedule[i].slot)
            high_schedule[i].task(sc->t_high);
    }

    if (++sc->base_tic >= FREQ_HIGH_LOOP) {
        sc->base_tic = 0;
        for (uint8_t i = 0; i < FREQ_MEDIUM_LOOP; i++) {
            if (sc->high_tic == medium_schedule[i].slot)
                medium_schedule[i].task(sc->t_med);
        }

        if (++sc->high_tic >= FREQ_MEDIUM_LOOP) {
            sc->high_tic = 0;
            for (uint8_t i = 0; i < FREQ_LOW_LOOP; i++) {
                if (sc->medium_tic == low_schedule[i].slot)
                    low_schedule[i].task(sc->t_low);
            }
            if (++sc->medium_tic >= FREQ_LOW_LOOP) {
                sc->medium_tic = 0;
            }
        }
    }
    TIME_SAMPLE_STOP(high_s);
    if (max_task_us != high_s.max_task_us) {
        max_task_us = high_s.max_task_us;
        max_task_ht = sc->high_tic;
        max_task_mt = sc->medium_tic;
    }
}
