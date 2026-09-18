#include "svpwm.h"
#include "math_fast.h"

void svpwm_init(tSvpwm *sv, uint16_t tic_pwm, float Vbus,
                float tpwm, float ts_us, float tn_us, float td_us)
{
    memset(sv, 0, sizeof(tSvpwm));
    sv->tic_pwm = tic_pwm;
    sv->vbus = Vbus;
    sv->k = MATH_SQRT3 * (float)sv->tic_pwm / Vbus;

    sv->ticTs = ts_us * sv->tic_pwm / (tpwm * 1000000); // 采样时间提前量（计数值）
    sv->ticTn = tn_us * sv->tic_pwm / (tpwm * 1000000); // 噪声时间（计数值）
    sv->ticTd = td_us * sv->tic_pwm / (tpwm * 1000000); // 死区时间（计数值）
}

void svpwm_update(tSvpwm *sv, float ua, float ub)
{
    // 反clark变换，不是标准的，只是为了方便判断扇区
    float U1 = ub;
    float U2 = MATH_SQRT3_2 * ua - 0.5f * ub;
    float U3 = -U2 - ub;

    uint8_t A = U1 > 0;
    uint8_t B = U2 > 0;
    uint8_t C = U3 > 0;
    uint8_t vN = 4 * C + 2 * B + A;

    // 计算三相电压时间分量，超前当前转子三轴90°，定子的三轴需要输出的电压，转化为计数值
    float X = sv->k * U1;
    float Y = sv->k * U3;
    float Z = sv->k * U2;

    float T1, T2, T0; // T1:第一相作用时间 T2:第二相作用时间 T0:零向量作用时间
    // 分配时间和扇区（扇区是转子的扇区）
    switch (vN)
    {
    case 0:             // Zero vector (all negative)
    case 7:             // Zero vector (all positive)
        sv->sector = 0; // Use special sector 0
        T1 = 0;
        T2 = 0;
        break;
    case 1:
        sv->sector = 2; //+--
        T1 = -Y;
        T2 = -Z;
        break;
    case 2:
        sv->sector = 6; //--+
        T1 = -X;
        T2 = -Y;
        break;
    case 3:
        sv->sector = 1; //+-+
        T1 = X;
        T2 = Z;
        break;
    case 4:
        sv->sector = 4; //-+-
        T1 = -Z;
        T2 = -X;
        break;
    case 5:
        sv->sector = 3; //++-
        T1 = Y;
        T2 = X;
        break;
    case 6:
        sv->sector = 5; //-++
        T1 = Z;
        T2 = Y;
        break;
    default:
        break;
    }

    if (T1 + T2 > sv->tic_pwm)
    {
        float ratio = sv->tic_pwm / (T1 + T2);
        T1 *= ratio;
        T2 *= ratio;
        T0 = 0;
    }
    else
    {
        T0 = sv->tic_pwm - T1 - T2;
    }
    // 以七段式开关序列方式输出--更小的电流纹波和中心对称性（5段 可以减小开关次数）

    float t0 = T0 * 0.5f; // V0作用起点零向量（0，0，0）
    float t1 = t0 + T1;   // V1作用
    float t2 = t1 + T2;   // V2作用

    switch (sv->sector)
    {
    case 1: // V1(100), V2(110)
        sv->ticA = (uint16_t)t2;
        sv->ticB = (uint16_t)t1;
        sv->ticC = (uint16_t)t0;
        break;
    case 2: // V2(110), V3(010)
        sv->ticA = (uint16_t)t1;
        sv->ticB = (uint16_t)t2;
        sv->ticC = (uint16_t)t0;
        break;
    case 3: // V3(010), V4(011)
        sv->ticA = (uint16_t)t0;
        sv->ticB = (uint16_t)t2;
        sv->ticC = (uint16_t)t1;
        break;
    case 4: // V4(011), V5(001)
        sv->ticA = (uint16_t)t0;
        sv->ticB = (uint16_t)t1;
        sv->ticC = (uint16_t)t2;
        break;
    case 5: // V5(001), V6(101)
        sv->ticA = (uint16_t)t1;
        sv->ticB = (uint16_t)t0;
        sv->ticC = (uint16_t)t2;
        break;
    case 6: // V6(101), V1(100)
        sv->ticA = (uint16_t)t2;
        sv->ticB = (uint16_t)t0;
        sv->ticC = (uint16_t)t1;
        break;
    default: // (1,1,1)
        sv->ticA = (uint16_t)t0;
        sv->ticB = (uint16_t)t0;
        sv->ticC = (uint16_t)t0;
        break;
    }
}

// 电压参数校准
void svpwm_vbus_calibration(tSvpwm *sv, float Vbus)
{
    sv->vbus = Vbus;
    sv->k = MATH_SQRT3 * (float)sv->tic_pwm / Vbus;
}

// 电流采样点校准 返回采样时机ccr
uint16_t svpwm_sp_calibration(tSvpwm *sv)
{
    uint16_t tic_ref; // 当前扇区的参考相计数
    uint16_t tic_out; // 目标点ccr
    const ticall = sv->ticTd + sv->ticTn + sv->ticTs;
    const tic_td_tn = sv->ticTd + sv->ticTn;
    // 根据扇区确定参考相，并保存tic值
    switch (sv->sector)
    {
    case 0:
    case 7:
        tic_ref = sv->tic_pwm / 2;
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
    if (tic_ref > sv->ticTd + sv->ticTn)
    {
        sv->index = 1; // 低调制
    }
    else if (ticall > 2 * tic_ref)
    {
        sv->index = 3; // 高调制
    }
    else
    {
        sv->index = 2; // 中调制
    }

    // 设置ADC采样触发点
    switch (sv->index)
    {
    case 1:
        tic_out = tic_ref - sv->ticTs;
        break;
    case 2:
        tic_out = tic_ref + sv->ticTs;
        break;
    case 3:
        // 确保减后不溢出（可根据实际需求加限幅）
        if (tic_ref >= tic_td_tn)
            tic_out = tic_ref - tic_td_tn;
        else
            tic_out = sv->tic_pwm - 1; // 默认采样点
        break;
    default:
        break;
    }
}
