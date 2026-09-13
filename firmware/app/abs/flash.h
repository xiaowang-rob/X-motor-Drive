#ifndef __FLASH_H
#define __FLASH_H

#include "device.h"

// ============================================================
// flash.h — Flash 介质契约与日志式存储单元（abs）
//
// [驱动契约] 描述"一块可寻址、扇区擦除的 NOR Flash 介质"的最小能力。
//   驱动只返回单次操作的 bool 成败；设备状态由本层 tFlash.dstate 持有。
// [业务对象] tFlashUnit：顺序追加记录的存储单元（写满才擦除，磨损友好）。
//
// 地址为芯片内绝对地址（从 0 起），几何参数由驱动提供。
// 上层保证：write 目标扇区已擦除；erase 的 addr/len 按扇区对齐
// （驱动内部也会做扇区粒度扩展）。
// ============================================================

typedef void *FlashChipHandle;

typedef struct
{
    // 芯片初始化（含连接校验，如 JEDEC ID）；MCU 内部 Flash 可为 NULL
    bool (*init)(FlashChipHandle h);

    // 连续读取 len 字节
    bool (*read)(FlashChipHandle h, uint32_t addr, uint8_t *data, uint32_t len);

    // 连续写入（自动处理页边界），调用方保证目标扇区已擦除
    bool (*write)(FlashChipHandle h, uint32_t addr, const uint8_t *data, uint32_t len);

    // 擦除从 addr 起覆盖 len 的区域（按扇区粒度向上取整）
    bool (*erase_addr)(FlashChipHandle h, uint32_t addr, uint32_t len);

    // 擦除指定扇区（用户区编号）
    bool (*erase_sector)(FlashChipHandle h, uint8_t sec_id);

    // 用户扇区几何
    uint8_t (*get_sector_count)(FlashChipHandle h);
    uint32_t (*get_sector_addr)(FlashChipHandle h, uint8_t sec_id);
    uint32_t (*get_sector_size)(FlashChipHandle h, uint8_t sec_id);
} tFlashDriverOps;

// Flash 业务对象
typedef struct
{
    const tFlashDriverOps *ops; // 绑定的介质驱动 ops
    FlashChipHandle handle;     // 介质句柄
    eDeviceStatus dstate;       // 设备状态（本层维护）

    // 用户扇区注册位图：最多 16 个用户扇区，位 = 1 表示该扇区已被注册占用
    uint16_t usr_sector_bit_status;
    uint32_t usr_sector_count;
    uint8_t usr_sector_free;
} tFlash;

// ==================== 业务对象：日志式存储单元 ====================

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

bool flash_init(tFlash *s, const tFlashDriverOps *ops, FlashChipHandle h);

// 注册/注销 一个日志式存储单元（unit 由调用方提供，无堆分配）
bool flash_unit_register(tFlash *s, tFlashUnit *unit);
void flash_unit_unregister(tFlash *s, tFlashUnit *unit);

// 追加写入一条记录（自动落在 free_addr）；空间不足先擦除再从头写
bool flash_unit_append(tFlash *s, tFlashUnit *unit, const uint8_t *data, uint32_t len);

// 读上一条记录数据
bool flash_unit_read(tFlash *s, tFlashUnit *unit, uint8_t *data, uint32_t len);

// 擦除整个单元并复位写位置
bool flash_unit_erase(tFlash *s, tFlashUnit *unit);

#endif // __ABS_FLASH_H
