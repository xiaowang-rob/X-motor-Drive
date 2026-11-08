#ifndef __ENC_AS5047_H
#define __ENC_AS5047_H

#include "enc.h"

// AS5047：Mode1/16bit，读命令与 NOP 各占一次 CS 事务。
const tEncDevHandle *Get_EncAS5047Handle(void);

#endif // __ENC_AS5047_H
