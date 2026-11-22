#include "mc_command.h"

#include <string.h>
#include "xmath.h"
#include "core/mc_config.h"

tCmd cmd_default(tMcMode mode, const tParameter *p) {
    tCmd c = {0};
    c.mode = mode;
    if (mode == MC_MODE_MIT) {
        c.value.mit.kp = p->mit_kp;
        c.value.mit.kd = p->mit_kd;
    }
    return c;
}

tMcResult cmd_validate(const tCmd *c, const tParameter *p) {
    if ((unsigned)c->mode > (unsigned)MC_MODE_MIT)
        return MC_INVALID_ARGUMENT;
    if (c->fw && c->mode != MC_MODE_VELOCITY)
        return MC_UNSUPPORTED;
    /* TODO(feature): PVT、SMO/HFI、融合分阶段实现，不静默退回错误角度源。 */
    if (c->mode != MC_MODE_OPENLOOP &&
        (p->sensorless != SENSORLESS_NONE || p->rotor_enc == ENC_NONE))
        return MC_UNSUPPORTED;
    if ((c->mode == MC_MODE_TORQUE || c->mode == MC_MODE_MIT) &&
        p->motor_ke <= 0.0f)
        return MC_INVALID_CONFIG;
    return MC_OK;
}

static float position_limit(float x, const tParameter *p) {
    if (p->pos_limit_mode & POS_LIMIT_MIN)
        x = XMAXF(x, p->pos_min_limit);
    if (p->pos_limit_mode & POS_LIMIT_MAX)
        x = XMINF(x, p->pos_max_limit);
    return x;
}

void cmd_accept(tCmdState *s, const tCmd *c, const tParameter *p) {
    bool was_rotating = s->mode == MC_MODE_OPENLOOP && s->rotate;
    s->mode = c->mode;
    s->rotate = c->mode == MC_MODE_OPENLOOP && c->value.openloop.rotate;
    switch (c->mode) {
    case MC_MODE_OPENLOOP:
        if (!c->value.openloop.rotate || !was_rotating)
            s->ref.theta_e = XANGLE_2PI(c->value.openloop.angle);
        s->ref.vel_e = c->value.openloop.rotate ? c->value.openloop.vel : 0.0f;
        s->ref.voltage = (tDq){c->value.openloop.voltage, 0.0f};
        break;
    case MC_MODE_CURRENT:
        s->ref.iq = XCLAMP(c->value.current, -p->motor_rated_current, p->motor_rated_current);
        break;
    case MC_MODE_TORQUE:
        s->ref.tau_ff = XCLAMP(c->value.torque,
            -p->motor_rated_current * p->motor_ke,
             p->motor_rated_current * p->motor_ke);
        break;
    case MC_MODE_VELOCITY:
        traj_set_target(&s->traj,
            XCLAMP(c->value.vel, -p->velocity_limit, p->velocity_limit));
        break;
    case MC_MODE_POSITION:
        traj_set_target(&s->traj, position_limit(c->value.pos, p));
        break;
    case MC_MODE_MIT:
        s->ref.pos = position_limit(c->value.mit.pos, p);
        s->ref.vel = XCLAMP(c->value.mit.vel, -p->velocity_limit, p->velocity_limit);
        s->ref.tau_ff = XCLAMP(c->value.mit.tau_ff, -p->mit_tmax, p->mit_tmax);
        s->ref.kp = XCLAMP(c->value.mit.kp, 0.0f, p->mit_kpmax);
        s->ref.kd = XCLAMP(c->value.mit.kd, 0.0f, p->mit_kdmax);
        /* MIT 五元组直接执行，避免全局轨迹配置丢弃 vel/tau_ff。 */
        break;
    }
}

void cmd_begin(tCmdState *s, const tCmd *c,
                      const tParameter *p, const tRotor *r) {

    s->rotate = false;
    if (c->mode == MC_MODE_POSITION || c->mode == MC_MODE_VELOCITY) {
        double initial = c->mode == MC_MODE_VELOCITY ? r->vel : r->pos;
        const tTrajCfg traj = {
            .mode = (tTrajMode)p->traj_mode,
            .vmax = p->traj_limit_d1,
            .amax = p->traj_limit_d2,
            .jmax = p->traj_limit_d3,
            .tol = p->traj_tolerance > 0.0f ? p->traj_tolerance : 0.001f
        };
        traj_init(&s->traj, &traj, initial);
    }
    s->ref = (tRef){.pos = r->pos, .vel = c->mode == MC_MODE_VELOCITY ? r->vel : 0.0f};
    cmd_accept(s, c, p);
}

bool cmd_update(tCmdState *s, const tParameter *p, float dt) {
    if (!traj_update(&s->traj, dt))
        return false;
    if (s->mode == MC_MODE_VELOCITY) {
        s->ref.vel = XCLAMP((float)s->traj.value, -p->velocity_limit, p->velocity_limit);
    } else {
        s->ref.pos = position_limit((float)s->traj.value, p);
        s->ref.vel = XCLAMP((float)s->traj.vel, -p->velocity_limit, p->velocity_limit);
    }
    return true;
}
