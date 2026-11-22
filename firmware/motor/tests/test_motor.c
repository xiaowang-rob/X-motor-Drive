#include "mc_motor.h"
#include "xmath.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define NEAR(a, b, eps) assert(fabs((double)(a) - (double)(b)) <= (eps))

typedef struct {
    tMotorCtrl *motor;
    tSample sample;
    tPwm written;
    unsigned writes, enables, lock_calls, depth, interrupt_at_lock;
    bool enabled, bad_sample, stale_sample, interrupt_stop, interrupt_clear;
} tMock;

static bool read_sample(void *user, const tPwm *pwm, tSample *sample) {
    tMock *h = user;
    assert(pwm->sequence == h->written.sequence);
    *sample = h->sample;
    sample->pwm_sequence = pwm->sequence + (h->stale_sample ? 1u : 0u);
    return !h->bad_sample;
}

static void write_pwm(void *user, const tPwm *pwm) {
    tMock *h = user;
    h->written = *pwm;
    ++h->writes;
    for (unsigned i = 0; i < 3; ++i)
        assert(isfinite(pwm->duty[i]) && pwm->duty[i] >= 0 && pwm->duty[i] <= 1);
}

static void set_enable(void *user, bool enable) {
    tMock *h = user;
    if (enable) {
        assert(h->writes > 0); /* PWM 必须在使能之前写入。 */
        ++h->enables;
    }
    h->enabled = enable;
}

static uintptr_t enter_critical(void *user) {
    tMock *h = user;
    ++h->lock_calls;
    /* 模拟在进入临界区之前被高优先级停机/故障中断抢占。 */
    if (h->interrupt_at_lock == h->lock_calls) {
        h->interrupt_at_lock = 0;
        if (h->interrupt_stop)
            motor_stop(h->motor);
        else {
            motor_fault(h->motor, MC_FAULT_EXTERNAL);
            if (h->interrupt_clear)
                assert(motor_clear_fault(h->motor) == MC_OK);
        }
    }
    return h->depth++;
}

static void exit_critical(void *user, uintptr_t token) {
    tMock *h = user;
    assert(h->depth == token + 1u);
    h->depth = (unsigned)token;
}

static tParameter parameters(void) {
    tParameter p = {
#define DEFAULT(id, field, type, value) .field = value,
        PARAM_FIELDS(DEFAULT)
#undef DEFAULT
    };
    p.rotor_enc = ENC_SPI;
    p.positive_dir = true;
    p.motor_ke = 0.2f;
    p.qclkp = p.dclkp = 0.1f;
    p.qclki = p.dclki = 1.0f;
    p.vlkp = 0.2f;
    p.vlki = 1.0f;
    p.plkp = 1.0f;
    p.plkd = 0.01f;
    p.mit_kpmax = 100.0f;
    p.mit_kdmax = 10.0f;
    p.mit_tmax = 1.0f;
    p.cfalpha = 1.0f;
    p.traj_limit_d1 = 2.0f;
    p.traj_limit_d2 = 4.0f;
    p.traj_limit_d3 = 20.0f;
    p.traj_tolerance = 1e-4f;
    return p;
}

static void fixture(tMotorCtrl *m, tMock *h, tParameter p) {
    memset(h, 0, sizeof(*h));
    h->motor = m;
    h->sample.vbus = 24.0f;
    h->sample.rotor_valid = true;
    h->sample.phase_mask = MC_PHASE_ALL;
    tMotorOps port = {.user = h, .read_sample = read_sample, .write_pwm = write_pwm,
        .set_enable = set_enable, .enter_critical = enter_critical,
        .exit_critical = exit_critical};
    tMotorTiming timing = mc_timing_default(0.00005f);
    assert(motor_init(m, &p, &timing, &port) == MC_OK);
    assert(!h->enabled);
}

static void run(tMotorCtrl *m, tCmd cmd) {
    assert(motor_submit(m, &cmd) == MC_OK);
    motor_tick(m); /* 首次有效采样，仍未使能。 */
    assert(motor_start(m) == MC_OK);
    motor_tick(m);
    assert(m->state == MC_RUNNING);
}

/* 用三相线电压重建 alpha/beta，独立验证最终 PWM 的物理输出。 */
static tAb pwm_voltage(const tPwm *pwm, float bus) {
    float mean = (pwm->duty[0] + pwm->duty[1] + pwm->duty[2]) / 3.0f;
    return (tAb){(pwm->duty[0] - mean) * bus,
                 (pwm->duty[1] - pwm->duty[2]) * bus / sqrtf(3.0f)};
}

static void test_reconstruction_and_transforms(void) {
    const unsigned masks[] = {3, 5, 6, 7};
    for (unsigned n = 0; n < 4; ++n) {
        tMcFoc foc;
        foc_init(&foc);
        tSample s = {.current = {1.0f, 2.0f, -3.0f},
                      .phase_mask = (uint8_t)masks[n]};
        for (unsigned i = 0; i < 3; ++i)
            if (!(masks[n] & (1u << i)))
                s.current[i] = NAN;
        assert(foc_current(&foc, &s, 1.0f));
        foc_park(&foc, XMATH_PI / 2);
        NEAR(foc.is[0], 1.0, 1e-6);
        NEAR(foc.is[1], 2.0, 1e-6);
        NEAR(foc.is[2], -3.0, 1e-6);
        NEAR(foc.i_dq.q, -1.0, 1e-5);
        NEAR(foc.i_dq.d, 5.0 * XMATH_INSQRT3, 1e-5);
    }
    tMcFoc f;
    foc_init(&f);
    tSample invalid = {.phase_mask = MC_PHASE_U};
    assert(!foc_current(&f, &invalid, 1));
    for (unsigned degree = 0; degree < 360; ++degree) {
        float angle = degree * XMATH_PI / 180.0f;
        foc_park(&f, angle);
        foc_pwm(&f, (tDq){3 * XMATH_SQRT3 / 24, 0});
        tAb v = pwm_voltage(&f.pwm, 24);
        NEAR(v.alpha, 3 * cosf(angle), 3e-6);
        NEAR(v.beta, 3 * sinf(angle), 3e-6);
        assert(f.pwm.sector >= 1 && f.pwm.sector <= 6);
    }
    foc_park(&f, 0);
    foc_pwm(&f, (tDq){0, 1});
    assert(f.pwm.sector == 2); /* 转子 0 度，电压 90 度。 */
}

/* 与独立的电压/角度参考比较，验证去掉 atan2 和重复限幅后的调制。 */
static void test_modulation_equivalence(void) {
    tMcFoc f;
    foc_init(&f);
    for (unsigned n = 0; n < 720; ++n) {
        float angle = (n * 0.5f + 0.13f) * XMATH_PI / 180.0f;
        float dq_angle = (n * 1.5f + 0.21f) * XMATH_PI / 180.0f;
        foc_park(&f, angle);
        for (unsigned radius = 1; radius <= 4; ++radius) {
            float magnitude = radius * 0.25f;
            tDq u = {magnitude * cosf(dq_angle), magnitude * sinf(dq_angle)};
            float bus = n & 1 ? 12.0f : 48.0f;
            foc_pwm(&f, u);
            tAb v = pwm_voltage(&f.pwm, bus);
            float theta = angle + dq_angle;
            float base = bus / sqrtf(3.0f);
            NEAR(v.alpha, magnitude * base * cosf(theta), 3e-5);
            NEAR(v.beta, magnitude * base * sinf(theta), 3e-5);
            float phase = atan2f(v.beta, v.alpha);
            if (phase < 0)
                phase += XMATH_2PI;
            unsigned sector = (unsigned)(phase / XMATH_PI_3) + 1u;
            assert(f.pwm.sector == sector);
        }
    }
    /* 每条边界两侧，以及可精确表示的 0/180 度边界和零矢量。 */
    foc_park(&f, 0);
    for (unsigned boundary = 0; boundary < 6; ++boundary) {
        for (int side = -1; side <= 1; side += 2) {
            float angle = boundary * XMATH_PI_3 + side * 0.0001f;
            foc_pwm(&f, (tDq){cosf(angle), sinf(angle)});
            unsigned expected = side < 0 ? (boundary + 5u) % 6u + 1u : boundary + 1u;
            assert(f.pwm.sector == expected);
        }
    }
    foc_pwm(&f, (tDq){1, 0});
    assert(f.pwm.sector == 1);
    foc_pwm(&f, (tDq){-1, 0});
    assert(f.pwm.sector == 4);
    foc_pwm(&f, (tDq){0, 0});
    assert(f.pwm.sector == 0);
    for (unsigned i = 0; i < 3; ++i)
        NEAR(f.pwm.duty[i], 0.5f, 1e-7);
}

static void test_sensor_coordinates(void) {
    tParameter p = parameters();
    tMcSensor s;
    sensor_init(&s, &p, 0.001f);
    assert(sensor_update(&s, 6.2f, true));
    NEAR(s.rotor.pos, 6.2f, 1e-6); /* 首帧不伪造跨圈。 */
    sensor_zero(&s);
    assert(sensor_update(&s, 0.1f, true));
    NEAR(s.rotor.pos, XMATH_2PI - 6.2f + 0.1f, 1e-6);
    assert(s.rotor.vel > 0);
    double pos = s.rotor.pos;
    p.positive_dir = false;
    tMcSensor reverse;
    sensor_init(&reverse, &p, 0.001f);
    assert(sensor_update(&reverse, 6.2f, true));
    sensor_zero(&reverse);
    assert(sensor_update(&reverse, 0.1f, true));
    NEAR(reverse.rotor.pos, -pos, 1e-6);
    NEAR(reverse.rotor.vel, -s.rotor.vel, 1e-6);
    NEAR(sin(reverse.rotor.theta_e), -sin(s.rotor.theta_e), 2e-6);
    assert(!sensor_update(&reverse, NAN, false));
    NEAR(reverse.rotor.pos, -pos, 1e-6);
}

static void test_pid_and_mit(void) {
    tMotorTiming timing = mc_timing_default(0.0001f); /* MIT 实际更新周期 1 ms。 */
    tPID pid;
    pid_init(&pid, 0, 0, 1, 1, 0.01f, -1000, 1000);
    NEAR(pid_update(&pid, 10, 5), 0, 1e-6);
    assert(pid_update(&pid, 10, 6) < 0);
    tPI pi;
    pi_init(&pi, 1, 10, 0.001f, -1, 1);
    for (unsigned i = 0; i < 10000; ++i)
        NEAR(pi_update(&pi, 100, 0), 1, 1e-6);
    NEAR(pi.integral, 0, 1e-6);
    assert(pi_update(&pi, -1, 0) < 0);
    /* 离散系数在 init 固定，PI 的运行状态没有微分项。 */
    assert(sizeof(tPI) < sizeof(tPID));
    pi_init(&pi, 2, 4, 0.01f, -10, 10);
    NEAR(pi_update(&pi, 1, 0), 2.04, 1e-6);
    NEAR(pi_update(&pi, 1, 0.5f), 1.06, 1e-6);
    pi_track(&pi, 0.5f);
    NEAR(pi_update(&pi, 0.5f, 0.5f), -0.5, 1e-6);
    pi_reset(&pi);
    NEAR(pi_update(&pi, 0.5f, 0.5f), 0, 1e-6);
    pi_init(&pi, 2, 0, 0.01f, -10, 10);
    pi_update(&pi, 1, 0);
    pi_track(&pi, 1);
    NEAR(pi.integral, 0, 1e-6); /* Ki 为零时不由 tracking 引入隐含积分。 */
    pid_init(&pid, 2, 4, 0.5f, 1, 0.01f, -100, 100);
    NEAR(pid_update(&pid, 1, 0), 2.04, 1e-6);
    NEAR(pid_update(&pid, 1, 0.1f), -3.124, 1e-5);
    pid_reset(&pid);
    NEAR(pid_update(&pid, 1, 0.1f), 1.836, 1e-5);

    tParameter p = parameters();
    tCtrl c;
    ctrl_init(&c, &p, &timing);
    tRef t = {.pos = 0.1f, .kp = 1.0f};
    tRotor r = {0};
    ctrl_cmd(&c, &t, MC_MODE_MIT, false);
    ctrl_mit(&c, &t, &r);
    NEAR(c.qref, 0.5, 1e-6);
    float old = c.qref;
    t.kp = 2;
    ctrl_cmd(&c, &t, MC_MODE_MIT, false);
    ctrl_mit(&c, &t, &r);
    NEAR(c.qref, old, 1e-6);
    for (unsigned i = 0; i < 1000; ++i)
        ctrl_mit(&c, &t, &r);
    NEAR(c.qref, 1.0, 1e-5);
    NEAR(c.mit.comp, 0, 1e-7);
    ctrl_init(&c, &p, &timing);
    t = (tRef){0};
    for (unsigned i = 0; i < 100; ++i) {
        ctrl_mit(&c, &t, &r);
        NEAR(c.qref, 0, 1e-7);
        NEAR(c.mit.comp, 0, 1e-7);
    }
}

static void test_current_limits_and_weakening(void) {
    tMotorTiming timing = mc_timing_default(0.00005f);
    tParameter p = parameters();
    p.fwlkp = 0;
    p.fwlki = 1;
    tCtrl c;
    ctrl_init(&c, &p, &timing);
    c.u2 = 1.0f;
    c.qref = p.motor_rated_current;
    ctrl_cmd(&c, &(tRef){0}, MC_MODE_VELOCITY, true);
    ctrl_current(&c, (tDq){0, 0});
    tDq voltage = c.u;
    NEAR(c.iref.d, -0.05 * 0.00005, 1e-10);
    assert(c.iref.d <= 0);
    assert(hypotf(c.iref.d, c.iref.q) <= p.motor_rated_current + 1e-5);
    assert(hypotf(voltage.d, voltage.q) <= 1.0f + 1e-5);
    ctrl_cmd(&c, &(tRef){0}, MC_MODE_VELOCITY, false);
    ctrl_current(&c, (tDq){0, 0});
    NEAR(c.iref.d, 0, 1e-7);
    NEAR(c.fw.integral, 0, 1e-7);

    p.qclkp = p.dclkp = 1;
    ctrl_init(&c, &p, &timing);
    c.qref = 3;
    ctrl_current(&c, (tDq){-3, 0});
    voltage = c.u;
    NEAR(hypotf(voltage.d, voltage.q), 1.0f, 1e-5);
    assert(c.id.integral < 0 && c.iq.integral < 0);
}

static void trajectory_init(tTraj *t, const tParameter *p, double initial) {
    tTrajCfg cfg = {
        .mode = (tTrajMode)p->traj_mode,
        .vmax = p->traj_limit_d1, .amax = p->traj_limit_d2,
        .jmax = p->traj_limit_d3,
        .tol = p->traj_tolerance > 0 ? p->traj_tolerance : 0.001f
    };
    traj_init(t, &cfg, initial);
}

static void test_trajectory(void) {
    tParameter p = parameters();
    for (unsigned mode = TRAJ_TRAPEZOID; mode <= TRAJ_S_CURVE; ++mode) {
        p.traj_mode = (uint8_t)mode;
        tTraj t;
        trajectory_init(&t, &p, 0);
        traj_set_target(&t, 1.0);
        double previous_v = 0, previous_a = 0;
        for (unsigned i = 0; i < 20000; ++i) {
            if (i == 300)
                traj_set_target(&t, -0.75);
            (void)traj_plan(&t, 0.001f);
            traj_update(&t, 0.001f);
            assert(isfinite(t.value));
            assert(fabs(t.vel) <= p.traj_limit_d1 + 1e-5);
            assert(fabs(t.acceleration) <= p.traj_limit_d2 + 1e-5);
            assert(fabs(t.vel - previous_v) <= p.traj_limit_d2 * 0.001 + 1e-5);
            if (mode == TRAJ_S_CURVE)
                assert(fabs(t.acceleration - previous_a) <= p.traj_limit_d3 * 0.001 + 1e-5);
            previous_v = t.vel;
            previous_a = t.acceleration;
        }
        assert(!t.busy);
        NEAR(t.value, -0.75, p.traj_tolerance);
        NEAR(t.vel, 0, 1e-6);
        NEAR(t.acceleration, 0, 1e-6);
    }
    /* 连续在线改目标、不同距离和非零位置，不以终点清零掩盖导数跳变。 */
    p.traj_mode = TRAJ_S_CURVE;
    tTraj t;
    trajectory_init(&t, &p, 3.0);
    uint32_t random = 17u;
    double last_v = 0, last_a = 0;
    for (unsigned i = 0; i < 100000; ++i) {
        if (i < 50000 && i % 137u == 0) {
            random = random * 1664525u + 1013904223u;
            double ref = ((double)(random & 0xFFFFu) / 65535.0 - 0.5) * 10.0;
            traj_set_target(&t, ref);
        }
        (void)traj_plan(&t, 0.001f);
        traj_update(&t, 0.001f);
        assert(fabs(t.vel) <= p.traj_limit_d1 + 1e-5);
        assert(fabs(t.acceleration) <= p.traj_limit_d2 + 1e-5);
        assert(fabs(t.vel - last_v) <= p.traj_limit_d2 * 0.001 + 1e-5);
        assert(fabs(t.acceleration - last_a) <= p.traj_limit_d3 * 0.001 + 1e-5);
        last_v = t.vel;
        last_a = t.acceleration;
    }
    if (t.busy)
        fprintf(stderr, "traj busy: value=%g ref=%g v=%g a=%g duration=%g elapsed=%g replan=%d\n",
            t.value, t.requested, t.vel, t.acceleration, t.duration, t.elapsed, t.replan);
    assert(!t.busy);
    NEAR(t.value, t.requested, 1e-5);
}

static void test_start_and_multirate(void) {
    tMotorCtrl m;
    tMock h;
    tParameter p = parameters();
    fixture(&m, &h, p);
    tCmd c = cmd_default(MC_MODE_OPENLOOP, &p);
    c.value.openloop.angle = 1.0f;
    c.value.openloop.voltage = 2.0f;
    assert(motor_submit(&m, &c) == MC_OK);
    assert(motor_start(&m) == MC_NOT_READY);
    motor_tick(&m);
    assert(motor_start(&m) == MC_OK);
    assert(!h.enabled);
    motor_tick(&m);
    assert(h.enabled && m.state == MC_RUNNING);
    tMotorFb fb;
    assert(motor_get_fb(&m, &fb) == MC_OK);
    NEAR(fb.applied_voltage.alpha, 2 * cosf(1), 1e-5);
    NEAR(fb.applied_voltage.beta, 2 * sinf(1), 1e-5);
    for (unsigned i = 1; i < 100; ++i)
        motor_tick(&m);
    assert(m.cmd_tick == 0 && m.vel_tick == 0 && m.pos_tick == 0);
    assert(m.traj_seq == 1); /* 开环不执行轨迹和外环任务。 */
    assert(motor_config(&m, &p) == MC_BUSY);
    tCmd other = cmd_default(MC_MODE_CURRENT, &p);
    assert(motor_submit(&m, &other) == MC_BUSY);
    motor_stop(&m);
    assert(!h.enabled);
    assert(motor_start(&m) == MC_NOT_READY);
    motor_tick(&m);
    assert(m.state == MC_IDLE);
    assert(motor_start(&m) == MC_NOT_READY);
    assert(motor_config(&m, &p) == MC_OK);
}

/* 用实际积分输出检查分频，不在固件里保留额外统计计数器。 */
static void test_outer_periods(void) {
    tMotorCtrl m;
    tMock h;
    tParameter p = parameters();
    p.vlkp = 0;
    p.vlki = 1;
    fixture(&m, &h, p);
    tCmd cmd = cmd_default(MC_MODE_VELOCITY, &p);
    cmd.value.vel = 1;
    run(&m, cmd);
    for (unsigned tick = 0; tick < 101; ++tick) {
        float expected = (tick / m.timing.vel_div + 1u) * m.timing.dt * m.timing.vel_div;
        NEAR(m.ctrl.qref, expected, 1e-7);
        assert(m.pos_tick == 0); /* 速度模式不调度位置环。 */
        motor_tick(&m);
    }
    p.pos_pidmode = POS_PID_PVC;
    p.plkp = p.plkd = 0;
    p.plki = 1;
    p.vlkp = 1;
    p.vlki = 0;
    fixture(&m, &h, p);
    cmd = cmd_default(MC_MODE_POSITION, &p);
    cmd.value.pos = 1;
    run(&m, cmd);
    for (unsigned tick = 0; tick < 101; ++tick) {
        float expected = (tick / m.timing.pos_div + 1u) * m.timing.dt * m.timing.pos_div;
        NEAR(m.ctrl.vref, expected, 1e-7);
        NEAR(m.ctrl.qref, expected, 1e-7);
        motor_tick(&m);
    }
    motor_stop(&m);
    assert(m.state == MC_IDLE && !h.enabled);
    /* 重新采样、提交并启动时，只复位一次控制器，不沿用旧积分。 */
    run(&m, cmd);
    NEAR(m.ctrl.vref, m.timing.dt * m.timing.pos_div, 1e-7);
}

static void test_openloop_voltage_limit(void) {
    tMotorCtrl m;
    tMock h;
    tParameter p = parameters();
    fixture(&m, &h, p);
    tCmd cmd = cmd_default(MC_MODE_OPENLOOP, &p);
    cmd.value.openloop.voltage = 100;
    run(&m, cmd);
    for (unsigned n = 0; n < 360; ++n) {
        cmd.value.openloop.angle = n * XMATH_PI / 180.0f;
        assert(motor_submit(&m, &cmd) == MC_OK);
        motor_tick(&m);
        tMotorFb fb;
        assert(motor_get_fb(&m, &fb) == MC_OK);
        NEAR(hypotf(fb.applied_voltage.alpha, fb.applied_voltage.beta), 24 / sqrtf(3.0f), 1e-5);
        NEAR(fb.voltage_utilization, 1, 1e-7);
    }
}

static void test_foreground_planning(void) {
    tMotorCtrl m;
    tMock h;
    tParameter p = parameters();
    p.traj_mode = TRAJ_S_CURVE;
    fixture(&m, &h, p);
    tCmd cmd = cmd_default(MC_MODE_POSITION, &p);
    cmd.value.pos = 1;
    run(&m, cmd);
    for (unsigned i = 0; i < 100; ++i)
        motor_tick(&m);
    NEAR(m.cmd.ref.pos, 0, 1e-7); /* 快环不搜索新轨迹。 */
    assert(m.cmd.traj.replan);
    assert(motor_service(&m) == MC_OK);
    assert(!m.cmd.traj.replan);
    for (unsigned i = 0; i < 1000; ++i)
        motor_tick(&m);
    assert(m.cmd.ref.pos > 0);
    cmd.value.pos = -1;
    assert(motor_submit(&m, &cmd) == MC_OK);
    motor_tick(&m);
    h.interrupt_at_lock = h.lock_calls + 2u;
    assert(motor_service(&m) == MC_BUSY);
    assert(!h.enabled && m.state == MC_FAULT);
    assert(m.cmd.traj.replan); /* 过期规划没有覆盖故障之后的状态。 */
}

static void test_first_closed_loop_angle_and_modes(void) {
    tParameter p = parameters();
    p.motor_polepairs = 1;
    p.qclkp = p.qclki = p.dclki = 0;
    p.dclkp = 0.1f;
    tMotorCtrl m;
    tMock h;
    fixture(&m, &h, p);
    h.sample.angle = XMATH_PI / 2;
    h.sample.current[0] = 1.0f;
    h.sample.current[1] = h.sample.current[2] = -0.5f;
    run(&m, cmd_default(MC_MODE_CURRENT, &p));
    NEAR(m.foc.i_dq.d, 0, 1e-6);
    NEAR(m.foc.i_dq.q, -1, 1e-6);
    NEAR(m.ctrl.u2, 0, 1e-6);
    for (tMcMode mode = MC_MODE_CURRENT; mode <= MC_MODE_MIT; ++mode) {
        p = parameters();
        fixture(&m, &h, p);
        tCmd c = cmd_default(mode, &p);
        switch (mode) {
        case MC_MODE_CURRENT: c.value.current = 1; break;
        case MC_MODE_TORQUE: c.value.torque = 0.2f; break;
        case MC_MODE_VELOCITY: c.value.vel = 1; break;
        case MC_MODE_POSITION: c.value.pos = 1; break;
        case MC_MODE_MIT:
            c.value.mit.tau_ff = 0.2f;
            c.value.mit.vel = 0.3f;
            break;
        default: break;
        }
        run(&m, c);
        assert(m.ctrl.iref.q > 0);
        assert(m.ctrl.iref.q <= p.motor_rated_current);
        if (mode == MC_MODE_MIT) {
            NEAR(m.cmd.ref.tau_ff, 0.2, 1e-6);
            NEAR(m.cmd.ref.vel, 0.3, 1e-6);
        }
    }
}

static void test_faults_and_interrupts(void) {
    for (unsigned which = 0; which < 5; ++which) {
        tMotorCtrl m;
        tMock h;
        tParameter p = parameters();
        fixture(&m, &h, p);
        tCmd c = cmd_default(MC_MODE_CURRENT, &p);
        c.value.current = 1;
        run(&m, c);
        switch (which) {
        case 0: h.sample.rotor_valid = false; break;
        case 1: h.sample.vbus = 0; break;
        case 2: h.sample.phase_mask = MC_PHASE_U; break;
        case 3: h.stale_sample = true; break;
        case 4: h.bad_sample = true; break;
        }
        motor_tick(&m);
        assert(!h.enabled && m.state == MC_FAULT && m.faults);
        assert(motor_submit(&m, &c) == MC_FAULT_LATCHED);
        motor_stop(&m);
        motor_tick(&m);
        assert(m.state == MC_FAULT);
        assert(motor_clear_fault(&m) == MC_OK);
        assert(motor_start(&m) == MC_NOT_READY);
    }
    for (unsigned action = 0; action < 3; ++action) {
        tMotorCtrl m;
        tMock h;
        tParameter p = parameters();
        fixture(&m, &h, p);
        run(&m, cmd_default(MC_MODE_CURRENT, &p));
        unsigned enables = h.enables;
        h.interrupt_stop = action == 1;
        h.interrupt_clear = action == 2;
        h.interrupt_at_lock = h.lock_calls + 3u; /* 输出提交前抢占。 */
        motor_tick(&m);
        assert(!h.enabled && h.enables == enables);
        if (action == 0)
            assert(m.faults & MC_FAULT_EXTERNAL);
        motor_tick(&m);
        assert(!h.enabled && h.enables == enables);
    }
}

static void test_config_and_instance_isolation(void) {
    tParameter p = parameters();
    tMotorTiming timing = mc_timing_default(0.00005f);
    assert(mc_config_validate(&p, &timing) == MC_OK);
    p.cfalpha = 1.1f;
    assert(mc_config_validate(&p, &timing) == MC_INVALID_CONFIG);
    p = parameters();
    p.traj_mode = TRAJ_S_CURVE;
    p.traj_limit_d3 = 0;
    assert(mc_config_validate(&p, &timing) == MC_INVALID_CONFIG);
    p = parameters();
    tCmd cmd = cmd_default(MC_MODE_TORQUE, &p);
    p.motor_ke = 0;
    assert(cmd_validate(&cmd, &p) == MC_INVALID_CONFIG);
    p = parameters();
    cmd.mode = MC_MODE_CURRENT;
    p.sensorless = SENSORLESS_HFISMO;
    assert(cmd_validate(&cmd, &p) == MC_UNSUPPORTED);
    p = parameters();
    cmd.mode = (tMcMode)-1;
    assert(cmd_validate(&cmd, &p) == MC_INVALID_ARGUMENT);

    tMotorCtrl a, b;
    tMock ha, hb;
    p = parameters();
    fixture(&a, &ha, p);
    fixture(&b, &hb, p);
    tCmd ca = cmd_default(MC_MODE_CURRENT, &p);
    ca.value.current = 1;
    run(&a, ca);
    for (unsigned i = 0; i < 100; ++i)
        motor_tick(&a);
    assert(a.ctrl.iq.integral != b.ctrl.iq.integral);
    assert(!hb.enabled && b.state == MC_IDLE && b.pwm.sequence == 1);
    p.qclkp = 99;
    NEAR(a.param.qclkp, 0.1, 1e-6); /* 外部协议参数修改不渗入快环。 */
    motor_fault(&a, MC_FAULT_EXTERNAL);
    assert(!hb.enabled && b.faults == 0);
    tMotorFb fb;
    assert(motor_get_fb(&a, &fb) == MC_OK);
    assert(fb.state == MC_FAULT && !fb.pwm.enabled);
    assert(ha.depth == 0 && hb.depth == 0);
    tMotorOps port = a.port;
    p.motor_rated_current = 0;
    ha.enabled = true;
    tMotorCtrl invalid;
    assert(motor_init(&invalid, &p, &timing, &port) == MC_INVALID_CONFIG);
    assert(!ha.enabled);
}

int main(void) {
    test_reconstruction_and_transforms();
    test_modulation_equivalence();
    test_sensor_coordinates();
    test_pid_and_mit();
    test_current_limits_and_weakening();
    test_trajectory();
    test_start_and_multirate();
    test_outer_periods();
    test_openloop_voltage_limit();
    test_foreground_planning();
    test_first_closed_loop_angle_and_modes();
    test_faults_and_interrupts();
    test_config_and_instance_isolation();
    puts("motor regression tests passed");
    return 0;
}
