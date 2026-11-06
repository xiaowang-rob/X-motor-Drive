#ifndef __ENC_MT6835_H
#define __ENC_MT6835_H

#include "encoder.h"

// MT6835：Mode3/8bit，一次连续 5 字节 CS 事务。
const tEncDevHandle *enc_mt6835_ext_handle(void);

#endif // __ENC_MT6835_H
