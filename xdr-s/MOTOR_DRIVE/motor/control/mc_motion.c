#include "mc_motion.h"
#include "bsp_math.h"

static float position_limit(float x, const tParameter *p) {
    if (p->pos_limit_mode & POS_LIMIT_MIN)
        x = MATH_MAXF(x, p->pos_min_limit);
    if (p->pos_limit_mode & POS_LIMIT_MAX)
        x = MATH_MINF(x, p->pos_max_limit);
    return x;
}

void motion_begin(tMcMotion *s, const tCmdCfg *cfg, const tParameter *p,
                  const tRotor *r, const tMotorTiming *timing) {
    float dt = timing->dt * timing->medium_div;
    *s = (tMcMotion){
        .cfg = *cfg,
        .ref.pos = r->pos,
        .ref.vel = cfg->mode == MC_MODE_VELOCITY ? r->vel : 0.0f,
        .target_speed = r->vel,
        /* 速度模式复用协议的 D1、D2，分别作为加速、减速率。 */
        .accel_step = p->traj_limit_d1 * dt,
        .decel_step = p->traj_limit_d2 * dt,
        .phase = r->theta_e
    };
}

void motion_accept(tMcMotion *s, const tCmdValue *c, const tParameter *p) {
    switch (s->cfg.mode) {
    case MC_MODE_OPENLOOP:
        if (s->cfg.config.openloop_speed_control)
            s->ref.vel_e = c->value.openloop.value;
        else
            s->phase = MATH_ANGLE_2PI(c->value.openloop.value);
        s->ref.openloop_duty = MATH_CLAMP(c->value.openloop.duty, 0.0f, 1.0f);
        break;
    case MC_MODE_CURRENT: {
        float iq = c->value.current;
        if (s->cfg.config.torque_control)
            iq /= p->motor_ke;
        s->ref.iq = MATH_CLAMP(iq, -p->motor_rated_current, p->motor_rated_current);
        break;
    }
    case MC_MODE_VELOCITY:
        s->target_speed = MATH_CLAMP(c->value.vel, -p->velocity_limit, p->velocity_limit);
        break;
    case MC_MODE_POSITION:
        s->ref.pos = position_limit(c->value.position.pos, p);
        break;
    case MC_MODE_MIT:
        s->ref.pos = position_limit(c->value.mit.pos, p);
        s->ref.vel = MATH_CLAMP(c->value.mit.vel, -p->velocity_limit, p->velocity_limit);
        s->ref.tau_ff = MATH_CLAMP(c->value.mit.tau_ff, -p->mit_tmax, p->mit_tmax);
        break;
    case MC_MODE_IDLE:
    case MC_MODE_STARTING:
        break;
    }
}

void motion_step(tMcMotion *s) {
    if (s->cfg.mode != MC_MODE_VELOCITY)
        return;

    float speed = s->ref.vel;
    float target = s->target_speed;
    /* 换向时先按减速斜率降到零，下一拍再反向加速。 */
    if ((speed > 0.0f && target < 0.0f) || (speed < 0.0f && target > 0.0f))
        target = 0.0f;
    float step = MATH_FABSF(target) > MATH_FABSF(speed) ? s->accel_step : s->decel_step;
    float delta = target - speed;
    s->ref.vel = (step <= 0.0f || MATH_FABSF(delta) <= step)
        ? target
        : speed + MATH_COPYSIGNF(step, delta);
}

float motion_openloop_angle(tMcMotion *s, float pwm_dt) {
    s->ref.theta_e = s->phase;
    if (s->cfg.config.openloop_speed_control)
        s->phase = MATH_ANGLE_2PI(s->phase + s->ref.vel_e * pwm_dt);
    return s->ref.theta_e;
}
