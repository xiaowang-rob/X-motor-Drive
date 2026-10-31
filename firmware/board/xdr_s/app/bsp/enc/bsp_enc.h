#ifndef __BSP_ENC_H
#define __BSP_ENC_H

#include "encoder.h"
#include "parameter_list.h"

// sincos encoder
const tEncDevHandle *Get_SINCOS_ENCHandle(void);
// spi encoder handle
const tEncDevHandle *Get_SPI_ENCHandle(eSpiEncList chip);
// 内置 encoder
const tEncDevHandle *Get_INT_ENCHandle(void);
// abi encoder
const tEncDevHandle *Get_ABI_ENCHandle(void);
// hall encoder
const tEncDevHandle *Get_HALL_ENCHandle(void);

#endif