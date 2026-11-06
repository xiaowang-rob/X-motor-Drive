// 编码器总 API：把观测器类型映射到具体驱动句柄。
// 本文件是唯一 include 各具体驱动接口头的地方。

#include "bsp_enc.h"

#include "abi/enc_abi.h"
#include "hall/enc_hall.h"
#include "sincos/enc_sincos.h"
#include "spi/enc_as5047.h"
#include "spi/enc_mt6816.h"
#include "spi/enc_mt6835.h"

const tEncDevHandle *bsp_enc_get_handle(eObsList enc, eSpiEncList spi_chip) {
    switch (enc) {
    case OBS_INT_ENC:
        return enc_mt6816_int_handle();
    case OBS_ENC_HALL:
        return enc_hall_handle();
    case OBS_ENC_SPI:
        switch (spi_chip) {
        case ENC_MT6816:
            return enc_mt6816_ext_handle();
        case ENC_MT6835:
            return enc_mt6835_ext_handle();
        case ENC_AS5047:
            return enc_as5047_ext_handle();
        default:
            return NULL;
        }
    case OBS_ENC_ABI:
        return enc_abi_handle();
    case OBS_ENC_SINCOS:
        return enc_sincos_handle();
    case OBS_NONE:
    default:
        return NULL;
    }
}
