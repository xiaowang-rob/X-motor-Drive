#ifndef __ABS_FLASH_H
#define __ABS_FLASH_H

#include "device.h"

// ============================================================
// flash.h — 串行/并行 Flash 介质驱动契约
//
// 本契约描述"一块可寻址、扇区擦除的 NOR Flash 介质"的最小能力，
// 供 abs 层做单元磨损 / IAP 编排时调用，也约束驱动实现（w25qxx 等）。
// 地址为芯片内绝对地址（从 0 起），几何参数由驱动提供。
//
// 上层保证：write/erase 操作区在已擦除的扇区上；erase 的 addr/len
// 为扇区大小对齐（驱动内部也会做扇区粒度扩展）。
// ============================================================

typedef void *FlashChipHandle;

typedef struct
{
    // 芯片初始化（含连接校验，如 JEDEC ID）
    bool (*init)(FlashChipHandle h);

    // 连续读取 len 字节
    bool (*read)(FlashChipHandle h, uint32_t addr, uint8_t *data, uint32_t len);

    // 连续写入（自动处理页边界），调用方保证目标扇区已擦除
    bool (*write)(FlashChipHandle h, uint32_t addr, const uint8_t *data, uint32_t len);

    // 擦除从 addr 起覆盖 len 的区域（按扇区粒度向上取整）
    bool (*erase_addr)(FlashChipHandle h, uint32_t addr, uint32_t len);

    // 擦除指定扇区
    bool (*erase_sector)(FlashChipHandle h, uint8_t sec_id);

    // ---- 几何 ----// 获取 Bootloader 分区 ID 和 数量
    uint32_t (*get_bl_ids)(FlashChipHandle h, uint8_t *num);
    uint32_t (*get_app_ids)(FlashChipHandle h, uint8_t *num); // 获取 App 分区 ID
    uint32_t (*get_usr_ids)(FlashChipHandle h, uint8_t *num); // 获取用户分区 ID
    uint32_t (*get_sector_addr)(FlashChipHandle h, uint8_t sec_id);
    uint32_t (*get_sector_size)(FlashChipHandle h, uint8_t sec_id);
    bool (*jump_app)(FlashChipHandle h);
    bool (*jump_bl)(FlashChipHandle h);
    // 设备状态（eDeviceStatus 值）
    uint8_t (*get_state)(FlashChipHandle h);
} tFlashDriverOps;

// flash对象
typedef struct
{
    const tFlashDriverOps *ops; // 绑定的介质驱动 ops
    FlashChipHandle handle;     // 介质句柄

    // 分区信息
    const uint8_t *bl_sector_ids;
    const uint8_t *app_sector_ids;
    const uint8_t *usr_sector_ids;

    uint32_t bl_sector_count;  // Bootloader 分区数
    uint32_t app_sector_count; // App 分区数
    uint32_t usr_sector_count; // 用户分区数

    uint16_t usr_sector_bit_status; // 用户分区注册 位状态（0=未注册，1=已注册/不存在）
    uint8_t usr_sector_free;
} tFlash;

bool flash_init(tFlash *s, const tFlashDriverOps *ops, FlashChipHandle h);

// ==================== 业务对象：日志式存储单元 ====================

// 一个存储单元 = 介质上的一个擦除单元（扇区）区间。
// 采用"顺序追加记录"的日志式写法：写满才擦除，减少擦除次数（磨损友好）。
// 记录格式由上层定义；本层保证：写入字节后该区域不再为全 0xFF，
// 空闲边界由二分探测"首个未写位置"得到（重启后可恢复）。

typedef struct
{
    uint8_t id;         // 单元 ID（由上层定义)
    uint32_t base_addr; // 单元在介质中的基地址（须擦除单元对齐）
    uint32_t size;      // 单元大小（= 一个擦除单元）
    uint32_t free_addr; // 下一条记录的写入偏移（相对 base，0=空）
} tFlashUnit;

// 注册/注销 一个日志式存储单元
tFlashUnit *flash_unit_register(tFlash *s);
void flash_unit_unregister(tFlash *s, tFlashUnit *unit);

// 追加写入一条记录（自动落在 free_addr）；空间不足擦除从头开始写
bool flash_unit_append(tFlash *s, tFlashUnit *unit, const uint8_t *data, uint32_t len);

// 读上一条记录数据
bool flash_unit_read(tFlash *s, tFlashUnit *unit, uint8_t *data, uint32_t len);

// 擦除整个单元并复位写位置
bool flash_unit_erase(tFlash *s, tFlashUnit *unit);

// ==================== 业务对象：IAP 固件升级编排 ====================

// 便捷：擦写/校验整个 App / BL 区
bool flash_iap_erase_app(tFlash *s);
bool flash_iap_write_app(tFlash *s, uint32_t offset,
                         const uint8_t *data, uint32_t size);
bool flash_iap_verify_app(tFlash *s, uint32_t offset,
                          const uint8_t *data, uint32_t size);
bool flash_iap_erase_bl(tFlash *s);
bool flash_iap_write_bl(tFlash *s, uint32_t offset,
                        const uint8_t *data, uint32_t size);
bool flash_iap_verify_bl(tFlash *s, uint32_t offset,
                         const uint8_t *data, uint32_t size);

// 跳转 / 复位
void flash_iap_jump_app(tFlash *s);
void flash_iap_reset(tFlash *s);

#endif // __ABS_FLASH_H
