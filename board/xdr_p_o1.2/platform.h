#ifndef __PLATFORM_H
#define __PLATFORM_H

// ============================================================
// platform.h — xdr_p_o1.2 板级总头（v2：drv 直连锚点）
//
// v2 架构下驱动直接面向厂商库。本文件是"该板如何接入"的唯一入口：
//   - include 本板厂商库总头与 CubeMX 外设头（extern 实例）
//   - 集中外设实例 / 引脚 / 计数值 / 模拟前端系数宏
//   - 声明板级时间服务
//
// 换板 = 提供同名 platform.h/.c（新板厂商库/外设实例）；换标准库板 =
// 本文件 include 该板的 HAL 兼容封装即可，驱动源码不变。
//
// 注：本文件含厂商库符号，只允许板级代码与 usr/drv 引用（v2 决策）。
// ============================================================

// ---- 厂商库总头 + CubeMX 外设头（extern SPI_HandleTypeDef hspi3 等） ----
#include "stm32f4xx_hal.h"
#include "adc.h"
#include "can.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"

// ============================================================
// 外设实例 / 引脚 / 硬件参数（自 hw_pinmap.h 收编）
// ============================================================

// ---------- PWM（电机驱动 / FOC 节拍） ----------
#define PWM_GET_HTIM (htim8) // 电机控制定时器句柄
#define TIC_PWM 2099         // 定时器周期计数值（ARR，对应 F_PWM=20kHz）

// ---------- 编码器 SPI + CS ----------
#define ENCODER_SPI_CH (hspi3)
#define ENCODER_SPI SPI3
#define ENCODER_INT_CS_GPIOx GPIOA
#define ENCODER_INT_CS_GPIOx_PIN GPIO_PIN_15
#define ENCODER_EXT_CS_GPIOx GPIOA
#define ENCODER_EXT_CS_GPIOx_PIN GPIO_PIN_15

// ---------- Flash SPI + CS ----------
#define FLASH_SPI_CH (hspi2)
#define FLASH_CS_GPIOx GPIOB
#define FLASH_CS_GPIOx_PIN GPIO_PIN_12

// ---------- RGB (WS2812) ----------
#define RGB_PWM_GET_HTIM (htim4)
#define RGB_PWM_CHANNEL1 TIM_CHANNEL_2
#define Pixel_NUM 2  // 板上灯珠数量
#define CODE_1 (75)  // 逻辑 1 占空比 CCR 值
#define CODE_0 (35)  // 逻辑 0 占空比 CCR 值

// ---------- GPIO：LED / 电源 / USB ----------
#define LED_ENCODER_GPIOx GPIOD
#define LED_ENCODER_GPIOx_PIN GPIO_PIN_2
#define LED_CANrx_GPIOx GPIOB
#define LED_CANrx_GPIOx_PIN GPIO_PIN_3
#define POWER12V_GPIOx GPIOC
#define POWER12V_GPIOx_PIN GPIO_PIN_13
#define USB_CS_GPIOx GPIOA
#define USB_CS_GPIOx_PIN GPIO_PIN_8

// ---------- 串口 / CAN（通讯重构后置，句柄宏先收在此） ----------
#define UART_CH (huart1)
#define UART_INSTANCE USART1
#define CAN_CH (hcan2)
#define CAN_INSTANCE CAN2

// ---------- 模拟前端系数（码→物理量换算） ----------
#define RATE_CURRENT_SAMPLE 100.0f // 电流采样放大比率
#define RATE_VOLTAGE_SAMPLE 16     // 电压采样分压比率

// ============================================================
// 板级时间服务（替代 bsp_get_tick / HAL_GetTick 直用）
// ============================================================

// 板级初始化（使能 DWT 周期计数等）；HAL_Init 之后调用一次
void platform_init(void);

// 毫秒 tick（SysTick，可回绕，调用方用差值）
uint32_t platform_get_ms(void);

// 微秒 tick（DWT 周期计数 / 主频）
uint32_t platform_get_us(void);

// 阻塞延时（毫秒）
void platform_delay_ms(uint32_t ms);

// ============================================================
// 板级固件服务（中断/向量/复位/跳转）
// ============================================================

// 全局中断开关
void platform_enable_irq(void);
void platform_disable_irq(void);

// 设置中断向量表偏移（APP 启动早期用 VECT_TABLE_OFFSET）
void platform_set_vector_offset(uint32_t offset);

// 系统复位（NVIC）
void platform_system_reset(void);

// 跳转到指定地址固件（如 BL→APP）；要求 addr 处为合法向量表
void platform_jump_to_addr(uint32_t addr);

#endif // __PLATFORM_H
