// ============================================================
// sense.c — 采样业务对象实现（abs，纯逻辑）
//
// 原始码 → 物理量；零点 EMA 在空闲阶段累积；
// Vbus/温度按周期节流触发并换算（NTC 温度查表）。
// 原始码经 ops + handle（装配时挂）从驱动取回。
// ============================================================

#include "sense.h"

#include "IF_time.h"

// 温度查表：Vbus 归一化后的 NTC 采样码 → ℃
// 表项 adc_eq 按旧驱动公式 adc_eq = code*24/(Vbus-0.3)+0.5 截断取整
// clang-format off
static const uint8_t SENSE_TEMP_TABLE[256] = {
120, 120, 120, 120, 120, 120, 120, 120, 120, 115, 110, 106, 102,  99,  96,  93,
 90,  88,  86,  84,  82,  80,  78,  77,  75,  74,  72,  71,  70,  68,  67,  66,
 65,  64,  63,  62,  61,  60,  59,  58,  57,  57,  56,  55,  54,  54,  53,  52,
 51,  51,  50,  50,  49,  48,  48,  47,  47,  46,  45,  45,  44,  44,  43,  43,
 42,  42,  42,  41,  41,  40,  40,  39,  39,  38,  38,  38,  37,  37,  37,  36,
 36,  35,  35,  35,  34,  34,  34,  33,  33,  33,  32,  32,  32,  31,  31,  31,
 30,  30,  30,  30,  29,  29,  29,  28,  28,  28,  28,  27,  27,  27,  27,  26,
 26,  26,  26,  25,  25,  25,  25,  24,  24,  24,  24,  23,  23,  23,  23,  23,
 22,  22,  22,  22,  21,  21,  21,  21,  21,  20,  20,  20,  20,  20,  19,  19,
 19,  19,  19,  19,  18,  18,  18,  18,  18,  17,  17,  17,  17,  17,  17,  16,
 16,  16,  16,  16,  16,  15,  15,  15,  15,  15,  15,  14,  14,  14,  14,  14,
 14,  14,  13,  13,  13,  13,  13,  13,  12,  12,  12,  12,  12,  12,  12,  11,
 11,  11,  11,  11,  11,  11,  11,  10,  10,  10,  10,  10,  10,  10,   9,   9,
  9,   9,   9,   9,   9,   9,   8,   8,   8,   8,   8,   8,   8,   8,   7,   7,
  7,   7,   7,   7,   7,   7,   7,   6,   6,   6,   6,   6,   6,   6,   6,   6,
  5,   5,   5,   5,   5,   5,   5,   5,   5,   4,   4,   4,   4,   4,   4,   4,};
// clang-format on

bool sense_init(tSense *s)
{
    if (!s || !s->ops || !s->handle || !s->ops->open ||
        !s->ops->get_cur_raw || !s->ops->get_vt_raw)
        return false;

    // 只重置业务字段；ops / handle 是装配结果，不在本层改动
    s->cur_scale = 0.0f;
    s->vbus_scale = 0.0f;
    for (uint8_t i = 0U; i < 3U; i++)
    {
        s->cur_zero[i] = 0.0f;
        s->cur[i] = 0.0f;
    }
    s->zero_ready = false;
    s->vbus = 0.0f;
    s->temperature = 0.0f;
    s->last_vt_ms = 0U;

    // 启动采样前端并取回换算系数；返回前资源须可用
    return s->ops->open(s->handle, &s->cur_scale, &s->vbus_scale);
}

void sense_set_sample_point(tSense *s, uint32_t tic)
{
    if (!s || !s->ops || !s->ops->set_sample_point)
        return;
    s->ops->set_sample_point(s->handle, tic);
}

void sense_update(tSense *s, bool motor_idle)
{
    if (!s || !s->ops || !s->ops->get_cur_raw)
        return;

    uint16_t raw[3];
    if (!s->ops->get_cur_raw(s->handle, raw))
        return;

    // ---- 三相电流 / 零点 ----
    if (motor_idle)
    {
        // 空闲：电流应为 0，用采样码累积零点（EMA）
        for (uint8_t i = 0U; i < 3U; i++)
        {
            if (!s->zero_ready)
                s->cur_zero[i] = (float)raw[i]; // 首次直接取，避免大跳
            else
                s->cur_zero[i] += ((float)raw[i] - s->cur_zero[i]) * SENSE_IDLE_K;
            s->cur[i] = 0.0f;
        }
        s->zero_ready = true;
    }
    else
    {
        if (s->zero_ready)
        {
            for (uint8_t i = 0U; i < 3U; i++)
                s->cur[i] = ((float)raw[i] - s->cur_zero[i]) * s->cur_scale;
        }
        else
        {
            // 零点未校准（产品流程要求使能前先空闲校准），先输出 0 防异常大电流
            for (uint8_t i = 0U; i < 3U; i++)
                s->cur[i] = 0.0f;
        }
    }

    // ---- Vbus / 温度（节流触发 + 取新帧换算） ----
    uint32_t now = time_get_ms();
    if ((now - s->last_vt_ms) >= SENSE_VT_REFRESH_MS)
    {
        if (s->ops->vt_trigger)
            s->ops->vt_trigger(s->handle);
        s->last_vt_ms = now;
    }

    uint16_t vbus_raw = 0U, temp_raw = 0U;
    if (s->ops->get_vt_raw(s->handle, &vbus_raw, &temp_raw))
    {
        s->vbus = (float)vbus_raw * s->vbus_scale;

        // NTC：采样码按母线电压归一后查表（用 uint16 计算再 clamp，防低 Vbus 溢出回绕）
        if (s->vbus > 0.4f)
        {
            uint16_t adc_eq = (uint16_t)((float)temp_raw * 24.0f / (s->vbus - 0.3f) + 0.5f);
            if (adc_eq > 255U)
                adc_eq = 255U;
            s->temperature = (float)SENSE_TEMP_TABLE[adc_eq];
        }
    }
}

void sense_get_current(const tSense *s, float *iu, float *iv, float *iw)
{
    if (!s)
        return;
    if (iu)
        *iu = s->cur[0];
    if (iv)
        *iv = s->cur[1];
    if (iw)
        *iw = s->cur[2];
}

float sense_get_vbus(const tSense *s)
{
    return s ? s->vbus : 0.0f;
}

float sense_get_temperature(const tSense *s)
{
    return s ? s->temperature : 0.0f;
}

bool sense_is_zero_ready(const tSense *s)
{
    return s ? s->zero_ready : false;
}
