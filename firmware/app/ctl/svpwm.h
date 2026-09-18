#ifndef __SVPWM_H
#define __SVPWM_H

#include "device_cfg.h"

typedef struct
{
    uint16_t tic_pwm; // pwm arr
    float vbus;       // 母线电压
    float k;
    uint16_t ticTs;
    uint16_t ticTn;
    uint16_t ticTd;
    uint8_t index;

    volatile uint8_t sector;
    uint16_t ticA;
    uint16_t ticB;
    uint16_t ticC;
} tSvpwm;

// SVPWM 核心接口
void svpwm_init(tSvpwm *sv, uint16_t tic_pwm, float Vbus,
                float tpwm, float ts_us, float tn_us, float td_us);
void svpwm_update(tSvpwm *sv, float ua, float ub);
void svpwm_vbus_calibration(tSvpwm *sv, float Vbus);
uint16_t svpwm_sp_calibration(tSvpwm *sv);
static inline uint8_t svpwm_get_sector(tSvpwm *sv) { return sv->sector; };

#endif // __SVPWM_H