#include "svpwm.h"
#include "usr_config.h"
#include "math_fast.h"
#include "string.h"
#include "device.h"

tSvpwm g_svpwm = {0};

void svpwm_init(float Vbus)
{
    memset(&g_svpwm, 0, sizeof(tSvpwm));
    g_svpwm.k = MATH_SQRT3 * (float)TIC_PWM / Vbus;
}

void svpwm_run(float ualpha, float ubeta)
{
    // 反clark变换，不是标准的，只是为了方便判断扇区
    float U1 = ubeta;
    float U2 = MATH_SQRT3_2 * ualpha - 0.5f * ubeta;
    float U3 = -U2 - ubeta;

    u8 A = U1 > 0;
    u8 B = U2 > 0;
    u8 C = U3 > 0;
    u8 vN = 4 * C + 2 * B + A;

    // 计算三相电压时间分量，超前当前转子三轴90°，定子的三轴需要输出的电压，转化为计数值
    float X = g_svpwm.k * U1;
    float Y = g_svpwm.k * U3;
    float Z = g_svpwm.k * U2;

    float T1, T2, T0; // T1:第一相作用时间 T2:第二相作用时间 T0:零向量作用时间
    // 分配时间和扇区（扇区是转子的扇区）
    switch (vN)
    {
    case 0:                 // Zero vector (all negative)
    case 7:                 // Zero vector (all positive)
        g_svpwm.sector = 0; // Use special sector 0
        T1 = 0;
        T2 = 0;
        break;
    case 1:
        g_svpwm.sector = 2; //+--
        T1 = -Y;
        T2 = -Z;
        break;
    case 2:
        g_svpwm.sector = 6; //--+
        T1 = -X;
        T2 = -Y;
        break;
    case 3:
        g_svpwm.sector = 1; //+-+
        T1 = X;
        T2 = Z;
        break;
    case 4:
        g_svpwm.sector = 4; //-+-
        T1 = -Z;
        T2 = -X;
        break;
    case 5:
        g_svpwm.sector = 3; //++-
        T1 = Y;
        T2 = X;
        break;
    case 6:
        g_svpwm.sector = 5; //-++
        T1 = Z;
        T2 = Y;
        break;
    default:
        break;
    }

    if (T1 + T2 > TIC_PWM)
    {
        float ratio = TIC_PWM / (T1 + T2);
        T1 *= ratio;
        T2 *= ratio;
        T0 = 0;
    }
    else
    {
        T0 = TIC_PWM - T1 - T2;
    }
    // 以七段式开关序列方式输出--更小的电流纹波和中心对称性（5段 可以减小开关次数）

    float t0 = T0 * 0.5f; // V0作用起点零向量（0，0，0）
    float t1 = t0 + T1;   // V1作用
    float t2 = t1 + T2;   // V2作用

    switch (g_svpwm.sector)
    {
    case 1: // V1(100), V2(110)
        g_svpwm.ticu = (u16)t2;
        g_svpwm.ticv = (u16)t1;
        g_svpwm.ticw = (u16)t0;
        break;
    case 2: // V2(110), V3(010)
        g_svpwm.ticu = (u16)t1;
        g_svpwm.ticv = (u16)t2;
        g_svpwm.ticw = (u16)t0;
        break;
    case 3: // V3(010), V4(011)
        g_svpwm.ticu = (u16)t0;
        g_svpwm.ticv = (u16)t2;
        g_svpwm.ticw = (u16)t1;
        break;
    case 4: // V4(011), V5(001)
        g_svpwm.ticu = (u16)t0;
        g_svpwm.ticv = (u16)t1;
        g_svpwm.ticw = (u16)t2;
        break;
    case 5: // V5(001), V6(101)
        g_svpwm.ticu = (u16)t1;
        g_svpwm.ticv = (u16)t0;
        g_svpwm.ticw = (u16)t2;
        break;
    case 6: // V6(101), V1(100)
        g_svpwm.ticu = (u16)t2;
        g_svpwm.ticv = (u16)t0;
        g_svpwm.ticw = (u16)t1;
        break;
    default: // (1,1,1)
        g_svpwm.ticu = (u16)t0;
        g_svpwm.ticv = (u16)t0;
        g_svpwm.ticw = (u16)t0;
        break;
    }

    // 更新比较值
    bsp_pwm_set_compare(g_svpwm.ticu, g_svpwm.ticv, g_svpwm.ticw);
}
// 电流采样点改变
u8 change_Index = 0;
const u16 ticTs = T_SAMPLE_us * TIC_PWM / (T_PWM * 1000000);   // 采样时间提前量（计数值）
const u16 ticTn = T_NOISE_us * TIC_PWM / (T_PWM * 1000000);    // 噪声时间（计数值）
const u16 ticTd = T_DEADTIME_us * TIC_PWM / (T_PWM * 1000000); // 死区时间（计数值）
const u16 ticAll = ticTs + ticTn + ticTd;                      // 总时间（计数值

void svpwm_sample_point_calibration()
{
    u16 tic_ref; // 当前扇区的参考相计数

    // 根据扇区确定参考相，并保存tic值
    switch (g_svpwm.sector)
    {
    case 0:
    case 7:
        tic_ref = TIC_PWM / 2;
        break;
    case 1:
    case 6:
        tic_ref = g_svpwm.ticu;
        break;
    case 2:
    case 3:
        tic_ref = g_svpwm.ticv;
        break;
    default: // 45
        tic_ref = g_svpwm.ticw;
        break;
    }

    // 根据参考相占空比判断调制深度
    if (tic_ref > ticTd + ticTn)
    {
         change_Index = 1; // 低调制
    }
    else if (ticAll > 2 * tic_ref)
    {
         change_Index = 3; // 高调制
    }
    else
    {
         change_Index = 2; // 中调制
    }

    // 设置ADC采样触发点
    switch (change_Index)
    {
    case 1:
        //        fAdcSampleChange(ticpwm - 1);
        bsp_adc_sample_change(tic_ref - ticTs);
        break;
    case 2:
        //        fAdcSampleChange(tic_ref + ticTs);
        bsp_adc_sample_change(tic_ref - ticTs);
        break;
    case 3:
        // 确保减后不溢出（可根据实际需求加限幅）
        if (tic_ref >= ticTd + ticTn)
            bsp_adc_sample_change(tic_ref - ticTd - ticTn);
        else
            bsp_adc_sample_change(0);
        break;
    default:
        break;
    }
}
float get_voltage_u()
{
    return g_svpwm.ticu * MATH_SQRT3 / g_svpwm.k;
}
float get_voltage_v()
{
    return g_svpwm.ticv * MATH_SQRT3 / g_svpwm.k;
}
float get_voltage_w()
{
    return g_svpwm.ticw * MATH_SQRT3 / g_svpwm.k;
}

void svpwm_set_vbus(float Vbus)
{
    g_svpwm.k = MATH_SQRT3 * TIC_PWM / Vbus;
}

u8 svpwm_get_sector()
{
    return g_svpwm.sector;
}