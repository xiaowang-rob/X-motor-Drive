#ifndef __SVPWM_H
#define __SVPWM_H

#include "bsp_pwm.h"
#include "bsp_adc.h"

typedef struct
{
    float k;
    u8 sector;
    u16 ticu;
    u16 ticv;
    u16 ticw;
} tSvpwm;


// SVPWM 核心接口
void svpwm_init(float Vbus);
void svpwm_run(float ualpha, float ubeta);
void svpwm_sample_point_calibration(void);
void svpwm_set_vbus(float Vbus);
u8 svpwm_get_sector(void);

// 调试接口（理论电压）
float get_voltage_u(void);
float get_voltage_v(void);
float get_voltage_w(void);

#endif // __SVPWM_H