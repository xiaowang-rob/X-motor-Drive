#include "gate_drv.h"
#include "xmath.h"

tGateDrv g_gate_drv;

void gd_init(tPwmDrv *pd, tSample *sp) {
    if (!pd || !sp)
        return;
    g_gate_drv.pd = pd;
    g_gate_drv.sp = sp;
    g_gate_drv.Vbus = g_gate_drv.sp->vbus;
    // g_motor.fb.vmax = g_motor.fb.udc * XMATH_INSQRT3;
    g_gate_drv.Temp = g_gate_drv.sp->temperature;
    g_gate_drv.pwm_arr = g_gate_drv.pd->cfg.pwm_arr;
    pwm_power_on(g_gate_drv.pd, true);
}

void gd_loop_task(bool motor_idle) {
    sample_update(g_gate_drv.sp, motor_idle);

    g_gate_drv.Vbus = g_gate_drv.sp->vbus;
    // g_motor.fb.vmax = g_motor.fb.udc * XMATH_INSQRT3;
    g_gate_drv.Temp = g_gate_drv.sp->temperature;
}
// 对外接口
void gd_poweron(bool on) { pwm_power_on(g_gate_drv.pd, on); }
void gd_enable(bool en) { pwm_enable(g_gate_drv.pd, en); }
void gd_output(float Du, float Dv, float Dw) {
    g_gate_drv.ticU = Du * g_gate_drv.pwm_arr + 0.5f;
    g_gate_drv.ticV = Dv * g_gate_drv.pwm_arr + 0.5f;
    g_gate_drv.ticW = Dw * g_gate_drv.pwm_arr + 0.5f;
    pwm_set_compare(
        g_gate_drv.pd, g_gate_drv.ticU, g_gate_drv.ticV, g_gate_drv.ticW);
}
void gd_get_current(float is[3]) {
    memcpy(g_gate_drv.sp->cur, is, 3 * sizeof(float));
}
void gd_calibration(uint8_t sec) {
    uint16_t tic_ref; // 当前扇区的参考相计数
    uint16_t tic_out; // 目标点ccr
    uint8_t index;
    const uint16_t ticall = g_gate_drv.pd->cfg.ticTd +
                            g_gate_drv.pd->cfg.ticTn + g_gate_drv.pd->cfg.ticTs;
    const uint16_t tic_td_tn =
        g_gate_drv.pd->cfg.ticTd + g_gate_drv.pd->cfg.ticTn;
    // 根据扇区确定参考相，并保存tic值
    switch (sec) {
    case 0:
    case 7:
        tic_ref = g_gate_drv.pd->cfg.pwm_arr / 2;
        break;
    case 1:
    case 6:
        tic_ref = g_gate_drv.ticU;
        break;
    case 2:
    case 3:
        tic_ref = g_gate_drv.ticV;
        break;
    default: // 45
        tic_ref = g_gate_drv.ticW;
        break;
    }

    // 根据参考相占空比判断调制深度
    if (tic_ref > g_gate_drv.pd->cfg.ticTd + g_gate_drv.pd->cfg.ticTn) {
        index = 1; // 低调制
    } else if (ticall > 2 * tic_ref) {
        index = 3; // 高调制
    } else {
        index = 2; // 中调制
    }

    // 设置ADC采样触发点
    switch (index) {
    case 1:
        tic_out = tic_ref - g_gate_drv.pd->cfg.ticTs;
        break;
    case 2:
        tic_out = tic_ref + g_gate_drv.pd->cfg.ticTs;
        break;
    case 3:
        // 确保减后不溢出（可根据实际需求加限幅）
        if (tic_ref >= tic_td_tn)
            tic_out = tic_ref - tic_td_tn;
        else
            tic_out = g_gate_drv.pd->cfg.pwm_arr - 1; // 默认采样点
        break;
    default:
        break;
    }

    sample_set_sample_point(g_gate_drv.sp, tic_out);
}