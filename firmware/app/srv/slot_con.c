
#include "slot_con.h"
#include "math_fast.h"
#include "IF_irq.h"

tSlotCon g_slotcon;

void slot_con_init(float f_con)
{
    g_slotcon.base_tic = 0;
    g_slotcon.high_tic = 0;
    g_slotcon.medium_tic = 0;

    g_slotcon.t_high = 1 / f_con;
    g_slotcon.t_med = g_slotcon.t_high * FREQ_MEDIUM_LOOP;
    g_slotcon.t_low = g_slotcon.t_med * FREQ_LOW_LOOP;
}

// 在定时中断中调用
void slot_con_update(void)
{
    for (uint8_t i = 0; i < FREQ_HIGH_LOOP; i++)
    {
        if (g_slotcon.base_tic == high_schedule[i].slot)
            high_schedule[i].task(g_slotcon.t_high);
    }
    g_slotcon.base_tic++;
    if (FREQ_HIGH_LOOP - 1 <= g_slotcon.base_tic)
    {
        g_slotcon.base_tic = 0;
        g_slotcon.high_tic++;
        for (uint8_t i = 0; i < FREQ_MEDIUM_LOOP; i++)
        {
            if (g_slotcon.high_tic == medium_schedule[i].slot)
                medium_schedule[i].task(g_slotcon.t_med);
        }

        if (FREQ_MEDIUM_LOOP - 1 <= g_slotcon.high_tic)
        {
            g_slotcon.high_tic = 0;
            g_slotcon.medium_tic++;

            for (uint8_t i = 0; i < FREQ_LOW_LOOP; i++)
            {
                if (g_slotcon.medium_tic == low_schedule[i].slot)
                    low_schedule[i].task(g_slotcon.t_low);
            }
            if (FREQ_LOW_LOOP - 1 <= g_slotcon.medium_tic)
            {
                g_slotcon.medium_tic = 0;
            }
        }
    }
}
