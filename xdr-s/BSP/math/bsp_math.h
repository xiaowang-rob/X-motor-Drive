#ifndef BSP_MATH_H
#define BSP_MATH_H

#define MATH_BACKEND_DSP 1
#define MATH_BACKEND_CORDIC 2
#define MATH_BACKEND_LIBM 3

/* 在这里切换固件的数学后端。LIBM 可用于主机测试。 */
#ifndef MATH_BACKEND
#define MATH_BACKEND MATH_BACKEND_DSP
#endif

#include <math.h>
#include <stdint.h>

#if MATH_BACKEND == MATH_BACKEND_DSP || MATH_BACKEND == MATH_BACKEND_CORDIC
#include "arm_math.h"
#elif MATH_BACKEND != MATH_BACKEND_LIBM
#error "Invalid MATH_BACKEND"
#endif

#define MATH_PI 3.1415926535f
#define MATH_2PI 6.2831853f
#define MATH_PI_3 1.047197551f
#define MATH_IN2PI 0.1591549431f
#define MATH_SQRT3 1.732050807f
#define MATH_SQRT3_2 0.8660254035f
#define MATH_INSQRT3 0.5773502693f
#define MATH_INSQRT2 0.7071067812f
#define MATH_SQRT2 1.4142135624f
#define MATH_F180_PI 57.2957795147f

/* 有效指针、有限输入和数学定义域由调用者保证。 */
static inline float MATH_CLAMP(float x, float min, float max) {
    return x < min ? min : (x > max ? max : x);
}
static inline float MATH_FABSF(float x) { return __builtin_fabsf(x); }
static inline float MATH_FSIGN(float x) { return (x > 0.0f) - (x < 0.0f); }
static inline uint32_t MATH_FROUNDF(float x) { return (uint32_t)(x + 0.5f); }
static inline float MATH_MINF(float x, float y) { return x < y ? x : y; }
static inline float MATH_MAXF(float x, float y) { return x > y ? x : y; }
static inline float MATH_COPYSIGNF(float x, float y) { return copysignf(x, y); }
static inline float MATH_EXPF(float x) { return expf(x); }
static inline float MATH_TANHF(float x) { return tanhf(x); }
static inline float MATH_NORM2(float x, float y) { return x * x + y * y; }

/* 轨迹规划用双精度；沿用 C 数学函数的 F 后缀区分 float。 */
static inline double MATH_FABS(double x) { return fabs(x); }
static inline double MATH_MIN(double x, double y) { return x < y ? x : y; }
static inline double MATH_MAX(double x, double y) { return x > y ? x : y; }
static inline double MATH_CLAMPD(double x, double min, double max) {
    return x < min ? min : (x > max ? max : x);
}
static inline double MATH_COPYSIGN(double x, double y) { return copysign(x, y); }
static inline double MATH_SQRT(double x) { return sqrt(x); }
static inline double MATH_CBRT(double x) { return cbrt(x); }

/* [0, 2pi)。保持原先乘倒数取整的算法，常见范围直接加减。
 * 输入圈数须可表示为 int32_t；控制角度不要长期不归一化累积。 */
static inline float MATH_ANGLE_2PI(float angle) {
    if (angle >= 0.0f && angle < MATH_2PI)
        return angle;
    if (angle >= -MATH_2PI && angle < 0.0f) {
        float result = angle + MATH_2PI;
        return result < MATH_2PI ? result : 0.0f;
    }
    if (angle >= MATH_2PI && angle < 2.0f * MATH_2PI)
        return angle - MATH_2PI;
    float turns = angle * MATH_IN2PI;
    turns -= (float)(int32_t)turns;
    if (turns < 0.0f)
        turns += 1.0f;
    float result = turns * MATH_2PI;
    return result < MATH_2PI ? result : 0.0f;
}

/* [-pi, pi)。 */
static inline float MATH_ANGLE_PI(float angle) {
    if (angle >= -MATH_PI && angle < MATH_PI)
        return angle;
    float result = MATH_ANGLE_2PI(angle);
    return result >= MATH_PI ? result - MATH_2PI : result;
}

static inline float MATH_SQRTF(float x) {
#if MATH_BACKEND == MATH_BACKEND_DSP || MATH_BACKEND == MATH_BACKEND_CORDIC
    float r;
    arm_sqrt_f32(x, &r);
    return r;
#else
    return sqrtf(x);
#endif
}

#if MATH_BACKEND == MATH_BACKEND_CORDIC
float MATH_SIN(float x);
float MATH_COS(float x);
void MATH_SIN_COS(float angle, float *sin, float *cos);
float MATH_ATAN2(float y, float x);
#else
static inline float MATH_SIN(float x) {
#if MATH_BACKEND == MATH_BACKEND_DSP
    return arm_sin_f32(x);
#else
    return sinf(x);
#endif
}
static inline float MATH_COS(float x) {
#if MATH_BACKEND == MATH_BACKEND_DSP
    return arm_cos_f32(x);
#else
    return cosf(x);
#endif
}
static inline void MATH_SIN_COS(float angle, float *sin, float *cos) {
#if MATH_BACKEND == MATH_BACKEND_DSP
    arm_sin_cos_f32(angle * MATH_F180_PI, sin, cos);
#else
    *sin = sinf(angle);
    *cos = cosf(angle);
#endif
}
static inline float MATH_ATAN2(float y, float x) { return atan2f(y, x); }
#endif

static inline float MATH_HYPOT(float x, float y) { return MATH_SQRTF(MATH_NORM2(x, y)); }

#endif
