#ifndef __BSP_ENC_H
#define __BSP_ENC_H

#include "encoder.h"
#include "parameter_list.h"

// sincos encoder
tEncDevHandle *Get_SINCOS_ENCHandle(void);
// spi encoder handle
tEncDevHandle *Get_SPI_ENCHandle(eSpiEncList chip);
// 内置 encoder
tEncDevHandle *Get_INT_ENCHandle(void);
// abi encoder
tEncDevHandle *Get_ABI_ENCHandle(void);
// hall encoder
tEncDevHandle *Get_HALL_ENCHandle(void);

#endif