#include "svpwm.h"
#include "control.h"
#include "xmath.h"

// 输入 alpha/beta轴的 占空比 输出 三相占空比
void svpwm_update(uint8_t sector, float da, float db, float *Du, float *Dv,
                  float *Dw) {

    // 反clark变换
    float duty_u, duty_v, duty_w;
    inv_clarke_transform(da, db, &duty_u, &duty_v, &duty_w);

    // 求最大最小调制 查表简化计算
    float Dmax, Dmin;
    switch (sector) {
    case 1:
        Dmax = duty_u;
        Dmin = duty_w;
        break;
    case 2:
        Dmax = duty_v;
        Dmin = duty_w;
        break;
    case 3:
        Dmax = duty_v;
        Dmin = duty_u;
        break;
    case 4:
        Dmax = duty_w;
        Dmin = duty_u;
        break;
    case 5:
        Dmax = duty_w;
        Dmin = duty_v;
        break;
    case 6:
        Dmax = duty_u;
        Dmin = duty_v;
        break;
    default:
        Dmax = 0.0f;
        Dmin = 0.0f;
        break; // 零矢量/异常
    }

    // 零序注入
    float Doffset = -0.5f * (Dmax + Dmin);

    *Du = XCLAMP(duty_u + 0.5f + Doffset, 0.0f, 1.0f);
    *Dv = XCLAMP(duty_v + 0.5f + Doffset, 0.0f, 1.0f);
    *Dw = XCLAMP(duty_w + 0.5f + Doffset, 0.0f, 1.0f);
}

// 电流采样点校准 返回采样时机ccr
uint16_t svpwm_sp_calibration(tSvpwm *sv) {
    uint16_t tic_ref; // 当前扇区的参考相计数
    uint16_t tic_out; // 目标点ccr
    const uint16_t ticall = sv->pwm_drv->cfg.ticTd + sv->pwm_drv->cfg.ticTn +
                            sv->pwm_drv->cfg.ticTs;
    const uint16_t tic_td_tn = sv->pwm_drv->cfg.ticTd + sv->pwm_drv->cfg.ticTn;
    // 根据扇区确定参考相，并保存tic值
    switch (sv->sector) {
    case 0:
    case 7:
        tic_ref = sv->pwm_drv->cfg.pwm_arr / 2;
        break;
    case 1:
    case 6:
        tic_ref = sv->ticA;
        break;
    case 2:
    case 3:
        tic_ref = sv->ticB;
        break;
    default: // 45
        tic_ref = sv->ticC;
        break;
    }

    // 根据参考相占空比判断调制深度
    if (tic_ref > sv->pwm_drv->cfg.ticTd + sv->pwm_drv->cfg.ticTn) {
        sv->index = 1; // 低调制
    } else if (ticall > 2 * tic_ref) {
        sv->index = 3; // 高调制
    } else {
        sv->index = 2; // 中调制
    }

    // 设置ADC采样触发点
    switch (sv->index) {
    case 1:
        tic_out = tic_ref - sv->pwm_drv->cfg.ticTs;
        break;
    case 2:
        tic_out = tic_ref + sv->pwm_drv->cfg.ticTs;
        break;
    case 3:
        // 确保减后不溢出（可根据实际需求加限幅）
        if (tic_ref >= tic_td_tn)
            tic_out = tic_ref - tic_td_tn;
        else
            tic_out = sv->pwm_drv->cfg.pwm_arr - 1; // 默认采样点
        break;
    default:
        break;
    }
    return tic_out;
}
