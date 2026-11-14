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