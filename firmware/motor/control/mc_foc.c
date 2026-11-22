
#include "mc_foc.h"
#include "xmath.h"

// Clark 变换 (abc → αβ)(等幅值)
// ia, ib, ic: 三相电流或电压; alpha, beta: 输出的 αβ 轴分量
static inline void clarke_transform(float ia, float ib, float ic, float *alpha,
                                    float *beta) {
    // 使用幅值不变变换（系数 2/3）
    // 简化电流 ia+ib+ic=0
    *alpha = ia;
    *beta = XMATH_INSQRT3 * (ib - ic);
}
// Clark 反变换 (αβ → abc)(等幅值)
// alpha, beta: αβ 轴分量; ia, ib, ic: 输出的三相值
static inline void inv_clarke_transform(float alpha, float beta, float *ia,
                                        float *ib, float *ic) {
    *ia = alpha;
    *ib = -0.5f * alpha + XMATH_SQRT3_2 * beta;
    *ic = -0.5f * alpha - XMATH_SQRT3_2 * beta;
}
// Park 变换 (αβ → dq)
// alpha, beta: αβ 轴分量; sin/cos: 电角度; d, q: 输出的 dq 轴分量
static inline void park_transform(float alpha, float beta, float sin_angle,
                                  float cos_angle, float *d, float *q) {
    *d = alpha * cos_angle + beta * sin_angle;
    *q = -alpha * sin_angle + beta * cos_angle;
}
// Park 反变换 (dq → αβ)
// d, q: dq 轴分量; sin/cos: 电角度; alpha, beta: 输出的 αβ 轴分量
static inline void inv_park_transform(float d, float q, float sin_angle,
                                      float cos_angle, float *alpha,
                                      float *beta) {
    *alpha = d * cos_angle - q * sin_angle;
    *beta = d * sin_angle + q * cos_angle;
}

// 输入 alpha/beta轴的 占空比 输出 三相占空比
static inline void svpwm_update(uint8_t sector, float da, float db, float *Du,
                                float *Dv, float *Dw) {

    // 反clark变换
    float duty_u, duty_v, duty_w;
    inv_clarke_transform(da, db, &duty_u, &duty_v, &duty_w);

    // 求最大最小调制 查表简化计算
    float Dmax, Dmin;
    switch (sector) {
    case 1:
        Dmax = duty_u;
        Dmin = duty_w;
        break;
    case 2:
        Dmax = duty_v;
        Dmin = duty_w;
        break;
    case 3:
        Dmax = duty_v;
        Dmin = duty_u;
        break;
    case 4:
        Dmax = duty_w;
        Dmin = duty_u;
        break;
    case 5:
        Dmax = duty_w;
        Dmin = duty_v;
        break;
    case 6:
        Dmax = duty_u;
        Dmin = duty_v;
        break;
    default:
        Dmax = 0.0f;
        Dmin = 0.0f;
        break; // 零矢量/异常
    }

    // 零序注入
    float Doffset = -0.5f * (Dmax + Dmin);

    *Du = XCLAMP(duty_u + 0.5f + Doffset, 0.0f, 1.0f);
    *Dv = XCLAMP(duty_v + 0.5f + Doffset, 0.0f, 1.0f);
    *Dw = XCLAMP(duty_w + 0.5f + Doffset, 0.0f, 1.0f);
}

void mc_foc_init(tMcFoc *foc, tParameter *param, float ts) {
    pi_init(&foc->PI_iq, param->qclkp, param->qclki, 1.0f, ts);
    pi_init(&foc->PI_id, param->dclkp, param->dclki, 1.0f, ts);

    pi_init(
        &foc->PI_fw, param->vlkp, param->vlki, param->motor_rated_current, ts);

    filter_first_order_lag_init(&foc->cf_u, param->cfalpha, 0);
    filter_first_order_lag_init(&foc->cf_v, param->cfalpha, 0);
    filter_first_order_lag_init(&foc->cf_w, param->cfalpha, 0);
}

void mc_foc_reset(tMcFoc *foc) {
    filter_first_order_lag_reset(&foc->cf_u, 0);
    filter_first_order_lag_reset(&foc->cf_v, 0);
    filter_first_order_lag_reset(&foc->cf_w, 0);
    pi_reset(&foc->PI_fw);
    pi_reset(&foc->PI_iq);
    pi_reset(&foc->PI_id);
}
// FOC 电流重构处理
void mc_foc_current_process(tMcFoc *foc, float is_in[3]) {
    if (foc->ref.sec == 1 || foc->ref.sec == 6) { // 最短相=W
        foc->fb.is[0] = is_in[1] + is_in[2];
        foc->fb.is[1] = -is_in[1];
        foc->fb.is[2] = -is_in[2];
    } else if (foc->ref.sec == 2 || foc->ref.sec == 3) { // 最短相=U
        foc->fb.is[0] = -is_in[0];
        foc->fb.is[1] = is_in[0] + is_in[2];
        foc->fb.is[2] = -is_in[2];
    } else if (foc->ref.sec == 4 || foc->ref.sec == 5) { // 最短相=V
        foc->fb.is[0] = -is_in[0];
        foc->fb.is[1] = -is_in[1];
        foc->fb.is[2] = is_in[0] + is_in[1];
    } else { // foc->ref.sec 0/7: 零矢量
        foc->fb.is[0] = is_in[0];
        foc->fb.is[1] = is_in[1];
        foc->fb.is[2] = is_in[2];
    }

    foc->fb.is[0] = filter_first_order_lag(&foc->cf_u, foc->fb.is[0]);
    foc->fb.is[1] = filter_first_order_lag(&foc->cf_v, foc->fb.is[1]);
    foc->fb.is[2] = filter_first_order_lag(&foc->cf_w, foc->fb.is[2]);

    // Clarke 变换
    clarke_transform(
        foc->fb.is[0], foc->fb.is[1], foc->fb.is[2], &foc->fb.ia, &foc->fb.ib);
    // Park 变换
    park_transform(foc->fb.ia,
                   foc->fb.ib,
                   foc->ref.sin_e,
                   foc->ref.cos_e,
                   &foc->fb.id,
                   &foc->fb.iq);
}

// foc 运行 处理电角度 和 电流环 更新ud uq
void mc_foc_update(tMcFoc *foc, float ts, float fb_theta_e) {
    if (foc->to_enable) {
        foc->ref.theta_e += foc->ref.vel_elec * ts;
        foc->ref.theta_e = XANGLE_2PI(foc->ref.theta_e);
    } else {
        foc->ref.theta_e = fb_theta_e;
    }
    foc->ref.sec = (foc->ref.theta_e / XMATH_PI_3) + 0.5f;

    // 预计算sin cos
    XSIN_COS(foc->ref.theta_e, &foc->ref.sin_e, &foc->ref.cos_e);

    if (foc->cl_enable) {
        foc->out.duty_d = pi_update(&foc->PI_id, foc->ref.id, foc->fb.id);
        foc->out.duty_q = pi_update(&foc->PI_iq, foc->ref.iq, foc->fb.iq);
    }
}

void mc_foc_svpwm_output(tMcFoc *foc) {
    inv_park_transform(foc->out.duty_d,
                       foc->out.duty_q,
                       foc->ref.sin_e,
                       foc->ref.cos_e,
                       &foc->out.duty_a,
                       &foc->out.duty_b);
    // 前面uduq输出是按照 1占空比 对 Vsmax 最大相电压算的 后面得转化为
    // 对母线电压，因为svpwm 调制是对 三相逆变电路而言的
    foc->out.duty_a *= XMATH_INSQRT3;
    foc->out.duty_b *= XMATH_INSQRT3;
    svpwm_update(foc->ref.sec,
                 foc->out.duty_a,
                 foc->out.duty_b,
                 &foc->out.dutyU,
                 &foc->out.dutyV,
                 &foc->out.dutyW);
}
