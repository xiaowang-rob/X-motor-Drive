#ifndef __CRC_H
#define __CRC_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// crc.h — 校验工具（纯 C，无 CMSIS / HAL 依赖，可按 host 编译）
//
//   crc8  ：poly 0x07，无反射，初值 0        —— 帧校验
//   crc32 ：IEEE 802.3（反射，poly 0xEDB88320）—— 固件整区校验
//           与 zlib.crc32 / Python zlib.crc32 结果一致，便于上位机配合
// ============================================================

uint8_t crc8(const uint8_t *data, uint16_t len);

// 增量式 CRC32：初值传 0xFFFFFFFF，结束时再取反
uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len);

// 一次性 CRC32（内部已处理初值与末尾取反）
uint32_t crc32(const uint8_t *data, uint32_t len);

#endif // __CRC_H
