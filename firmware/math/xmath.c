#include "xmath.h"

#ifdef MATH_LIB_CORDIC

#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_cordic.h"

#define XMATH_Q31 2147483648.0f /* 2^31 */
#define XMATH_Q31_INV (1.0f / XMATH_Q31)
#define XMATH_Q31_OVER_PI (XMATH_Q31 / XMATH_PI) /* 弧度 → Q31 */
#define XMATH_PI_OVER_Q31 (XMATH_PI / XMATH_Q31) /* Q31 → 弧度 */
#define XMATH_CORDIC_PREC 6U                     /* 24 次迭代，最高精度档 */
#define XMATH_CORDIC_TMO 10000U

static inline void cordic_clock_on(void) {
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_CORDIC);
}

/* 等结果就绪，成功返回 1，超时返回 0 */
static inline int cordic_wait_rrdy(void) {
    uint32_t t = XMATH_CORDIC_TMO;
    while (!(CORDIC->CSR & CORDIC_CSR_RRDY)) {
        if (--t == 0U)
            return 0;
    }
    return 1;
}

/* ---------- SIN ---------- */
float XSIN(float x) {
    cordic_clock_on();
    float th = XANGLE_PI(x); /* [-π, π) */
    int32_t q = (int32_t)(th * XMATH_Q31_OVER_PI);

    CORDIC->CSR =
        1U                                                 /* FUNC=Sine */
        | (XMATH_CORDIC_PREC << CORDIC_CSR_PRECISION_Pos); /* NARGS=0: 1 arg */

    CORDIC->WDATA = (uint32_t)q;
    if (!cordic_wait_rrdy())
        return arm_sin_f32(x);
    return (int32_t)CORDIC->RDATA * XMATH_Q31_INV;
}

/* ---------- COS ---------- */
float XCOS(float x) {
    cordic_clock_on();
    float th = XANGLE_PI(x);
    int32_t q = (int32_t)(th * XMATH_Q31_OVER_PI);

    CORDIC->CSR = 0U /* FUNC=Cosine */
                  | (XMATH_CORDIC_PREC << CORDIC_CSR_PRECISION_Pos);

    CORDIC->WDATA = (uint32_t)q;
    if (!cordic_wait_rrdy())
        return arm_cos_f32(x);
    return (int32_t)CORDIC->RDATA * XMATH_Q31_INV;
}

/* ---------- SIN + COS（流水线，两次请求一次读完） ---------- */
void XSIN_COS(float theta_rad, float *pSin, float *pCos) {
    cordic_clock_on();
    float th = XANGLE_PI(theta_rad);
    int32_t q = (int32_t)(th * XMATH_Q31_OVER_PI);

    /* 先发 Cosine，再发 Sine，硬件流水线可重叠 */
    CORDIC->CSR = 0U | (XMATH_CORDIC_PREC << CORDIC_CSR_PRECISION_Pos);
    CORDIC->WDATA = (uint32_t)q;
    CORDIC->CSR = 1U | (XMATH_CORDIC_PREC << CORDIC_CSR_PRECISION_Pos);
    CORDIC->WDATA = (uint32_t)q;

    if (!cordic_wait_rrdy()) {
        *pSin = arm_sin_f32(theta_rad);
        *pCos = arm_cos_f32(theta_rad);
        return;
    }
    *pCos = (int32_t)CORDIC->RDATA * XMATH_Q31_INV;

    if (!cordic_wait_rrdy()) {
        *pSin = arm_sin_f32(theta_rad);
        return;
    }
    *pSin = (int32_t)CORDIC->RDATA * XMATH_Q31_INV;
}

/* ---------- ATAN2 ---------- */
float XATAN2(float y, float x) {
    cordic_clock_on();
    float scale = fmaxf(fabsf(x), fabsf(y));
    if (scale < 1e-20f)
        return 0.0f;

    /* 用 2^30 而非 2^31：归一化后 |x|/scale 可达 1，乘 2^31 会溢出，
     * 乘 2^30 等价把输入限制在 Q31 的 ±0.5 区间，atan2 对比例不敏感 */
    int32_t qx = (int32_t)((x / scale) * 1073741824.0f);
    int32_t qy = (int32_t)((y / scale) * 1073741824.0f);

    CORDIC->CSR = 2U /* FUNC=Phase */
                  | (XMATH_CORDIC_PREC << CORDIC_CSR_PRECISION_Pos) |
                  CORDIC_CSR_NARGS; /* 2 args */

    CORDIC->WDATA = (uint32_t)qx;
    CORDIC->WDATA = (uint32_t)qy;

    if (!cordic_wait_rrdy())
        return atan2f(y, x);
    return (int32_t)CORDIC->RDATA * XMATH_PI_OVER_Q31; /* [-π, π] */
}

/* ---------- SQRT：保持软件实现 ---------- */
float XSQRTF(float x) {
    float r;
    arm_sqrt_f32(x, &r);
    return r;
}

/* ---------- HYPOT：目前用软件，见下文说明 ---------- */
float XHYPOT(float x, float y) {
    float r;
    arm_sqrt_f32(x * x + y * y, &r);
    return r;
}

#endif
