
#include "loop_control.h"
#include "string.h"
#include "math_fast.h"

tFrequencyDivision g_freqD = {0};

// 初始化分频系数并计算各环周期
void freq_div_init(void)
{
    memset(&g_freqD, 0, sizeof(tFrequencyDivision));

    g_freqD.t_high = T_PWM * FREQ_HIGH_LOOP;
    g_freqD.t_med = g_freqD.t_high * FREQ_MEDIUM_LOOP;
    g_freqD.t_low = g_freqD.t_high * FREQ_LOW_LOOP;
}

// 每PWM周期调用：更新分频计数器，置位各环更新标志,放在最前面
// 跟随 pwm 基频运行 更低频的每一段都可能和前面高频的0 段一起执行 所以高频的0段一般不放任务 按照任务重要程度从最后一段开始安排
void freq_div_update(void)
{
    if (g_freqD.base_tic > 0)
        g_freqD.high_update[g_freqD.base_tic - 1] = false;
    else
        g_freqD.high_update[g_freqD.base_tic] = false;

    if (g_freqD.high_tic > 0)
        g_freqD.medium_update[g_freqD.high_tic - 1] = false;
    else
        g_freqD.medium_update[g_freqD.high_tic] = false;

    if (g_freqD.medium_tic > 0)
        g_freqD.low_update[g_freqD.medium_tic - 1] = false;
    else
        g_freqD.low_update[g_freqD.medium_tic] = false;

    g_freqD.high_update[g_freqD.base_tic++] = true;
    if (g_freqD.base_tic >= FREQ_HIGH_LOOP)
    {
        g_freqD.base_tic = 0;
        g_freqD.medium_update[g_freqD.high_tic++] = true;
        if (g_freqD.high_tic >= FREQ_MEDIUM_LOOP)
        {
            g_freqD.high_tic = 0;
            g_freqD.low_update[g_freqD.medium_tic++] = true;
            if (g_freqD.medium_tic >= FREQ_LOW_LOOP)
            {
                g_freqD.medium_tic = 0;
            }
        }
    }
}
