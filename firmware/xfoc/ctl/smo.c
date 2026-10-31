#include "smo.h"
#include "bsp_cfg.h"
#include "filter.h"
#include "xmath.h"

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

// 默认配置
static const tSMO_Config SMO_DEFAULT_CFG = {.k_sl_base = 15.0f,
                                            .k_sl_min_ratio = 0.5f,
                                            .vel_adapt_start = 100.0f,
                                            .vel_adapt_end = 400.0f,
                                            .delta = 0.1f,
                                            .min_vel_elec = 50.0f,
                                            .max_vel_elec = 2000.0f,
                                            .emf_max = 15.0f};

// 预计算不变量
static inline void _SmoPrecompute(tSMO *p, float ts) {
    float L_avg = (p->ld + p->lq) * 0.5f;
    p->inv_l_eff = 1.0f / (L_avg + p->rs * ts);
}

// 自适应滑模增益 — 基于电压模长（BEMF信号强度），vs 基于估计速度
// 基于电压: v_mag 越大说明信号越好，增益可以降低
// 避免了"速度不准→增益乱调"的鸡生蛋问题
static inline float _CalcAdaptiveGain(tSMO *p, float vel_abs, float v_alpha,
                                      float v_beta) {
#if SMO_GAIN_BY_DUTY
    float v_mag = XSQRTF(v_alpha * v_alpha + v_beta * v_beta);
    float v_ratio = v_mag / 12.0f; // 12V 为参考基准
    v_ratio = XCLAMP(v_ratio, 0.0f, 1.0f);
    // 电压高→信号好→增益降
    float k_sl =
        p->cfg.k_sl_base * (p->cfg.k_sl_min_ratio +
                            (1.0f - p->cfg.k_sl_min_ratio) * (1.0f - v_ratio));
    // 最低增益保障
    k_sl = (k_sl < p->cfg.k_sl_base * 0.1f) ? (p->cfg.k_sl_base * 0.1f) : k_sl;
    return k_sl;
#else
    // 原方案：基于速度（保留作为对比）
    float k_sl = p->cfg.k_sl_base;
    if (vel_abs > p->cfg.vel_adapt_start) {
        float ratio = (vel_abs - p->cfg.vel_adapt_start) /
                      (p->cfg.vel_adapt_end - p->cfg.vel_adapt_start);
        ratio = XCLAMP(ratio, 0.0f, 1.0f);
        k_sl = p->cfg.k_sl_base *
               (p->cfg.k_sl_min_ratio +
                (1.0f - p->cfg.k_sl_min_ratio) * (1.0f - ratio));
    }
    return k_sl;
#endif
}

static inline void smo_pll_init(tPLL *pll, float kp, float ki) {
    pll->theta = 0.0f;
    pll->vel = 0.0f;
    pll->kp = kp;
    pll->ki = ki;
}

void smo_init(tSMO *smo, tParameter *param, float ts) {
    smo->rs = param->motor_rs;
    smo->ld = param->motor_ld;
    smo->lq = param->motor_lq;
    smo->psi_f = param->motor_psif;

    smo->cfg = SMO_DEFAULT_CFG;
    _SmoPrecompute(smo, ts);
    // PLL 初始化: Kp=200, Ki=10000  @ dt 约 20us(SMO_DTICK=4)
    smo_pll_init(&smo->pll, 200.0f, 10000.0f);
    smo_reset(smo);

    // 滤波器初始化
    filter_first_order_lag_init(&I_alpha_lpf, CURRENT_ALPHA, 0.0f);
    filter_first_order_lag_init(&I_beta_lpf, CURRENT_ALPHA, 0.0f);
    filter_first_order_lag_init(&emf_alpha_lpf, EMF_ALPHA, 0.0f);
    filter_first_order_lag_init(&emf_beta_lpf, EMF_ALPHA, 0.0f);
    filter_first_order_lag_init(&vel_lpf, VEL_ALPHA, 0.0f);
}

void smo_reset(tSMO *smo) {
    smo->i_alpha_hat = smo->i_beta_hat = 0.0f;
    smo->e_alpha = smo->e_beta = 0.0f;
    smo->e_alpha_filt = smo->e_beta_filt = 0.0f;
    smo->theta_elec = smo->theta_prev = smo->vel_elec = 0.0f;
    smo->k_sl_curr = smo->cfg.k_sl_base;
    smo->pll.theta = 0;
    smo->pll.vel = 0;
}

void smo_set_config(tSMO *smo, tSMO_Config *cfg, float ts) {
    if (cfg == NULL)
        return;
    smo->cfg = *cfg;
    _SmoPrecompute(smo, ts);
}

void smo_main_loop(tSMO *smo, float v_alpha, float v_beta, float i_alpha,
                   float i_beta, float ts) {
    // 空闲时刻对电流进行滤波

    if (smo->ts_tick++ < SMO_DTICK)
        return;
    smo->ts_tick = 0;

    // === 步骤 1：电流观测器 ===
#if SMO_USE_CURRENT_OBSERVER
    float i_err_alpha = i_alpha - smo->i_alpha_hat;
    float i_err_beta = i_beta - smo->i_beta_hat;
    i_err_alpha = XCLAMP(i_err_alpha, -2.0f, 2.0f);
    i_err_beta = XCLAMP(i_err_beta, -2.0f, 2.0f);

    float di_alpha =
        (v_alpha - smo->rs * smo->i_alpha_hat - smo->e_alpha) * smo->inv_l_eff;
    float di_beta =
        (v_beta - smo->rs * smo->i_beta_hat - smo->e_beta) * smo->inv_l_eff;

    float k_slm = smo->k_sl_curr * smo->inv_l_eff;
    di_alpha += k_slm * tanhf(i_err_alpha / smo->cfg.delta);
    di_beta += k_slm * tanhf(i_err_beta / smo->cfg.delta);

    smo->i_alpha_hat += di_alpha * ts;
    smo->i_beta_hat += di_beta * ts;
    smo->i_alpha_hat = XCLAMP(smo->i_alpha_hat, -MAX_CURRENT, MAX_CURRENT);
    smo->i_beta_hat = XCLAMP(smo->i_beta_hat, -MAX_CURRENT, MAX_CURRENT);

    smo->e_alpha = k_slm * tanhf(i_err_alpha / smo->cfg.delta);
    smo->e_beta = k_slm * tanhf(i_err_beta / smo->cfg.delta);
#else
    // 测试模式：直接用反馈电流估算反电动势
    smo->e_alpha = v_alpha - smo->rs * i_alpha;
    smo->e_beta = v_beta - smo->rs * i_beta;
    smo->e_alpha = XCLAMP(smo->e_alpha, -smo->cfg.emf_max, smo->cfg.emf_max);
    smo->e_beta = XCLAMP(smo->e_beta, -smo->cfg.emf_max, smo->cfg.emf_max);
    smo->i_alpha_hat = i_alpha;
    smo->i_beta_hat = i_beta;
#endif

    // === 步骤 2：自适应增益（基于电压模长，避免速度不准的影响） ===
    float vel_abs = XFABSF(smo->vel_elec);
    smo->k_sl_curr = _CalcAdaptiveGain(smo, vel_abs, v_alpha, v_beta);

    // === 步骤 3：反电动势滤波 ===

    smo->e_alpha_filt = filter_first_order_lag(&emf_alpha_lpf, smo->e_alpha);
    smo->e_beta_filt = filter_first_order_lag(&emf_beta_lpf, smo->e_beta);

    // === 步骤 4：角度计算（PLL vs atan2+平滑） ===
    float emf_mag_sq = smo->e_alpha_filt * smo->e_alpha_filt +
                       smo->e_beta_filt * smo->e_beta_filt;

#if SMO_USE_PLL
    if (emf_mag_sq > 0.01f) {
        // PLL: 角度速度联合估计，无附加LPF
        float theta_obs =
            atan2f(smo->e_beta_filt, smo->e_alpha_filt); // [-PI, PI] rad
        pll_update(&smo->pll, theta_obs, ts);
        smo->theta_elec = smo->pll.theta; // 直接 rad
        smo->vel_elec = smo->pll.vel;
    } else {
        smo->theta_elec = smo->theta_prev; // 保持上次值
        smo->vel_elec = 0;
    }
#else
    // 原方案：atan2 + 50%融合平滑（用于对比）
    if (emf_mag_sq > 0.01f) {
        float theta_new = atan2f(smo->e_beta_filt, smo->e_alpha_filt);
        float diff = normalize_angle_pi(theta_new - smo->theta_elec);
        smo->theta_elec += 0.5f * diff;
    }
    smo->theta_elec = normalize_angle_360(smo->theta_elec);

    float angle_diff = normalize_angle_pi(smo->theta_elec - smo->theta_prev);
    float speed_raw = angle_diff / smo->dt;
    smo->vel_elec = filter_first_order_lag(&vel_lpf, speed_raw);
    smo->vel_elec =
        XCLAMP(smo->vel_elec, -smo->cfg.max_vel_elec, smo->cfg.max_vel_elec);
#endif

    smo->theta_prev = smo->theta_elec;
}
