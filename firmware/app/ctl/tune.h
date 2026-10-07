#ifndef __TUNE_H
#define __TUNE_H

#include "tune_alg.h"

#include "protocol.h"
#include "parameters.h"
#include "foc.h"
#include "core.h"

// 整定参数配置

#define RS_I_TARGET_1_COEF 0.2f      // 第一点目标电流 = cur_limit × 0.2
#define RS_I_TARGET_2_COEF 0.6f      // 第二点目标电流 = cur_limit × 0.6
#define RS_STEADY_ERR_THR_COEF 0.02f // 稳态电流误差阈值 = cur_limit × 0.02
#define RS_STEADY_MS 10.0f           // 稳态持续时间
#define RS_RANGE_MIN 0.02f           // 电阻合理下限 (Ω)
#define RS_RANGE_MAX 0.5f            // 电阻合理上限 (Ω)
#define RS_PHASE_DIFF_THR_COEF 0.1f  // 三相电阻差异阈值 = 0.1

#define LS_INJECT_FREQ_HZ 1000U  // 注入频率 (Hz)
#define LS_ALIGN_MS 100.0f       // 对齐持续时间
#define LS_V_START_MIN 0.2f      // 注入电压最小值 (V)
#define LS_V_LIMIT 10.0f         // 注入电压上限(V)
#define LS_V_START_COEF 0.4f     // 起始电压 = Rs × cur_limit × 0.15
#define LS_V_MAX_COEF 0.8f       // 最大电压 = Rs × cur_limit × 0.6
#define LS_V_LIMIT_BUS_COEF 0.1f // 电压上限不超过母线 × 0.1
#define LS_I_TARGET_COEF 0.4f    // 目标电流 = cur_limit × 0.4
#define LS_I_TARGET_HYST 0.05f   // 目标电流滞环 ±10%
#define LS_V_ADJ_STEP 0.01f      // 电压自适应调整步长 (V)
#define LS_RANGE_MIN 20e-6f      // 电感合理下限(H)
#define LS_RANGE_MAX 300e-6f     // 电感合理上限 (H)
#define LS_AVG_CYCLES 5          // 平均滤波次数

#define EC_ALIGN_MS 100.0f          // 编码器校准对齐等待时间
#define EC_OPEN_LOOP_OMEGA 17.4533f // 开环角速度 (rad/s), 原 1000°/s
#define EC_SAMPLE_STEP_E 0.1745f    // 采样步长 (rad)
#define EC_TRAVEL_POS 6.3f          // 拖动机械角度rad
#define EC_FIT_MAX_MSE 0.001f       // 最大拟合质量阈值

#define PSIF_VEL_LOW 50.0f     // 低速限
#define PSIF_VEL_HIGH 200.0f   // 高速限
#define PSIF_NUM_POINTS 5      // 转速点数
#define PSIF_STEADY_MS 1000.0f // 稳态持续时间
#define PSIF_SAMPLE_MS 1000.0f // 稳态持续时间
#define PSIF_VEL_BAND 5.0f     // 转速带宽 (rad/s)

#define JB_IQ_HIGH_COEF 0.8f // 阶跃电流幅值 = cur_limit × 0.8
#define JB_VEL_MAX 200.0f    // 最大转速 (rad/s)
#define JB_VEL_SETTLE 5.0f   // 减速终止判定阈值 (rad/s)
#define JB_ALPHA_LPF 0.3f    // 加速度低通滤波系数
#define JB_SKIP_MS 1.0f      // 跳过阶跃后 1ms 数据
#define JB_MIN_SAMPLES 10    // 最少采样点数

typedef struct
{
    // 控制参数
    float cur_filter_alpha; // 电流滤波系数

    float iq_kp;
    float iq_ki;
    float id_kp;
    float id_ki;

    // 电气参数
    float kv; // 电压转化器增益 (V/V) 用于检验参数有效性

    float rs;    // 定子电阻(Ω)
    float ld;    // d 轴电感 (H)
    float lq;    // q 轴电感 (H)
    float psi_f; // 永磁体磁链 (Wb)
    float ke;    // 反电动势常数 (V/(rad/s))

    // 机械参数
    float j; // 转动惯量 (kg·m²)
    float b; // 摩擦系数 (N·m·s/rad)

    // 编码器参数
    float theta_offset; // 编码器角度偏移 (rad)
    uint8_t pole_pairs; // 极对数
    bool direction;     // 转动方向 (true:逆 false 顺)

} tTuneParams;

typedef struct
{
    eTuneState state;
    bool TO_init; // 单项是否初始化
    float cur_limit;
    eFault fault;

    tTuneParams params;

    tTune_rs_oc_ctx rs_ctx;
    tTune_ls_hfi_ctx ls_ctx;
    tTune_ls_hfi_ctx ls_ctx;
    tTune_ls_hfi_ctx ls_ctx;
    tTune_ls_hfi_ctx_hfi_ctx ls_ctx;
    tEncCal_ctx enc_ctx;
    tTune_psif_ctx psif_ctx;
    tTune_JB_ctx jb_ctx;

} tTune;

#endif // __TUNE_H