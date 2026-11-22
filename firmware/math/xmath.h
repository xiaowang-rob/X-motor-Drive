#ifndef __XMATH_H
#define __XMATH_H

#include <math.h>
#include <stdint.h>

#if (defined(MATH_LIB_DSP) + defined(MATH_LIB_CORDIC) + defined(MATH_LIB_LIBM)) > 1
#error "Select one math backend"
#endif
#if !defined(MATH_LIB_DSP) && !defined(MATH_LIB_CORDIC) && !defined(MATH_LIB_LIBM)
#define MATH_LIB_DSP
#endif
#if defined(MATH_LIB_DSP) || defined(MATH_LIB_CORDIC)
#include "arm_math.h"
#endif

#define XMATH_PI 3.1415926535f
#define XMATH_2PI 6.2831853f
#define XMATH_PI_3 1.047197551f
#define XMATH_IN2PI 0.1591549431f
#define XMATH_SQRT3 1.732050807f
#define XMATH_SQRT3_2 0.8660254035f
#define XMATH_INSQRT3 0.5773502693f
#define XMATH_INSQRT2 0.7071067812f
#define XMATH_SQRT2 1.4142135624f
#define XMATH_F180_PI 57.2957795147f

/* 有效指针、有限输入和数学定义域由调用者保证。 */
static inline float XCLAMP(float x, float min, float max) {
    return x < min ? min : (x > max ? max : x);
}
static inline float XFABSF(float x) { return __builtin_fabsf(x); }
static inline float XFSIGN(float x) { return (x > 0.0f) - (x < 0.0f); }
static inline uint32_t XFROUNDF(float x) { return (uint32_t)(x + 0.5f); }
static inline float XMINF(float x, float y) { return x < y ? x : y; }
static inline float XMAXF(float x, float y) { return x > y ? x : y; }
static inline float XCOPYSIGNF(float x, float y) { return copysignf(x, y); }
static inline float XEXPF(float x) { return expf(x); }
static inline float XTANHF(float x) { return tanhf(x); }
static inline float XNORM2(float x, float y) { return x * x + y * y; }

/* 轨迹规划用双精度；沿用 C 数学函数的 F 后缀区分 float。 */
static inline double XFABS(double x) { return fabs(x); }
static inline double XMIN(double x, double y) { return x < y ? x : y; }
static inline double XMAX(double x, double y) { return x > y ? x : y; }
static inline double XCLAMPD(double x, double min, double max) {
    return x < min ? min : (x > max ? max : x);
}
static inline double XCOPYSIGN(double x, double y) { return copysign(x, y); }
static inline double XSQRT(double x) { return sqrt(x); }
static inline double XCBRT(double x) { return cbrt(x); }

/* [0, 2pi)。保持原先乘倒数取整的算法，常见范围直接加减。
 * 输入圈数须可表示为 int32_t；控制角度不要长期不归一化累积。 */
static inline float XANGLE_2PI(float angle) {
    if (angle >= 0.0f && angle < XMATH_2PI)
        return angle;
    if (angle >= -XMATH_2PI && angle < 0.0f) {
        float result = angle + XMATH_2PI;
        return result < XMATH_2PI ? result : 0.0f;
    }
    if (angle >= XMATH_2PI && angle < 2.0f * XMATH_2PI)
        return angle - XMATH_2PI;
    float turns = angle * XMATH_IN2PI;
    turns -= (float)(int32_t)turns;
    if (turns < 0.0f)
        turns += 1.0f;
    float result = turns * XMATH_2PI;
    return result < XMATH_2PI ? result : 0.0f;
}

/* [-pi, pi)。 */
static inline float XANGLE_PI(float angle) {
    if (angle >= -XMATH_PI && angle < XMATH_PI)
        return angle;
    float result = XANGLE_2PI(angle);
    return result >= XMATH_PI ? result - XMATH_2PI : result;
}

static inline float XSQRTF(float x) {
#if defined(MATH_LIB_DSP) || defined(MATH_LIB_CORDIC)
    float r;
    arm_sqrt_f32(x, &r);
    return r;
#else
    return sqrtf(x);
#endif
}

#ifdef MATH_LIB_CORDIC
float XSIN(float x);
float XCOS(float x);
void XSIN_COS(float angle, float *sin, float *cos);
float XATAN2(float y, float x);
#else
static inline float XSIN(float x) {
#ifdef MATH_LIB_DSP
    return arm_sin_f32(x);
#else
    return sinf(x);
#endif
}
static inline float XCOS(float x) {
#ifdef MATH_LIB_DSP
    return arm_cos_f32(x);
#else
    return cosf(x);
#endif
}
static inline void XSIN_COS(float angle, float *sin, float *cos) {
#ifdef MATH_LIB_DSP
    arm_sin_cos_f32(angle * XMATH_F180_PI, sin, cos);
#else
    *sin = sinf(angle);
    *cos = cosf(angle);
#endif
}
static inline float XATAN2(float y, float x) { return atan2f(y, x); }
#endif

static inline float XHYPOT(float x, float y) { return XSQRTF(XNORM2(x, y)); }

#endif
