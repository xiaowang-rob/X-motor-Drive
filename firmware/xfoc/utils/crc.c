// ============================================================
// crc.c — 校验工具实现（纯 C，无 CMSIS / HAL 依赖）
// ============================================================

#include "crc.h"

// ---- CRC8：poly 0x07，无反射，初值 0 ----
uint8_t crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0U;
    for (uint16_t i = 0U; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t b = 0U; b < 8U; b++)
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ 0x07U) : (uint8_t)(crc << 1);
    }
    return crc;
}

// ---- CRC32：IEEE 802.3（反射，poly 0xEDB88320） ----
// 表在首次使用时惰性构建：省 1KB flash，只多一次性 256×8 次循环
static uint32_t s_crc32_table[256];
static bool s_crc32_table_ready = false;

static void crc32_build_table(void)
{
    for (uint32_t i = 0U; i < 256U; i++)
    {
        uint32_t c = i;
        for (uint8_t k = 0U; k < 8U; k++)
            c = (c & 1U) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
        s_crc32_table[i] = c;
    }
    s_crc32_table_ready = true;
}

uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    if (!data)
        return crc;
    if (!s_crc32_table_ready)
        crc32_build_table();

    for (uint32_t i = 0U; i < len; i++)
        crc = s_crc32_table[(crc ^ data[i]) & 0xFFU] ^ (crc >> 8);
    return crc;
}

uint32_t crc32(const uint8_t *data, uint32_t len)
{
    return crc32_update(0xFFFFFFFFU, data, len) ^ 0xFFFFFFFFU;
}
