#include "mc_motor.h"
#include "bsp_math.h"
#include "bsp_critical.h"
#include "common/trajectory.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define NEAR(a, b, eps) assert(fabs((double)(a) - (double)(b)) <= (eps))

typedef struct {
    tMotorCtrl *motor;
    tMcSample sample;
    tPwm written;
    unsigned writes, enables, lock_calls, depth, interrupt_at_lock;
    bool enabled, interrupt_restart;
} tMock;

static tMock *active_mock;

static void read_sample(tMcSample *sample) {
    tMock *h = active_mock;
    *sample = h->sample;
}

static void write_pwm(const tPwm *pwm) {
    tMock *h = active_mock;
    h->written = *pwm;
    ++h->writes;
    for (unsigned i = 0; i < 3; ++i)
        assert(isfinite(pwm->duty[i]) && pwm->duty[i] >= 0 && pwm->duty[i] <= 1);
}

static void set_enable(bool enable) {
    tMock *h = active_mock;
    if (enable) {
        assert(h->writes > 0); /* PWM 必须在使能之前写入。 */
        ++h->enables;
    }
    h->enabled = enable;
}

uint32_t bsp_critical_enter(void) {
    tMock *h = active_mock;
    ++h->lock_calls;
    /* 模拟在进入临界区之前被高优先级停机中断抢占。 */
    if (h->interrupt_at_lock == h->lock_calls) {
        h->interrupt_at_lock = 0;
        motor_stop(h->motor);
        if (h->interrupt_restart) {
            tCmdCfg cfg = {.mode = MC_MODE_CURRENT};
            motor_cmd_cfg(h->motor, &cfg);
        }
    }
    return h->depth++;
}

void bsp_critical_exit(uint32_t token) {
    tMock *h = active_mock;
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
    active_mock = h;
    tMotorOps port = {
        .read_sample = read_sample,
        .write_pwm = write_pwm,
        .set_enable = set_enable
    };
    tMotorTiming timing = mc_timing_default(0.00005f);
    motor_init(m, &p, &timing, &port);
    assert(!h->enabled);
}

static void run_config(tMotorCtrl *m, tCmdCfg cfg, tCmdValue cmd) {
    motor_cmd_cfg(m, &cfg);
    assert(m->mode == MC_MODE_STARTING);
    motor_cmd_set(m, &cmd);
    for (unsigned slot = 0; slot < 3; ++slot) {
        motor_tick(m);
        assert(m->mode == (slot < 2 ? MC_MODE_STARTING : cfg.mode));
        assert(!m->pwm.enabled);
    }
    motor_tick(m);
    assert(m->mode == cfg.mode && m->pwm.enabled);
}

static void run(tMotorCtrl *m, tCmdValue cmd) {
    run_config(m, cmd_default(cmd.mode, &m->param), cmd);
}

/* 用三相线电压重建 alpha/beta，独立验证最终 PWM 的物理输出。 */
static tAb pwm_voltage(const tPwm *pwm, float bus) {
    float mean = (pwm->duty[0] + pwm->duty[1] + pwm->duty[2]) / 3.0f;
    return (tAb){(pwm->duty[0] - mean) * bus,
                 (pwm->duty[1] - pwm->duty[2]) * bus / sqrtf(3.0f)};
}

static void test_current_transforms(void) {
    const float current[3] = {1.0f, 2.0f, -3.0f};
    tMcSensor sensor;
    tParameter p = parameters();
    sensor_init(&sensor, &p, 0.00005f);
    sensor_set_electrical_angle(&sensor, MATH_PI / 2);
    sensor_current(&sensor, current);
    NEAR(sensor.is[0], 1.0f, 1e-6);
    NEAR(sensor.is[1], 2.0f, 1e-6);
    NEAR(sensor.is[2], -3.0f, 1e-6);
    NEAR(sensor.i_dq.d, 5.0f * MATH_INSQRT3, 1e-5);
    NEAR(sensor.i_dq.q, -1.0f, 1e-5);

    sensor_set_electrical_angle(&sensor, 0);
    sensor_current(&sensor, (float[3]){1.0f, -0.5f, -0.5f});
    sensor_set_electrical_angle(&sensor, MATH_PI / 2);
    NEAR(sensor.i_dq.d, 1.0f, 1e-6);
    NEAR(sensor.i_dq.q, 0.0f, 1e-6);

    tPwm pwm;
    mc_pwm_modulate((tDq){0.5f, 0.0f}, sensor.sin, sensor.cos, pwm.duty);
    tAb applied = pwm_voltage(&pwm, 24.0f);
    NEAR(applied.alpha, 0.0f, 1e-6);
    NEAR(applied.beta, 12.0f * MATH_INSQRT3, 1e-6);
}

/* 与独立的电压/角度参考比较。 */
static void test_modulation_equivalence(void) {
    tPwm pwm;
    for (unsigned n = 0; n < 720; ++n) {
        float angle = (n * 0.5f + 0.13f) * MATH_PI / 180.0f;
        float dq_angle = (n * 1.5f + 0.21f) * MATH_PI / 180.0f;
        for (unsigned radius = 1; radius <= 4; ++radius) {
            float magnitude = radius * 0.25f;
            tDq u = {magnitude * cosf(dq_angle), magnitude * sinf(dq_angle)};
            float bus = n & 1 ? 12.0f : 48.0f;
            mc_pwm_modulate(u, sinf(angle), cosf(angle), pwm.duty);
            tAb v = pwm_voltage(&pwm, bus);
            float theta = angle + dq_angle;
            float base = bus / sqrtf(3.0f);
            NEAR(v.alpha, magnitude * base * cosf(theta), 3e-5);
            NEAR(v.beta, magnitude * base * sinf(theta), 3e-5);
        }
    }
    mc_pwm_modulate((tDq){0}, 0, 1, pwm.duty);
    for (unsigned i = 0; i < 3; ++i)
        NEAR(pwm.duty[i], 0.5f, 1e-7);
}

static void test_sensor_coordinates(void) {
    tParameter p = parameters();
    tMcSensor s;
    sensor_init(&s, &p, 0.001f);
    sensor_update(&s, 6.2f);
    NEAR(s.rotor.pos, 6.2f, 1e-6); /* 首帧不伪造跨圈。 */
    sensor_zero(&s);
    sensor_update(&s, 0.1f);
    NEAR(s.rotor.pos, MATH_2PI - 6.2f + 0.1f, 1e-6);
    assert(s.rotor.vel > 0);
    double pos = s.rotor.pos;
    p.positive_dir = false;
    tMcSensor reverse;
    sensor_init(&reverse, &p, 0.001f);
    sensor_update(&reverse, 6.2f);
    sensor_zero(&reverse);
    sensor_update(&reverse, 0.1f);
    NEAR(reverse.rotor.pos, -pos, 1e-6);
    NEAR(reverse.rotor.vel, -s.rotor.vel, 1e-6);
    NEAR(sin(reverse.rotor.theta_e), -sin(s.rotor.theta_e), 2e-6);
    p.sensorless = SENSORLESS_HFISMO;
    sensor_init(&s, &p, 0.001f);
    sensor_update(&s, 0.1f);
    assert(!s.seeded);
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
    pi_reset(&pi);
    NEAR(pi_update(&pi, 0.5f, 0.5f), 0, 1e-6);
    pid_init(&pid, 2, 4, 0.5f, 1, 0.01f, -100, 100);
    NEAR(pid_update(&pid, 1, 0), 2.04, 1e-6);
    NEAR(pid_update(&pid, 1, 0.1f), -3.124, 1e-5);
    pid_reset(&pid);
    NEAR(pid_update(&pid, 1, 0.1f), 1.836, 1e-5);

    tParameter p = parameters();
    tCtrl c;
    tCmdCfg cfg = cmd_default(MC_MODE_MIT, &p);
    cfg.config.mit.kp = 1.0f;
    ctrl_configure(&c, &p, &timing, &cfg);
    tRef ref = {.pos = 0.1f};
    tRotor rotor = {0};
    ctrl_position_step(&c, &ref, &rotor);
    NEAR(c.qref, 0.5, 1e-6);
    ctrl_velocity_step(&c, &ref, &rotor);
    NEAR(c.qref, 0.5, 1e-6);
    cfg.config.mit.kp = 2.0f;
    ctrl_configure(&c, &p, &timing, &cfg);
    ctrl_position_step(&c, &ref, &rotor);
    NEAR(c.qref, 1.0, 1e-6);
}

static void test_current_limits_and_weakening(void) {
    tMotorTiming timing = mc_timing_default(0.00005f);
    tParameter p = parameters();
    p.fwlkp = 0;
    p.fwlki = 1;
    tCtrl c;
    tCmdCfg cfg = {.mode = MC_MODE_VELOCITY, .config.fw_enable = true};
    ctrl_configure(&c, &p, &timing, &cfg);
    c.qref = p.motor_rated_current;
    ctrl_step(&c, NULL, (tDq){0});
    NEAR(c.iref.d, 0, 1e-7);
    c.duty = (tDq){1, 0};
    ctrl_measure(&c, (tDq){3, 4});
    NEAR(c.current_ref, p.motor_rated_current, 1e-7);
    NEAR(c.current_fb, 5.0f, 1e-7);
    NEAR(c.duty_output, 1.0f, 1e-7);
    ctrl_velocity_step(&c, &(tRef){0}, &(tRotor){0});
    tDq duty = ctrl_step(&c, NULL, (tDq){0});
    NEAR(c.iref.d, -0.05f * timing.dt * timing.medium_div, 1e-10);
    assert(hypotf(c.iref.d, c.iref.q) <= p.motor_rated_current + 1e-5);
    assert(hypotf(duty.d, duty.q) <= 1.0f + 1e-5);

    p.qclkp = p.dclkp = 1;
    cfg = (tCmdCfg){.mode = MC_MODE_CURRENT};
    ctrl_configure(&c, &p, &timing, &cfg);
    duty = ctrl_step(&c, &(tRef){.iq = 3}, (tDq){-3, 0});
    NEAR(hypotf(duty.d, duty.q), 1.0f, 1e-5);
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
    tCmdValue c = {.mode = MC_MODE_OPENLOOP};
    c.value.openloop.value = 1.0f;
    c.value.openloop.duty = 0.25f;
    motor_cmd_set(&m, &c);
    assert(m.mode == MC_MODE_IDLE && !m.value_pending);
    run(&m, c);
    assert(h.enabled);
    tAb applied = pwm_voltage(&h.written, m.vbus);
    NEAR(applied.alpha, 6.0f * MATH_INSQRT3 * cosf(1), 1e-5);
    NEAR(applied.beta, 6.0f * MATH_INSQRT3 * sinf(1), 1e-5);
    for (unsigned i = 1; i < 100; ++i)
        motor_tick(&m);
    tParameter modified = p;
    modified.qclkp = 0.0f;
    motor_config(&m, &modified);
    assert(m.param.qclkp == p.qclkp);
    tCmdValue other = {.mode = MC_MODE_CURRENT};
    motor_cmd_set(&m, &other);
    assert(!m.value_pending);
    motor_stop(&m);
    assert(!h.enabled);
    assert(m.mode == MC_MODE_IDLE);
    motor_tick(&m);
    assert(m.mode == MC_MODE_IDLE);
    motor_config(&m, &p);
}

/* 用实际积分输出检查分频，不在固件里保留额外统计计数器。 */
static void test_outer_periods(void) {
    tMotorCtrl m;
    tMock h;
    tParameter p = parameters();
    p.vlkp = 0;
    p.vlki = 1;
    p.traj_limit_d1 = p.traj_limit_d2 = 0.0f;
    fixture(&m, &h, p);
    tCmdValue cmd = {.mode = MC_MODE_VELOCITY};
    cmd.value.vel = 1;
    run(&m, cmd);
    float medium_dt = m.timing.dt * m.timing.medium_div;
    for (unsigned tick = 0; tick < 111; ++tick) {
        unsigned updates = tick < 1 ? 0 : 1 + (tick - 1) / m.timing.medium_div;
        NEAR(m.ctrl.qref, updates * medium_dt, 1e-7);
        motor_tick(&m);
    }

    p.plkp = p.plkd = 0;
    p.plki = 1;
    p.vlkp = 1;
    p.vlki = 0;
    for (unsigned cascade = 0; cascade < 2; ++cascade) {
        p.pos_pidmode = cascade ? POS_PID_PVC : POS_PID_PC;
        fixture(&m, &h, p);
        cmd = (tCmdValue){.mode = MC_MODE_POSITION};
        cmd.value.position.pos = 1;
        run(&m, cmd);
        unsigned factor = cascade ? MC_PVC_DIV : 1;
        unsigned pos_period = m.timing.medium_div * factor;
        float pos_dt = medium_dt * factor;
        for (unsigned tick = 0; tick < 111; ++tick) {
            unsigned pos_updates = 1 + tick / pos_period;
            unsigned q_updates = cascade && (tick % pos_period == 0)
                ? pos_updates - 1 : pos_updates;
            if (cascade)
                NEAR(m.ctrl.vref, pos_updates * pos_dt, 1e-7);
            NEAR(m.ctrl.qref, q_updates * pos_dt, 1e-7);
            motor_tick(&m);
        }
        motor_stop(&m);
        run(&m, cmd);
        NEAR(m.ctrl.iref.q, 0, 1e-7); /* 首拍快环在位置时隙之前。 */
    }
}

static void test_openloop_duty_limit(void) {
    tMotorCtrl m;
    tMock h;
    tParameter p = parameters();
    fixture(&m, &h, p);
    tCmdValue cmd = {.mode = MC_MODE_OPENLOOP};
    cmd.value.openloop.duty = 2.0f;
    run(&m, cmd);
    tPwm initial = h.written;
    h.sample.vbus = 12.0f;
    motor_tick(&m);
    for (unsigned i = 0; i < 3; ++i)
        NEAR(h.written.duty[i], initial.duty[i], 1e-7);
    h.sample.vbus = 24.0f;
    for (unsigned n = 0; n < 360; ++n) {
        cmd.value.openloop.value = n * MATH_PI / 180.0f;
        motor_cmd_set(&m, &cmd);
        for (unsigned i = 0; i < m.timing.medium_div + 2u; ++i)
            motor_tick(&m);
        tAb applied = pwm_voltage(&h.written, m.vbus);
        NEAR(hypotf(applied.alpha, applied.beta), 24 / sqrtf(3.0f), 1e-5);
        NEAR(m.ctrl.duty.d, 1, 1e-7);
    }
}

static void test_current_and_torque_command(void) {
    tMotorCtrl m;
    tMock h;
    tParameter p = parameters();
    fixture(&m, &h, p);
    tCmdValue cmd = {.mode = MC_MODE_CURRENT};
    cmd.value.current = 1.0f;
    run(&m, cmd);
    NEAR(m.ctrl.iref.q, 1.0f, 1e-6);
    m.ctrl.iq.integral = 0.2f;
    tCmdCfg cfg = {.mode = MC_MODE_CURRENT, .config.torque_control = true};
    motor_cmd_cfg(&m, &cfg);
    cmd.value.current = 0.4f;
    motor_cmd_set(&m, &cmd);
    for (unsigned i = 0; i < m.timing.medium_div + 1u; ++i)
        motor_tick(&m);
    NEAR(m.ctrl.iref.q, 0.4f, 1e-6);
    assert(m.ctrl.iq.integral > 0.2f);
    motor_stop(&m);
    run_config(&m, cfg, cmd);
    NEAR(m.ctrl.iref.q, 2.0f, 1e-6);
    assert(m.ctrl.mode == MC_MODE_CURRENT);
}

static void test_velocity_ramp(void) {
    tParameter p = parameters();
    tMotorTiming timing = mc_timing_default(0.001f);
    p.traj_limit_d1 = 100.0f;
    p.traj_limit_d2 = 50.0f;
    tCmdCfg cfg = {.mode = MC_MODE_VELOCITY};
    tCmdValue cmd = {.mode = MC_MODE_VELOCITY, .value.vel = 2.5f};
    tMcMotion motion;
    motion_begin(&motion, &cfg, &p, &(tRotor){0}, &timing);
    motion_accept(&motion, &cmd, &p);
    for (unsigned i = 1; i <= 3; ++i) {
        motion_step(&motion);
        NEAR(motion.ref.vel, fminf((float)i, 2.5f), 1e-6);
    }

    cmd.value.vel = -1.5f;
    motion_accept(&motion, &cmd, &p);
    for (unsigned i = 1; i <= 5; ++i) {
        motion_step(&motion);
        NEAR(motion.ref.vel, 2.5f - 0.5f * i, 1e-6);
    }
    motion_step(&motion);
    NEAR(motion.ref.vel, -1.0f, 1e-6);
    motion_step(&motion);
    NEAR(motion.ref.vel, -1.5f, 1e-6);

    cmd.value.vel = -0.25f;
    motion_accept(&motion, &cmd, &p);
    motion_step(&motion);
    NEAR(motion.ref.vel, -1.0f, 1e-6);

    p.traj_limit_d1 = p.traj_limit_d2 = 0.0f;
    motion_begin(&motion, &cfg, &p, &(tRotor){0}, &timing);
    motion_accept(&motion, &cmd, &p);
    motion_step(&motion);
    NEAR(motion.ref.vel, -0.25f, 1e-6);
}

static void test_current_duty_compensation(void) {
    tParameter p = parameters();
    tMotorTiming timing = mc_timing_default(0.00005f);
    tCtrl c;
    ctrl_configure(&c, &p, &timing, &(tCmdCfg){.mode = MC_MODE_CURRENT});
    c.voltage_compensation_duty = (tDq){0.2f, -0.3f};
    c.hfi_duty_d = 0.1f;
    tDq duty = ctrl_step(&c, &(tRef){0}, (tDq){0});
    NEAR(duty.d, 0.3f, 1e-6);
    NEAR(duty.q, -0.3f, 1e-6);
    c.hfi_duty_d = 1.0f;
    duty = ctrl_step(&c, &(tRef){0}, (tDq){0});
    NEAR(hypotf(duty.d, duty.q), 1.0f, 1e-6);
}

static void test_openloop_speed(void) {
    tMotorCtrl m;
    tMock h;
    tParameter p = parameters();
    fixture(&m, &h, p);
    tCmdValue cmd = {.mode = MC_MODE_OPENLOOP};
    cmd.value.openloop.duty = 0.5f;
    tCmdCfg cfg = {.mode = MC_MODE_OPENLOOP, .config.openloop_speed_control = true};
    cmd.value.openloop.value = 1000.0f;
    run_config(&m, cfg, cmd);
    for (unsigned tick = 0; tick <= 25; ++tick) {
        tAb applied = pwm_voltage(&h.written, m.vbus);
        float angle = tick * cmd.value.openloop.value * m.timing.dt;
        float voltage = 0.5f * m.vbus * MATH_INSQRT3;
        NEAR(m.motion.ref.theta_e, angle, 1e-6);
        NEAR(applied.alpha, voltage * cosf(m.motion.ref.theta_e), 3e-6);
        NEAR(applied.beta, voltage * sinf(m.motion.ref.theta_e), 3e-6);
        motor_tick(&m);
    }
}

static void test_first_closed_loop_angle_and_modes(void) {
    tParameter p = parameters();
    p.motor_polepairs = 1;
    p.qclkp = p.qclki = p.dclki = 0;
    p.dclkp = 0.1f;
    tMotorCtrl m;
    tMock h;
    fixture(&m, &h, p);
    h.sample.angle = MATH_PI / 2;
    h.sample.current[0] = 1.0f;
    h.sample.current[1] = h.sample.current[2] = -0.5f;
    run(&m, (tCmdValue){.mode = MC_MODE_CURRENT});
    NEAR(m.sensor.i_dq.d, 0, 1e-6);
    NEAR(m.sensor.i_dq.q, -1, 1e-6);
    NEAR(m.ctrl.duty.d, 0, 1e-6);
    for (tMcMode mode = MC_MODE_CURRENT; mode <= MC_MODE_MIT; ++mode) {
        p = parameters();
        fixture(&m, &h, p);
        tCmdValue c = {.mode = mode};
        switch (mode) {
        case MC_MODE_CURRENT: c.value.current = 1; break;
        case MC_MODE_VELOCITY: c.value.vel = 1; break;
        case MC_MODE_POSITION: c.value.position.pos = 1; break;
        case MC_MODE_MIT:
            c.value.mit.tau_ff = 0.2f;
            c.value.mit.vel = 0.3f;
            break;
        default: break;
        }
        run(&m, c);
        for (unsigned i = 0; i < m.timing.medium_div; ++i)
            motor_tick(&m);
        assert(m.ctrl.iref.q > 0);
        assert(m.ctrl.iref.q <= p.motor_rated_current);
        if (mode == MC_MODE_MIT) {
            NEAR(m.motion.ref.tau_ff, 0.2, 1e-6);
            NEAR(m.motion.ref.vel, 0.3, 1e-6);
        }
    }
}

static void test_stop_interrupt(void) {
    for (unsigned restart = 0; restart < 2; ++restart) {
        tMotorCtrl m;
        tMock h;
        tParameter p = parameters();
        fixture(&m, &h, p);
        run(&m, (tCmdValue){.mode = MC_MODE_CURRENT});
        unsigned enables = h.enables;
        h.interrupt_restart = restart != 0;
        h.interrupt_at_lock = h.lock_calls + 2u; /* 输出提交前抢占。 */
        motor_tick(&m);
        assert(!h.enabled && h.enables == enables);
        for (unsigned slot = 0; slot < 4; ++slot)
            motor_tick(&m);
        assert(h.enabled == h.interrupt_restart);
        assert(h.enables == enables + restart);
    }
}

static void test_config_and_instance_isolation(void) {
    tParameter p = parameters();
    tMotorCtrl a, b;
    tMock ha, hb;
    p = parameters();
    fixture(&a, &ha, p);
    fixture(&b, &hb, p);
    active_mock = &ha;
    tCmdValue ca = {.mode = MC_MODE_CURRENT};
    ca.value.current = 1;
    run(&a, ca);
    for (unsigned i = 0; i < 100; ++i)
        motor_tick(&a);
    assert(a.ctrl.iq.integral != b.ctrl.iq.integral);
    assert(!hb.enabled && b.mode == MC_MODE_IDLE && hb.writes == 1);
    p.qclkp = 99;
    NEAR(a.param.qclkp, 0.1, 1e-6); /* 外部协议参数修改不渗入快环。 */
    motor_stop(&a);
    assert(!hb.enabled && b.mode == MC_MODE_IDLE);
    assert(a.mode == MC_MODE_IDLE && !ha.written.enabled);
    assert(ha.depth == 0 && hb.depth == 0);

}

int main(void) {
    test_current_transforms();
    test_modulation_equivalence();
    test_sensor_coordinates();
    test_pid_and_mit();
    test_current_limits_and_weakening();
    test_trajectory();
    test_start_and_multirate();
    test_outer_periods();
    test_openloop_duty_limit();
    test_current_and_torque_command();
    test_velocity_ramp();
    test_current_duty_compensation();
    test_openloop_speed();
    test_first_closed_loop_angle_and_modes();
    test_stop_interrupt();
    test_config_and_instance_isolation();
    puts("motor regression tests passed");
    return 0;
}
