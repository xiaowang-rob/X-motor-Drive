#ifndef XDR_APP_ABS_IAP_H
#define XDR_APP_ABS_IAP_H

#include "flash.h"

// ============================================================
// iap.h — 在线升级（IAP）业务对象（abs）
//
// 设计要点：
//   1) IAP **不自建读写 ops**，而是复用 Flash 介质（flash_board 钩子）
//      + 一份分区表；
//   2) 跳转能力是平台行为，由板级以回调注入；
//   3) 校验采用**整区 CRC32**：上位机只给期望值，设备流式遍历 flash 计算，
//      无需把固件数据再搬一遍（省 RAM 与传输）。
//
// 编译期绑定：介质经 eFlashDev 指定，操作走 flash_board_* 直接调用。
// ============================================================

typedef enum
{
    IAP_BL = 0,
    IAP_APP
} eIAPtype;

#define IAP_PART_COUNT 2U

// 分区几何（绝对地址 / 大小）
typedef struct
{
    uint32_t base;
    uint32_t size;
} tIAPPartition;

// IAP 配置：介质 + 分区表 + 平台跳转
typedef struct
{
    eFlashDev flash_dev;        // 复用的介质
    const tIAPPartition *parts; // 分区表，按 eIAPtype 索引
    uint8_t part_count;         // 分区表长度
    bool (*jump)(eIAPtype type); // 平台跳转（板级注入）
    eIAPtype type;              // 默认操作分区
} tIAPConfig;

typedef struct
{
    tIAPConfig cfg;       // 配置副本
    eDeviceStatus dstate; // 设备状态（本层维护）
} tIAP;

bool iap_init(tIAP *iap, const tIAPConfig *cfg);

// ---- 分区操作（以 iap->cfg.type 为默认分区） ----
bool iap_erase(tIAP *iap);
bool iap_write(tIAP *iap, uint32_t offset, const uint8_t *data, uint32_t len);
bool iap_read(tIAP *iap, uint32_t offset, uint8_t *data, uint32_t len);

// 整区校验：流式计算整个分区的 CRC32 并与 expect_crc 比对
bool iap_verify_crc(tIAP *iap, uint32_t expect_crc);

// 跳转到默认分区 / 复位回 BL
bool iap_jump(tIAP *iap);
bool iap_reset(tIAP *iap);

// 取分区几何（供上层计算偏移/大小）
const tIAPPartition *iap_get_partition(const tIAP *iap, eIAPtype type);

#endif // XDR_APP_ABS_IAP_H
