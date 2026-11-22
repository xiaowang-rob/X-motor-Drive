#include "mc_motor.h"

#include <string.h>
#include "xmath.h"

static uintptr_t lock(tMotorCtrl *m) {
    return m->port.enter_critical ? m->port.enter_critical(m->port.user) : 0;
}
static void unlock(tMotorCtrl *m, uintptr_t token) {
    if (m->port.exit_critical)
        m->port.exit_critical(m->port.user, token);
}

static void pwm_off(tMotorCtrl *m) {
    m->port.set_enable(m->port.user, false);
    m->pwm.enabled = false;
    m->pwm.sector = 0;
    for (unsigned i = 0; i < 3; ++i)
        m->pwm.duty[i] = 0.5f;
    ++m->pwm.sequence;
    m->port.write_pwm(m->port.user, &m->pwm);
}

static bool due(uint16_t *tick, uint16_t div) {
    if (*tick == 0) {
        *tick = div - 1u;
        return true;
    }
    --*tick;
    return false;
}

static void reset_algorithms(tMotorCtrl *m) {
    ctrl_init(&m->ctrl, &m->param, &m->timing);
    foc_init(&m->foc);
    sensor_init(&m->sensor, &m->param, m->timing.dt);
    m->cmd_dt = m->timing.dt * m->timing.cmd_div;
    memset(&m->cmd, 0, sizeof(m->cmd));
    m->cmd_tick = m->vel_tick = m->pos_tick = m->slow_tick = 0;
    ++m->traj_seq;
    m->ready = false;
    m->cmd_pending = false;
}

tMcResult motor_init(tMotorCtrl *m, const tParameter *p,
                        const tMotorTiming *t, const tMotorOps *port) {
    /* 合法端口下，即使配置被拒绝，也先保持功率级关闭。 */
    port->set_enable(port->user, false);
    tMcResult result = mc_config_validate(p, t);
    if (result != MC_OK)
        return result;
    memset(m, 0, sizeof(*m));
    m->param = *p;
    m->timing = *t;
    m->port = *port;
    reset_algorithms(m);
    pwm_off(m);
    m->state = MC_IDLE;
    return MC_OK;
}

tMcResult motor_config(tMotorCtrl *m, const tParameter *p) {
    tMcResult result = mc_config_validate(p, &m->timing);
    if (result != MC_OK)
        return result;
    uintptr_t token = lock(m);
    if (m->state != MC_IDLE) {
        unlock(m, token);
        return MC_BUSY;
    }
    m->param = *p;
    ++m->cmd_seq;
    reset_algorithms(m);
    unlock(m, token);
    return MC_OK;
}

tMcResult motor_submit(tMotorCtrl *m, const tCmd *c) {
    uintptr_t token = lock(m);
    tMcResult result = cmd_validate(c, &m->param);
    if (result == MC_OK) {
        if (m->faults)
            result = MC_FAULT_LATCHED;
        else if ((m->state == MC_RUNNING && c->mode != m->cmd.mode) ||
                 (m->state == MC_STARTING && c->mode != m->mailbox.mode))
            result = MC_BUSY;
        else {
            m->mailbox = *c;
            m->cmd_pending = true;
            ++m->cmd_seq;
        }
    }
    unlock(m, token);
    return result;
}

tMcResult motor_start(tMotorCtrl *m) {
    uintptr_t token = lock(m);
    tMcResult result = MC_OK;
    if (m->faults)
        result = MC_FAULT_LATCHED;
    else if (m->state != MC_IDLE)
        result = MC_BUSY;
    else if (!m->cmd_pending || !m->ready ||
             (m->mailbox.mode != MC_MODE_OPENLOOP && !m->sensor.rotor.valid))
        result = MC_NOT_READY;
    else
        m->state = MC_STARTING;
    unlock(m, token);
    return result;
}

void motor_stop(tMotorCtrl *m) {
    uintptr_t token = lock(m);
    m->state = m->faults ? MC_FAULT : MC_IDLE;
    m->ready = m->cmd_pending = false;
    ++m->cmd_seq;
    pwm_off(m);
    unlock(m, token);
}

void motor_fault(tMotorCtrl *m, uint32_t faults) {
    uintptr_t token = lock(m);
    m->faults |= faults;
    ++m->cmd_seq;
    m->state = MC_FAULT;
    m->ready = m->cmd_pending = false;
    pwm_off(m);
    unlock(m, token);
}

tMcResult motor_clear_fault(tMotorCtrl *m) {
    uintptr_t token = lock(m);
    if (m->state != MC_FAULT) {
        unlock(m, token);
        return MC_BUSY;
    }
    m->faults = 0;
    ++m->cmd_seq;
    m->state = MC_IDLE;
    m->cmd_pending = m->ready = false;
    unlock(m, token);
    return MC_OK;
}

tMcResult motor_zero(tMotorCtrl *m) {
    uintptr_t token = lock(m);
    tMcResult result = MC_OK;
    if (m->state != MC_IDLE)
        result = MC_BUSY;
    else if (!m->sensor.rotor.valid)
        result = MC_NOT_READY;
    else {
        sensor_zero(&m->sensor);
        m->cmd_pending = false;
    }
    unlock(m, token);
    return result;
}

/* 只调度当前模式需要的任务；电流模式没有外环和轨迹调用。 */
static void control(tMotorCtrl *m) {
    tCmdState *cmd = &m->cmd;
    tCtrl *c = &m->ctrl;
    const tRotor *fb = &m->sensor.rotor;
    if ((cmd->mode == MC_MODE_VELOCITY || cmd->mode == MC_MODE_POSITION) &&
        due(&m->cmd_tick, m->timing.cmd_div) && cmd_update(cmd, &m->param, m->cmd_dt))
        ++m->traj_seq;
    switch (cmd->mode) {
    case MC_MODE_POSITION:
        if (due(&m->pos_tick, m->timing.pos_div))
            ctrl_pos(c, &m->param, &cmd->ref, fb);
        if (m->param.pos_pidmode == POS_PID_PVC && due(&m->vel_tick, m->timing.vel_div))
            ctrl_vel(c, c->vref, fb->vel);
        break;
    case MC_MODE_VELOCITY:
        if (due(&m->vel_tick, m->timing.vel_div))
            ctrl_vel(c, cmd->ref.vel, fb->vel);
        break;
    case MC_MODE_MIT:
        if (due(&m->vel_tick, m->timing.vel_div))
            ctrl_mit(c, &cmd->ref, fb);
        break;
    default:
        break;
    }
}

void motor_tick(tMotorCtrl *m) {
    /* 1. 采样与反馈。PWM 快照防止异步停机修改采样元数据。 */
    uintptr_t token = lock(m);
    tPwm pwm = m->pwm;
    unlock(m, token);
    tSample sample = {0};
    bool sampled = m->port.read_sample(m->port.user, &pwm, &sample);
    bool rotor_ok = sensor_update(&m->sensor, sample.angle, sampled && sample.rotor_valid);
    bool bus_ok = sampled && sample.vbus > 1e-3f;
    bool current_ok = sampled && sample.pwm_sequence == pwm.sequence &&
        foc_current(&m->foc, &sample, m->param.cfalpha);
    if (bus_ok)
        m->vbus = sample.vbus;
    if (due(&m->slow_tick, m->timing.slow_div) && m->port.get_temp)
        m->temp_valid = m->port.get_temp(m->port.user, &m->temp);

    /* 2. 启动/新指令。IDLE 保留邮箱，直到 start 消费。 */
    token = lock(m);
    m->ready = bus_ok && current_ok;
    bool start = m->state == MC_STARTING;
    bool run = start || m->state == MC_RUNNING;
    bool changed = run && m->cmd_pending;
    tCmd cmd;
    if (changed) {
        cmd = m->mailbox;
        m->cmd_pending = false;
    }
    unlock(m, token);
    if (!run)
        return;
    uint32_t fault = (!bus_ok ? MC_FAULT_BUS : 0u) |
                     (!current_ok ? MC_FAULT_SAMPLE : 0u);
    tMcMode mode = start ? cmd.mode : m->cmd.mode;
    if (mode != MC_MODE_OPENLOOP && !rotor_ok)
        fault |= MC_FAULT_SENSOR;
    if (fault) {
        motor_fault(m, fault);
        return;
    }
    if (start) {
        ctrl_reset(&m->ctrl, &m->param);
        cmd_begin(&m->cmd, &cmd, &m->param, &m->sensor.rotor);
        m->cmd_tick = m->vel_tick = m->pos_tick = 0;
    } else if (changed) {
        cmd_accept(&m->cmd, &cmd, &m->param);
    }
    if (changed)
        ctrl_cmd(&m->ctrl, &m->cmd.ref, mode, cmd.fw);

    /* 3. 到期的外环 -> Park -> 电流环 -> 反 Park / SVPWM。 */
    control(m);
    foc_park(&m->foc, mode == MC_MODE_OPENLOOP ? m->cmd.ref.theta_e : m->sensor.rotor.theta_e);
    if (mode == MC_MODE_OPENLOOP) {
        float scale = XMATH_SQRT3 / m->vbus;
        m->ctrl.u = (tDq){m->cmd.ref.voltage.d * scale, m->cmd.ref.voltage.q * scale};
        m->ctrl.u2 = XNORM2(m->ctrl.u.d, m->ctrl.u.q);
        if (m->ctrl.u2 > 1.0f) {
            scale = 1.0f / XSQRTF(m->ctrl.u2);
            m->ctrl.u.d *= scale;
            m->ctrl.u.q *= scale;
            m->ctrl.u2 = 1.0f;
        }
    } else {
        ctrl_current(&m->ctrl, m->foc.i_dq);
    }
    foc_pwm(&m->foc, m->ctrl.u);
    /* TODO(parameter): ADC/PWM 角度延迟补偿系数缺失。 */

    /* 4. 提交 PWM；计算过程中发生的 stop/fault 优先。 */
    token = lock(m);
    if (m->state == MC_STARTING || m->state == MC_RUNNING) {
        bool enable = !m->pwm.enabled;
        memcpy(m->pwm.duty, m->foc.pwm.duty, sizeof(m->pwm.duty));
        m->pwm.sector = m->foc.pwm.sector;
        ++m->pwm.sequence;
        m->pwm.enabled = true;
        m->port.write_pwm(m->port.user, &m->pwm);
        if (enable)
            m->port.set_enable(m->port.user, true);
        m->state = MC_RUNNING;
    }
    unlock(m, token);
    if (mode == MC_MODE_OPENLOOP && m->cmd.ref.vel_e != 0.0f)
        m->cmd.ref.theta_e = XANGLE_2PI(m->cmd.ref.theta_e + m->cmd.ref.vel_e * m->timing.dt);
}

tMcResult motor_service(tMotorCtrl *m) {
    uintptr_t token = lock(m);
    bool active = m->state == MC_RUNNING;
    bool needed = active && m->cmd.traj.mode == MC_TRAJ_S_CURVE &&
        (m->cmd.mode == MC_MODE_POSITION || m->cmd.mode == MC_MODE_VELOCITY) &&
        m->cmd.traj.replan && !m->cmd_pending;
    if (!needed) {
        unlock(m, token);
        return active ? MC_OK : MC_NOT_READY;
    }
    tTraj plan = m->cmd.traj;
    uint32_t traj_seq = m->traj_seq;
    uint32_t generation = m->cmd_seq;
    float dt = m->cmd_dt;
    unlock(m, token);

    if (!traj_plan(&plan, dt))
        return MC_BUSY;

    token = lock(m);
    bool fresh = generation == m->cmd_seq && traj_seq == m->traj_seq;
    if (fresh)
        m->cmd.traj = plan;
    unlock(m, token);
    return fresh ? MC_OK : MC_BUSY;
}

tMcResult motor_get_fb(tMotorCtrl *m, tMotorFb *f) {
    uintptr_t token = lock(m);
    *f = (tMotorFb){
        .state = m->state, .mode = m->cmd.mode, .faults = m->faults,
        .rotor = m->sensor.rotor, .ref = m->cmd.ref,
        .current = m->foc.i_dq, .iref = m->ctrl.iref,
        .voltage_utilization = m->pwm.enabled ? m->ctrl.u2 : 0.0f,
        .vref = m->ctrl.vref,
        .pwm = m->pwm,
        .vbus = m->vbus, .temp = m->temp,
        .temp_valid = m->temp_valid
    };
    memcpy(f->is, m->foc.is, sizeof(f->is));
    unlock(m, token);
    if (f->pwm.enabled) {
        f->voltage_utilization = XSQRTF(f->voltage_utilization);
        float mean = (f->pwm.duty[0] + f->pwm.duty[1] + f->pwm.duty[2]) * (1.0f / 3.0f);
        f->applied_voltage.alpha = (f->pwm.duty[0] - mean) * f->vbus;
        f->applied_voltage.beta = (f->pwm.duty[1] - f->pwm.duty[2]) * f->vbus * XMATH_INSQRT3;
    }
    return MC_OK;
}
