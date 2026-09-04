#ifndef __PLATFORM_H
#define __PLATFORM_H

// ============================================================
// plat.h — xdr_p_o1.2 板级总头
//
// 换板 = 提供同名 plat.h/.c（新板厂商库/外设实例）；换标准库板 =
// 本文件 include 该板的 HAL 兼容封装即可，驱动源码不变。
//
// 注：本文件含厂商库符号，只允许板级代码与 usr/drv 引用。
// ============================================================

#include "stm32f4xx_hal.h"
#include "adc.h"
#include "can.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"

#include <stdint.h> // 消除uint32_t 警告

// ============================================================
// 外设实例 / 引脚 / 硬件参数
// ============================================================

// ---------- 栅极驱动驱动 PWM + POWER ----------
#define GATE_TIC_PWM 2099 // 定时器周期计数值（ARR，对应 F_PWM=20kHz）

#define GATE_PWM_HTIM (htim8) // 电机控制定时器句柄
#define GATE_PWM_A_CHANNEL TIM_CHANNEL_3
#define GATE_PWM_B_CHANNEL TIM_CHANNEL_2
#define GATE_PWM_C_CHANNEL TIM_CHANNEL_1

#define POWER12V_GPIOx GPIOC
#define POWER12V_GPIOx_PIN GPIO_PIN_13

// ---------- 采样触发 PWM  ----------
#define SAMPLE_TIC_PWM 2099
#define SAMPLE_PWM_HTIM (htim8)
#define SAMPLE_PWM_CHANNEL TIM_CHANNEL_4

// ---------- 编码器 SPI + CS ----------
#define ENCODER_HSPI (hspi3)

#define ENCODER_INT_CS_GPIOx GPIOA
#define ENCODER_INT_CS_GPIOx_PIN GPIO_PIN_15
#define ENCODER_EXT_CS_GPIOx GPIOA
#define ENCODER_EXT_CS_GPIOx_PIN GPIO_PIN_15

// ---------- Flash SPI + CS ----------
#define FLASH_HSPI (hspi2)
#define FLASH_CS_GPIOx GPIOB
#define FLASH_CS_GPIOx_PIN GPIO_PIN_12

// ---------- RGB PWM  ----------
#define RGB_PWM_HTIM (htim4)
#define RGB_PWM_CHANNEL TIM_CHANNEL_2
#define RGB_NUM 2   // 板上灯珠数量
#define CODE_1 (75) // 逻辑 1 占空比 CCR 值
#define CODE_0 (35) // 逻辑 0 占空比 CCR 值

// ---------- GPIO：LED / USB ----------
#define LED_0_GPIOx GPIOD
#define LED_0_GPIOx_PIN GPIO_PIN_2
#define LED_1_GPIOx GPIOB
#define LED_1_GPIOx_PIN GPIO_PIN_3

#define USB_CS_GPIOx GPIOA
#define USB_CS_GPIOx_PIN GPIO_PIN_8

// ---------- 串口 / CAN  ----------
#define UART_CH (huart1)
#define CAN_CH (hcan2)

// ---------- 模拟前端系数（码→物理量换算） ----------
#define RATE_CURRENT_SAMPLE 100.0f // 电流采样放大比率
#define RATE_VOLTAGE_SAMPLE 16     // 电压采样分压比率

// ---------- Flash 存储规划（含内部 MCU Flash IAP）

#define FLASH_START_ADDR 0x08000000U   // Flash 起始地址
#define FLASH_CAPACITY (1024U * 1024U) // Flash 容量 (bytes)
#define FLASH_END_ADDR 0x080FFFFFU     // Flash 结束地址
#define NORMAL_MAGIC 0xFFFFFFFF        // 空数

#define MCU_FLASH_NUM_SECTORS 12U
#define MCU_NUM_SECTOR_BL 2U  // Bootloader 扇区数量
#define MCU_NUM_SECTOR_APP 7U // App 扇区数量
#define MCU_NUM_SECTOR_USR 3U // 用户数据扇区数量-至少三个，1 参数 2 log 3 iap标志

const uint32_t SECTOR_BOUNDS[MCU_FLASH_NUM_SECTORS + 1];

const uint8_t BL_SECTOR_ID[MCU_NUM_SECTOR_BL];
const uint8_t APP_SECTOR_ID[MCU_NUM_SECTOR_APP];
const uint8_t USR_SECTOR_ID[MCU_NUM_SECTOR_USR];

const uint32_t BL_START_ADDR;
const uint32_t BL_SIZE;
const uint32_t APP_START_ADDR;
const uint32_t APP_SIZE;
const uint32_t USR_START_ADDR;
const uint32_t USR_SIZE;

// ---------- 启动配置 ----------
#define VECT_TABLE_OFFSET BL_SIZE // 中断向量表偏移 整个BL的空间

// ============================================================
// 板级时间服务（替代 bsp_get_tick / HAL_GetTick 直用）
// ============================================================

// 板级初始化（使能 DWT 周期计数等）；HAL_Init 之后调用一次 （只在调试阶段使用）
void plat_init(void);

// 毫秒 tick（SysTick，可回绕，调用方用差值）
uint32_t plat_get_ms(void);

// 微秒 tick（DWT 周期计数 / 主频）
uint32_t plat_get_us(void);

// 阻塞延时（毫秒）
void plat_delay_ms(uint32_t ms);

// ============================================================
// 板级固件服务（中断/向量/复位/跳转）
// ============================================================

// 全局中断开关
void plat_enable_irq(void);
void plat_disable_irq(void);

// 设置中断向量表偏移（APP 启动早期用 VECT_TABLE_OFFSET ）
void plat_set_vector_offset(void);

// 系统复位（NVIC）
void plat_system_reset(void);

// 跳转到app
void plat_jump_to_app(void);

#endif // __PLATFORM_H
