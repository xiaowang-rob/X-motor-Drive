#ifndef MC_PWM_H
#define MC_PWM_H

#include "core/mc_types.h"

/* Dd/Dq 以 Vmax = Vbus/sqrt(3) 为基准，输出 U/V/W 占空比。 */
void mc_pwm_modulate(tDq duty, float sin, float cos, float phases[3]);

#endif
