#include "smo.h"
#include "math_fast.h"
#include "usr_config.h"
#include "filter.h"

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

static tSMO smo;

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
__STATIC_INLINE void _SmoPrecompute(tSMO *p)
{
    float L_avg = (p->ld + p->lq) * 0.5f;
    p->inv_l_eff = 1.0f / (L_avg + p->rs * p->dt);
}

// 自适应滑模增益 — 基于电压模长（BEMF信号强度），vs 基于估计速度
// 基于电压: v_mag 越大说明信号越好，增益可以降低
// 避免了"速度不准→增益乱调"的鸡生蛋问题
__STATIC_INLINE float _CalcAdaptiveGain(tSMO *p, float vel_abs, float v_alpha, float v_beta)
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

// ===== PLL 角度跟踪 =====
// 归一化到 [-PI, PI]
static inline float _norm_rad_pi(float a)
{
    while (a > 3.14159265f)
        a -= 6.2831853f;
    while (a < -3.14159265f)
        a += 6.2831853f;
    return a;
}

void smo_pll_init(tSmoPll *pll, float kp, float ki, float dt)
{
    pll->theta_pll = 0.0f;
    pll->vel_pll = 0.0f;
    pll->kp = kp;
    pll->ki = ki;
    pll->dt = dt;
}

void smo_pll_update(tSmoPll *pll, float theta_obs_rad)
{
    // Type-1 PLL: 角度误差 → 速度积分 + 比例修正
    float delta = _norm_rad_pi(theta_obs_rad - pll->theta_pll);

    // 比例 + 积分
    pll->theta_pll += (pll->vel_pll + pll->kp * delta) * pll->dt;
    pll->vel_pll += pll->ki * delta * pll->dt;

    // 归一化输出角度
    while (pll->theta_pll > 6.2831853f)
        pll->theta_pll -= 6.2831853f;
    while (pll->theta_pll < 0.0f)
        pll->theta_pll += 6.2831853f;
}

void smo_init(tMotor *motor)
{
    smo.rs = motor->rs;
    smo.ld = motor->ld;
    smo.lq = motor->lq;
    smo.psi_f = motor->psi_f;
    smo.dt = T_CON * SMO_DTICK;

    smo.cfg = SMO_DEFAULT_CFG;
    _SmoPrecompute(&smo);
    // PLL 初始化: Kp=200, Ki=10000  @ dt 约 20us(SMO_DTICK=4)
    smo_pll_init(&smo.pll, 200.0f, 10000.0f, smo.dt);
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
    smo.i_alpha_hat = smo.i_beta_hat = 0.0f;
    smo.e_alpha = smo.e_beta = 0.0f;
    smo.e_alpha_filt = smo.e_beta_filt = 0.0f;
    smo.theta_elec = smo.theta_prev = smo.vel_elec = 0.0f;
    smo.k_sl_curr = smo.cfg.k_sl_base;
    smo.pll.theta_pll = 0;
    smo.pll.vel_pll = 0;
}

void smo_set_config(tSMO_Config *cfg)
{
    if (cfg == NULL)
        return;
    smo.cfg = *cfg;
    _SmoPrecompute(&smo);
}

void smo_main_loop(float v_alpha, float v_beta,
                  float i_alpha, float i_beta)
{
    // 空闲时刻对电流进行滤波

    if (smo.ts_tick++ < SMO_DTICK)
        return;
    smo.ts_tick = 0;

    // === 步骤 1：电流观测器 ===
#if SMO_USE_CURRENT_OBSERVER
    float i_err_alpha = i_alpha - smo.i_alpha_hat;
    float i_err_beta = i_beta - smo.i_beta_hat;
    i_err_alpha = CLAMP(i_err_alpha, -2.0f, 2.0f);
    i_err_beta = CLAMP(i_err_beta, -2.0f, 2.0f);

    float di_alpha = (v_alpha - smo.rs * smo.i_alpha_hat - smo.e_alpha) * smo.inv_l_eff;
    float di_beta = (v_beta - smo.rs * smo.i_beta_hat - smo.e_beta) * smo.inv_l_eff;

    float k_slm = smo.k_sl_curr * smo.inv_l_eff;
    di_alpha += k_slm * tanhf(i_err_alpha / smo.cfg.delta);
    di_beta += k_slm * tanhf(i_err_beta / smo.cfg.delta);

    smo.i_alpha_hat += di_alpha * smo.dt;
    smo.i_beta_hat += di_beta * smo.dt;
    smo.i_alpha_hat = CLAMP(smo.i_alpha_hat, -MAX_CURRENT, MAX_CURRENT);
    smo.i_beta_hat = CLAMP(smo.i_beta_hat, -MAX_CURRENT, MAX_CURRENT);

    smo.e_alpha = k_slm * tanhf(i_err_alpha / smo.cfg.delta);
    smo.e_beta = k_slm * tanhf(i_err_beta / smo.cfg.delta);
#else
    // 测试模式：直接用反馈电流估算反电动势
    smo.e_alpha = v_alpha - smo.rs * i_alpha;
    smo.e_beta = v_beta - smo.rs * i_beta;
    smo.e_alpha = CLAMP(smo.e_alpha, -smo.cfg.emf_max, smo.cfg.emf_max);
    smo.e_beta = CLAMP(smo.e_beta, -smo.cfg.emf_max, smo.cfg.emf_max);
    smo.i_alpha_hat = i_alpha;
    smo.i_beta_hat = i_beta;
#endif

    // === 步骤 2：自适应增益（基于电压模长，避免速度不准的影响） ===
    float vel_abs = FABSF(smo.vel_elec);
    smo.k_sl_curr = _CalcAdaptiveGain(&smo, vel_abs, v_alpha, v_beta);

    // === 步骤 3：反电动势滤波 ===

    smo.e_alpha_filt = filter_first_order_lag(&emf_alpha_lpf, smo.e_alpha);
    smo.e_beta_filt = filter_first_order_lag(&emf_beta_lpf, smo.e_beta);

    // === 步骤 4：角度计算（PLL vs atan2+平滑） ===
    float emf_mag_sq = smo.e_alpha_filt * smo.e_alpha_filt +
                       smo.e_beta_filt * smo.e_beta_filt;

#if SMO_USE_PLL
    if (emf_mag_sq > 0.01f)
    {
        // PLL: 角度速度联合估计，无附加LPF
        float theta_obs = atan2f(smo.e_beta_filt, smo.e_alpha_filt); // [-PI, PI] rad
        smo_pll_update(&smo.pll, theta_obs);
        smo.theta_elec = smo.pll.theta_pll; // 直接 rad
        smo.vel_elec = smo.pll.vel_pll;
    }
    else
    {
         smo.theta_elec = smo.theta_prev; // 保持上次值
        smo.vel_elec = 0;
    }
#else
    // 原方案：atan2 + 50%融合平滑（用于对比）
    if (emf_mag_sq > 0.01f)
    {
        float theta_new = atan2f(smo.e_beta_filt, smo.e_alpha_filt);
        float diff = normalize_angle_pi(theta_new - smo.theta_elec);
        smo.theta_elec += 0.5f * diff;
    }
    smo.theta_elec = normalize_angle_360(smo.theta_elec);

    float angle_diff = normalize_angle_pi(smo.theta_elec - smo.theta_prev);
    float speed_raw = angle_diff / smo.dt;
    smo.vel_elec = filter_first_order_lag(&vel_lpf, speed_raw);
    smo.vel_elec = CLAMP(smo.vel_elec, -smo.cfg.max_vel_elec, smo.cfg.max_vel_elec);
#endif

    smo.theta_prev = smo.theta_elec;
}

// 数据获取
float smo_get_theta(void) { return smo.theta_elec; }
float smo_get_vel(void) { return smo.vel_elec; }
