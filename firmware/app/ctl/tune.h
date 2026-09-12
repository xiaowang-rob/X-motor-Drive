#ifndef __TUNE_H
#define __TUNE_H

#include "bsp_base.h"
#include "foc_core.h"
#include "usr_config.h"
#include "protocol.h"

// ================================= 整定参数配置 =================================
// 通用时间转换 (假设 20kHz 中断，1 tick = 50us)

#define TICK_TO_MS(tick) ((tick) * 0.05f)
#define MS_TO_TICK(ms) ((u16)((ms) * 20.0f))

#define TUNE_WAIT_TICKS MS_TO_TICK(1000) // 先静止等待时间

// ================== 校准电流锚点 ==================
// 所有电流相关目标值 = tune_cur_limit × 系数
// tune_cur_limit 在 init 时从 g_Param.limit_current 获取
// tune_cur_limit == 0 时直接返回 TUNE_FAULT

// ================== 电阻整定系数 ==================
#define RS_FREQ_F 10                      // 电阻整定分频系数
#define RS_I_TARGET_1_COEF 0.2f           // 第一点目标电流 = tune_cur_limit × 0.2
#define RS_I_TARGET_2_COEF 0.6f           // 第二点目标电流 = tune_cur_limit × 0.6
#define RS_HYST_BAND_COEF 0.12f           // 滞环带宽 = tune_cur_limit × 0.12
#define RS_V_LIMIT_COEF 0.1f              // 电压限幅 = bus_voltage × 0.1 (最高10%母线)
#define RS_V_HOLD_MAX_TICKS 5             // 误差带内保持最大周期数 (防静差)
#define RS_V_STEP_MIN 0.01f               // 保持超时后微调步长 (V)
#define RS_STEADY_ERR_THR_COEF 0.02f      // 稳态电流误差阈值 = tune_cur_limit × 0.02
#define RS_STEADY_TICKS MS_TO_TICK(7)     // 稳态持续周期数 (7ms@20kHz)
#define RS_MIN_DELTA_I_COEF 0.25f         // 最小电流变化量 = tune_cur_limit × 0.25
#define RS_RANGE_MIN 0.02f                // 电阻合理下限 (Ω)
#define RS_RANGE_MAX 0.5f                 // 电阻合理上限 (Ω)
#define RS_DEADTIME_VCOMP 0.04f           // 死区补偿电压 (V)

// ================== 电感整定系数 ==================
#define LS_INJECT_FREQ_TICK 20                          // 注入分频
#define LS_INJECT_FREQ_HZ (F_PWM / LS_INJECT_FREQ_TICK) // 注入频率 (Hz)

#define LS_V_START_MIN 0.2f       // 注入电压最小值 (V)
#define LS_V_START_COEF 0.4f      // 起始电压 = Rs × tune_cur_limit × 0.15
#define LS_V_MAX_COEF 0.8f        // 最大电压 = Rs × tune_cur_limit × 0.6
#define LS_V_LIMIT_BUS_COEF 0.1f  // 电压上限不超过母线 × 0.1
#define LS_I_TARGET_COEF 0.2f     // 目标电流 = tune_cur_limit × 0.2
#define LS_I_TARGET_HYST 0.05f    // 目标电流滞环 ±10%
#define LS_V_ADJ_STEP 0.01f       // 电压自适应调整步长 (V)

// ================== 转子预定位 ==================
#define ALIGN_TIME_MS_BASE 200  // 定位基础持续时间(ms)
#define WAIT_AFTER_ALIGN_MS 200 // 定位后等待电流衰减时间(ms)
#define ALIGN_CUR_REF 10.0f     // 对齐时间参照电流 (A)

// ================== DFT测量 ==================
#define DFT_AVG_CYCLES 20 // 取20个注入周期的平均

#define LS_RANGE_MIN 20e-6f   // 电感合理下限(H)
#define LS_RANGE_MAX 300e-6f  // 电感合理上限 (H)

// ================== 编码器校准系数 ==================
#define EC_FREQ_F 10                // 编码器校准分频系数
#define EC_ALIGN_ms MS_TO_TICK(300) // 编码器校准等待时间
#define EC_OPEN_LOOP_OMEGA 17.4533f // 开环角速度 (rad/s), 原 1000°/s


#define EC_FIT_MAX_ERROR 100.0f // 最大拟合误差
#define EC_MIN_POLE_PAIRS 1     // 最小极对数
#define EC_MAX_POLE_PAIRS 16    // 最大极对数

// ================== 角度偏移 (开环强励磁) ==================

// ================================= 电机参数结构(独立存储) =================================
typedef struct
{
    // 系统参数
    float dt;             // 控制周期 (s)
    float tune_cur_limit; // 电流限幅 (A)

    // 控制参数
    float bandwidth_current; // 电流滞环带宽
    float bandwidth_speed;   // 速度滞环带宽
    float bandwidth_pos;     // 位置滞环带宽

    float uadc_offset; // adc 偏移
    float vadc_offset;
    float wadc_offset;

    float cur_filter_alpha; // 电流滤波系数

    float iq_kp;
    float iq_ki;
    float id_kp;
    float id_ki;
    float speed_kp;
    float speed_ki;
    float mag_kp;
    float mag_ki;
    float pos_kp;
    float pos_ki;
    float pos_kd;

    // ================================= 电机参数 ==========
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

    // ========== 编码器参数 =====
    float theta_offset; // 编码器角度偏移 (rad)
    u8 pole_pairs;      // 极对数
    bool direction;     // 转动方向 (true:逆 false 顺)
    bool theta_elec_need_180;

} tTuneParams;

// ================================= 整定上下文(内部状态) =================================
typedef struct
{
    // 通用状态
    eTuneState state;
    eFaultState fault;
    u8 freq_tick;          // 频率计数
    u32 steady_tick;       // 稳态计数
    u32 timeout_tick;      // 超时计数
    u32 align_total_ticks; // 对齐总时长 (tick)，按电流比例缩放
    u8 tune_round;         // 整定轮次: 0=第一轮(电压), 1=第二轮(电流评估)
    // 电阻整定上下文 (阈值在进入阶段时一次预计算)
    struct
    {
        float i_target;    // 当前点目标电流 (A)
        float i_target_2;  // 第二点目标电流 (A)
        float hyst_band;   // 滞环带宽 (A)
        float v_limit;     // 电压限幅 (V)
        float steady_err;  // 稳态误差阈值 (A)
        float min_delta_i; // 最小电流变化量 (A)
        float v_meas[2];
        float i_meas[2];
        float v_cmd;
        float ol_rs[3];
        float ol_u[3];
        u16 hold_cnt;
        u16 step_ticks;
        u8 step;
        u8 ol_stage;
    } rs_ctx;

    // 电感整定上下文 (阈值在进入阶段时一次预计算)
    struct
    {
        // 状态机 (0~5)
        uint8_t state;
        bool ready;

        // ----- 电压自适应相关 -----
        float v_inj;     // 当前注入电压幅值 (V)
        float i_target;  // 目标电流幅值 (A)
        float i_hyst_lo; // 电流下限 = i_target × (1-hyst)
        float i_hyst_hi; // 电流上限 = i_target × (1+hyst)
        float v_step;    // 电压自适应调整步长 (V)
        float v_max;     // 电压上限 (V)
        float i_meas;    // 当前测量的电流幅值 (用于自适应判断)

        // ----- DFT 累加器（无缓冲区，周期累加）-----
        float sum_re;        // 实部累加和
        float sum_im;        // 虚部累加和
        uint16_t sample_cnt; // 当前周期已采样点数

        // ----- 多周期平均 -----
        float amp_sum;     // 多个周期的幅值累加
        uint8_t cycle_cnt; // 已完成的有效周期数

    } ls_ctx;

    // 编码器校准上下文 (阈值在进入阶段时一次预计算)
    struct
    {
        float theta_elec;
        float cur_test;    // 当前试探电流 (A)
        float theta_e_acc;    // 连续电角度
        float theta_e_raw;    // 上一次电角度
        float theta_m_unwrap; // 解包后的连续机械角度
        float theta_m_start;  // 起始机械角度
        float delta_theta[2];

        float k[2];
        float b[2];
        float err[2];

        float sum_m[2];  // Σθ_m
        float sum_e[2];  // Σθ_e
        float sum_me[2]; // Σ(θ_m·θ_e)
        float sum_mm[2]; // Σ(θ_m²)

        u16 cnt[2]; // 采样点数

        bool forward_done;
        bool backward_done;
        u8 test_step;
        u8 step; // 0对齐 1正向 2反向 3拟合计算 4完成

    } encoder_ctx;

    // 磁链上下文
    struct
    {
        float sum_e_mag;
        float sum_vel;
        u16 valid_cnt;
        bool ready;
    } psi_ctx;

    // 机械参数上下文
    struct
    {
        float vel_start;
        float sum_torque;
        float sum_accel;
        u16 sample_cnt;
        bool accel_phase;
        bool ready;
    } jb_ctx;

    // 临时变量
    float temp_val[4];
    bool temp_flag[4];
} tTuneContext;

// ================================= 公共接口 =================================
void motor_param_tune_init();
void motor_param_tune_reset();
eTuneState tune_main_loop(tFOC_val *foc_val);
u8 motor_param_tune_get_progress(void);
eFaultState tune_get_fault(void);

#endif // __TUNE_H