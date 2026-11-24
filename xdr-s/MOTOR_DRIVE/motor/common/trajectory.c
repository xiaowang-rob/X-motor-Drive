#include "trajectory.h"

#include "bsp_math.h"
#include <string.h>

/**
 * @brief 初始化轨迹规划器
 *
 * @param t 轨迹规划器实例指针
 * @param cfg 轨迹配置参数指针
 * @param initial 初始位置值
 */
void traj_init(tTraj *t, const tTrajCfg *cfg, double initial) {
    /* 清零结构体，确保所有状态和导数初始为0 */
    memset(t, 0, sizeof(*t));

    /* 设置初始位置、请求位置和终点位置 */
    t->value = t->requested = t->endpoint = initial;

    /* 从配置中读取运动学限制和模式参数 */
    t->mode = cfg->mode;
    t->vmax = cfg->vmax;
    t->amax = cfg->amax;
    t->jmax = cfg->jmax;
    t->tol = cfg->tol;
}

/**
 * @brief 设置轨迹规划器的目标位置
 *
 * @param t 轨迹规划器实例指针
 * @param ref 目标位置参考值
 */
void traj_set_target(tTraj *t, double ref) {
    /* 如果目标位置未改变，则直接返回以避免不必要的重规划 */
    if (ref == t->requested)
        return;

    /* 更新目标位置，并标记需要重新规划和处于忙碌状态 */
    t->requested = ref;
    t->replan = true;
    t->busy = true;
}

/**
 * @brief 检查伯恩斯坦控制点的导数是否满足运动学约束
 * @details 利用伯恩斯坦导数控制点的凸包性质给出整个区间的上界，避免只抽样检查约束
 *
 * @param b 伯恩斯坦多项式控制点数组（6个点，5次多项式）
 * @param duration 轨迹段持续时间
 * @param t 轨迹规划器实例指针，包含速度、加速度、加加速度限制
 * @return true 满足约束，false 超出约束
 */
static bool bounded(const double *b, double duration, const tTraj *t) {
    double derivative[6];
    /* 复制控制点用于逐阶计算导数 */
    memcpy(derivative, b, sizeof(derivative));

    /* 各阶导数的限制值：速度、加速度、加加速度 */
    const double limits[3] = {t->vmax, t->amax, t->jmax};

    /* 逐阶计算导数并检查是否超限 */
    for (unsigned order = 0, degree = 5; order < 3; ++order, --degree) {
        for (unsigned i = 0; i < degree; ++i) {
            /* 计算当前阶导数的控制点 */
            derivative[i] = degree * (derivative[i + 1u] - derivative[i]) / duration;

            /* 考虑微小浮点误差，增加 1e-8 的容差 */
            if (MATH_FABS(derivative[i]) > limits[order] * (1.0 + 1e-8))
                return false;
        }
    }
    return true;
}

/**
 * @brief 规划S型曲线轨迹段
 * @details 使用5次伯恩斯坦多项式生成平滑轨迹，通过迭代调整持续时间以满足运动学约束
 *
 * @param t 轨迹规划器实例指针
 * @param dt 控制周期（时间步长）
 * @return true 规划成功，false 规划失败（保留旧轨迹）
 */
static bool plan_s_curve(tTraj *t, double dt) {
    /* 计算当前位置与目标位置的距离 */
    double distance = MATH_FABS(t->requested - t->value);

    /* 根据距离和运动学限制估算最小所需时间 */
    double duration = MATH_MAX(dt, MATH_MAX(5.0 * distance / t->vmax,
                                            MATH_MAX(MATH_SQRT(20.0 * distance / t->amax),
                                                     MATH_CBRT(60.0 * distance / t->jmax))));

    /* 非零起始导数下，单纯扩大时长可能生成很长的绕行曲线。
     * 对时长和控制点位置一起设保守边界；不可行时沿用旧段至可重规划。 */
    double stop_time = t->vmax / t->amax + t->amax / t->jmax;
    double maximum_duration = MATH_MAX(duration * 4.0, stop_time * 4.0);
    double margin = t->vmax * stop_time;
    double lower = MATH_MIN(t->value, t->requested) - margin;
    double upper = MATH_MAX(t->value, t->requested) + margin;

    /* 初始尝试时间为估算时间的1/4 */
    duration *= 0.25;

    /* 迭代尝试不同的持续时间，最多40次，每次增加20% */
    for (unsigned attempt = 0; attempt < 40 && duration <= maximum_duration;
         ++attempt, duration *= 1.2) {
        /* 根据当前状态和目标构造5次伯恩斯坦多项式的6个控制点 */
        double b[6] = {
            t->value,
            t->value + t->vel * duration / 5.0,
            t->value + 2.0 * t->vel * duration / 5.0 +
            t->acceleration * duration * duration / 20.0,
            t->requested, t->requested, t->requested
        };

        /* 检查所有控制点的位置是否在安全边界内 */
        bool position_bounded = true;
        for (unsigned i = 0; i < 6; ++i)
            position_bounded = position_bounded && b[i] >= lower && b[i] <= upper;

        /* 如果位置有界且各阶导数满足约束，则规划成功 */
        if (position_bounded && bounded(b, duration, t)) {
            /* Bernstein -> 幂基，只在前台规划时转换一次。
             * p(u) = c0 + c1*u + ... + c5*u^5，u=t/T。 */
            double d[6];
            memcpy(d, b, sizeof(d));

            /* 5次多项式的二项式系数 */
            static const unsigned binomial[6] = {1, 5, 10, 10, 5, 1};

            /* 将伯恩斯坦基转换为幂基（多项式系数） */
            for (unsigned n = 0; n < 6; ++n) {
                t->coeff[n] = binomial[n] * d[0];
                for (unsigned i = 0; i < 5 - n; ++i)
                    d[i] = d[i + 1] - d[i];
            }

            /* 保存规划结果 */
            t->inv_time = 1.0 / duration;
            t->duration = duration;
            t->elapsed = 0.0;
            t->endpoint = t->requested;
            t->replan = false;
            return true;
        }
    }

    /* 当前导数条件下找不到保守可行段时继续旧轨迹，由前台重试最新目标。
     * 保留速度/加速度连续性，不通过清零导数强行重规划。 */
    return false;
}

/**
 * @brief 执行轨迹规划
 * @details 根据当前模式判断是否需要重新规划轨迹
 *
 * @param t 轨迹规划器实例指针
 * @param dt 控制周期（时间步长）
 * @return true 规划成功或无需规划，false 规划失败
 */
bool traj_plan(tTraj *t, float dt) {
    /* 如果不是S曲线模式，或者不需要重新规划，则直接返回成功
     * 否则尝试规划S曲线 */
    return t->mode != MC_TRAJ_S_CURVE || !t->replan || plan_s_curve(t, dt);
}

/**
 * @brief 更新轨迹状态，计算当前位置、速度和加速度
 *
 * @param t 轨迹规划器实例指针
 * @param dt 控制周期（时间步长）
 * @return true 轨迹仍在运行或刚完成，false 轨迹已停止
 */
bool traj_update(tTraj *t, float dt) {
    /* 如果规划器不忙，直接返回 */
    if (!t->busy)
        return false;

    /* 模式1：禁用模式，直接跳转到目标位置 */
    if (t->mode == MC_TRAJ_DISABLED) {
        t->value = t->requested;
        t->vel = t->acceleration = 0.0;
        t->busy = t->replan = false;
        return true;
    }

    /* 模式2：梯形速度曲线模式 */
    if (t->mode == MC_TRAJ_TRAPEZOID) {
        double error = t->requested - t->value;

        /* 计算期望速度，考虑最大速度和制动距离限制 */
        double desired = MATH_COPYSIGN(MATH_MIN(t->vmax,
                                                MATH_SQRT(2.0 * t->amax * MATH_FABS(error))), error);
        double old_velocity = t->vel;
        double step = t->amax * dt;

        /* 限制速度变化量，确保加速度不超过限制 */
        t->vel += MATH_MIN(step, MATH_MAX(-step, desired - t->vel));

        /* 使用梯形积分更新位置 */
        t->value += 0.5 * (old_velocity + t->vel) * dt;

        /* 计算当前加速度 */
        t->acceleration = (t->vel - old_velocity) / dt;

        /* 检查是否到达目标位置且速度足够小 */
        if (MATH_FABS(error) <= t->tol && MATH_FABS(old_velocity) <= step) {
            t->value = t->requested;
            t->vel = 0.0;
            t->acceleration = 0.0;
            t->busy = false;
        } else {
            t->busy = true;
        }
        t->replan = false;
        return true;
    }

    /* 模式3：S型曲线模式（5次多项式） */
    /* 如果当前段已执行完毕，返回false */
    if (t->elapsed >= t->duration)
        return false;

    /* 更新已用时间 */
    t->elapsed = MATH_MIN(t->duration, t->elapsed + dt);

    /* 计算归一化时间 u = t / T */
    double u = t->elapsed * t->inv_time;
    const double *c = t->coeff;

    /* 使用霍纳法则计算位置、速度和加速度 */
    t->value = ((((c[5] * u + c[4]) * u + c[3]) * u + c[2]) * u + c[1]) * u + c[0];
    t->vel = (((5.0 * c[5] * u + 4.0 * c[4]) * u + 3.0 * c[3]) * u + 2.0 * c[2]) * u + c[1];
    t->vel *= t->inv_time;
    t->acceleration = ((20.0 * c[5] * u + 12.0 * c[4]) * u + 6.0 * c[3]) * u + 2.0 * c[2];
    t->acceleration *= t->inv_time * t->inv_time;

    /* 如果当前段执行完毕，修正终点状态 */
    if (t->elapsed >= t->duration) {
        t->value = t->endpoint;
        t->vel = t->acceleration = 0.0;
        /* 如果还有未处理的重规划请求，保持busy状态 */
        t->busy = t->replan;
    }
    return true;
}
