#ifndef __MATH_FAST_H
#define __MATH_FAST_H

#include <stdint.h>

// 加载目标芯片要使用的数学库头文件
#include "arm_math.h"
#include "math.h"

// ============================================================
// math_fast.h — 数学工具集中入口
//
// 约定：应用侧（abs / drv / utils）统一从这里取数学函数，
//       不直接调用 fabsf / sqrtf / fmodf 等裸库函数，
//       以便集中替换实现、控制精度与开销。
//       （校验类工具见 crc.h）
// ============================================================

//  数学常量定义
#define MATH_PI 3.1415926535f
#define MATH_2PI 6.2831853f
#define MATH_INV_2PI 0.1591549431f // 1 / (2π)
#define MATH_SQRT3 1.732050807f
#define MATH_SQRT3_2 0.8660254035f
#define MATH_INSQRT3 0.5773502693f
#define MATH_INSQRT2 0.7071067812f
#define F180_PI 57.2957795147f

// 内联函数--小函数 经常调用 -- 牺牲flash 提高代码调用效率

// 限幅
static inline float CLAMP(float val, float min, float max)
{
    return (val < min) ? min : ((val > max) ? max : val);
}
// 快速绝对值
static inline float FABSF(float x)
{
    return __builtin_fabsf(x);
}
// 快速开方（CMSIS-DSP 单精度）
static inline float SQRTF(float x)
{
    float r = 0.0f;
    arm_sqrt_f32(x, &r);
    return r;
}
// 快速符号函数
static inline float FSIGN(float x)
{
    return (x > 0.0f) - (x < 0.0f); // 分支消除
}
// 快速浮点数四舍五入
static inline uint32_t FROUNDF(float x)
{
    return (uint32_t)(x + 0.5f);
}

// 将角度标准化到 [0, 2π) 范围
//
// 性能：不用 fmodf —— 后者在 Cortex-M4 上是库调用（几十~上百周期），
// 而本函数常出现在 PLL / 电角度归一这类高频路径上。
// 改用"常量倒数 + 取整"：1 次乘法 + 1 次浮点转整数 + 1~2 次加法。
// 适用前提：|angle| < 2π·2^31（远超任何实际角度）。
static inline float normalize_angle_2pi(float angle)
{
    float turns = angle * MATH_INV_2PI; // 换算成"圈"
    turns -= (float)(int32_t)turns;     // 取小数部分（int32_t 转换向零取整）
    if (turns < 0.0f)
        turns += 1.0f;
    return turns * MATH_2PI;
}

// 将角度标准化到 [-π, π] 范围
static inline float normalize_angle_pi(float angle)
{
    return normalize_angle_2pi(angle + MATH_PI) - MATH_PI;
}

// arm_sin_cos 弧度版：输入角度为 rad，内部转 deg 后调用 arm_sin_cos_f32
static inline void arm_sin_cos_rad_f32(float theta_rad, float *pSinVal, float *pCosVal)
{
    arm_sin_cos_f32(theta_rad * F180_PI, pSinVal, pCosVal);
}

// Clark 变换 (abc → αβ)(等幅值)
// ia, ib, ic: 三相电流或电压; alpha, beta: 输出的 αβ 轴分量
static inline void clarke_transform(float ia, float ib, float ic, float *alpha, float *beta)
{
    // 使用幅值不变变换（系数 2/3）
    // 简化电流 ia+ib+ic=0
    *alpha = ia;
    *beta = MATH_INSQRT3 * (ib - ic);
}
// Clark 反变换 (αβ → abc)(等幅值)
// alpha, beta: αβ 轴分量; ia, ib, ic: 输出的三相值
static inline void inv_clarke_transform(float alpha, float beta, float *ia, float *ib, float *ic)
{
    *ia = alpha;
    *ib = -0.5f * alpha + MATH_SQRT3_2 * beta;
    *ic = -0.5f * alpha - MATH_SQRT3_2 * beta;
}
// Park 变换 (αβ → dq)
// alpha, beta: αβ 轴分量; sin/cos: 电角度; d, q: 输出的 dq 轴分量
static inline void park_transform(float alpha, float beta, float sin_angle, float cos_angle, float *d, float *q)
{
    *d = alpha * cos_angle + beta * sin_angle;
    *q = -alpha * sin_angle + beta * cos_angle;
}
// Park 反变换 (dq → αβ)
// d, q: dq 轴分量; sin/cos: 电角度; alpha, beta: 输出的 αβ 轴分量
static inline void inv_park_transform(float d, float q, float sin_angle, float cos_angle, float *alpha, float *beta)
{
    *alpha = d * cos_angle - q * sin_angle;
    *beta = d * sin_angle + q * cos_angle;
}

// 普通函数--大型函数 调用少，不需要牺牲体积

#endif //  __MATH_FAST_H
