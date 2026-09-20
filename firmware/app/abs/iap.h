#ifndef __IAP_H
#define __IAP_H

#include "flash.h"

// ============================================================
// iap.h — 在线升级（IAP）业务对象（abs）
//
// 设计要点：
//   1) IAP **不自建读写 ops**，而是复用 Flash 介质的 ops + handle
//      （装配层把同一份介质 ops/handle 同时挂给 flash 与 iap）+ 一份分区表；
//   2) 跳转能力是平台行为，由板级以回调注入；
//   3) 校验采用**整区 CRC32**：上位机只给期望值，设备流式遍历 flash 计算，
//      无需把固件数据再搬一遍（省 RAM 与传输）。
// ============================================================

// 分区几何（绝对地址 / 大小）
typedef struct
{
    uint32_t base;
    uint32_t size;
} tIAPPartition;

// IAP 配置：介质（复用 flash ops/handle）+ 分区表 + 平台跳转

typedef struct
{
    const tFlashOps *flash_ops; // 复用的介质 ops
    void *flash_handle;         // 复用的介质实例
    const tIAPPartition *parts; // 分区表，按 eIAPtype 索引
    bool (*jump)(void);         // 平台跳转（板级注入）
} tIAP;

// 启动对象：cfg 须已由装配层填好（parts / flash_ops 为空返回 false）
bool iap_init(tIAP *iap);

// ---- 分区操作（以 iap->cfg.type 为默认分区） ----
bool iap_erase(tIAP *iap);
bool iap_write(tIAP *iap, uint32_t offset, const uint8_t *data, uint32_t len);
bool iap_read(tIAP *iap, uint32_t offset, uint8_t *data, uint32_t len);

// 整区校验：流式计算整个分区的 CRC32 并与 expect_crc 比对
bool iap_verify_crc(tIAP *iap, uint32_t expect_crc);

// 跳转到默认分区 / 复位回 BL
bool iap_jump(tIAP *iap);
bool iap_reset(tIAP *iap);

#endif // __IAP_H
