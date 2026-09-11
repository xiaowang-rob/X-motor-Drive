// 高频注入(HFI)无感位置估算
// 基于αβ轴脉振方波注入 + PLL跟踪
//          全程角度制 (degree)，纯电气角度
#include "hfi.h"
#include "filter.h"
#include "math_fast.h"
#include "usr_config.h"

// ---------- 速度 LPF（2nd Butterworth fc=120Hz fs=20kHz，由 tools/filter_coeffs.py 生成） ----------

#define LPF_W_B0 0.0003460413
#define LPF_W_B1 0.0006920827
#define LPF_W_B2 0.0003460413
#define LPF_W_A1 1.9466975408
#define LPF_W_A2 -0.9480817061

#define HFI_INJ_VOLT_AMP 2.0f      // 高频注入电压幅值 (V)
#define HFI_INJ_FREQ_HZ 5000.0f    // 高频注入频率 (Hz)
#define HFI_PLL_BANDWIDTH_HZ 60.0f // HFI-PLL 带宽 (Hz)
#define SPEED_LPF_FACTOR 2.0f      // 速度 LPF 截止频率系数

// ================ 全局/静态变量 =================
tHFI_Handle g_hfi;

tBW_FilterInstance speed_lpf_inst;
tFirstOrderLagFilter speed_lpf;

// 巴特沃斯低通滤波器系数 (自动生成)
float32_t lpf_w_coeffs[5] = {LPF_W_B0, LPF_W_B1, LPF_W_B2, LPF_W_A1, LPF_W_A2};

// HFI模块初始化
void hfi_init()
{
    memset(&g_hfi, 0, sizeof(tHFI_Handle));
    g_hfi.inj_signal = 1;

    filter_butterworth_init(&speed_lpf_inst, (float *)lpf_w_coeffs);
    filter_first_order_lag_init(&speed_lpf, 0.01f, 0.0f);
}

// HFI核心步进 (每控制周期调用)
// ialpha, ibeta  αβ电流 [A]
// u_alpha_h, u_beta_h  高频注入电压αβ分量 [V]
//
// 流程：高频电流提取 → 位置误差解耦 → PLL → 角度更新 → 注入电压生成

volatile float Hfi_Kp = 2 * 1.0f * 200;
volatile float Hfi_Ki = 5000 * T_CON;

void hfi_step(float ialpha, float ibeta, float *u_alpha_h, float *u_beta_h)
{
    // 1. 高频电流提取 (二阶差分 + 注入极性解调)
    g_hfi.ialpha_h[0] = (ialpha - g_hfi.ialpha_z[0] * 2 + g_hfi.ialpha_z[1]) / 4;
    g_hfi.ibeta_h[0] = (ibeta - g_hfi.ibeta_z[0] * 2 + g_hfi.ibeta_z[1]) / 4;

    g_hfi.ialpha_z[1] = g_hfi.ialpha_z[0];
    g_hfi.ialpha_z[0] = ialpha;

    g_hfi.ibeta_z[1] = g_hfi.ibeta_z[0];
    g_hfi.ibeta_z[0] = ibeta;

    g_hfi.i_hf_alpha = (g_hfi.ialpha_h[0] - g_hfi.ialpha_h[1]) * g_hfi.inj_signal;
    g_hfi.i_hf_beta = (g_hfi.ibeta_h[0] - g_hfi.ibeta_h[1]) * g_hfi.inj_signal;

    g_hfi.ialpha_h[1] = g_hfi.ialpha_h[0];
    g_hfi.ibeta_h[1] = g_hfi.ibeta_h[0];

    // 2. 位置误差解耦 (矢量叉乘)
    float sin_theta, cos_theta;
    arm_sin_cos_rad_f32(g_hfi.theta_e, &sin_theta, &cos_theta);

    float pll_error = g_hfi.i_hf_alpha * sin_theta -
                      g_hfi.i_hf_beta * cos_theta;

    g_hfi.pll_error = g_hfi.pll_error * 0.9f + pll_error * 0.1f;
    // g_hfi.pll_error = g_hfi.i_hf_alpha * sin_theta -
    //                   g_hfi.i_hf_beta * cos_theta;

    // 3. PLL跟踪 (PI控制器)
    float pll_prop = g_hfi.pll_error * Hfi_Kp;
    g_hfi.pll_integrator += g_hfi.pll_error * Hfi_Ki;

    g_hfi.vel_e = pll_prop + g_hfi.pll_integrator;

    // 4. 速度滤波
    g_hfi.vel_filtered = filter_butterworth_process(&speed_lpf_inst, g_hfi.vel_e);

    // 5. 角度更新 (积分 + 归一化)
    g_hfi.theta_e += g_hfi.vel_filtered * T_CON;
    // g_hfi.theta_e += g_hfi.vel_e * T_CON;
    g_hfi.theta_e = normalize_angle_360(g_hfi.theta_e);

    // 6. 更新方波注入信号 (+1/-1 交替)
    g_hfi.inj_count++;
    if (g_hfi.inj_count > 1)
    {
        g_hfi.inj_count = 0;
        g_hfi.inj_signal = -g_hfi.inj_signal;
    }

    // 7. 生成高频注入电压 (估计d轴方波 → αβ)
    float ud_inj = g_hfi.inj_signal * HFI_INJ_VOLT_AMP;
    *u_alpha_h = ud_inj * cos_theta;
    *u_beta_h = ud_inj * sin_theta;
}

// 初始位置辨识 (磁饱和极性判断)
// id       d轴电流 [A]
// ualpha, ubeta  αβ轴输出电压 [V]
// 通过正反向d轴脉冲比较电流幅值，消除180°模糊
static u16 detect_timer = 0;
void hfi_detect_initial_position(float id, float *ualpha, float *ubeta)
{
    if (g_hfi.init_flag)
        return;

    float ud_ref;
    float sin_angle, cos_angle;
    arm_sin_cos_rad_f32(g_hfi.theta_e, &sin_angle, &cos_angle);

    detect_timer++;
    g_hfi.id_h = (id - 2 * g_hfi.id_z[0] + g_hfi.id_z[1]) / 4;
    g_hfi.id_z[1] = g_hfi.id_z[0];
    g_hfi.id_z[0] = id;

    if (detect_timer < 400)
    {
        ud_ref = 0.0f;
    }
    else if (detect_timer < 600)
    {
        ud_ref = HFI_INIT_VOLT; // 正脉冲
    }
    else if (detect_timer < 610)
    {
        ud_ref = HFI_INIT_VOLT;
        g_hfi.init_curr_pos += FABSF(g_hfi.id_h);
    }
    else if (detect_timer < 1000)
    {
        ud_ref = 0.0f;
    }
    else if (detect_timer < 1200)
    {
        ud_ref = -HFI_INIT_VOLT; // 负脉冲
    }
    else if (detect_timer < 1210)
    {
        ud_ref = -HFI_INIT_VOLT;
        g_hfi.init_curr_neg += FABSF(g_hfi.id_h);
    }
    else // 判决：电流大的一侧为N极
    {
        ud_ref = 0.0f;
        if (g_hfi.init_curr_pos < g_hfi.init_curr_neg)
        {
            g_hfi.theta_e += MATH_PI;
            g_hfi.theta_e = normalize_angle_360(g_hfi.theta_e);
        }
        g_hfi.init_flag = true;
    }

    inv_park_transform(ud_ref, 0.0f, sin_angle, cos_angle, ualpha, ubeta);
}

bool hfi_get_status(void)
{
    return g_hfi.init_flag;
}

void hfi_reset_initial_position(void)
{
    memset(&g_hfi, 0, sizeof(tHFI_Handle));
    g_hfi.inj_signal = 1;
    filter_butterworth_reset(&speed_lpf_inst);
}

float hfi_get_vel_elec(void)
{
    return g_hfi.vel_filtered;
}

float hfi_get_theta_elec(void)
{
    return g_hfi.theta_e;
}