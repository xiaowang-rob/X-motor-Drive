#include "trajectory.h"
#include "xmath.h"
//  常量定义
#define TRAJ_EPSILON 1.0e-6f
#define TRAJ_TOL_DEFAULT 0.001f

//  初始化
bool traj_init(tTraj *traj, tParameter *param) {

    tTraj_Config cfg = {
        .limit_d1 = param->traj_limit_d1,
        .limit_d2 = param->traj_limit_d2,
        .limit_d3 = param->traj_limit_d3,
        .tolerance = param->traj_tolerance,
        .mode = param->traj_mode,
    };
    traj->cfg = cfg;
    traj_reset(traj, 0.0f);
    return true;
}

//  重置状态
void traj_reset(tTraj *traj, float current_value) {
    traj->target = current_value;
    traj->current = current_value;
    traj->rate = 0.0f;
    traj->rate_d2 = 0.0f;
    traj->busy = false;
}

//  设置目标
void traj_set_target(tTraj *traj, float target) {
    traj->target = target;
    float err = XFABSF(target - traj->current);
    float tol =
        (traj->cfg.tolerance > 0.0f) ? traj->cfg.tolerance : TRAJ_TOL_DEFAULT;
    traj->busy = (err > tol);
}

//  核心更新函数
// TODO: 1、增加PV PT模式的轨迹规划器
void traj_Update(tTraj *traj, float dt) {

    if (traj->cfg.mode == TRAJ_DISABLE) {
        traj->out.value = traj->target;
        traj->out.rate_d1 = 0.0f;
        traj->out.rate_d2 = 0.0f;
        traj->busy = false;
        return;
    }

    if (dt <= TRAJ_EPSILON) {
        // 周期输入有问题
        return;
    }

    //  1. 误差计算
    float error = traj->target - traj->current;
    float dist = XFABSF(error);
    float dir = XFSIGN(error);

    //  2. 刹车距离计算 (rate² / 2a)
    float rate_abs = XFABSF(traj->rate);
    float stop_dist = (traj->cfg.limit_d2 > TRAJ_EPSILON)
                          ? (rate_abs * rate_abs) / (2.0f * traj->cfg.limit_d2)
                          : 0.0f;
    //  3. 计算允许的最大变化率
    float rate_limit;
    if (dist <= stop_dist) {
        //  减速段：使用 ARM 优化 sqrt
        rate_limit = XSQRTF(2.0f * traj->cfg.limit_d2 * dist);
    } else {
        rate_limit = traj->cfg.limit_d1;
    }

    //  4. 分支：T 型 vs S 型
    if (traj->cfg.mode == TRAJ_TRAPEZOID) {
        //  梯形轨迹
        float target_rate = rate_limit * dir;
        float rate_diff = target_rate - traj->rate;
        float max_rate_step = traj->cfg.limit_d2 * dt;

        //  变化率斜坡 (分支消除)
        float rate_step = (XFABSF(rate_diff) > max_rate_step)
                              ? XFSIGN(rate_diff) * max_rate_step
                              : rate_diff;

        traj->rate += rate_step;
        traj->rate_d2 = 0;
    } else {
        //  S 型轨迹
        float target_rate = rate_limit * dir;
        float rate_err = target_rate - traj->rate;

        //  目标二阶变化率 (加速度) 计算
        float target_rate_d2 = (dt > TRAJ_EPSILON) ? (rate_err / dt) : 0.0f;
        target_rate_d2 =
            XCLAMP(target_rate_d2, -traj->cfg.limit_d2, traj->cfg.limit_d2);

        //  二阶变化率 限制
        float rate_d2_err = target_rate_d2 - traj->rate_d2;
        float max_d2_step = traj->cfg.limit_d3 * dt;

        float rate_d2_step = (XFABSF(rate_d2_err) > max_d2_step)
                                 ? XFSIGN(rate_d2_err) * max_d2_step
                                 : rate_d2_err;

        traj->rate_d2 += rate_d2_step;
        traj->rate += traj->rate_d2 * dt;
    }

    //  5. 积分得到规划值
    traj->current += traj->rate * dt;

    //  6. 输出赋值
    traj->out.value = traj->current;   // 核心输出
    traj->out.rate_d1 = traj->rate;    // 可选：变化率前馈
    traj->out.rate_d2 = traj->rate_d2; // 可选：加速度前馈
    //  7. 到达判断
    float tol =
        (traj->cfg.tolerance > 0.0f) ? traj->cfg.tolerance : TRAJ_TOL_DEFAULT;
    bool done = (dist <= tol) && (XFABSF(traj->rate) <= tol);

    if (done) {
        traj->current = traj->target; // 修正累积误差
        traj->rate = 0.0f;
        traj->rate_d2 = 0.0f;
        traj->busy = false;
    } else {
        traj->busy = true;
    }
}