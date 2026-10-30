#include "smo.h"
#include "bsp_math.h"
#include "filter.h"
#include "bsp_cfg.h"

// TODO:完善smo算法
#define SMO_DTICK 4
#define SMO_FREQ fpwm / SMO_DTICK

#define CURRENT_ALPHA 0.25f
#define EMF_ALPHA 0.07f
#define VEL_ALPHA 0.01f

tFirstOrderLagFilter I_alpha_lpf;
tFirstOrderLagFilter I_beta_lpf;
tFirstOrderLagFilter emf_alpha_lpf;
tFirstOrderLagFilter emf_beta_lpf;

tFirstOrderLagFilter vel_lpf;

tSMO g_smo;

// 默认配置
static const tSMO_Config SMO_DEFAULT_CFG = {
    .k_sl_base = 15.0f,
    .k_sl_min_ratio = 0.5f,
    .vel_adapt_start = 100.0f,
    .vel_adapt_end = 400.0f,
    .delta = 0.1f,
    .min_vel_elec = 50.0f,
    .max_vel_elec = 2000.0f,
    .emf_max = 15.0f};

// 预计算不变量
static inline void _SmoPrecompute(tSMO *p, float ts)
{
    float L_avg = (p->ld + p->lq) * 0.5f;
    p->inv_l_eff = 1.0f / (L_avg + p->rs * ts);
}

// 自适应滑模增益 — 基于电压模长（BEMF信号强度），vs 基于估计速度
// 基于电压: v_mag 越大说明信号越好，增益可以降低
// 避免了"速度不准→增益乱调"的鸡生蛋问题
static inline float _CalcAdaptiveGain(tSMO *p, float vel_abs, float v_alpha, float v_beta)
{
#if SMO_GAIN_BY_DUTY
    float v_mag = sqrtf(v_alpha * v_alpha + v_beta * v_beta);
    float v_ratio = v_mag / 12.0f; // 12V 为参考基准
    v_ratio = CLAMP(v_ratio, 0.0f, 1.0f);
    // 电压高→信号好→增益降
    float k_sl = p->cfg.k_sl_base * (p->cfg.k_sl_min_ratio + (1.0f - p->cfg.k_sl_min_ratio) * (1.0f - v_ratio));
    // 最低增益保障
    k_sl = (k_sl < p->cfg.k_sl_base * 0.1f) ? (p->cfg.k_sl_base * 0.1f) : k_sl;
    return k_sl;
#else
    // 原方案：基于速度（保留作为对比）
    float k_sl = p->cfg.k_sl_base;
    if (vel_abs > p->cfg.vel_adapt_start)
    {
        float ratio = (vel_abs - p->cfg.vel_adapt_start) /
                      (p->cfg.vel_adapt_end - p->cfg.vel_adapt_start);
        ratio = CLAMP(ratio, 0.0f, 1.0f);
        k_sl = p->cfg.k_sl_base * (p->cfg.k_sl_min_ratio +
                                   (1.0f - p->cfg.k_sl_min_ratio) * (1.0f - ratio));
    }
    return k_sl;
#endif
}

static inline void smo_pll_init(tPLL *pll, float kp, float ki)
{
    pll->theta = 0.0f;
    pll->vel = 0.0f;
    pll->kp = kp;
    pll->ki = ki;
}

void smo_init(tParameter *param, float ts)
{
    g_smo.rs = param->motor_rs;
    g_smo.ld = param->motor_ld;
    g_smo.lq = param->motor_lq;
    g_smo.psi_f = param->motor_psif;

    g_smo.cfg = SMO_DEFAULT_CFG;
    _SmoPrecompute(&g_smo, ts);
    // PLL 初始化: Kp=200, Ki=10000  @ dt 约 20us(SMO_DTICK=4)
    smo_pll_init(&g_smo.pll, 200.0f, 10000.0f);
    smo_reset();

    // 滤波器初始化
    filter_first_order_lag_init(&I_alpha_lpf, CURRENT_ALPHA, 0.0f);
    filter_first_order_lag_init(&I_beta_lpf, CURRENT_ALPHA, 0.0f);
    filter_first_order_lag_init(&emf_alpha_lpf, EMF_ALPHA, 0.0f);
    filter_first_order_lag_init(&emf_beta_lpf, EMF_ALPHA, 0.0f);
    filter_first_order_lag_init(&vel_lpf, VEL_ALPHA, 0.0f);
}

void smo_reset(void)
{
    g_smo.i_alpha_hat = g_smo.i_beta_hat = 0.0f;
    g_smo.e_alpha = g_smo.e_beta = 0.0f;
    g_smo.e_alpha_filt = g_smo.e_beta_filt = 0.0f;
    g_smo.theta_elec = g_smo.theta_prev = g_smo.vel_elec = 0.0f;
    g_smo.k_sl_curr = g_smo.cfg.k_sl_base;
    g_smo.pll.theta = 0;
    g_smo.pll.vel = 0;
}

void smo_set_config(tSMO_Config *cfg, float ts)
{
    if (cfg == NULL)
        return;
    g_smo.cfg = *cfg;
    _SmoPrecompute(&g_smo, ts);
}

void smo_main_loop(float v_alpha, float v_beta,
                   float i_alpha, float i_beta, float ts)
{
    // 空闲时刻对电流进行滤波

    if (g_smo.ts_tick++ < SMO_DTICK)
        return;
    g_smo.ts_tick = 0;

    // === 步骤 1：电流观测器 ===
#if SMO_USE_CURRENT_OBSERVER
    float i_err_alpha = i_alpha - g_smo.i_alpha_hat;
    float i_err_beta = i_beta - g_smo.i_beta_hat;
    i_err_alpha = CLAMP(i_err_alpha, -2.0f, 2.0f);
    i_err_beta = CLAMP(i_err_beta, -2.0f, 2.0f);

    float di_alpha = (v_alpha - g_smo.rs * g_smo.i_alpha_hat - g_smo.e_alpha) * g_smo.inv_l_eff;
    float di_beta = (v_beta - g_smo.rs * g_smo.i_beta_hat - g_smo.e_beta) * g_smo.inv_l_eff;

    float k_slm = g_smo.k_sl_curr * g_smo.inv_l_eff;
    di_alpha += k_slm * tanhf(i_err_alpha / g_smo.cfg.delta);
    di_beta += k_slm * tanhf(i_err_beta / g_smo.cfg.delta);

    g_smo.i_alpha_hat += di_alpha * ts;
    g_smo.i_beta_hat += di_beta * ts;
    g_smo.i_alpha_hat = CLAMP(g_smo.i_alpha_hat, -MAX_CURRENT, MAX_CURRENT);
    g_smo.i_beta_hat = CLAMP(g_smo.i_beta_hat, -MAX_CURRENT, MAX_CURRENT);

    g_smo.e_alpha = k_slm * tanhf(i_err_alpha / g_smo.cfg.delta);
    g_smo.e_beta = k_slm * tanhf(i_err_beta / g_smo.cfg.delta);
#else
    // 测试模式：直接用反馈电流估算反电动势
    g_smo.e_alpha = v_alpha - g_smo.rs * i_alpha;
    g_smo.e_beta = v_beta - g_smo.rs * i_beta;
    g_smo.e_alpha = CLAMP(g_smo.e_alpha, -g_smo.cfg.emf_max, g_smo.cfg.emf_max);
    g_smo.e_beta = CLAMP(g_smo.e_beta, -g_smo.cfg.emf_max, g_smo.cfg.emf_max);
    g_smo.i_alpha_hat = i_alpha;
    g_smo.i_beta_hat = i_beta;
#endif

    // === 步骤 2：自适应增益（基于电压模长，避免速度不准的影响） ===
    float vel_abs = FABSF(g_smo.vel_elec);
    g_smo.k_sl_curr = _CalcAdaptiveGain(&g_smo, vel_abs, v_alpha, v_beta);

    // === 步骤 3：反电动势滤波 ===

    g_smo.e_alpha_filt = filter_first_order_lag(&emf_alpha_lpf, g_smo.e_alpha);
    g_smo.e_beta_filt = filter_first_order_lag(&emf_beta_lpf, g_smo.e_beta);

    // === 步骤 4：角度计算（PLL vs atan2+平滑） ===
    float emf_mag_sq = g_smo.e_alpha_filt * g_smo.e_alpha_filt +
                       g_smo.e_beta_filt * g_smo.e_beta_filt;

#if SMO_USE_PLL
    if (emf_mag_sq > 0.01f)
    {
        // PLL: 角度速度联合估计，无附加LPF
        float theta_obs = atan2f(g_smo.e_beta_filt, g_smo.e_alpha_filt); // [-PI, PI] rad
        pll_update(&g_smo.pll, theta_obs, ts);
        g_smo.theta_elec = g_smo.pll.theta; // 直接 rad
        g_smo.vel_elec = g_smo.pll.vel;
    }
    else
    {
        g_smo.theta_elec = g_smo.theta_prev; // 保持上次值
        g_smo.vel_elec = 0;
    }
#else
    // 原方案：atan2 + 50%融合平滑（用于对比）
    if (emf_mag_sq > 0.01f)
    {
        float theta_new = atan2f(g_smo.e_beta_filt, g_smo.e_alpha_filt);
        float diff = normalize_angle_pi(theta_new - g_smo.theta_elec);
        g_smo.theta_elec += 0.5f * diff;
    }
    g_smo.theta_elec = normalize_angle_360(g_smo.theta_elec);

    float angle_diff = normalize_angle_pi(g_smo.theta_elec - g_smo.theta_prev);
    float speed_raw = angle_diff / g_smo.dt;
    g_smo.vel_elec = filter_first_order_lag(&vel_lpf, speed_raw);
    g_smo.vel_elec = CLAMP(g_smo.vel_elec, -g_smo.cfg.max_vel_elec, g_smo.cfg.max_vel_elec);
#endif

    g_smo.theta_prev = g_smo.theta_elec;
}
