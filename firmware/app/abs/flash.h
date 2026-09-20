#ifndef __FLASH_H
#define __FLASH_H

#include "device.h"

// ============================================================
// flash.h — Flash 介质 + 日志式存储单元（abs）
//
// 接口形态：ops + handle（装配层在定义 tFlash 对象时挂上）。
// 介质差异（MCU 内部 Flash / 外部 SPI NOR）由 ops 承载，
// 本层不出现任何介质标识。
//
// [介质能力] 一块可寻址、扇区擦除的 NOR Flash。
// [业务对象] tFlashUnit：顺序追加记录的存储单元（写满才擦除，磨损友好）。
//
// 地址为介质内绝对地址（从 0 起），几何参数由驱动提供。
// 上层保证：write 目标扇区已擦除；erase 的 addr/len 按扇区对齐
// （驱动内部也会做扇区粒度扩展）。
// ============================================================

// 驱动接口（板级实现）；handle 为驱动实例
typedef struct
{
    bool (*open)(void *handle);                                                  // 初始化介质，返回前资源须可用
    bool (*read)(void *handle, uint32_t addr, uint8_t *data, uint32_t len);      // 连续读
    bool (*write)(void *handle, uint32_t addr, const uint8_t *data, uint32_t len); // 连续写（目标须已擦除）
    bool (*erase_addr)(void *handle, uint32_t addr, uint32_t len);               // 擦除覆盖 [addr, addr+len)，按扇区向上取整
    bool (*erase_sector)(void *handle, uint8_t sec_id);                          // 擦除指定用户扇区
    uint8_t (*sector_count)(void *handle);                                       // 用户扇区数量
    uint32_t (*sector_addr)(void *handle, uint8_t sec_id);                       // 用户扇区基地址
    uint32_t (*sector_size)(void *handle, uint8_t sec_id);                       // 用户扇区大小
} tFlashOps;

// Flash 业务对象
typedef struct
{
    const tFlashOps *ops; // 驱动 ops（装配时挂）
    void *handle;         // 驱动实例（装配时挂）

    eDeviceStatus dstate; // 设备状态（本层维护）

    // 用户扇区注册位图：最多 16 个用户扇区，位 = 1 表示该扇区已被注册占用
    uint16_t usr_sector_bit_status;
    uint32_t usr_sector_count;
    uint8_t usr_sector_free;
} tFlash;

// ==================== 日志式存储单元 ====================

// 一个存储单元 = 介质上的一个擦除单元（扇区）。
// 顺序追加记录：写满才擦除，减少擦除次数（磨损友好）。
// 记录格式由上层定义；空闲边界由二分探测"首个未写位置"得到（重启后可恢复）。
typedef struct
{
    uint8_t id;         // 单元 ID（= 用户扇区编号）
    uint32_t base_addr; // 单元在介质中的基地址（须擦除单元对齐）
    uint32_t size;      // 单元大小（= 一个擦除单元）
    uint32_t free_addr; // 下一条记录的写入偏移（相对 base，0=空）
} tFlashUnit;

// 启动对象：ops/handle 须已由装配层挂好（任一为空返回 false）
bool flash_init(tFlash *s);

// 注册/注销 一个日志式存储单元（unit 由调用方提供，无堆分配）
bool flash_unit_register(tFlash *s, tFlashUnit *unit);
void flash_unit_unregister(tFlash *s, tFlashUnit *unit);

// 追加写入一条记录（自动落在 free_addr）；空间不足先擦除再从头写
bool flash_unit_append(tFlash *s, tFlashUnit *unit, const uint8_t *data, uint32_t len);

// 读上一条记录数据
bool flash_unit_read(tFlash *s, tFlashUnit *unit, uint8_t *data, uint32_t len);

// 擦除整个单元并复位写位置
bool flash_unit_erase(tFlash *s, tFlashUnit *unit);

#endif // __FLASH_H
