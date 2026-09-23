#include "bsp_cfg.h"

void bsp_init_front(void)
{
    bsp_set_vector_table_offset(VECT_TABLE_OFFSET); // iap下 需要设置偏移量
    bsp_enable_irq();                               // 使能全局中断,bl中关断了
}
void bsp_init_back(void)
{
    // 这里的顺序不能乱，因为里面有一些初始化函数，如果顺序不对，会导致一些变量没有初始化，导致程序出错
    // 参数必须尽早出现 flash在其之前，初始化的时候最好不要有其他中断干涉，所有adc得往后放，但是foc初始化必须得有adc值，故最后
    // 正确的顺序应该是：
    // flash和参数一起-通讯-保护-日志-adc -foc初始化

    // 电流采样初始化  get 电压温度电流
    // 编码器初始化
    // svpwm 初始化
    flash_init();
    if (!dm_param_init())
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

void main_init(void)
{
    iap_app_init();

    // 1、初始化驱动层 （存储-iap-状态-传感-gate-通讯）
    dev_base_init();
    // 2、初始化参数服务 读参数

    // 3、初始化日志服务 读日志

    // TODO:重写参数从这里开始重新初始化

    // 4、初始化状态服务

    // 5、初始化保护服务

    // 6、配置编码器设备驱动
    dev_int_enc_init();
    dev_ext_enc_init();
    // 7、启动通信

    // 6、初始化时间槽服务

    // 7、初始化core、启动时间槽服务
}
void core_reset(void)
{
}
