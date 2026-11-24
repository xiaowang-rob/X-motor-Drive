#include "mc_motor.h"
#include "bsp_critical.h"
#include <assert.h>
#include <string.h>

static void pwm_off(tMotorCtrl *m) {
    m->port.set_enable(false);
    m->pwm.enabled = false;
    for (unsigned i = 0; i < 3; ++i)
        m->pwm.duty[i] = 0.5f;
    m->port.write_pwm(&m->pwm);
}

enum {
    MC_SLOT_MOTION,
    MC_SLOT_MEASURE,
    MC_SLOT_CONFIG,
    MC_SLOT_POSITION,
    MC_SLOT_VELOCITY
};

_Static_assert(MC_SLOT_VELOCITY + 1 == MC_MEDIUM_MIN_DIV, "invalid medium schedule");

static void run_motion_slot(tMotorCtrl *m, tMcMode mode) {
    uint32_t token = bsp_critical_enter();
    if (m->mode != mode) {
        bsp_critical_exit(token);
        return;
    }
    bool starting = mode == MC_MODE_STARTING;
    tCmdCfg cfg;
    tCmdValue value;
    if (starting)
        cfg = m->pending_config;
    bool pending = m->value_pending;
    if (pending) {
        value = m->pending_value;
        m->value_pending = false;
    }
    bsp_critical_exit(token);

    if (starting)
        motion_begin(&m->motion, &cfg, &m->param, &m->sensor.rotor, &m->timing);
    if (pending)
        motion_accept(&m->motion, &value, &m->param);
    motion_step(&m->motion);
}

static void run_medium_slot(tMotorCtrl *m, uint16_t slot, tMcMode mode) {
    const tRef *ref = motion_reference(&m->motion);
    const tRotor *rotor = &m->sensor.rotor;
    switch (slot) {
    case MC_SLOT_MOTION:
        run_motion_slot(m, mode);
        break;
    case MC_SLOT_MEASURE:
        ctrl_measure(&m->ctrl, m->sensor.i_dq);
        break;
    case MC_SLOT_CONFIG:
        if (mode == MC_MODE_STARTING) {
            ctrl_configure(&m->ctrl, &m->param, &m->timing, &m->motion.cfg);
            uint32_t token = bsp_critical_enter();
            if (m->mode == mode)
                m->mode = m->motion.cfg.mode;
            bsp_critical_exit(token);
        }
        break;
    case MC_SLOT_POSITION:
        ctrl_position_step(&m->ctrl, ref, rotor);
        break;
    case MC_SLOT_VELOCITY:
        ctrl_velocity_step(&m->ctrl, ref, rotor);
        break;
    default:
        break;
    }
}

static void reset_algorithms(tMotorCtrl *m) {
    sensor_init(&m->sensor, &m->param, m->timing.dt);
    m->ctrl = (tCtrl){0};
    m->motion = (tMcMotion){0};
    m->value_pending = false;
}

void motor_init(tMotorCtrl *m, const tParameter *p,
                const tMotorTiming *t, const tMotorOps *port) {
    assert(mc_config_valid(p, t));
    memset(m, 0, sizeof(*m));
    m->mode = MC_MODE_IDLE;
    m->param = *p;
    m->timing = *t;
    m->port = *port;
    pwm_off(m);
    reset_algorithms(m);
}

void motor_config(tMotorCtrl *m, const tParameter *p) {
    assert(mc_config_valid(p, &m->timing));
    uint32_t token = bsp_critical_enter();
    if (m->mode == MC_MODE_IDLE) {
        m->param = *p;
        reset_algorithms(m);
    }
    bsp_critical_exit(token);
}

void motor_cmd_cfg(tMotorCtrl *m, const tCmdCfg *cfg) {
    uint32_t token = bsp_critical_enter();
    if (m->mode == MC_MODE_IDLE && mc_mode_is_active(cfg->mode)) {
        assert(cmd_config_valid(cfg, &m->param));
        m->pending_config = *cfg;
        m->value_pending = false;
        m->medium_slot = MC_SLOT_MOTION;
        m->mode = MC_MODE_STARTING;
    }
    bsp_critical_exit(token);
}

void motor_cmd_set(tMotorCtrl *m, const tCmdValue *cmd) {
    uint32_t token = bsp_critical_enter();
    tMcMode mode = m->mode == MC_MODE_STARTING ? m->pending_config.mode : m->mode;
    if (mc_mode_is_active(mode) && cmd->mode == mode) {
        m->pending_value = *cmd;
        m->value_pending = true;
    }
    bsp_critical_exit(token);
}

void motor_stop(tMotorCtrl *m) {
    uint32_t token = bsp_critical_enter();
    m->mode = MC_MODE_IDLE;
    m->value_pending = false;
    pwm_off(m);
    bsp_critical_exit(token);
}

void motor_zero(tMotorCtrl *m) {
    uint32_t token = bsp_critical_enter();
    if (m->mode == MC_MODE_IDLE) {
        sensor_zero(&m->sensor);
    }
    bsp_critical_exit(token);
}

void motor_tick(tMotorCtrl *m) {
    uint32_t token = bsp_critical_enter();
    tPwm pwm = m->pwm;
    tMcMode mode = m->mode;
    uint16_t slot = m->medium_slot;
    bsp_critical_exit(token);

    tMcSample sample;
    m->port.read_sample(&sample);
    sensor_angle(&m->sensor, sample.angle);
    m->vbus = sample.vbus;

    bool active = mc_mode_is_active(mode);
    if (active) {
        float angle = mode == MC_MODE_OPENLOOP
            ? motion_openloop_angle(&m->motion, m->timing.dt)
            : m->sensor.rotor.theta_e;
        if (!pwm.enabled)
            sensor_set_electrical_angle(&m->sensor, angle);
        sensor_current(&m->sensor, sample.current);
        if (pwm.enabled)
            sensor_set_electrical_angle(&m->sensor, angle);

        tDq duty = ctrl_step(&m->ctrl, motion_reference(&m->motion), m->sensor.i_dq);
        mc_pwm_modulate(duty, m->sensor.sin, m->sensor.cos, pwm.duty);
    } else {
        sensor_current(&m->sensor, sample.current);
        if (mode == MC_MODE_IDLE)
            return;
    }

    /* 模式变化时丢弃当前拍的 PWM 和中频任务。 */
    token = bsp_critical_enter();
    if (m->mode != mode) {
        bsp_critical_exit(token);
        return;
    }
    if (active) {
        bool enable = !pwm.enabled;
        pwm.enabled = true;
        m->pwm = pwm;
        m->port.write_pwm(&m->pwm);
        if (enable)
            m->port.set_enable(true);
    }
    m->medium_slot = slot + 1u == m->timing.medium_div ? 0u : slot + 1u;
    bsp_critical_exit(token);

    if (slot <= MC_SLOT_VELOCITY)
        run_medium_slot(m, slot, mode);
}
