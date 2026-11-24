#include "mc_controller.h"
#include "bsp_math.h"

static inline void track_pi_output(tPI *pi, float applied) {
    if (pi->ki_dt > 0.0f)
        pi->integral = MATH_CLAMP(pi->integral + applied - pi->output, pi->minimum, pi->maximum);
    pi->output = applied;
}

void ctrl_configure(tCtrl *c, const tParameter *p, const tMotorTiming *t,
                    const tCmdCfg *cfg) {
    *c = (tCtrl){.mode = cfg->mode, .imax = p->motor_rated_current,
                 .q_limit = p->motor_rated_current, .velocity_limit = p->velocity_limit};
    if (cfg->mode == MC_MODE_OPENLOOP)
        return;

    float medium_dt = t->dt * t->medium_div;
    pi_init(&c->id, p->dclkp, p->dclki, t->dt, -1.0f, 1.0f);
    pi_init(&c->iq, p->qclkp, p->qclki, t->dt, -1.0f, 1.0f);

    switch (cfg->mode) {
    case MC_MODE_POSITION: {
        c->cascade_position = p->pos_pidmode == POS_PID_PVC;
        float dt = c->cascade_position ? medium_dt * MC_PVC_DIV : medium_dt;
        float limit = c->cascade_position ? c->velocity_limit : c->imax;
        pid_init(&c->pos, p->plkp, p->plki, p->plkd, p->plalpha, dt, -limit, limit);
        if (c->cascade_position)
            pi_init(&c->vel, p->vlkp, p->vlki, medium_dt, -c->imax, c->imax);
        break;
    }
    case MC_MODE_VELOCITY:
        pi_init(&c->vel, p->vlkp, p->vlki, medium_dt, -c->imax, c->imax);
        c->fw_enable = cfg->config.fw_enable;
        if (c->fw_enable)
            pi_init(&c->fw, p->fwlkp, p->fwlki, medium_dt, -c->imax, 0.0f);
        break;
    case MC_MODE_MIT:
        c->inv_kt = 1.0f / p->motor_ke;
        mit_init(&c->mit, MATH_CLAMP(cfg->config.mit.kp, 0.0f, p->mit_kpmax),
                 MATH_CLAMP(cfg->config.mit.kd, 0.0f, p->mit_kdmax), p->mit_tmax, medium_dt, 0.05f);
        break;
    default:
        break;
    }
}

void ctrl_measure(tCtrl *c, tDq current) {
    c->current_ref = MATH_SQRTF(MATH_NORM2(c->iref.d, c->iref.q));
    c->current_fb = MATH_SQRTF(MATH_NORM2(current.d, current.q));
    c->duty_output = MATH_SQRTF(MATH_NORM2(c->duty.d, c->duty.q));
}

void ctrl_position_step(tCtrl *c, const tRef *ref, const tRotor *rotor) {
    if (c->mode == MC_MODE_MIT) {
        float torque = mit_update(&c->mit, ref->pos, rotor->pos, ref->vel, rotor->vel, ref->tau_ff);
        c->qref = MATH_CLAMP(torque * c->inv_kt, -c->imax, c->imax);
    } else if (c->mode == MC_MODE_POSITION) {
        if (c->cascade_position && c->pvc_remaining) {
            --c->pvc_remaining;
            return;
        }
        float output = pid_update(&c->pos, ref->pos, rotor->pos);
        if (c->cascade_position) {
            c->vref = MATH_CLAMP(output + ref->vel, -c->velocity_limit, c->velocity_limit);
            c->pvc_remaining = MC_PVC_DIV - 1u;
        } else {
            c->qref = output;
        }
    }
}

void ctrl_velocity_step(tCtrl *c, const tRef *ref, const tRotor *rotor) {
    if (c->mode != MC_MODE_VELOCITY && !(c->mode == MC_MODE_POSITION && c->cascade_position))
        return;

    if (c->mode == MC_MODE_VELOCITY)
        c->vref = ref->vel;
    c->qref = pi_update(&c->vel, c->vref, rotor->vel);
    if (c->fw_enable) {
        /* 当前协议只提供弱磁 PI 增益。 */
        float id = pi_update(&c->fw, 0.95f, c->duty_output);
        c->iref.d = id;
        c->q_limit = MATH_SQRTF(MATH_MAXF(0.0f, (c->imax - id) * (c->imax + id)));
        float applied = MATH_CLAMP(c->qref, -c->q_limit, c->q_limit);
        if (applied != c->qref)
            track_pi_output(&c->vel, applied);
    }
}

tDq ctrl_step(tCtrl *c, const tRef *ref, tDq current) {
    if (c->mode == MC_MODE_OPENLOOP) {
        c->duty = (tDq){ref->openloop_duty, 0.0f};
        return c->duty;
    }

    float iq = c->mode == MC_MODE_CURRENT ? ref->iq : c->qref;
    c->iref.q = MATH_CLAMP(iq, -c->q_limit, c->q_limit);
    float d_offset = c->voltage_compensation_duty.d + c->hfi_duty_d;
    float q_offset = c->voltage_compensation_duty.q;
    tDq duty = {
        pi_update(&c->id, c->iref.d, current.d) + d_offset,
        pi_update(&c->iq, c->iref.q, current.q) + q_offset
    };
    float duty_sq = MATH_NORM2(duty.d, duty.q);
    if (duty_sq > 1.0f) {
        float scale = 1.0f / MATH_SQRTF(duty_sq);
        duty.d *= scale;
        duty.q *= scale;
        track_pi_output(&c->id, duty.d - d_offset);
        track_pi_output(&c->iq, duty.q - q_offset);
    }
    c->duty = duty;
    return duty;
}
