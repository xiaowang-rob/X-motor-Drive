#ifndef __ENC_MT6816_H
#define __ENC_MT6816_H

#include "encoder.h"

// MT6816：Mode3/16bit，两次独立 CS 事务读取 0x03、0x04 寄存器。
const tEncDevHandle *enc_mt6816_int_handle(void);
const tEncDevHandle *enc_mt6816_ext_handle(void);

#endif // __ENC_MT6816_H
