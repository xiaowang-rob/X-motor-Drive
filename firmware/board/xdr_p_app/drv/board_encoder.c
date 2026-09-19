// ============================================================
// board_encoder.c — 编码器的板级钩子实现（编译期绑定接缝）
//
// 实现 app/abs/encoder_board.h 声明的三个钩子；abs 的 encoder.c
// 直接调用它们，链接期解析，无 ops 表、无 void* 强转。
//
// 装配事实：
//   - 内置编码器是板载硬件，型号固定（见 BOARD_INT_ENC_CHIP）
//   - 外部编码器型号来自参数，本文件按型号分派到对应芯片驱动
//   - 每路当前生效的型号记在 s_chip[]，read/abort 据此分派
// ============================================================
#include "encoder_board.h"

#include "enc_as5047.h"
#include "enc_mt6816.h"
#include "enc_mt6835.h"
#include "enc_spi.h"

// ---------- 本板装配配置 ----------
#define BOARD_INT_ENC_CHIP ENC_CHIP_MT6816 // 板载编码器型号

// ---------- 每路当前型号 ----------
static eEncoderChipId s_chip[2] = {
    [EXT_ENCODER] = ENC_CHIP_NONE,
    [INT_ENCODER] = BOARD_INT_ENC_CHIP,
};

bool encoder_board_open(eEncoderType type, eEncoderChipId chip, uint16_t *resolution)
{
    if (!resolution || type > INT_ENCODER)
        return false;

    // 内置编码器：型号由板载硬件决定，忽略传入的 chip
    if (type == INT_ENCODER)
        chip = BOARD_INT_ENC_CHIP;

    bool ok;
    switch (chip)
    {
    case ENC_CHIP_MT6816:
        ok = mt6816_open(type, resolution);
        break;
    case ENC_CHIP_MT6835:
        ok = mt6835_open(type, resolution);
        break;
    case ENC_CHIP_AS5047:
        ok = as5047_open(type, resolution);
        break;
    case ENC_CHIP_NONE:
    default:
        return false; // 未装配
    }

    if (ok)
        s_chip[type] = chip;
    return ok;
}

bool encoder_board_read(eEncoderType type, uint16_t *raw, uint32_t *ts_ms)
{
    if (type > INT_ENCODER)
        return false;

    switch (s_chip[type])
    {
    case ENC_CHIP_MT6816:
        return mt6816_read(type, raw, ts_ms);
    case ENC_CHIP_MT6835:
        return mt6835_read(type, raw, ts_ms);
    case ENC_CHIP_AS5047:
        return as5047_read(type, raw, ts_ms);
    case ENC_CHIP_NONE:
    default:
        return false;
    }
}

void encoder_board_abort(eEncoderType type)
{
    if (type > INT_ENCODER)
        return;

    switch (s_chip[type])
    {
    case ENC_CHIP_MT6816:
        mt6816_abort(type);
        break;
    case ENC_CHIP_MT6835:
        mt6835_abort(type);
        break;
    case ENC_CHIP_AS5047:
        as5047_abort(type);
        break;
    default:
        enc_spi_abort(type); // 未装配也要保证 CS 抬起
        break;
    }
}
