#include "trajectory.h"

#include "xmath.h"
#include <string.h>

void traj_init(tTraj *t, const tTrajCfg *cfg, double initial) {
    memset(t, 0, sizeof(*t));
    t->value = t->requested = t->endpoint = initial;
    t->mode = cfg->mode;
    t->vmax = cfg->vmax;
    t->amax = cfg->amax;
    t->jmax = cfg->jmax;
    t->tol = cfg->tol;
}

void traj_set_target(tTraj *t, double ref) {
    if (ref == t->requested)
        return;
    t->requested = ref;
    t->replan = true;
    t->busy = true;
}

/* Bernstein 导数控制点的凸包给出整个区间的上界，避免只抽样检查约束。 */
static bool bounded(const double *b, double duration, const tTraj *t) {
    double derivative[6];
    memcpy(derivative, b, sizeof(derivative));
    const double limits[3] = {t->vmax, t->amax, t->jmax};
    for (unsigned order = 0, degree = 5; order < 3; ++order, --degree) {
        for (unsigned i = 0; i < degree; ++i) {
            derivative[i] = degree * (derivative[i + 1u] - derivative[i]) / duration;
            if (XFABS(derivative[i]) > limits[order] * (1.0 + 1e-8))
                return false;
        }
    }
    return true;
}

static bool plan_s_curve(tTraj *t, double dt) {
    double distance = XFABS(t->requested - t->value);
    double duration = XMAX(dt, XMAX(5.0 * distance / t->vmax,
        XMAX(XSQRT(20.0 * distance / t->amax),
             XCBRT(60.0 * distance / t->jmax))));
    /* 非零起始导数下，单纯扩大时长可能生成很长的绕行曲线。
     * 对时长和控制点位置一起设保守边界；不可行时沿用旧段至可重规划。 */
    double stop_time = t->vmax / t->amax +
                       t->amax / t->jmax;
    double maximum_duration = XMAX(duration * 4.0, stop_time * 4.0);
    double margin = t->vmax * stop_time;
    double lower = XMIN(t->value, t->requested) - margin;
    double upper = XMAX(t->value, t->requested) + margin;
    duration *= 0.25;
    for (unsigned attempt = 0; attempt < 40 && duration <= maximum_duration;
         ++attempt, duration *= 1.2) {
        double b[6] = {t->value,
            t->value + t->vel * duration / 5.0,
            t->value + 2.0 * t->vel * duration / 5.0 +
                       t->acceleration * duration * duration / 20.0,
            t->requested, t->requested, t->requested};
        bool position_bounded = true;
        for (unsigned i = 0; i < 6; ++i)
            position_bounded = position_bounded && b[i] >= lower && b[i] <= upper;
        if (position_bounded && bounded(b, duration, t)) {
            /* Bernstein -> 幂基，只在前台规划时转换一次。
             * p(u) = c0 + c1*u + ... + c5*u^5，u=t/T。 */
            double d[6];
            memcpy(d, b, sizeof(d));
            static const unsigned binomial[6] = {1, 5, 10, 10, 5, 1};
            for (unsigned n = 0; n < 6; ++n) {
                t->coeff[n] = binomial[n] * d[0];
                for (unsigned i = 0; i < 5 - n; ++i)
                    d[i] = d[i + 1] - d[i];
            }
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

bool traj_plan(tTraj *t, float dt) {
    return t->mode != MC_TRAJ_S_CURVE || !t->replan || plan_s_curve(t, dt);
}

bool traj_update(tTraj *t, float dt) {
    if (!t->busy)
        return false;
    if (t->mode == MC_TRAJ_DISABLED) {
        t->value = t->requested;
        t->vel = t->acceleration = 0.0;
        t->busy = t->replan = false;
        return true;
    }
    if (t->mode == MC_TRAJ_TRAPEZOID) {
        double error = t->requested - t->value;
        double desired = XCOPYSIGN(XMIN(t->vmax,
            XSQRT(2.0 * t->amax * XFABS(error))), error);
        double old_velocity = t->vel;
        double step = t->amax * dt;
        t->vel += XMIN(step, XMAX(-step, desired - t->vel));
        t->value += 0.5 * (old_velocity + t->vel) * dt;
        t->acceleration = (t->vel - old_velocity) / dt;
        if (XFABS(error) <= t->tol && XFABS(old_velocity) <= step) {
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
    if (t->elapsed >= t->duration)
        return false;
    t->elapsed = XMIN(t->duration, t->elapsed + dt);
    double u = t->elapsed * t->inv_time;
    const double *c = t->coeff;
    t->value = ((((c[5] * u + c[4]) * u + c[3]) * u + c[2]) * u + c[1]) * u + c[0];
    t->vel = (((5.0 * c[5] * u + 4.0 * c[4]) * u + 3.0 * c[3]) * u + 2.0 * c[2]) * u + c[1];
    t->vel *= t->inv_time;
    t->acceleration = ((20.0 * c[5] * u + 12.0 * c[4]) * u + 6.0 * c[3]) * u + 2.0 * c[2];
    t->acceleration *= t->inv_time * t->inv_time;
    if (t->elapsed >= t->duration) {
        t->value = t->endpoint;
        t->vel = t->acceleration = 0.0;
        t->busy = t->replan;
    }
    return true;
}
