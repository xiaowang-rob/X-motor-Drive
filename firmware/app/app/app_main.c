
#include "bsp_adc.h"
#include "bsp_base.h"
#include "usr_config.h"
#include "main.h"

#include "foc_main.h"
#include "status_feedback.h"
#include "log.h"
#include "protection_manager.h"
#include "port_mapping.h"
#include "device.h"

#ifdef __DEBUG__ //***********调试************

u32 time_while_zero = 0;
u32 time_while_T = 0;
#endif

void bsp_init_front(void)
{
    bsp_set_vector_table_offset(VECT_TABLE_OFFSET);
    bsp_enable_irq(); // 使能全局中断,bl中关断了
}
void bsp_init_back(void)
{
    // 这里的顺序不能乱，因为里面有一些初始化函数，如果顺序不对，会导致一些变量没有初始化，导致程序出错
    // 参数必须尽早出现 flash在其之前，初始化的时候最好不要有其他中断干涉，所有adc得往后放，但是foc初始化必须得有adc值，故最后
    // 正确的顺序应该是：
    // flash和参数一起-通讯-保护-日志-adc -foc初始化

    flash_init();
    if (!param_init())
        bsp_error_handler();
    comm_init();
    pro_manager_init(&g_Param);
    bsp_adc_init(); // 这里就启动了foc的定时器
    foc_init();
}
void bsp_main(void)
{
    while (1)
    {
        //	编码器主循环
        encoder_main_loop_task();
        // 通讯层运行
        comm_main_loop();
        // 控制层由定时器驱动
        // 服务层运行
        pro_manager_main_loop();
        status_feedback_main_loop();
#ifdef __DEBUG__ //***********调试************

        time_while_T = bsp_get_tick_us() - time_while_zero;
        time_while_zero = bsp_get_tick_us();
#endif
    }
}

void bsp_error_handler()
{
    bsp_disable_irq();
    while (1)
    {
        system_fault_feedback();
    }
}
