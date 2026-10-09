#ifndef __FOC_H
#define __FOC_H

#include "protocol.h"
#include "pid.h"
#include "mit.h"
#include "filter.h"

#include "parameters.h"

typedef struct
{

    tPI PI_iq;
    tPI PI_id;
    tPI PI_weakmag;
    tPI PI_vel;
    tPID PID_pos;
    tMIT mit;

} tFOC;

void foc_init(tFOC *foc, tParameter *param,
              float t_cl, float t_vl, float t_pl, float vmax);
void foc_reset(tFOC *foc);
void foc_process(tFOC *foc, uint8_t sec);
void foc_update(tFOC *foc);

#endif // __FOC_H