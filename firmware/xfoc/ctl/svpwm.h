#ifndef __SVPWM_H
#define __SVPWM_H

// SVPWM 核心接口

void svpwm_update(uint8_t sector, float da, float db, float *Du, float *Dv,
                  float *Dw);
void svpwm_vbus_calibration(float Vbus);
uint16_t svpwm_sp_calibration(tSvpwm *sv);
static inline uint8_t svpwm_get_sector(tSvpwm *sv) { return sv->sector; };

#endif // __SVPWM_H