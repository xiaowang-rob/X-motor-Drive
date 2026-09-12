
#include "slot_con.h"
#include "math_fast.h"
#include "IF_irq.h"

void slot_con_init(tSlotCon *sc, uint32_t pwm_period)
{
    sc->base_tic = 0;
    sc->high_tic = 0;
    sc->medium_tic = 0;

    sc->t_high = 1 / F_CON;
    sc->t_med = sc->t_high * FREQ_MEDIUM_LOOP;
    sc->t_low = sc->t_med * FREQ_LOW_LOOP;
}

// 在pwm中断中调用 上升（含采样）给1 下溢 给0
void slot_con_update(tSlotCon *sc, uint8_t tic)
{
    sc->base_tic = tic;
    for (uint8_t i = 0; i < FREQ_HIGH_LOOP; i++)
    {
        if (sc->base_tic == high_schedule[i].slot)
            high_schedule[i].task();
    }
    if (FREQ_HIGH_LOOP - 1 <= sc->base_tic)
    {
        sc->high_tic++;
        for (uint8_t i = 0; i < FREQ_MEDIUM_LOOP; i++)
        {
            if (sc->high_tic == medium_schedule[i].slot)
                medium_schedule[i].task();
        }

        if (FREQ_MEDIUM_LOOP - 1 <= sc->high_tic)
        {
            sc->high_tic = 0;
            sc->medium_tic++;

            for (uint8_t i = 0; i < FREQ_LOW_LOOP; i++)
            {
                if (sc->medium_tic == low_schedule[i].slot)
                    low_schedule[i].task();
            }
            if (FREQ_LOW_LOOP - 1 <= sc->medium_tic)
            {
                sc->medium_tic = 0;
            }
        }
    }
}
