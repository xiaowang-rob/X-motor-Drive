#include "gate_drv.h"

#include "bsp_adc.h"
#include "bsp_pwm.h"


static inline uint8_t pwm_sector(uint16_t u, uint16_t v, uint16_t w) {
    if (v > w) {
        if (u > v)
            return 1;
        return u > w ? 2 : 3;
    }
    if (v < w) {
        if (u < v)
            return 4;
        return u < w ? 5 : 6;
    }
    if (u == v)
        return 0;
    return u > v ? 1 : 4;
}


static void gd_calibration(tGateDrv *gd) {
    uint16_t tic_ref; // 当前扇区的参考相计数
    uint16_t tic_out; // 目标点ccr
    uint8_t index;
    const uint16_t ticall = gd->pd.cfg.ticTd +
                            gd->pd.cfg.ticTn + gd->pd.cfg.ticTs;
    const uint16_t tic_td_tn =
            gd->pd.cfg.ticTd + gd->pd.cfg.ticTn;
    // 根据扇区确定参考相，并保存tic值
    switch (gd->sector) {
        case 0:
        case 7:
            tic_ref = gd->pd.cfg.pwm_arr / 2;
            break;
        case 1:
        case 6:
            tic_ref = gd->ticU;
            break;
        case 2:
        case 3:
            tic_ref = gd->ticV;
            break;
        default: // 45
            tic_ref = gd->ticW;
            break;
    }

    // 根据参考相占空比判断调制深度
    if (tic_ref > gd->pd.cfg.ticTd + gd->pd.cfg.ticTn) {
        index = 1; // 低调制
    } else if (ticall > 2 * tic_ref) {
        index = 3; // 高调制
    } else {
        index = 2; // 中调制
    }

    // 设置ADC采样触发点
    switch (index) {
        case 1:
            tic_out = tic_ref - gd->pd.cfg.ticTs;
            break;
        case 2:
            tic_out = tic_ref + gd->pd.cfg.ticTs;
            break;
        case 3:
            // 确保减后不溢出（可根据实际需求加限幅）
            if (tic_ref >= tic_td_tn)
                tic_out = tic_ref - tic_td_tn;
            else
                tic_out = gd->pd.cfg.pwm_arr - 1; // 默认采样点
            break;
        default:
            break;
    }

    sample_set_sample_point(&gd->sp, tic_out);
}


bool gd_init(tGateDrv *gd) {
    if (!sample_init(&gd->sp, Get_MCU_ADCHandle()))
        return false;

    if (!pwm_init(&gd->pd, Get_MCU_PWMHandle()))
        return false;

    gd->Vbus = gd->sp.vbus;
    gd->Temp = gd->sp.temperature;
    gd->pwm_arr = gd->pd.cfg.pwm_arr;
    pwm_power_on(&gd->pd, true);
}

void gd_loop_task(tGateDrv *gd) {
    sample_update(&gd->sp, !gd->enable);
    gd->Vbus = gd->sp.vbus;
    gd->Temp = gd->sp.temperature;
}

// 对外接口
void gd_poweron(tGateDrv *gd, bool on) { pwm_power_on(&gd->pd, on); }

void gd_pwmenable(tGateDrv *gd, bool en) { pwm_enable(&gd->pd, en); }

void gd_output(tGateDrv *gd, float Du, float Dv, float Dw) {
    // TODO：添加死区补偿
    /*
    // 假设 duty_a, duty_b, duty_c 已由 SVPWM 生成，范围 [0.0, 1.0]
// dead_time_norm = t_dead / T_pwm，例如 500ns / 50us = 0.01
float dead_time_norm = 0.01f;

float sign_a = (i_a > 0.0f) ? 1.0f : ((i_a < 0.0f) ? -1.0f : 0.0f);
float sign_b = (i_b > 0.0f) ? 1.0f : ((i_b < 0.0f) ? -1.0f : 0.0f);
float sign_c = (i_c > 0.0f) ? 1.0f : ((i_c < 0.0f) ? -1.0f : 0.0f);

duty_a += sign_a * dead_time_norm;
duty_b += sign_b * dead_time_norm;
duty_c += sign_c * dead_time_norm;

// 在写入比较寄存器前必须限幅
duty_a = clampf(duty_a, 0.0f, 1.0f);
duty_b = clampf(duty_b, 0.0f, 1.0f);
duty_c = clampf(duty_c, 0.0f, 1.0f);
*/
    gd->ticU = (uint16_t) (Du * (float) gd->pwm_arr + 0.5f);
    gd->ticV = (uint16_t) (Dv * (float) gd->pwm_arr + 0.5f);
    gd->ticW = (uint16_t) (Dw * (float) gd->pwm_arr + 0.5f);
    pwm_set_compare(
        &gd->pd, gd->ticU, gd->ticV, gd->ticW);
    gd->sector = pwm_sector(gd->ticU, gd->ticV, gd->ticW);
}

void gd_read_sample(const tGateDrv *gd, float is[3], float *vbus) {
    const float *current = gd->sp.cur;

#if GD_CURRENT_SAMPLING == GD_SAMPLE_ABC
    if (gd->sector == 1 || gd->sector == 6) {
        // 最短相=W
        is[0] = current[1] + current[2];
        is[1] = -current[1];
        is[2] = -current[2];
    } else if (gd->sector == 2 || gd->sector == 3) {
        // 最短相=U
        is[0] = -current[0];
        is[1] = current[0] + current[2];
        is[2] = -current[2];
    } else if (gd->sector == 4 || gd->sector == 5) {
        // 最短相=V
        is[0] = -current[0];
        is[1] = -current[1];
        is[2] = current[0] + current[1];
    } else {
        // gd->sector 0/7: 零矢量
        is[0] = current[0];
        is[1] = current[1];
        is[2] = current[2];
    }
#elif GD_CURRENT_SAMPLING == GD_SAMPLE_UV
    is[0] = current[0];
    is[1] = current[1];
    is[2] = -(is[0] + is[1]);
#elif GD_CURRENT_SAMPLING == GD_SAMPLE_UW
    is[0] = current[0];
    is[2] = current[2];
    is[1] = -(is[0] + is[2]);
#elif GD_CURRENT_SAMPLING == GD_SAMPLE_VW
    is[1] = current[1];
    is[2] = current[2];
    is[0] = -(is[1] + is[2]);
#else
#error "Invalid GD_CURRENT_SAMPLING"
#endif
    *vbus = gd->Vbus;
}
