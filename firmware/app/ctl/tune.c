#include "tune.h"

#include "bsp_math.h"

#include "tune_alg.h"
#include "parameters.h"

#define RS_I_TARGET_1_COEF 0.2f      // 第一点目标电流 = tune_cur_limit × 0.2
#define RS_I_TARGET_2_COEF 0.6f      // 第二点目标电流 = tune_cur_limit × 0.6
#define RS_STEADY_ERR_THR_COEF 0.02f // 稳态电流误差阈值 = tune_cur_limit × 0.02
#define RS_STEADY_MS 10.0f           // 稳态持续时间
#define RS_RANGE_MIN 0.02f           // 电阻合理下限 (Ω)
#define RS_RANGE_MAX 0.5f            // 电阻合理上限 (Ω)
#define RS_PHASE_DIFF_THR_COEF 0.1f  // 三相电阻差异阈值 = 0.1

#define LS_INJECT_FREQ_HZ 1000U  // 注入频率 (Hz)
#define LS_ALIGN_MS 100.0f       // 对齐持续时间
#define LS_V_START_MIN 0.2f      // 注入电压最小值 (V)
#define LS_V_LIMIT 10.0f         // 注入电压上限(V)
#define LS_V_START_COEF 0.4f     // 起始电压 = Rs × tune_cur_limit × 0.15
#define LS_V_MAX_COEF 0.8f       // 最大电压 = Rs × tune_cur_limit × 0.6
#define LS_V_LIMIT_BUS_COEF 0.1f // 电压上限不超过母线 × 0.1
#define LS_I_TARGET_COEF 0.4f    // 目标电流 = tune_cur_limit × 0.4
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

#define PSIF_VEL_LOW 0.0f     // 低速限
#define PSIF_VEL_HIGH 10.0f   // 高速限
#define PSIF_NUM_POINTS 1000  // 转速点数
#define PSIF_STEADY_MS 100.0f // 稳态持续时间
#define PSIF_SAMPLE_MS 100.0f // 稳态持续时间
#define PSIF_VEL_BAND 0.05f   // 转速带宽 (rad/s)

typedef struct
{
    eTuneState state;

    tTuneParams params;

    tTune_rs_oc_ctx rs_ctx;
    tune_Ldq_hfi_ctx ls_ctx;
    tEncCal_ctx enc_ctx;
    tPsif_ctx psif_ctx;

} tTune;

void tune_init(tTune *tune, uint8_t pp, float tune_cur_limit, float ts)
{
    // 电阻参数辨识
    tTune_rs_oc_cfg rs_oc_cfg = {
        .cur_1 = tune_cur_limit * RS_I_TARGET_1_COEF,
        .cur_2 = tune_cur_limit * RS_I_TARGET_2_COEF,
        .cur_steady_err = tune_cur_limit * RS_STEADY_ERR_THR_COEF,
        .steady_ticks = RS_STEADY_MS / 1000.0f / ts,
        .rs_min = RS_RANGE_MIN,
        .rs_max = RS_RANGE_MAX,
        .rs_phase_diff_thr_coef = RS_PHASE_DIFF_THR_COEF,
    };
    tune_rs_ol_cur_init(&tune->rs_ctx, rs_oc_cfg);

    tTune_Ldq_hfi_cfg ls_hfi_cfg = {
        .omega_h = MATH_2PI * LS_INJECT_FREQ_HZ,
        .omega_dt = MATH_2PI * LS_INJECT_FREQ_HZ * ts,
        .n_per_cycle = (uint16_t)(1.0f / (LS_INJECT_FREQ_HZ * ts) + 0.5f),
        .align_ticks = LS_ALIGN_MS / 1000.0f / ts,
        .v_inj_start = LS_V_START_MIN,
        .v_inj_max = LS_V_LIMIT,
        .v_inj_step = LS_V_ADJ_STEP,
        .i_hyst_lo = tune_cur_limit * (1 - LS_I_TARGET_HYST) * LS_I_TARGET_COEF,
        .i_hyst_hi = tune_cur_limit * (1 + LS_I_TARGET_HYST) * LS_I_TARGET_COEF,

        .ls_min = LS_RANGE_MIN,
        .ls_max = LS_RANGE_MAX,
        .avg_cycles = LS_AVG_CYCLES,
    };
    tune_ldq_hfi_init(&tune->ls_ctx, ls_hfi_cfg);

    tEncCal_cfg enc_cal_cfg = {
        .i_tune = tune_cur_limit,
        .align_ticks = EC_ALIGN_MS / 1000.0f / ts,
        .delta_e = EC_OPEN_LOOP_OMEGA * ts,
        .sample_step_e = EC_SAMPLE_STEP_E,
        .travel_pos = EC_TRAVEL_POS,
        .pole_pairs_expected = pp,
        .fit_max_mse = EC_FIT_MAX_MSE,

    };
    enc_cal_init(&tune->enc_ctx, enc_cal_cfg);

    tPsif_cfg psif_cfg = {
        .rs_known = tune->params.rs,
        .vel_low = PSIF_VEL_LOW,
        .vel_high = PSIF_VEL_HIGH,
        .num_points = PSIF_NUM_POINTS,
        .steady_ticks = PSIF_STEADY_MS / 1000.0f / ts,
        .sample_ticks = PSIF_SAMPLE_MS / 1000.0f / ts,
        .vel_band = PSIF_VEL_BAND,
        .pole_pairs = tune->params.pole_pairs,
    };
}
void tune_update(tTune *tune, float ts)
{
    switch (tune->state)
    {
    case TUNE_INIT:

        break;
    }
}

// 1、电气参数层辨识 （Ld Lq（高频注入） Rs/Psif （RLS/MRAS））
// 2、编码器校准 （偏移值 极对数 方向）
// 3、机械参数辨识（J TL EKO+MRAS）

// ================================= 全局变量定义 =================================

static tTuneContext tune_ctx = {0};

// 控制带宽、滤波系数与PID参数的经验公式 fc-电流环频率
static inline void calculate_control_params(tParameter *p, float fc)
{
    // TODO:添加电流环带宽调节
    float fn_d = 1 / (MATH_2PI * temp_params.ld / temp_params.rs);
    float wc_d = MATH_2PI * 0.5f * (2 * fn_d < fc / 10 ? 2 * fn_d : fc / 10);
    temp_params.id_kp = wc_d * temp_params.ld;
    temp_params.id_ki = wc_d * temp_params.rs;

    float fn_q = 1 / (MATH_2PI * temp_params.lq / temp_params.rs);
    float wc_q = MATH_2PI * 0.6f * (2 * fn_q < fc / 10 ? 2 * fn_q : fc / 10);
    temp_params.iq_kp = wc_q * temp_params.lq;
    temp_params.iq_ki = wc_q * temp_params.rs;

    // 取电流环开环截止频率 (Hz)
    float f_c_open = fminf(wc_d, wc_q) / MATH_2PI;

    // 设计滤波截止频率：取开环截止频率的 4倍
    float f_filter = 4.0f * f_c_open;

    // 上下限约束
    float f_sw = fc;               // 这里和电流环同频
    float f_max = f_sw / 8.0f;     // 最大允许滤波截止频率（保证滤除开关纹波）
    float f_min = 2.0f * f_c_open; // 最小允许值（避免影响环路）

    if (f_filter > f_max)
        f_filter = f_max;
    if (f_filter < f_min)
        f_filter = f_min;

    // 计算一阶低通滤波系数 alpha
    temp_params.cur_filter_alpha = 1.0f - expf(-MATH_2PI * f_filter / f_sw);

    // 直接写入
    foc_set_cur_loop_param(temp_params.id_kp, temp_params.id_ki, temp_params.iq_kp, temp_params.iq_ki);
}

// ================================= 参数访问实现 =================================
void motor_param_tune_force_save(tParameter *p)
{
    //  这里是集中写入参数flash中转站
    p->motor_rs = temp_params.rs;
    p->motor_ld = temp_params.ld;
    p->motor_lq = temp_params.lq;
    p->motor_psif = temp_params.psi_f;
    p->motor_ke = temp_params.ke;
    p->motor_j = temp_params.j;
    p->motor_b = temp_params.b;
    p->motor_polepairs = temp_params.pole_pairs;
    p->theta_offset = temp_params.theta_offset;
    p->theta_elec_offset = temp_params.theta_elec_need_180;
    p->positive_dir = temp_params.direction;

    p->qclkp = temp_params.iq_kp;
    p->qclki = temp_params.iq_ki;
    p->dclkp = temp_params.id_kp;
    p->dclki = temp_params.id_ki;

    p->cfalpha = temp_params.cur_filter_alpha;
}

// ================================= 初始化与重置 =================================
// TODO:后续添加无感整定，根据选择的感应模式校准，无感只校准电机，有感校准电机和编码器，直接校准电机，直接用HFI、SMO做精准的控制
void motor_param_tune_init(tParameter *p)
{
    memset(&tune_ctx, 0, sizeof(tTuneContext));
    memset(&temp_params, 0, sizeof(tTuneParams));
    temp_params.kv = p->motor_kv;
    temp_params.tune_cur_limit = p->tune_current; // 从用户配置获取校准电流锚点
}

void motor_param_tune_reset()
{
    tune_ctx.fault = FAULT_NONE;
    tune_ctx.state = TUNE_INIT;
}

static bool _tune_psi_f()
{
    // TODO: SMO 高速整定框架
    // 1. 高速运行 (例如 1000rpm+)，SMO 估算反电动势
    // 2. ke = |E| / omega_elec
    // 3. psi_f = Ke / pole_pairs

    // 临时占位：使用 KV 反推（后续替换为 SMO 实测）
    temp_params.ke = 60.0f / (2.0f * MATH_PI * temp_params.kv * temp_params.pole_pairs);
    temp_params.psi_f = temp_params.ke / temp_params.pole_pairs;
    return true;
}

// ================================= 转动惯量/摩擦系数整定 (框架) =================================
static bool _tune_jb()
{
    // TODO: 阶跃响应法框架
    // 1. 施加阶跃转矩 (例如 iq=2A)
    // 2. 记录加速度曲线: alpha = d(omega)/dt
    // 3. j = T / alpha (忽略摩擦), b = (T - J*alpha) / omega (匀速段)

    // 临时占位：使用默认值跳过
    temp_params.j = 0.0001f;
    temp_params.b = 0.001f;
    return true;
}

// ================================= 整定主循环 (状态切换时初始化) =================================
eTuneState tune_main_loop(tFOC_val *foc_val, float ts)
{
    tTuneContext *ctx = &tune_ctx;
    ctx->steady_tick++; // 作为全局稳态计时器
    switch (ctx->state)
    {
    case TUNE_INIT:
        motor_param_tune_init();
        // TODO: 后续添加无感整定 可以添加HFI 和 SMO 参数整定
        foc_set_sensor_mode(ENCODER_CONTROL);
        foc_set_run_mode(OPEN_VOL); // 先切开环电压
        ctx->steady_tick = 0;
        ctx->state = TUNE_IDLE;
        break;
    case TUNE_IDLE:
        if (ctx->steady_tick < TUNE_WAIT_TICKS)
            break; // 先静止等待参数稳定

        // 检查校准电流锚点：tune_cur_limit <= 0 时无法校准 指向电机堵转
        if (temp_params.tune_cur_limit <= 0.0f)
        {
            ctx->fault = FAULT_MOTOR_LOCK;
            ctx->state = TUNE_FAILED;
            break;
        }

        if (false == bsp_adc_calibrate_current(&temp_params.uadc_offset, &temp_params.vadc_offset, &temp_params.wadc_offset))
            break;      // 电流校准
        filter_reset(); // 对电流滤波器进行复位

        // ========== 预计算：Rs 校准阶段所需阈值 (一次计算，整个阶段不变) ==========
        {
            float cur_lim = temp_params.tune_cur_limit;
            float bus_v = foc_val->udc;

            ctx->rs_ctx.i_target = cur_lim * RS_I_TARGET_1_COEF;
            ctx->rs_ctx.i_target_2 = cur_lim * RS_I_TARGET_2_COEF;
            ctx->rs_ctx.hyst_band = cur_lim * RS_HYST_BAND_COEF;
            ctx->rs_ctx.v_limit = bus_v * RS_V_LIMIT_COEF;
            ctx->rs_ctx.steady_err = cur_lim * RS_STEADY_ERR_THR_COEF;
            ctx->rs_ctx.min_delta_i = cur_lim * RS_MIN_DELTA_I_COEF;
            ctx->rs_ctx.v_cmd = 0;
            ctx->rs_ctx.step = 0;

            // 对齐时间：按电流比例缩放
            float align_scale = cur_lim / ALIGN_CUR_REF;
            if (align_scale < 1.0f)
                align_scale = 1.0f;
            ctx->align_total_ticks = (u32)(MS_TO_TICK(ALIGN_TIME_MS_BASE) * align_scale);
        }
        ctx->steady_tick = 0;
        ctx->timeout_tick = 0;

        ctx->state = TUNE_RESISTANCE;
        break;

    case TUNE_RESISTANCE:
        if (ctx->tune_round == 0)
        {
            // 第一轮：开环电压法测 Rs
            if (_tune_rs_ol_vol(foc_val->ialpha))
            {
                if (ctx->fault != FAULT_NONE)
                {
                    ctx->state = TUNE_FAILED;
                    break;
                }

                // 预计算电感阈值
                {
                    float cur_lim = temp_params.tune_cur_limit;
                    float rs_v_ref = temp_params.rs * cur_lim;
                    float bus_v = foc_val->udc;
                    ctx->ls_ctx.v_inj = rs_v_ref * LS_V_START_COEF;
                    if (ctx->ls_ctx.v_inj < LS_V_START_MIN)
                        ctx->ls_ctx.v_inj = LS_V_START_MIN;
                    float ls_v_max = rs_v_ref * LS_V_MAX_COEF;
                    float ls_v_bus = bus_v * LS_V_LIMIT_BUS_COEF;
                    if (ls_v_max > ls_v_bus)
                        ls_v_max = ls_v_bus;
                    ctx->ls_ctx.v_max = ls_v_max;
                    ctx->ls_ctx.i_target = cur_lim * LS_I_TARGET_COEF;
                    ctx->ls_ctx.i_hyst_lo = ctx->ls_ctx.i_target * (1.0f - LS_I_TARGET_HYST);
                    ctx->ls_ctx.i_hyst_hi = ctx->ls_ctx.i_target * (1.0f + LS_I_TARGET_HYST);
                    ctx->ls_ctx.v_step = LS_V_ADJ_STEP;
                    ctx->ls_ctx.ready = false;
                }
                ctx->steady_tick = 0;
                ctx->timeout_tick = 0;
                ctx->state = TUNE_INDUCTANCE;
            }
        }
        else
        {
            // 第二轮：开环电流三相电阻评估
            if (_tune_rs_ol_cur(foc_val))
            {
                if (ctx->fault != FAULT_NONE)
                {
                    ctx->state = TUNE_FAILED;
                    break;
                }

                // 更新参数，切回开环电压重跑电感
                calculate_control_params();
                motor_param_tune_force_save();

                foc_set_run_mode(OPEN_VOL);
                ctx->ls_ctx.state = 0;
                ctx->ls_ctx.ready = false;
                ctx->steady_tick = 0;
                ctx->timeout_tick = 0;
                ctx->state = TUNE_INDUCTANCE;
            }
        }
        break;

    case TUNE_INDUCTANCE:
        if (_TuneLs(foc_val->ualpha, foc_val->ubeta, foc_val->ialpha, foc_val->ibeta))
        {
            ctx->rs_ctx.step_ticks = 0;
            if (ctx->fault != FAULT_NONE)
            {
                ctx->state = TUNE_FAILED;
                break;
            }

            calculate_control_params();
            motor_param_tune_force_save();

            if (ctx->tune_round == 0)
            {
                // 第一轮结束 → 第二轮：切开环电流做三相电阻评估
                ctx->tune_round = 1;
                foc_set_run_mode(OPEN_CUR);
                ctx->rs_ctx.ol_stage = 0;
                ctx->steady_tick = 0;
                ctx->timeout_tick = 0;
                ctx->state = TUNE_RESISTANCE;
            }
            else
            {
                // 第二轮结束 → 编码器校准
                foc_set_run_mode(OPEN_CUR);
                encoder_set_angle_zero();
                ctx->encoder_ctx.theta_elec = 0;
                ctx->encoder_ctx.cur_test = temp_params.tune_cur_limit * 0.1f;
                ctx->encoder_ctx.forward_done = false;
                ctx->encoder_ctx.backward_done = false;
                ctx->encoder_ctx.step = 0;
                ctx->steady_tick = 0;
                ctx->timeout_tick = 0;
                ctx->state = TUNE_ENCODER;
            }
        }
        break;

    case TUNE_ENCODER:
        if (_tune_encoder(foc_val->theta_mech))
        {
            if (ctx->fault != FAULT_NONE)
            {
                ctx->state = TUNE_FAILED;
                break;
            }

            ctx->psi_ctx.sum_e_mag = 0;
            ctx->psi_ctx.sum_vel = 0;
            ctx->psi_ctx.valid_cnt = 0;
            ctx->psi_ctx.ready = false;
            ctx->steady_tick = 0;

            ctx->state = TUNE_ELEC_PARAM;
        }
        break;

    case TUNE_ELEC_PARAM:
        if (_tune_psi_f())
        {
            if (ctx->fault != FAULT_NONE)
            {
                ctx->state = TUNE_FAILED;
                break;
            }

            //  磁链完成：初始化机械参数上下文
            ctx->jb_ctx.accel_phase = false;
            ctx->jb_ctx.sample_cnt = 0;
            ctx->jb_ctx.sum_torque = 0;
            ctx->jb_ctx.sum_accel = 0;
            ctx->jb_ctx.ready = false;
            ctx->steady_tick = 0;

            ctx->state = TUNE_MECH_PARAM;
        }
        break;

    case TUNE_MECH_PARAM:
        if (_tune_jb())
        {
            if (ctx->fault != FAULT_NONE)
            {
                ctx->state = TUNE_FAILED;
                break;
            }

            // todo:这里可以对速度环PI和位置环PID 参数进行调节
            //  结束：保存参数并进入完成状态

            motor_param_tune_force_save();
            ctx->state = TUNE_DONE;
        }
        break;

    default:
        break;
    }
    return ctx->state;
}

// ================================= 辅助接口 =================================
eFaultState tune_get_fault(void) { return tune_ctx.fault; }