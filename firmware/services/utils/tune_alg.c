#include "tune_alg.h"
#include <float.h>
#include <string.h>

static bool positive(float x) { return isfinite(x) && x > 0.0f; }
static bool adapt_valid(const tDutyAdapt *a) {
    return positive(a->i_target_max) && isfinite(a->duty_max) &&
           a->duty_max >= 0.01f && a->duty_max <= 1 && a->settle_ticks;
}
/* 返回 1=首次越过目标，退回实际验证过的前一点并永久锁定；
 * 0=继续以整数百分比递增；-1=起始点就超标，没有安全前一点。
 * 等于目标仍属未超过，记录后继续试下一点。
 */
static bool duty_adapt(const tDutyAdapt *a, float i, float *duty,
                       float *previous) {
    if (i > a->i_target_max) {
        if (*duty <= 0.001f) {
            *duty = 0.001f;
            return true;
        }
        *duty -= 0.001f;
        return true;
    }
    *duty += 0.001f;
    return false;
}
static float dft_mag(float re, float im, uint16_t n) {
    return 2.0f * hypotf(re, im) / (float)n;
}

/* 一维最小二乘模型 y=k*x+b。
 * Welford 中心化递推 Sxx/Sxy/Syy，避免 sum(x²)-sum(x)²/n 大数相消。
 * k=Sxy/Sxx，b=mean_y-k*mean_x，MSE=(Syy-k*Sxy)/n。
 */
void ls1d_reset(tLS1D *a) { memset(a, 0, sizeof(*a)); }
void ls1d_accum(tLS1D *a, float x, float y) {
    float dx = x - a->mean_x, dy = y - a->mean_y;
    a->n++;
    a->mean_x += dx / (float)a->n;
    a->mean_y += dy / (float)a->n;
    a->sxx += dx * (x - a->mean_x);
    a->sxy += dx * (y - a->mean_y);
    a->syy += dy * (y - a->mean_y);
}
bool ls1d_fit(const tLS1D *a, float *k, float *b, float *mse) {
    *k = *b = 0.0f;
    *mse = FLT_MAX;
    if (a->n < 10 || !positive(a->sxx))
        return false;
    *k = a->sxy / a->sxx;
    *b = a->mean_y - *k * a->mean_x;
    *mse = fmaxf(0.0f, (a->syy - *k * a->sxy) / (float)a->n);
    return isfinite(*k) && isfinite(*b) && isfinite(a->syy) && isfinite(*mse);
}

/* 双变量带截距模型 y=k1*x1+k2*x2+k3。
 * 中心化后解 2x2 协方差方程；1-rho² 检查回归量共线性，
 * 避免直接对不同量纲的 3x3 正规方程采用绝对主元阈值。
 */
void ls3d_reset(tLS3D *a) { memset(a, 0, sizeof(*a)); }
void ls3d_accum(tLS3D *a, float x1, float x2, float y) {
    float d1 = x1 - a->mean_x1, d2 = x2 - a->mean_x2;
    float dy = y - a->mean_y;
    a->n++;
    a->mean_x1 += d1 / (float)a->n;
    a->mean_x2 += d2 / (float)a->n;
    a->mean_y += dy / (float)a->n;
    a->s11 += d1 * (x1 - a->mean_x1);
    a->s12 += d1 * (x2 - a->mean_x2);
    a->s22 += d2 * (x2 - a->mean_x2);
    a->s1y += d1 * (y - a->mean_y);
    a->s2y += d2 * (y - a->mean_y);
    a->syy += dy * (y - a->mean_y);
}
bool ls3d_fit(const tLS3D *a, float *k1, float *k2, float *k3) {
    *k1 = *k2 = *k3 = 0.0f;
    if (a->n < 10 || !positive(a->s11) || !positive(a->s22))
        return false;
    float scale = sqrtf(a->s11) * sqrtf(a->s22);
    float rho = a->s12 / scale, den = 1.0f - rho * rho;
    if (!isfinite(den) || den < 1e-4f)
        return false;
    *k1 = (a->s1y / a->s11 - rho * a->s2y / scale) / den;
    *k2 = (a->s2y / a->s22 - rho * a->s1y / scale) / den;
    *k3 = a->mean_y - *k1 * a->mean_x1 - *k2 * a->mean_x2;
    return isfinite(*k1) && isfinite(*k2) && isfinite(*k3);
}

/* Rs/L 合并模型与流程：
 * 0: 在电角 0 以 1% 起、1% 步长找电阻第二点 duty2；每次调整等待电流稳定。
 * 1/2: 三个方向分别固定 duty1=0.6*duty2 和 duty2 取稳态平均，
 * Rs=(ud2-ud1)/(id2-id1)。方向间检查后取均值；不对第一点再次自适应。
 * 三方向是 (2Ru+Rv+Rw)/4 等效组合，不是独立相电阻。
 * 3: 回到固定 α 方向，以 duty2 直流对齐；4: 保持此偏置，
 * 分别以 duty2*sin(Ωn) 叠加 α/β 小信号，计算 Ld/Lq，不再自适应。
 * 保持偏置与注入叠加后最大 2*duty2，因此 duty2 上限为 0.5。
 *
 * 电感模型 u=Rs*i+L*di/dt；DFT 得到 |Z|=U1/I1。
 * PWM 零阶保持下 a=exp(-Rs*Ts/L)，
 * q=(|Z|²/Rs²-1)/(4*sin²(Ω/2))=a/(1-a)²，
 * 1-a=2/(1+sqrt(1+4q))，L=-Rs*Ts/log(a)。跳过两个周期暂态。
 * ud/uq 为调用者传入的上一拍限幅后指令，按上一拍 theta_e 转回 αβ。
 * 该电压不包含未补偿的死区和管压降；转子必须静止，电流测量链幅值需校正。
 * 电感是当前偏置/频率下的增量值；固定复用 duty2 不保证高频信噪比充足。
 */
void tune_rl_init(tTune_rl_ctx *c, tTune_rl_cfg cfg) {
    memset(c, 0, sizeof(*c));
    c->cfg = cfg;
    if (!adapt_valid(&cfg.adapt) || cfg.adapt.duty_max > 0.5f ||
        !positive(cfg.cur_steady_err) || !cfg.steady_ticks ||
        !cfg.align_ticks || cfg.timeout_ticks <= cfg.steady_ticks ||
        cfg.timeout_ticks <= cfg.align_ticks ||
        cfg.timeout_ticks <= cfg.adapt.settle_ticks || !positive(cfg.rs_min) ||
        !isfinite(cfg.rs_max) || cfg.rs_max <= cfg.rs_min ||
        !positive(cfg.rs_phase_diff_thr_coef) || !positive(cfg.omega_h) ||
        !positive(cfg.omega_dt) || cfg.n_per_cycle < 8 ||
        fabsf(cfg.omega_dt * cfg.n_per_cycle - XMATH_2PI) > 1e-3f ||
        !cfg.avg_cycles || !positive(cfg.ls_min) || !isfinite(cfg.ls_max) ||
        cfg.ls_max <= cfg.ls_min) {
        c->state = TO_DATA_INVALID;
        return;
    }
    c->duty2 = c->cmd.duty = 0.01f;
}
static void rl_next(tTune_rl_ctx *c, uint8_t step) {
    c->step = step;
    c->tick_cnt = c->wait_tick = c->steady_tick = 0;
    c->sum_i = c->sum_u = 0;
}
eTuneOneState tune_rl_update(tTune_rl_ctx *c, float id, float iq, float ud,
                             float uq) {
    if (c->state != TO_RUNNING)
        return c->state;
    if (!isfinite(id) || !isfinite(iq) || !isfinite(ud) || !isfinite(uq)) {
        c->state = TO_DATA_INVALID;
        goto stop;
    }
    if (++c->tick_cnt > c->cfg.timeout_ticks) {
        c->state = TO_TIMEOUT;
        goto stop;
    }
    float co = XCOS(c->cmd.theta_e), si = XSIN(c->cmd.theta_e);
    float ia = id * co - iq * si, ib = id * si + iq * co;
    float ua = ud * co - uq * si, ub = ud * si + uq * co;
    float err = fabsf(id - c->id_last);
    c->id_last = id;
    if (c->step == 0) {
        if (++c->wait_tick < c->cfg.adapt.settle_ticks)
            return c->state;
        if (!c->duty_locked) {
            int result =
                duty_adapt(&c->cfg.adapt, id, &c->duty2, &c->duty_previous);
            if (result < 0) {
                c->state = TO_DATA_INVALID;
                goto stop;
            }
            c->duty_locked = result > 0;
            c->wait_tick = c->steady_tick = 0;
            c->cmd.duty = c->duty2;
            return c->state; // 回退后也必须等待反馈稳定，不能使用超标拍样本。
        } else if (err < c->cfg.cur_steady_err) {
            if (++c->steady_tick >= c->cfg.steady_ticks)
                rl_next(c, 1);
        } else
            c->steady_tick = 0;
        c->cmd.duty = c->step == 1 ? 0.6f * c->duty2 : c->duty2;
    } else if (c->step == 1 || c->step == 2) {
        if (++c->wait_tick >= c->cfg.adapt.settle_ticks &&
            err < c->cfg.cur_steady_err) {
            c->sum_i += id;
            c->sum_u += ud;
            if (++c->steady_tick >= c->cfg.steady_ticks) {
                uint8_t p = c->step - 1;
                c->id_meas[p] = c->sum_i / c->steady_tick;
                c->ud_meas[p] = c->sum_u / c->steady_tick;
                if (p == 0)
                    rl_next(c, 2);
                else {
                    float di = c->id_meas[1] - c->id_meas[0];
                    float r = (c->ud_meas[1] - c->ud_meas[0]) / di;
                    if (di <= c->cfg.cur_steady_err || !positive(r)) {
                        c->state = TO_DATA_INVALID;
                        goto stop;
                    }
                    c->rs_meas[c->angle_idx] = r;
                    if (++c->angle_idx < 3)
                        rl_next(c, 1);
                    else {
                        float lo = fminf(c->rs_meas[0],
                                         fminf(c->rs_meas[1], c->rs_meas[2]));
                        float hi = fmaxf(c->rs_meas[0],
                                         fmaxf(c->rs_meas[1], c->rs_meas[2]));
                        c->out.rs =
                            (c->rs_meas[0] + c->rs_meas[1] + c->rs_meas[2]) / 3;
                        if (c->out.rs < c->cfg.rs_min ||
                            c->out.rs > c->cfg.rs_max) {
                            c->state = TO_DATA_INVALID;
                            goto stop;
                        }
                        if ((hi - lo) / c->out.rs >
                            c->cfg.rs_phase_diff_thr_coef) {
                            c->state = TO_DATA_IMBALANCE;
                            goto stop;
                        }
                        rl_next(c, 3);
                    }
                }
            }
        } else {
            c->steady_tick = 0;
            c->sum_i = c->sum_u = 0;
        }
        c->cmd.theta_e = c->step == 3 ? 0 : c->angle_idx * XMATH_2PI / 3;
        c->cmd.duty = c->step == 1 ? 0.6f * c->duty2 : c->duty2;
    } else if (c->step == 3) {
        if (++c->wait_tick >= c->cfg.align_ticks)
            rl_next(c, 4);
        c->cmd.theta_e = 0;
        c->cmd.duty = c->duty2;
    } else {
        float phase = c->cfg.omega_dt * c->dft_cnt;
        float sn = XSIN(phase), cs = XCOS(phase);
        float i = c->uidx == 0 ? ia : ib, u = c->uidx == 0 ? ua : ub;
        c->sum_re += i * cs;
        c->sum_im += i * sn;
        c->sum_ure += u * cs;
        c->sum_uim += u * sn;
        if (++c->dft_cnt >= c->cfg.n_per_cycle) {
            c->dft_cnt = 0;
            if (c->cycle_cnt >= 2) {
                c->amp_sum += dft_mag(c->sum_re, c->sum_im, c->cfg.n_per_cycle);
                c->uamp_sum +=
                    dft_mag(c->sum_ure, c->sum_uim, c->cfg.n_per_cycle);
            }
            c->sum_re = c->sum_im = c->sum_ure = c->sum_uim = 0;
            if (++c->cycle_cnt >= (uint16_t)c->cfg.avg_cycles + 2) {
                float im = c->amp_sum / c->cfg.avg_cycles,
                      um = c->uamp_sum / c->cfg.avg_cycles;
                if (im < 1e-6f || !positive(im) || !positive(um)) {
                    c->state = TO_DATA_NOISE;
                    goto stop;
                }
                float z = um / im, r = c->out.rs;
                float sh = sinf(c->cfg.omega_dt / 2),
                      q = (z * z - r * r) / (4 * r * r * sh * sh);
                if (!positive(q)) {
                    c->state = TO_DATA_INVALID;
                    goto stop;
                }
                float one_minus_a = 2 / (1 + sqrtf(1 + 4 * q));
                float l = -r * (c->cfg.omega_dt / c->cfg.omega_h) /
                          log1pf(-one_minus_a);
                if (!isfinite(l) || l < c->cfg.ls_min || l > c->cfg.ls_max) {
                    c->state = TO_DATA_INVALID;
                    goto stop;
                }
                c->out.ldq[c->uidx] = l;
                if (c->uidx == 1) {
                    c->state = TO_DONE;
                    goto stop;
                }
                c->uidx = 1;
                c->cycle_cnt = 0;
                c->tick_cnt = 0;
                c->amp_sum = c->uamp_sum = 0;
            }
        }
        float h = c->duty2 * XSIN(c->cfg.omega_dt * c->dft_cnt);
        float da = c->duty2 + (c->uidx == 0 ? h : 0), db = c->uidx == 1 ? h : 0;
        c->cmd.duty = hypotf(da, db);
        c->cmd.theta_e = xmath_normalize_angle_2pi(atan2f(db, da));
    }
stop:
    if (c->state != TO_RUNNING)
        memset(&c->cmd, 0, sizeof(c->cmd));
    return c->state;
}

/* 编码器模型 θe=k*(pos-pos_origin)+b，k=s*p，s=±1。
 * 连续正反扫描分别回归，验证斜率整数残差、方向一致性和拟合 MSE。
 * 两截距按 2π 电周期圆周融合，机械 offset=pos_origin-b/(s*p)。
 * 正反扫描共用连续的电角轨迹，避免回零时跳转到另一磁极。
 * 删除反向 π 跳变判极性：精确反向场的启动方向不唯一，不能用于判零偏。
 * 先以 1% 起、1% 步长在角度 0 自适应，首次超过目标后回退前一点并锁定，
 * 再固定该占空比正反扫描。开环拖动电流随速度/负载变化，外部须独立限流。
 * 要求低速同步跟随；负载/齿槽导致的滞后仍需台架检查。
 */
void enc_cal_init(tEncCal_ctx *c, tEncCal_cfg cfg) {
    memset(c, 0, sizeof(*c));
    c->cfg = cfg;
    if (!adapt_valid(&cfg.adapt) || !cfg.align_ticks ||
        cfg.timeout_ticks <= cfg.align_ticks || !positive(cfg.delta_e) ||
        !positive(cfg.sample_step_e) || cfg.delta_e > cfg.sample_step_e ||
        !positive(cfg.travel_pos) || !cfg.pole_pairs_expected ||
        !positive(cfg.fit_max_mse))
        c->state = TO_DATA_INVALID;
    if (c->state == TO_RUNNING)
        c->cmd.duty = 0.01f;
}
eTuneOneState enc_cal_update(tEncCal_ctx *c, float pos, float id) {
    if (c->state != TO_RUNNING)
        return c->state;
    if (!isfinite(pos) || !isfinite(id)) {
        c->state = TO_DATA_INVALID;
        goto stop;
    }
    if (++c->elapsed_ticks > c->cfg.timeout_ticks) {
        c->state = TO_TIMEOUT;
        goto stop;
    }
    if (c->step == 0) {
        if (++c->wait_tick < c->cfg.adapt.settle_ticks)
            return c->state;
        if (!c->duty_locked) {
            int result =
                duty_adapt(&c->cfg.adapt, id, &c->cmd.duty, &c->duty_previous);
            if (result < 0) {
                c->state = TO_DATA_INVALID;
                goto stop;
            }
            c->duty_locked = result > 0;
            c->wait_tick = c->tick_cnt = 0;
            return c->state;
        }
        if (++c->tick_cnt >= c->cfg.align_ticks) {
            c->pos_origin = c->pos_start = pos;
            c->step = 1;
            c->tick_cnt = c->elapsed_ticks = 0;
        }
    } else if (c->step == 1 || c->step == 2) {
        // 当前反馈配上一拍真正施加的角度，而非递增后的角度。
        if (fabsf(c->theta_e_acc - c->theta_e_raw) >= c->cfg.sample_step_e) {
            ls1d_accum(c->step == 1 ? &c->ls_fwd : &c->ls_bwd,
                       pos - c->pos_origin,
                       c->theta_e_acc);
            c->theta_e_raw = c->theta_e_acc;
        }
        if (fabsf(pos - c->pos_start) >= c->cfg.travel_pos) {
            c->pos_start = pos;
            c->elapsed_ticks = 0;
            c->step++;
        }
        if (c->step <= 2)
            c->theta_e_acc += (c->step == 1 ? 1 : -1) * c->cfg.delta_e;
        c->cmd.theta_e = xmath_normalize_angle_2pi(c->theta_e_acc);
    }
    if (c->step == 3) {
        float kf, bf, mf, kb, bb, mb;
        if (!ls1d_fit(&c->ls_fwd, &kf, &bf, &mf) ||
            !ls1d_fit(&c->ls_bwd, &kb, &bb, &mb) || mf > c->cfg.fit_max_mse ||
            mb > c->cfg.fit_max_mse || (kf > 0) != (kb > 0)) {
            c->state = TO_DATA_INVALID;
            goto stop;
        }
        float p = c->cfg.pole_pairs_expected;
        if (fabsf(fabsf(kf) - p) > 0.1f || fabsf(fabsf(kb) - p) > 0.1f) {
            c->state = TO_DATA_IMBALANCE;
            goto stop;
        }
        float delta = xmath_normalize_angle_pi(bb - bf);
        if (fabsf(delta) > 0.5f) {
            c->state = TO_DATA_IMBALANCE;
            goto stop;
        }
        c->out.pole_pairs = c->cfg.pole_pairs_expected;
        c->out.direction = kf > 0;
        float k = c->out.direction ? p : -p;
        // 用已知整数斜率校正每条拟合截距，降低斜率误差造成的零偏。
        bf = c->ls_fwd.mean_y - k * c->ls_fwd.mean_x;
        bb = c->ls_bwd.mean_y - k * c->ls_bwd.mean_x;
        float b = bf + 0.5f * xmath_normalize_angle_pi(bb - bf);
        c->out.theta_offset = xmath_normalize_angle_2pi(c->pos_origin - b / k);
        c->state = TO_DONE;
    }
stop:
    if (c->state != TO_RUNNING)
        memset(&c->cmd, 0, sizeof(c->cmd));
    return c->state;
}

/* 磁链模型 uq=Rs*iq+Lq*diq/dt+ωe*(Ld*id+ψf)。
 * 稳态且 id≈0 时 y=uq-Rs*iq=ψf*ωe+b；跨多个转速拟合 ψf，
 * 截距 b 吸收近似固定电压偏差。持续在速度带内等待，再采样；失稳重新等待。
 * 调用者保证电流稳态、无弱磁、实际 uq 可信；速度带只检查转速，不证明 diq/dt≈0。
 * Ke=p*ψf，单位 V/(rad/s mech)，为相电压峰值约定，不能直接与线电压 RMS Kv
 * 混用。
 */
void tune_psif_init(tTune_psif_ctx *c, tTune_psif_cfg cfg) {
    memset(c, 0, sizeof(*c));
    c->cfg = cfg;
    if (!positive(cfg.rs_known) || !positive(cfg.vel_low) ||
        !isfinite(cfg.vel_high) || cfg.vel_high <= cfg.vel_low ||
        cfg.num_points < 2 || !cfg.pole_pairs || !cfg.steady_ticks ||
        !cfg.sample_ticks || !positive(cfg.vel_band) ||
        !positive(cfg.fit_max_mse) ||
        (uint64_t)cfg.timeout_ticks <=
            (uint64_t)cfg.steady_ticks + cfg.sample_ticks)
        c->state = TO_DATA_INVALID;
}
eTuneOneState tune_psif_update(tTune_psif_ctx *c, float uq, float iq,
                               float vel) {
    if (c->state != TO_RUNNING)
        return c->state;
    if (!isfinite(uq) || !isfinite(iq) || !isfinite(vel)) {
        c->state = TO_DATA_INVALID;
        goto stop;
    }
    if (++c->elapsed_ticks > c->cfg.timeout_ticks) {
        c->state = TO_TIMEOUT;
        goto stop;
    }
    float target = c->cfg.vel_low + (c->cfg.vel_high - c->cfg.vel_low) *
                                        c->point_idx /
                                        (float)(c->cfg.num_points - 1);
    c->cmd.vel = target;
    if (fabsf(vel - target) > c->cfg.vel_band) {
        c->tick_cnt = 0;
        // 已取得的稳态样本仍有效；只重置当前连续稳态等待。
        return c->state;
    }
    if (c->tick_cnt < c->cfg.steady_ticks) {
        c->tick_cnt++;
        return c->state;
    }
    ls1d_accum(&c->ls, vel * c->cfg.pole_pairs, uq - c->cfg.rs_known * iq);
    if (++c->samples >= c->cfg.sample_ticks) {
        c->point_idx++;
        c->tick_cnt = c->samples = c->elapsed_ticks = 0;
        if (c->point_idx >= c->cfg.num_points) {
            float k, b, mse;
            if (!ls1d_fit(&c->ls, &k, &b, &mse) || !positive(k) || k > 1.0f ||
                mse > c->cfg.fit_max_mse)
                c->state = TO_DATA_INVALID;
            else {
                c->out.psi_f = k;
                c->out.ke = k * c->cfg.pole_pairs;
                c->state = TO_DONE;
            }
        } else
            c->cmd.vel = c->cfg.vel_low + (c->cfg.vel_high - c->cfg.vel_low) *
                                              c->point_idx /
                                              (float)(c->cfg.num_points - 1);
    }
stop:
    if (c->state != TO_RUNNING)
        c->cmd.vel = 0;
    return c->state;
}

/* J/B 模型 Te=1.5*p*ψf*iq=J*α+B*ω+TL。
 * 正向加速与制动两段联合拟合；单一恒转矩段三参数可能共线，必须同时激励。
 * α 来自机械速度差分；α、ω、Te 用相同低通，保证滤波后的模型时间一致。
 * 只在正向运动范围采样，TL 可包含该方向的库仑摩擦；越零立即停止制动。
 * 输出为电机轴折算总惯量；负载近似恒定，id≈0，忽略磁阻转矩。
 * 噪声仍会造成自变量误差偏置，需用台架数据检查残差和参数重复性。
 */
void tune_jb_init(tTune_JB_ctx *c, tTune_JB_cfg cfg) {
    memset(c, 0, sizeof(*c));
    c->cfg = cfg;
    if (!positive(cfg.psi_f) || !cfg.pole_pairs || !positive(cfg.iq_high) ||
        !positive(cfg.vel_settle) || !isfinite(cfg.vel_max) ||
        cfg.vel_max <= cfg.vel_settle || !positive(cfg.alpha_lpf) ||
        cfg.alpha_lpf > 1 || cfg.min_samples < 10 ||
        cfg.timeout_ticks <= cfg.skip_ticks || cfg.timeout_ticks <= 500)
        c->state = TO_DATA_INVALID;
}
eTuneOneState tune_jb_update(tTune_JB_ctx *c, float vel, float iq, float ts) {
    if (c->state != TO_RUNNING)
        return c->state;
    if (!isfinite(vel) || !isfinite(iq) || !positive(ts)) {
        c->state = TO_DATA_INVALID;
        goto stop;
    }
    if (++c->tick_cnt > c->cfg.timeout_ticks) {
        c->state = TO_TIMEOUT;
        goto stop;
    }
    float te = 1.5f * c->cfg.pole_pairs * c->cfg.psi_f * iq;
    float a = (vel - c->vel_prev) / ts;
    c->vel_prev = vel;
    float f = c->cfg.alpha_lpf;
    c->alpha_filt += f * (a - c->alpha_filt);
    c->vel_filt += f * (vel - c->vel_filt);
    c->torque_filt += f * (te - c->torque_filt);
    c->cmd.iq = 0;
    if (c->step == 0) {
        if (fabsf(vel) < c->cfg.vel_settle)
            c->samples_accel++;
        else
            c->samples_accel = 0;
        if (c->samples_accel >= 500) {
            c->samples_accel = 0;
            c->tick_cnt = 0;
            c->step = 1;
            c->alpha_filt = 0;
            c->vel_filt = vel;
            c->torque_filt = te;
        }
    } else if (c->step == 1) {
        c->cmd.iq = c->cfg.iq_high;
        if (vel < -c->cfg.vel_settle) {
            c->state = TO_DATA_INVALID;
            goto stop;
        }
        if (c->tick_cnt > c->cfg.skip_ticks && vel > c->cfg.vel_settle) {
            ls3d_accum(&c->ls, c->alpha_filt, c->vel_filt, c->torque_filt);
            c->samples_accel++;
        }
        if (vel >= c->cfg.vel_max) {
            c->step = 2;
            c->tick_cnt = 0;
            c->cmd.iq = -c->cfg.iq_high;
        }
    } else if (c->step == 2) {
        c->cmd.iq = -c->cfg.iq_high;
        if (vel <= c->cfg.vel_settle) {
            c->step = 3;
            c->cmd.iq = 0;
        } else if (c->tick_cnt > c->cfg.skip_ticks) {
            ls3d_accum(&c->ls, c->alpha_filt, c->vel_filt, c->torque_filt);
            c->samples_decel++;
        }
    }
    if (c->step == 3) {
        float j, b, tl;
        if (c->ls.n < c->cfg.min_samples || c->samples_accel < 5 ||
            c->samples_decel < 5)
            c->state = TO_DATA_NOISE;
        else if (!ls3d_fit(&c->ls, &j, &b, &tl) || j < 1e-7f || j > 100 ||
                 b < 0)
            c->state = TO_DATA_INVALID;
        else {
            float residual =
                fmaxf(0, c->ls.syy - j * c->ls.s1y - b * c->ls.s2y);
            if (!isfinite(residual) || !positive(c->ls.syy) ||
                residual > 0.1f * c->ls.syy)
                c->state = TO_DATA_NOISE;
            else {
                c->out.j = j;
                c->out.b = b;
                c->out.tl = tl;
                c->state = TO_DONE;
            }
        }
    }
stop:
    if (c->state != TO_RUNNING)
        c->cmd.iq = 0;
    return c->state;
}
