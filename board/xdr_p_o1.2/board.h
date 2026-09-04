#ifndef __BOARD_H
#define __BOARD_H

// ============================================================
// board.h — xdr_p_o1.2 板级产品配置（v2 单点）
//
// 由原 usr_config.h + board_config.h 融合而来。只含"产品/算法/存储规划/
// 版本"参数，**不含任何厂商库符号**（引脚/外设宏在 platform.h）。
//
// usr 侧需要板参数的代码 include "board.h"（板目录在 include 路径）。
// ============================================================

// ---------- 固件版本与作者 ----------
#define FIRM_NAME      "XDr-P"
#define FIRM_AUTHOR    "wxd"
#define FIRM_V_DATE    "O_V1.2_260626"
#define FIRM_VERSION   FIRM_NAME " " FIRM_V_DATE

// ---------- 启动配置 ----------
#define VECT_TABLE_OFFSET  0x8000U

// ---------- 硬件性能（控制节拍） ----------
#define F_PWM 20000.0f
#define T_PWM 0.00005f
#define T_CON 0.00005f

// ---------- 控制时序 ----------
#define T_SAMPLE_us 7      // 采样 4-7us
#define T_DEADTIME_us 0.5f // 死区时间
#define T_NOISE_us 0.5f    // 开关噪声时间

// ---------- 保护阈值 ----------
#define MAX_CURRENT 100    // MOS管最大电流 100A
#define MAX_VOLTAGE 34     // 最大电压 34V
#define MIN_VOLTAGE 20     // 最小电压 20V
#define MAX_TEMPERATURE 80 // 最大工作温度

// ---------- 控制分频 ----------
#define FREQ_HIGH_LOOP 1    // 高环分频 (1 = 每PWM周期执行)
#define FREQ_MEDIUM_LOOP 10 // 中环分频 (每FREQ_HIGH_LOOP*FREQ_MEDIUM_LOOP = 10个PWM周期)
#define FREQ_LOW_LOOP 10    // 低环分频 (每FREQ_MEDIUM_LOOP*FREQ_LOW_LOOP = 100个PWM周期)

// ---------- 计算参数 ----------
#define F_CURRENT           (F_PWM / FREQ_HIGH_LOOP)
#define F_SPEED             (F_CURRENT / FREQ_MEDIUM_LOOP)
#define F_POSITION          (F_SPEED / FREQ_LOW_LOOP)

// ---------- 数据流 ----------
#define T_DATA_STREAM 1     // 数据流发送间隔 (ms)
#define T_STATE_STREAM 500  // 状态包发送间隔 (ms)
#define TEMP_VBUS_TS_MS 300 // 温度/电压采样间隔 (ms)

// ---------- 通讯（协议常量，通讯层后置重构） ----------
#define STD_ID_MASK 0x7FF      // 标准帧11位ID掩码
#define EXT_ID_MASK 0xFFFFFFFF // 扩展帧29位ID掩码

// ---------- 速度 LPF（2nd Butterworth fc=120Hz fs=20kHz，由 tools/filter_coeffs.py 生成） ----------
#define LPF_W_B0    0.0003460413
#define LPF_W_B1    0.0006920827
#define LPF_W_B2    0.0003460413
#define LPF_W_A1    1.9466975408
#define LPF_W_A2    -0.9480817061

// ---------- 滤波器截止频率 ----------
#define CUR_LPF_HZ    800.0f   // 电流采样 LPF 截止
#define SPEED_LPF_HZ  50.0f    // 速度 LPF 截止

// ---------- HFI 参数 ----------
#define HFI_INJ_VOLT_AMP 2.0f      // 高频注入电压幅值 (V)
#define HFI_INJ_FREQ_HZ 5000.0f    // 高频注入频率 (Hz)
#define HFI_PLL_BANDWIDTH_HZ 60.0f // HFI-PLL 带宽 (Hz)
#define SPEED_LPF_FACTOR 2.0f      // 速度 LPF 截止频率系数

// ---------- 硬件版本与产品信息 ----------
#define PROD_SERIES "P"
#define FUN_V "O"
#define FIRM_V "V1.2"

// ============================================================
// Flash 存储规划（含内部 MCU Flash IAP）
// ============================================================

#define FLASH_START_ADDR 0x08000000U // Flash 起始地址
#define FLASH_SIZE_KB 1024           // Flash 大小 (KB)
#define FLASH_END_ADDR 0x080FFFFFU   // Flash 结束地址
#define NORMAL_MAGIC 0xFFFFFFFF      // 空数

// ---------- IAP 分区（BL/APP） ----------
#define FIRMWARE_TYPE APP          // APP=应用固件，BL=Bootloader
#define BL_START_ADDR 0x08000000U  // Bootloader 起始地址
#define BL_SIZE_KB 32              // Bootloader 大小 (KB) 两个扇区
#define APP_START_ADDR 0x08008000U // App 起始地址
// APP 区上界由 BL 起始 + 分区表推导，见内部 flash 介质驱动/IAP 编排实现

// ---------- LOG 区 ----------
#define LOG_START_ADDR 0x080A0000U // 日志起始地址
#define LOG_SIZE_KB 128            // 日志空间大小 (KB)
// 注：扇区编号由内部 flash 介质驱动按几何管理；新架构擦写按地址寻址

// ---------- PARAMETER 区 ----------
#define PARAMETER_LOAD_ADDR 0x080C0000U // 参数加载/保存地址
#define PARAMETER_SIZE_KB 128           // 参数空间大小 (KB)

// ---------- 固件升级标志区 ----------
#define IAP_FLAG_ADDRESS 0x080E0000
#define IAP_FLAG_SIZE_KB 128

#endif // __BOARD_H
