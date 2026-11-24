#ifndef __GTAE_DRV_H
#define __GTAE_DRV_H

#include "pwm.h"
#include "sample.h"

#define GD_SAMPLE_ABC 3
#define GD_SAMPLE_UV  1
#define GD_SAMPLE_UW  2
#define GD_SAMPLE_VW  4

/* 选择 ADC 能直接提供的相，缺失相由 gd_get_current() 重构。 */
#ifndef GD_CURRENT_SAMPLING
#define GD_CURRENT_SAMPLING GD_SAMPLE_ABC
#endif
// 所有栅极驱动的管理
typedef struct {
    tPwmDrv pd; // pwm 驱动业务
    tSample sp; // 采样业务

    bool sample_ready;
    bool enable;
    float Vbus;
    float Temp;
    uint16_t pwm_arr;
    uint8_t sector;
    uint16_t ticU, ticV, ticW;

} tGateDrv;

bool gd_init(tGateDrv *gd);
void gd_board_task(tGateDrv *gd);
void gd_loop_task(tGateDrv *gd);
// 对外接口
void gd_poweron(tGateDrv *gd, bool on);
void gd_pwmenable(tGateDrv *gd, bool en);
void gd_output(tGateDrv *gd, float Du, float Dv, float Dw);
void gd_read_sample(const tGateDrv *gd, float is[3], float *vbus);
#endif
