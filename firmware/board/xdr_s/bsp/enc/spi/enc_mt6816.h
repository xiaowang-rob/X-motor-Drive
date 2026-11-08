#ifndef __ENC_MT6816_H
#define __ENC_MT6816_H

#include "enc.h"

// MT6816：Mode3/16bit，两次独立 CS 事务读取 0x03、0x04 寄存器。
const tEncDevHandle *Get_EncIntHandle(void);
const tEncDevHandle *Get_EncMT6816Handle(void);

#endif // __ENC_MT6816_H
