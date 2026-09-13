
// 在这里对板上的所有驱动进行组装 然后给高层提供设备接口

#include "device_cfg.h"

void dv_init(void)
{

    flash_init(&flash_mcu, &mcu_flash_driver_ops, void); // mcu flash没有句柄
    iap_init(&iap_app, &mcu_iap_driver_ops, IAP_APP);

    led_init(&led0, &g_led_drv_ops)
        bus_init(&can, &can_drv_ops);

    // TODO:读取参数添加上位机设置好的编码器
    EncoderChipHandle mt6816_int = MT6816_create();
    encoder_init(&int_encoder, &MT6816_driver_ops, mt6816_int, INT_ENCODER);

    gate_drv_init(&motor_drv, &gate_fd6288q_ops);
}
// ============================================================
// dev_board.c — 组装层：板级设备装配（usr/app，v2）
/
// v2：驱动已直连厂商库（usr/drv + platform.h），装配简化为
// "无参工厂 + ops → abs 对象 init"，不再注入接口表。
// 本文件仍作为业务层唯一入口（include dev_board.h 拿 g_dev）。
// ============================================================

#include "dev_board.h"

#include <stddef.h> // NULL

#include "board.h"    // BL/APP/PARAM 分区（产品配置单点）
#include "platform.h" // platform_init / platform_get_ms/us / jump/reset

#include "usr/drv/encoder_drivers.h"
#include "usr/drv/flash_drivers.h"
#include "usr/drv/led_drivers.h"
#include "usr/drv/mcu_flash_drv.h"
#include "usr/drv/rgb_drivers.h"
#include "usr/drv/sense_drivers.h"

    // ============================================================
    // 产品配置区（换电机/芯片只改这里）
    // ============================================================
    // 编码器芯片：默认 MT6816；换芯片在编译定义启用其一：
    //   -DDEV_ENC_CHIP_AS5047 / -DDEV_ENC_CHIP_MT6835

    // ============================================================

    // 板级时间 → abs 注入适配（platform 裸函数包成 tTimeIf）
    static uint32_t dv_get_ms(void *ctx)
{
    (void)ctx;
    return platform_get_ms();
}

static uint32_t dv_get_us(void *ctx)
{
    (void)ctx;
    return platform_get_us();
}

static const tTimeIf g_time = {
    .ctx = NULL,
    .get_ms = dv_get_ms,
    .get_us = dv_get_us,
};

tDevBoard g_dev;

// ---- 编码器 ----
static bool dev_assemble_encoder(void)
{
    EncoderChipHandle chip = NULL;
    const tEncoderDriverOps *ops = NULL;

#if defined(DEV_ENC_CHIP_AS5047)
    chip = AS5047_create();
    ops = &AS5047_driver_ops;
#elif defined(DEV_ENC_CHIP_MT6835)
    chip = MT6835_create();
    ops = &MT6835_driver_ops;
#else // 默认 MT6816
    chip = MT6816_create();
    ops = &MT6816_driver_ops;
#endif

    if (!chip || !ops)
        return false;
    return encoder_init(&g_dev.enc, ops, chip);
}

// ---- RGB ----
static bool dev_assemble_rgb(void)
{
    RgbHandle chip = ws28xx_create();
    if (!chip)
        return false;
    return rgb_init(&g_dev.rgb, &ws28xx_driver_ops, chip, &g_time);
}

// ---- 采样 ----
static bool dev_assemble_sense(void)
{
    return sense_init(&g_dev.sense, sense_drv_get(), &g_time);
}

// ---- 外部 Flash（存储单元挂介质首个扇区；芯片缺失则不可用） ----
static bool dev_assemble_flash(void)
{
    FlashChipHandle chip = w25qxx_create();
    if (!chip)
        return false;

    if (!w25qxx_driver_ops.init(chip))
    {
        w25qxx_destroy(chip);
        return false;
    }
    bool ok = flash_unit_init(&g_dev.ext_flash, &w25qxx_driver_ops, chip, 0U);
    if (!ok)
        w25qxx_destroy(chip);
    return ok;
}

// ---- 内部 MCU Flash：参数区单元 + IAP 分区表（board.h 规划） ----
static bool dev_assemble_internal_flash(void)
{
    FlashChipHandle chip = mcu_flash_create();
    if (!chip)
        return false;
    if (!mcu_flash_driver_ops.init(chip))
    {
        mcu_flash_destroy(chip);
        return false;
    }

    // 参数区日志单元（PARAMETER_LOAD_ADDR 为 128K 扇区边界）
    g_dev.param_flash_ok =
        flash_unit_init(&g_dev.param_flash, &mcu_flash_driver_ops, chip, PARAMETER_LOAD_ADDR);

    // IAP 分区：App 区上界 = LOG 区起始（擦除/写/校验 + 跳转复位回调）
    g_dev.iap = (tFlashIAP){
        .bl_addr = BL_START_ADDR,
        .bl_size = BL_SIZE_KB * 1024U,
        .app_addr = APP_START_ADDR,
        .app_size = LOG_START_ADDR - APP_START_ADDR,
        .jump = platform_jump_to_addr,
        .reset = platform_system_reset,
    };
    g_dev.iap_ok = (g_dev.iap.jump != NULL) && (g_dev.iap.reset != NULL);
    return g_dev.param_flash_ok;
}

bool dev_board_init(void)
{
    // 1) 板级基础：DWT 周期计数（时间基准）
    platform_init();

    // 2) 时间（abs 注入）
    g_dev.time = &g_time;

    // 3) 逐设备装配（灯效失败不阻塞关键设备）
    g_dev.led_can = (tLed){0};
    g_dev.led_enc = (tLed){0};
    g_dev.rgb = (tRgb){0};
    g_dev.sense = (tCurrentSense){0};

    bool led_ok = led_init(&g_dev.led_can, led_drv_ops(), led_drv_handle(0), &g_time) &&
                  led_init(&g_dev.led_enc, led_drv_ops(), led_drv_handle(1), &g_time);
    (void)led_ok;

    g_dev.rgb_ok = dev_assemble_rgb();
    g_dev.sense_ok = dev_assemble_sense();
    g_dev.flash_ok = dev_assemble_flash();
    dev_assemble_internal_flash(); // 内部 flash/IAP（非关键，独立置 ok 标志）

    // 4) 编码器（关键设备）
    g_dev.enc_ok = dev_assemble_encoder();

    return g_dev.enc_ok;
}
