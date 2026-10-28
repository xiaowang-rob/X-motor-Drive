#ifndef __TUNE_H
#define __TUNE_H

#include "tune_alg.h"

#include "control.h"
#include "core.h"
#include "obs.h"
#include "parameters.h"

// 整定参数配置
#define RL_DUTY_MAX 0.5f             // 留出同幅值 HFI 叠加所需调制余量
#define DUTY_SETTLE_MS 10.0f         // 每次占空比调整后的等待时间
#define RS_STEADY_ERR_THR_COEF 0.02f // 稳态电流误差阈值 = cur_limit × 0.02
#define RS_STEADY_MS 10.0f           // 稳态持续时间
#define RS_RANGE_MIN 0.02f           // 电阻合理下限 (Ω)
#define RS_RANGE_MAX 10.0f           // 电阻合理上限 (Ω)
#define RS_PHASE_DIFF_THR_COEF 0.1f  // 三相电阻差异阈值 = 0.1

#define LS_INJECT_FREQ_HZ 1000U // 注入频率 (Hz)
#define LS_ALIGN_MS 100.0f      // 对齐持续时间
#define LS_RANGE_MIN 20e-6f     // 电感合理下限(H)
#define LS_RANGE_MAX 300e-6f    // 电感合理上限 (H)
#define LS_AVG_CYCLES 5         // 平均滤波次数

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

/*参数校准状态*/
typedef enum {
    TUNE_IDLE,    /* 空闲 */
    TUNE_RUNNING, /* 运行中 */
    TUNE_DONE,    /* 完成 */
    TUNE_FAILED,  /* 失败 */
} eTuneState;
// 校准类型
typedef enum {
    TUNE_FOC,  // FOC内参辨识、RS Ldq Psif
    TUNE_OBS,  // 观测器校准
    TUNE_MECH, // 机械参数辨识
    TUNE_PID,  // PID参数自适应
    TUNE_MIT,  // MIT参数自适应
} eTuneType;

typedef struct {
    eTuneState state;
    eTuneType type;
    eFault fault;

    uint8_t step;
    bool TO_init; // 单项是否初始化
    float cur_limit;

    tTune_rl_ctx rl_ctx;
    tEncCal_ctx enc_ctx;
    tTune_psif_ctx psif_ctx;
    tTune_JB_ctx jb_ctx;

} tTune;

#endif // __TUNE_H