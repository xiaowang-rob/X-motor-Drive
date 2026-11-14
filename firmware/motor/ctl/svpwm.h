#ifndef __SVPWM_H
#define __SVPWM_H

#include <stdint.h>
// SVPWM 核心接口

void svpwm_update(uint8_t sector, float da, float db, float *Du, float *Dv,
                  float *Dw);

#endif // __SVPWM_H