#include "mc_controller.h"
#include "xmath.h"
#include <string.h>

void ctrl_init(tCtrl *c, const tParameter *p, const tMotorTiming *t) {
    memset(c, 0, sizeof(*c));
    c->imax = p->motor_rated_current;
    c->inv_kt = p->motor_ke > 0.0f ? 1.0f / p->motor_ke : 0.0f;
    float limit = p->pos_pidmode == POS_PID_PVC ? p->velocity_limit : c->imax;
    pid_init(&c->pos, p->plkp, p->plki, p->plkd, p->plalpha,
             t->dt * t->pos_div, -limit, limit);
    pi_init(&c->vel, p->vlkp, p->vlki, t->dt * t->vel_div, -c->imax, c->imax);
    pi_init(&c->id, p->dclkp, p->dclki, t->dt, -1.0f, 1.0f);
    pi_init(&c->iq, p->qclkp, p->qclki, t->dt, -1.0f, 1.0f);
    pi_init(&c->fw, p->fwlkp, p->fwlki, t->dt, -c->imax, 0.0f);
    /* TODO(parameter): MIT 补偿时间常数缺失，暂用 50 ms。 */
    mit_init(&c->mit, p->mit_kp, p->mit_kd, p->mit_tmax, t->dt * t->vel_div, 0.05f);
}

void ctrl_reset(tCtrl *c, const tParameter *p) {
    pid_reset(&c->pos);
    pi_reset(&c->vel);
    pi_reset(&c->id);
    pi_reset(&c->iq);
    pi_reset(&c->fw);
    mit_reset(&c->mit);
    c->mit.kp = p->mit_kp;
    c->mit.kd = p->mit_kd;
    c->vref = c->qref = c->u2 = 0.0f;
    c->iref = c->u = (tDq){0};
    c->fw_enable = false;
}

void ctrl_cmd(tCtrl *c, const tRef *r, tMcMode mode, bool fw) {
    switch (mode) {
    case MC_MODE_CURRENT: c->qref = r->iq; break;
    case MC_MODE_TORQUE:  c->qref = r->tau_ff * c->inv_kt; break;
    case MC_MODE_MIT:     mit_config(&c->mit, r->kp, r->kd); break;
    default: break;
    }
    if (c->fw_enable && !fw) {
        pi_reset(&c->fw);
        c->iref.d = 0.0f;
    }
    c->fw_enable = fw;
}

void ctrl_pos(tCtrl *c, const tParameter *p, const tRef *r, const tRotor *fb) {
    float out = pid_update(&c->pos, r->pos, fb->pos);
    if (p->pos_pidmode == POS_PID_PVC)
        c->vref = XCLAMP(out + r->vel, -p->velocity_limit, p->velocity_limit);
    else
        c->qref = out;
}

void ctrl_vel(tCtrl *c, float ref, float fb) {
    c->vref = ref;
    c->qref = pi_update(&c->vel, ref, fb);
}

void ctrl_mit(tCtrl *c, const tRef *r, const tRotor *fb) {
    float tau = mit_update(&c->mit, r->pos, fb->pos, r->vel, fb->vel, r->tau_ff);
    c->qref = XCLAMP(tau * c->inv_kt, -c->imax, c->imax);
}

void ctrl_current(tCtrl *c, tDq fb) {
    float iq = c->qref;
    if (c->fw_enable) {
        /* TODO(parameter): 弱磁电压目标缺失，暂用 95% 线性调制范围。 */
        c->iref.d = pi_update(&c->fw, 0.95f, XSQRTF(c->u2));
        float limit = XSQRTF(XMAXF(0.0f, c->imax * c->imax - c->iref.d * c->iref.d));
        iq = XCLAMP(iq, -limit, limit);
        if (iq != c->qref)
            pi_track(&c->vel, iq);
    }
    c->iref.q = iq;
    /* TODO(parameter): d/qclkp、ki 沿用归一化电压增益，协议单位待统一。 */
    c->u.d = pi_update(&c->id, c->iref.d, fb.d);
    c->u.q = pi_update(&c->iq, iq, fb.q);
    c->u2 = XNORM2(c->u.d, c->u.q);
    if (c->u2 > 1.0f) {
        float scale = 1.0f / XSQRTF(c->u2);
        c->u.d *= scale;
        c->u.q *= scale;
        c->u2 = 1.0f;
        pi_track(&c->id, c->u.d);
        pi_track(&c->iq, c->u.q);
    }
}
